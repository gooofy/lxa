#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <linux/limits.h>
#include <pwd.h>
#include "vfs.h"
#include "util.h"

typedef struct drive_map_s drive_map_t;
struct drive_map_s {
    drive_map_t *next;
    char *amiga_name;
    char *linux_path;
};

static drive_map_t *g_drive_maps = NULL;
static char g_lxa_home[PATH_MAX] = "";

/*
 * Phase 7: Assignment System
 *
 * Assigns are stored separately from drives. An assign can have multiple
 * paths (for multi-assigns created with AssignAdd). We store them in a
 * linked list of assign entries, each containing a linked list of paths.
 */

typedef struct assign_path_s assign_path_t;
struct assign_path_s {
    assign_path_t *next;
    char *linux_path;
};

typedef struct assign_entry_s assign_entry_t;
struct assign_entry_s {
    assign_entry_t *next;
    char *name;              /* Assign name (without colon) */
    assign_type_t type;      /* Type of assign */
    assign_path_t *paths;    /* List of paths (first is primary) */
};

static assign_entry_t *g_assigns = NULL;

/* Forward declaration for find_assign (used by vfs_resolve_path) */
static assign_entry_t *find_assign(const char *name);

/* Get the user's home directory */
static const char *get_user_home(void)
{
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    return home;
}

/* Static buffer for data directory path */
static char g_data_dir[PATH_MAX] = "";

/* Get the installation data directory */
static const char *get_data_dir(void)
{
    /* Return cached result if already found */
    if (g_data_dir[0]) return g_data_dir;
    
    const char *data_dir = getenv("LXA_DATA_DIR");
    if (data_dir) {
        strncpy(g_data_dir, data_dir, PATH_MAX - 1);
        g_data_dir[PATH_MAX - 1] = '\0';
        return g_data_dir;
    }
    
    /* Try standard locations */
    if (access("/usr/share/lxa/System", F_OK) == 0) {
        strcpy(g_data_dir, "/usr/share/lxa");
        return g_data_dir;
    }
    if (access("/usr/local/share/lxa/System", F_OK) == 0) {
        strcpy(g_data_dir, "/usr/local/share/lxa");
        return g_data_dir;
    }
    
    /* Try to find data directory relative to executable */
    /* lxa binary is typically at: <prefix>/bin/lxa or build/host/bin/lxa */
    /* sys files are at: <prefix>/share/lxa or build/target/sys */
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        
        /* Find the directory containing the executable */
        char *last_slash = strrchr(exe_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            
            /* Try ../share/lxa (installed layout: bin/lxa -> share/lxa) */
            char try_path[PATH_MAX];
            int n = snprintf(try_path, sizeof(try_path), "%s/../share/lxa/System", exe_path);
            if (n > 0 && (size_t)n < sizeof(try_path) && access(try_path, F_OK) == 0) {
                n = snprintf(g_data_dir, sizeof(g_data_dir), "%s/../share/lxa", exe_path);
                if (n > 0 && (size_t)n < sizeof(g_data_dir)) {
                    /* Normalize the path */
                    char *real = realpath(g_data_dir, NULL);
                    if (real) {
                        strncpy(g_data_dir, real, PATH_MAX - 1);
                        g_data_dir[PATH_MAX - 1] = '\0';
                        free(real);
                        return g_data_dir;
                    }
                }
            }
            
            /* Try ../../target/sys (build layout: build/host/bin/lxa -> build/target/sys) */
            n = snprintf(try_path, sizeof(try_path), "%s/../../target/sys/System", exe_path);
            if (n > 0 && (size_t)n < sizeof(try_path) && access(try_path, F_OK) == 0) {
                n = snprintf(g_data_dir, sizeof(g_data_dir), "%s/../../target/sys", exe_path);
                if (n > 0 && (size_t)n < sizeof(g_data_dir)) {
                    char *real = realpath(g_data_dir, NULL);
                    if (real) {
                        strncpy(g_data_dir, real, PATH_MAX - 1);
                        g_data_dir[PATH_MAX - 1] = '\0';
                        free(real);
                        return g_data_dir;
                    }
                }
            }
        }
    }
    
    return NULL;
}

void vfs_init(void) {
    /* Check for LXA_PREFIX environment variable first */
    const char *prefix = getenv("LXA_PREFIX");
    if (prefix) {
        strncpy(g_lxa_home, prefix, PATH_MAX - 1);
        g_lxa_home[PATH_MAX - 1] = '\0';
        DPRINTF(LOG_INFO, "vfs: Using LXA_PREFIX=%s\n", g_lxa_home);
    } else {
        /* Fall back to ~/.lxa */
        const char *home = get_user_home();
        if (home) {
            snprintf(g_lxa_home, sizeof(g_lxa_home), "%s/.lxa", home);
        }
    }
}

bool vfs_add_drive(const char *amiga_name, const char *linux_path) {
    drive_map_t *map = malloc(sizeof(drive_map_t));
    if (!map) return false;

    map->amiga_name = strdup(amiga_name);
    
    char expanded_path[PATH_MAX];
    const char *path_to_use = linux_path;

    // Handle tilde expansion
    if (linux_path[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (home) {
            snprintf(expanded_path, sizeof(expanded_path), "%s%s", home, linux_path + 1);
            path_to_use = expanded_path;
        }
    }

    // Convert to absolute path if needed
    char abs_path[PATH_MAX];
    if (path_to_use[0] != '/') {
        if (!realpath(path_to_use, abs_path)) {
            // If realpath fails, just use the path as-is
            map->linux_path = strdup(path_to_use);
        } else {
            map->linux_path = strdup(abs_path);
        }
    } else {
        map->linux_path = strdup(path_to_use);
    }
    
    map->next = g_drive_maps;
    g_drive_maps = map;

    DPRINTF(LOG_DEBUG, "vfs: mapped %s: to %s\n", amiga_name, map->linux_path);
    return true;
}

static bool resolve_case(const char *base, const char *component, char *resolved, size_t maxlen) {
    DIR *dir = opendir(base);
    if (!dir) return false;

    struct dirent *de;
    bool found = false;
    while ((de = readdir(dir)) != NULL) {
        if (strcasecmp(de->d_name, component) == 0) {
            snprintf(resolved, maxlen, "%s/%s", base, de->d_name);
            found = true;
            break;
        }
    }
    closedir(dir);
    return found;
}

/* Helper function to resolve a path from a given root directory.
 * Returns true if the path exists, false otherwise.
 * If the last component doesn't exist but the parent does, still returns true
 * (for file creation scenarios).
 */
static bool resolve_path_from_root(const char *root, const char *relative_path, 
                                    char *linux_path, size_t maxlen) {
    char work_path[PATH_MAX];
    strncpy(work_path, root, sizeof(work_path));
    work_path[sizeof(work_path) - 1] = '\0';
    
    char *path_copy = strdup(relative_path);
    if (!path_copy) return false;
    
    char *saveptr;
    char *comp = strtok_r(path_copy, "/", &saveptr);
    
    bool error = false;
    while (comp) {
        if (strlen(comp) == 0 || strcmp(comp, ".") == 0) {
            // skip
        } else if (strcmp(comp, "..") == 0) {
            char *last_slash = strrchr(work_path, '/');
            if (last_slash && last_slash != work_path) {
                *last_slash = '\0';
            }
        } else {
            char next_step[PATH_MAX];
            if (resolve_case(work_path, comp, next_step, sizeof(next_step))) {
                strncpy(work_path, next_step, sizeof(work_path));
            } else {
                /* Component not found - check if this is the last component */
                char *next_comp = strtok_r(NULL, "/", &saveptr);
                if (next_comp) {
                    /* Not the last component - intermediate directory doesn't exist */
                    error = true;
                    break;
                } else {
                    /* Last component - append it and return (might be creating file) */
                    int n = snprintf(next_step, sizeof(next_step), "%s/%s", work_path, comp);
                    if (n >= (int)sizeof(next_step)) {
                        error = true;
                        break;
                    }
                    strncpy(work_path, next_step, sizeof(work_path));
                }
                break;
            }
        }
        comp = strtok_r(NULL, "/", &saveptr);
    }
    
    free(path_copy);
    if (error) return false;
    
    strncpy(linux_path, work_path, maxlen);
    linux_path[maxlen - 1] = '\0';
    return true;
}

bool vfs_resolve_path(const char *amiga_path, char *linux_path, size_t maxlen) {
    if (!strncasecmp(amiga_path, "NIL:", 4)) {
        strncpy(linux_path, "/dev/null", maxlen);
        return true;
    }

    /*
     * Handle absolute Linux paths - pass through unchanged.
     * BUT: In AmigaOS, a path starting with '/' means "parent directory",
     * so we only treat it as a Linux absolute path if it looks like one:
     * - Must start with '/' followed by an alphanumeric character
     * - Paths like '//' or '/' followed by another '/' are Amiga parent paths
     */
    if (amiga_path[0] == '/' && amiga_path[1] != '\0' && amiga_path[1] != '/') {
        /* Could be Linux absolute path - check if second char is alphanumeric */
        if ((amiga_path[1] >= 'a' && amiga_path[1] <= 'z') ||
            (amiga_path[1] >= 'A' && amiga_path[1] <= 'Z') ||
            (amiga_path[1] >= '0' && amiga_path[1] <= '9')) {
            strncpy(linux_path, amiga_path, maxlen);
            linux_path[maxlen - 1] = '\0';
            DPRINTF(LOG_DEBUG, "vfs: absolute Linux path: %s\n", amiga_path);
            return true;
        }
    }

    const char *p = amiga_path;
    char *colon = strchr(amiga_path, ':');
    const char *root = NULL;
    assign_entry_t *assign = NULL;

    if (colon) {
        size_t name_len = colon - amiga_path;
        char drive_name[name_len + 1];
        strncpy(drive_name, amiga_path, name_len);
        drive_name[name_len] = '\0';

        /* Phase 7: Check assigns first (they take precedence over drives) */
        assign = find_assign(drive_name);
        if (assign && assign->paths) {
            /*
             * Multi-assign support: Try each path in the assign until we find
             * a file that exists. This is important for programs that expect
             * their local directories to be added to standard assigns.
             */
            p = colon + 1;
            
            /* For multi-assigns, try each path */
            for (assign_path_t *ap = assign->paths; ap; ap = ap->next) {
                char candidate[PATH_MAX];
                if (resolve_path_from_root(ap->linux_path, p, candidate, sizeof(candidate))) {
                    struct stat st;
                    if (stat(candidate, &st) == 0) {
                        /* File exists in this assign path */
                        strncpy(linux_path, candidate, maxlen);
                        linux_path[maxlen - 1] = '\0';
                        DPRINTF(LOG_DEBUG, "vfs: resolved multi-assign %s:%s -> %s\n", 
                                drive_name, p, linux_path);
                        return true;
                    }
                }
            }
            
            /* If file not found in any path, use the first path (for file creation) */
            root = assign->paths->linux_path;
            DPRINTF(LOG_DEBUG, "vfs: resolved assign %s: -> %s (default)\n", drive_name, root);
        }

        /* Then check drives */
        if (!root) {
            drive_map_t *map = g_drive_maps;
            while (map) {
                if (strcasecmp(map->amiga_name, drive_name) == 0) {
                    root = map->linux_path;
                    break;
                }
                map = map->next;
            }
        }
        if (!root) {
            // Drive not found in VFS mappings - this is not an error
            // Just return false so the caller can use fallback logic
            return false;
        }
        p = colon + 1;
    } else {
        // No colon means it's a relative path or a plain path
        // For backward compatibility, if no drives are configured,
        // return false to use fallback logic
        if (!g_drive_maps) {
            return false;
        }
        
        // Try to find SYS: drive
        drive_map_t *map = g_drive_maps;
        while (map) {
            if (strcasecmp(map->amiga_name, "SYS") == 0) {
                root = map->linux_path;
                break;
            }
            map = map->next;
        }
        if (!root) return false;
    }

    // Use the helper to resolve from root
    return resolve_path_from_root(root, p, linux_path, maxlen);
}

const char *vfs_get_home_dir(void)
{
    return g_lxa_home[0] ? g_lxa_home : NULL;
}

bool vfs_has_sys_drive(void)
{
    drive_map_t *map = g_drive_maps;
    while (map) {
        if (strcasecmp(map->amiga_name, "SYS") == 0) {
            return true;
        }
        map = map->next;
    }
    return false;
}

/* Create a directory if it doesn't exist */
static bool ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    if (mkdir(path, 0755) == 0) {
        DPRINTF(LOG_INFO, "vfs: created directory %s\n", path);
        return true;
    }
    DPRINTF(LOG_ERROR, "vfs: failed to create directory %s: %s\n", path, strerror(errno));
    return false;
}

/* Write a text file if it doesn't exist */
static bool write_file_if_missing(const char *path, const char *content)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        /* File already exists */
        return true;
    }
    
    FILE *f = fopen(path, "w");
    if (!f) {
        DPRINTF(LOG_ERROR, "vfs: failed to create %s: %s\n", path, strerror(errno));
        return false;
    }
    
    fputs(content, f);
    fclose(f);
    DPRINTF(LOG_INFO, "vfs: created %s\n", path);
    return true;
}

/* Copy a file from source to destination (optionally force overwrite) */
static bool copy_file_ex(const char *src_path, const char *dst_path, bool force)
{
    struct stat st;
    if (!force && stat(dst_path, &st) == 0) {
        /* Destination already exists and we're not forcing */
        return true;
    }
    
    FILE *src = fopen(src_path, "rb");
    if (!src) {
        DPRINTF(LOG_DEBUG, "vfs: source file not found: %s\n", src_path);
        return false;
    }
    
    FILE *dst = fopen(dst_path, "wb");
    if (!dst) {
        DPRINTF(LOG_ERROR, "vfs: failed to create %s: %s\n", dst_path, strerror(errno));
        fclose(src);
        return false;
    }
    
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {
            DPRINTF(LOG_ERROR, "vfs: failed to write to %s\n", dst_path);
            fclose(src);
            fclose(dst);
            return false;
        }
    }
    
    fclose(src);
    fclose(dst);
    DPRINTF(LOG_INFO, "vfs: copied %s -> %s\n", src_path, dst_path);
    return true;
}

/* Copy a file from source to destination (don't overwrite existing) */
static bool copy_file(const char *src_path, const char *dst_path)
{
    return copy_file_ex(src_path, dst_path, false);
}

/* Check if source file is different from destination (needs update) */
static bool needs_update(const char *src_path, const char *dst_path)
{
    struct stat src_st, dst_st;
    
    if (stat(src_path, &src_st) != 0) {
        return false; /* Source doesn't exist */
    }
    
    if (stat(dst_path, &dst_st) != 0) {
        return true; /* Destination doesn't exist */
    }
    
    /* Update if size differs or source is newer */
    if (src_st.st_size != dst_st.st_size) {
        return true;
    }
    
    if (src_st.st_mtime > dst_st.st_mtime) {
        return true;
    }
    
    return false;
}

/* Update a file if the source is different (newer or different size) */
static bool update_file_if_changed(const char *src_path, const char *dst_path)
{
    if (needs_update(src_path, dst_path)) {
        DPRINTF(LOG_INFO, "vfs: updating %s\n", dst_path);
        return copy_file_ex(src_path, dst_path, true);
    }
    return true;
}

/* Copy system template files from installation directory */
static bool copy_system_template(const char *data_dir, const char *system_dir)
{
    (void)system_dir; /* unused parameter */
    
    if (!data_dir) return false;
    
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
    int n;
    
    /* NOTE: We don't copy config.ini from the template because we need
     * to generate our own with SYS drive properly configured to point
     * to ~/.lxa/System. The template config.ini has SYS commented out. */
    
    /* Copy system files from template System directory */
    n = snprintf(src_path, sizeof(src_path), "%s/System", data_dir);
    if (n < 0 || (size_t)n >= sizeof(src_path)) return false;
    
    /* Try to copy C/ commands */
    DIR *dir = opendir(src_path);
    if (!dir) {
        DPRINTF(LOG_DEBUG, "vfs: system template not found at %s\n", src_path);
        return false;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        /* Copy subdirectories (C, S, Libs, Devs, T) */
        n = snprintf(src_path, sizeof(src_path), "%s/System/%s", data_dir, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(src_path)) continue;
        n = snprintf(dst_path, sizeof(dst_path), "%s/System/%s", g_lxa_home, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(dst_path)) continue;
        
        struct stat st;
        if (stat(src_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            /* Create destination directory */
            ensure_dir(dst_path);
            
            /* Copy files in this directory */
            DIR *subdir = opendir(src_path);
            if (subdir) {
                struct dirent *subentry;
                while ((subentry = readdir(subdir)) != NULL) {
                    if (subentry->d_name[0] == '.') continue;
                    
                    char src_file[PATH_MAX];
                    char dst_file[PATH_MAX];
                    n = snprintf(src_file, sizeof(src_file), "%s/%s", src_path, subentry->d_name);
                    if (n < 0 || (size_t)n >= sizeof(src_file)) continue;
                    n = snprintf(dst_file, sizeof(dst_file), "%s/%s", dst_path, subentry->d_name);
                    if (n < 0 || (size_t)n >= sizeof(dst_file)) continue;
                    
                    struct stat fst;
                    if (stat(src_file, &fst) == 0 && S_ISREG(fst.st_mode)) {
                        copy_file(src_file, dst_file);
                        /* Make executable if it's a command */
                        chmod(dst_file, 0755);
                    }
                }
                closedir(subdir);
            }
        }
    }
    
    closedir(dir);
    return true;
}

/* Helper to safely create paths under g_lxa_home */
static bool make_path(char *buf, size_t buflen, const char *subpath)
{
    size_t home_len = strlen(g_lxa_home);
    size_t sub_len = strlen(subpath);
    
    if (home_len + sub_len + 1 >= buflen) {
        DPRINTF(LOG_ERROR, "vfs: path too long: %s%s\n", g_lxa_home, subpath);
        return false;
    }
    
    memcpy(buf, g_lxa_home, home_len);
    memcpy(buf + home_len, subpath, sub_len + 1);
    return true;
}

void vfs_setup_dynamic_drives(void)
{
    /* Set up HOME: drive pointing to user's home directory */
    const char *home = get_user_home();
    if (home) {
        vfs_add_drive("HOME", home);
        DPRINTF(LOG_INFO, "vfs: HOME: -> %s\n", home);
    }

    /* Set up CWD: drive pointing to current working directory */
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        vfs_add_drive("CWD", cwd);
        DPRINTF(LOG_INFO, "vfs: CWD: -> %s\n", cwd);
    }
}

void vfs_setup_default_assigns(void)
{
    /*
     * Set up standard AmigaOS assigns pointing to SYS: subdirectories.
     * These are the minimal set of assigns expected by the system.
     *
     * Note: We first resolve the SYS: path, then create assigns pointing
     * to the Linux paths. This avoids needing special handling for
     * "assign to assign" in the VFS layer.
     */
    
    char sys_path[PATH_MAX];
    char subdir_path[PATH_MAX];
    
    /* Get the resolved SYS: path */
    if (!vfs_resolve_path("SYS:", sys_path, sizeof(sys_path))) {
        DPRINTF(LOG_WARNING, "vfs: Cannot resolve SYS: for default assigns\n");
        return;
    }
    
    /* Standard AmigaOS assigns */
    struct {
        const char *name;
        const char *subdir;
    } default_assigns[] = {
        {"C",    "C"},       /* Commands */
        {"S",    "S"},       /* Startup scripts */
        {"L",    "L"},       /* Loaders */
        {"LIBS", "Libs"},    /* Libraries */
        {"DEVS", "Devs"},    /* Devices */
        {"T",    "T"},       /* Temporary files */
        {"ENV",  "Prefs/Env-Archive"},  /* Environment variables */
        {"ENVARC", "Prefs/Env-Archive"}, /* Environment archive */
        {"FONTS", "Fonts"},  /* Fonts */
        {NULL, NULL}
    };
    
    for (int i = 0; default_assigns[i].name != NULL; i++) {
        /* Skip if assign already exists (user may have custom config) */
        if (vfs_assign_exists(default_assigns[i].name)) {
            DPRINTF(LOG_DEBUG, "vfs: %s: already assigned, skipping\n", default_assigns[i].name);
            continue;
        }
        
        /* Build path to subdirectory */
        int n = snprintf(subdir_path, sizeof(subdir_path), "%s/%s", sys_path, default_assigns[i].subdir);
        if (n < 0 || (size_t)n >= sizeof(subdir_path)) {
            continue;
        }
        
        /* Check if the directory exists (don't create assigns to non-existent dirs) */
        struct stat st;
        if (stat(subdir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            DPRINTF(LOG_DEBUG, "vfs: %s: subdirectory %s doesn't exist, skipping\n", 
                    default_assigns[i].name, subdir_path);
            continue;
        }
        
        /* Create the assign */
        if (vfs_assign_add(default_assigns[i].name, subdir_path, ASSIGN_LOCK)) {
            DPRINTF(LOG_INFO, "vfs: %s: -> %s\n", default_assigns[i].name, subdir_path);
        }
    }
}

/* Update files in a directory from source to destination */
static int update_directory_files(const char *src_dir, const char *dst_dir)
{
    int updated_count = 0;
    
    DIR *dir = opendir(src_dir);
    if (!dir) return 0;
    
    /* Ensure destination directory exists */
    ensure_dir(dst_dir);
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char src_file[PATH_MAX];
        char dst_file[PATH_MAX];
        int n;
        
        n = snprintf(src_file, sizeof(src_file), "%s/%s", src_dir, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(src_file)) continue;
        n = snprintf(dst_file, sizeof(dst_file), "%s/%s", dst_dir, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(dst_file)) continue;
        
        struct stat st;
        if (stat(src_file, &st) == 0 && S_ISREG(st.st_mode)) {
            if (needs_update(src_file, dst_file)) {
                if (update_file_if_changed(src_file, dst_file)) {
                    chmod(dst_file, 0755);
                    updated_count++;
                }
            }
        }
    }
    closedir(dir);
    
    return updated_count;
}

/* Update system binaries from installation directory if newer versions exist */
static void update_system_binaries(void)
{
    const char *data_dir = get_data_dir();
    if (!data_dir) return;
    
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
    int n;
    int updated_count = 0;
    
    /*
     * Handle two possible directory structures:
     * 1. data_dir/System/{C,System,...}  (installed package structure)
     * 2. data_dir/{C,System,...}         (build output structure)
     * 
     * User's structure is always: ~/.lxa/System/{C,System,...}
     */
    
    /* First try: data_dir/System/... -> ~/.lxa/System/... */
    n = snprintf(src_path, sizeof(src_path), "%s/System", data_dir);
    if (n >= 0 && (size_t)n < sizeof(src_path)) {
        struct stat st;
        if (stat(src_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            DIR *dir = opendir(src_path);
            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (entry->d_name[0] == '.') continue;
                    
                    char sub_src[PATH_MAX];
                    char sub_dst[PATH_MAX];
                    n = snprintf(sub_src, sizeof(sub_src), "%s/System/%s", data_dir, entry->d_name);
                    if (n < 0 || (size_t)n >= sizeof(sub_src)) continue;
                    n = snprintf(sub_dst, sizeof(sub_dst), "%s/System/%s", g_lxa_home, entry->d_name);
                    if (n < 0 || (size_t)n >= sizeof(sub_dst)) continue;
                    
                    struct stat subst;
                    if (stat(sub_src, &subst) == 0 && S_ISDIR(subst.st_mode)) {
                        updated_count += update_directory_files(sub_src, sub_dst);
                    }
                }
                closedir(dir);
            }
        }
    }
    
    /* Second try: data_dir/C -> ~/.lxa/System/C, data_dir/System -> ~/.lxa/System/System */
    /* This handles the build output structure */
    const char *subdirs[] = {"C", "System", "Libs", "Devs", "L", NULL};
    for (int i = 0; subdirs[i]; i++) {
        n = snprintf(src_path, sizeof(src_path), "%s/%s", data_dir, subdirs[i]);
        if (n < 0 || (size_t)n >= sizeof(src_path)) continue;
        n = snprintf(dst_path, sizeof(dst_path), "%s/System/%s", g_lxa_home, subdirs[i]);
        if (n < 0 || (size_t)n >= sizeof(dst_path)) continue;
        
        struct stat st;
        if (stat(src_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            updated_count += update_directory_files(src_path, dst_path);
        }
    }
    
    if (updated_count > 0) {
        fprintf(stderr, "lxa: Updated %d system file(s) to latest version\n", updated_count);
    }
}

bool vfs_setup_environment(void)
{
    if (!g_lxa_home[0]) {
        DPRINTF(LOG_ERROR, "vfs: lxa home directory not set\n");
        return false;
    }
    
    /* Check if LXA_PREFIX already exists */
    struct stat st;
    if (stat(g_lxa_home, &st) == 0 && S_ISDIR(st.st_mode)) {
        DPRINTF(LOG_DEBUG, "vfs: lxa home directory %s already exists\n", g_lxa_home);
        
        /* Ensure new directories exist for existing installations */
        char path[PATH_MAX];
        if (make_path(path, sizeof(path), "/System/Fonts")) ensure_dir(path);
        if (make_path(path, sizeof(path), "/System/Prefs")) ensure_dir(path);
        if (make_path(path, sizeof(path), "/System/Prefs/Env-Archive")) ensure_dir(path);

        /* Check for and apply any updates to system binaries */
        update_system_binaries();
        return true;
    }
    
    fprintf(stderr, "lxa: First run detected - creating default environment at %s\n", g_lxa_home);
    
    /* Create the directory structure */
    if (!ensure_dir(g_lxa_home)) return false;
    
    char path[PATH_MAX];
    
    /* Create System directory (SYS:) */
    if (!make_path(path, sizeof(path), "/System")) return false;
    if (!ensure_dir(path)) return false;
    
    /* Create standard AmigaOS directories */
    if (!make_path(path, sizeof(path), "/System/S")) return false;
    if (!ensure_dir(path)) return false;
    
    if (!make_path(path, sizeof(path), "/System/C")) return false;
    if (!ensure_dir(path)) return false;
    
    if (!make_path(path, sizeof(path), "/System/Libs")) return false;
    if (!ensure_dir(path)) return false;
    
    if (!make_path(path, sizeof(path), "/System/Devs")) return false;
    if (!ensure_dir(path)) return false;
    
    if (!make_path(path, sizeof(path), "/System/T")) return false;
    if (!ensure_dir(path)) return false;
    
    /* Create L directory for localization */
    if (!make_path(path, sizeof(path), "/System/L")) return false;
    if (!ensure_dir(path)) return false;
    
    /* Create Fonts directory */
    if (!make_path(path, sizeof(path), "/System/Fonts")) return false;
    if (!ensure_dir(path)) return false;

    /* Create Prefs and Env-Archive */
    if (!make_path(path, sizeof(path), "/System/Prefs")) return false;
    if (!ensure_dir(path)) return false;

    if (!make_path(path, sizeof(path), "/System/Prefs/Env-Archive")) return false;
    if (!ensure_dir(path)) return false;
    
    /* Try to copy system files from installation or build directory */
    const char *data_dir = get_data_dir();
    if (data_dir) {
        fprintf(stderr, "lxa: Copying system files from %s\n", data_dir);
        copy_system_template(data_dir, path);
        /* Also use update_system_binaries to copy any binaries not in the template */
        update_system_binaries();
    } else {
        fprintf(stderr, "lxa: WARNING: No system files found to copy.\n");
        fprintf(stderr, "lxa: Shell and commands will not be available.\n");
        fprintf(stderr, "lxa: Set LXA_DATA_DIR or install lxa properly.\n");
    }
    
    /* Create default config.ini (only if not copied from template) */
    if (!make_path(path, sizeof(path), "/config.ini")) return false;

    char config_content[2048];
    size_t config_len = 0;
    config_len += (size_t)snprintf(config_content + config_len, sizeof(config_content) - config_len,
        "# lxa configuration file\n"
        "# Generated automatically on first run\n"
        "\n"
        "[system]\n"
        "# ROM and memory settings\n"
        "# rom_path = /path/to/lxa.rom\n"
        "# ram_size = 10485760\n"
        "\n"
        "[drives]\n"
        "# Map Amiga drives to Linux directories\n"
        "SYS = %s/System\n"
        "\n"
        "[floppies]\n"
        "# Map floppy drives to directories\n"
        "# DF0 = ~/.lxa/floppy0\n",
        g_lxa_home);
    
    if (!write_file_if_missing(path, config_content)) return false;
    
    /* Create default Startup-Sequence (only if not copied from template) */
    if (!make_path(path, sizeof(path), "/System/S/Startup-Sequence")) return false;
    
    const char *startup_content =
        "; lxa Startup-Sequence\n"
        "; Default startup script for lxa AmigaOS-compatible environment\n"
        ";\n"
        "; This file is executed when the system boots.\n"
        "; Uncomment and modify the commands below as needed.\n"
        "\n"
        "; Display welcome message\n"
        "Echo \"\"\n"
        "Echo \"Welcome to lxa - Linux Amiga Emulation Layer\"\n"
        "Echo \"AmigaOS-compatible environment running on Linux\"\n"
        "Echo \"\"\n"
        "\n"
        "; Set up command search path\n"
        "; Path SYS:C ADD\n"
        "\n"
        "; Display current directory\n"
        "; CD\n";
    
    if (!write_file_if_missing(path, startup_content)) return false;
    
    fprintf(stderr, "lxa: Environment created successfully.\n");
    fprintf(stderr, "lxa: Edit %s/config.ini to customize your setup.\n", g_lxa_home);
    return true;
}

/*
 * Phase 7: Assignment System Implementation
 */

/* Find an assign by name (case-insensitive) */
static assign_entry_t *find_assign(const char *name)
{
    for (assign_entry_t *a = g_assigns; a; a = a->next) {
        if (strcasecmp(a->name, name) == 0) {
            return a;
        }
    }
    return NULL;
}

bool vfs_assign_add(const char *name, const char *linux_path, assign_type_t type)
{
    DPRINTF(LOG_DEBUG, "vfs: vfs_assign_add(%s, %s, %d)\n", name, linux_path, type);
    
    /* Check if assign already exists */
    assign_entry_t *existing = find_assign(name);
    if (existing) {
        /* Replace the existing assign */
        /* Free old paths */
        assign_path_t *p = existing->paths;
        while (p) {
            assign_path_t *next = p->next;
            free(p->linux_path);
            free(p);
            p = next;
        }
        
        /* Create new path entry */
        assign_path_t *new_path = malloc(sizeof(assign_path_t));
        if (!new_path) return false;
        new_path->linux_path = strdup(linux_path);
        new_path->next = NULL;
        
        existing->paths = new_path;
        existing->type = type;
        
        DPRINTF(LOG_DEBUG, "vfs: replaced assign %s: -> %s\n", name, linux_path);
        return true;
    }
    
    /* Create new assign entry */
    assign_entry_t *entry = malloc(sizeof(assign_entry_t));
    if (!entry) return false;
    
    entry->name = strdup(name);
    entry->type = type;
    
    assign_path_t *path_entry = malloc(sizeof(assign_path_t));
    if (!path_entry) {
        free(entry->name);
        free(entry);
        return false;
    }
    path_entry->linux_path = strdup(linux_path);
    path_entry->next = NULL;
    
    entry->paths = path_entry;
    entry->next = g_assigns;
    g_assigns = entry;
    
    DPRINTF(LOG_DEBUG, "vfs: created assign %s: -> %s\n", name, linux_path);
    return true;
}

bool vfs_assign_remove(const char *name)
{
    DPRINTF(LOG_DEBUG, "vfs: vfs_assign_remove(%s)\n", name);
    
    assign_entry_t *prev = NULL;
    for (assign_entry_t *a = g_assigns; a; prev = a, a = a->next) {
        if (strcasecmp(a->name, name) == 0) {
            /* Remove from list */
            if (prev) {
                prev->next = a->next;
            } else {
                g_assigns = a->next;
            }
            
            /* Free paths */
            assign_path_t *p = a->paths;
            while (p) {
                assign_path_t *next = p->next;
                free(p->linux_path);
                free(p);
                p = next;
            }
            
            free(a->name);
            free(a);
            
            DPRINTF(LOG_DEBUG, "vfs: removed assign %s:\n", name);
            return true;
        }
    }
    
    return false; /* Not found */
}

bool vfs_assign_add_path(const char *name, const char *linux_path)
{
    DPRINTF(LOG_DEBUG, "vfs: vfs_assign_add_path(%s, %s)\n", name, linux_path);
    
    assign_entry_t *entry = find_assign(name);
    if (!entry) {
        /* Create new assign with this path */
        return vfs_assign_add(name, linux_path, ASSIGN_LOCK);
    }
    
    /* Add path to existing assign (at the end) */
    assign_path_t *new_path = malloc(sizeof(assign_path_t));
    if (!new_path) return false;
    new_path->linux_path = strdup(linux_path);
    new_path->next = NULL;
    
    /* Find end of path list */
    assign_path_t *last = entry->paths;
    if (!last) {
        entry->paths = new_path;
    } else {
        while (last->next) last = last->next;
        last->next = new_path;
    }
    
    DPRINTF(LOG_DEBUG, "vfs: added path to assign %s: -> %s\n", name, linux_path);
    return true;
}

/* Prepend a path to an existing assign (so it's searched first) */
bool vfs_assign_prepend_path(const char *name, const char *linux_path)
{
    DPRINTF(LOG_DEBUG, "vfs: vfs_assign_prepend_path(%s, %s)\n", name, linux_path);
    
    assign_entry_t *entry = find_assign(name);
    if (!entry) {
        /* Create new assign with this path */
        return vfs_assign_add(name, linux_path, ASSIGN_LOCK);
    }
    
    /* Prepend path to existing assign (at the beginning) */
    assign_path_t *new_path = malloc(sizeof(assign_path_t));
    if (!new_path) return false;
    new_path->linux_path = strdup(linux_path);
    new_path->next = entry->paths;  /* Link to existing paths */
    entry->paths = new_path;        /* Make this the first path */
    
    DPRINTF(LOG_DEBUG, "vfs: prepended path to assign %s: -> %s\n", name, linux_path);
    return true;
}

bool vfs_assign_remove_path(const char *name, const char *linux_path)
{
    assign_entry_t *entry = find_assign(name);
    if (!entry) return false;
    
    assign_path_t *prev = NULL;
    for (assign_path_t *p = entry->paths; p; prev = p, p = p->next) {
        if (strcmp(p->linux_path, linux_path) == 0) {
            if (prev) {
                prev->next = p->next;
            } else {
                entry->paths = p->next;
            }
            free(p->linux_path);
            free(p);
            
            /* If no paths left, remove the assign entirely */
            if (!entry->paths) {
                vfs_assign_remove(name);
            }
            
            return true;
        }
    }
    
    return false;
}

int vfs_assign_list(const char **names, const char **paths, int max_count)
{
    int count = 0;
    for (assign_entry_t *a = g_assigns; a && count < max_count; a = a->next) {
        if (names) names[count] = a->name;
        if (paths && a->paths) paths[count] = a->paths->linux_path;
        count++;
    }
    return count;
}

bool vfs_assign_exists(const char *name)
{
    return find_assign(name) != NULL;
}

const char *vfs_assign_get_path(const char *name)
{
    assign_entry_t *entry = find_assign(name);
    if (entry && entry->paths) {
        return entry->paths->linux_path;
    }
    return NULL;
}
