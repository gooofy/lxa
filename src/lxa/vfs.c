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

bool vfs_resolve_path(const char *amiga_path, char *linux_path, size_t maxlen) {
    if (!strncasecmp(amiga_path, "NIL:", 4)) {
        strncpy(linux_path, "/dev/null", maxlen);
        return true;
    }

    char work_path[PATH_MAX];
    const char *p = amiga_path;
    char *colon = strchr(amiga_path, ':');
    const char *root = NULL;

    if (colon) {
        size_t name_len = colon - amiga_path;
        char drive_name[name_len + 1];
        strncpy(drive_name, amiga_path, name_len);
        drive_name[name_len] = '\0';

        drive_map_t *map = g_drive_maps;
        while (map) {
            if (strcasecmp(map->amiga_name, drive_name) == 0) {
                root = map->linux_path;
                break;
            }
            map = map->next;
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

    // Now resolve components case-insensitively
    strncpy(work_path, root, sizeof(work_path));
    
    char *path_copy = strdup(p);
    char *saveptr;
    char *comp = strtok_r(path_copy, "/", &saveptr);
    
    bool error = false;
    while (comp) {
        if (strlen(comp) == 0 || strcmp(comp, ".") == 0) {
            // skip
        } else if (strcmp(comp, "..") == 0) {
            // handle parent - though Amiga uses '/' for parent if it's multiple.
            // But strtok handles '/' as separator.
            char *last_slash = strrchr(work_path, '/');
            if (last_slash && last_slash != work_path) {
                *last_slash = '\0';
            }
        } else {
            char next_step[PATH_MAX];
            if (resolve_case(work_path, comp, next_step, sizeof(next_step))) {
                strncpy(work_path, next_step, sizeof(work_path));
            } else {
                // If not found, we might be creating a file.
                // For the last component, it's okay if it doesn't exist.
                // But for intermediate directories, it's an error.
                char *next_comp = strtok_r(NULL, "/", &saveptr);
                if (next_comp) {
                    error = true;
                    break;
                } else {
                    // Last component
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
    return true;
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

/* Check if source file is newer than destination file */
static bool is_newer(const char *src_path, const char *dst_path)
{
    struct stat src_st, dst_st;
    
    if (stat(src_path, &src_st) != 0) {
        return false; /* Source doesn't exist */
    }
    
    if (stat(dst_path, &dst_st) != 0) {
        return true; /* Destination doesn't exist, source is "newer" */
    }
    
    return src_st.st_mtime > dst_st.st_mtime;
}

/* Update a file if the source is newer */
static bool update_file_if_newer(const char *src_path, const char *dst_path)
{
    if (is_newer(src_path, dst_path)) {
        DPRINTF(LOG_INFO, "vfs: updating %s (newer version available)\n", dst_path);
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
    
    /* Copy config.ini if it exists in template */
    n = snprintf(src_path, sizeof(src_path), "%s/config.ini", data_dir);
    if (n < 0 || (size_t)n >= sizeof(src_path)) return false;
    n = snprintf(dst_path, sizeof(dst_path), "%s/config.ini", g_lxa_home);
    if (n < 0 || (size_t)n >= sizeof(dst_path)) return false;
    copy_file(src_path, dst_path);
    
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
            if (is_newer(src_file, dst_file)) {
                if (update_file_if_newer(src_file, dst_file)) {
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
