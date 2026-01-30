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

void vfs_init(void) {
    /* Compute the lxa home directory path */
    const char *home = get_user_home();
    if (home) {
        snprintf(g_lxa_home, sizeof(g_lxa_home), "%s/.lxa", home);
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

bool vfs_setup_environment(void)
{
    if (!g_lxa_home[0]) {
        DPRINTF(LOG_ERROR, "vfs: lxa home directory not set\n");
        return false;
    }
    
    /* Check if ~/.lxa already exists */
    struct stat st;
    if (stat(g_lxa_home, &st) == 0 && S_ISDIR(st.st_mode)) {
        DPRINTF(LOG_DEBUG, "vfs: lxa home directory %s already exists\n", g_lxa_home);
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
    
    /* Create default config.ini */
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
        "# SYS = /path/to/amiga/system\n"
        "# Note: If SYS is not specified, it defaults to the current directory\n"
        "# Uncomment the line below to use ~/.lxa/System as SYS:\n"
        "# SYS = ");
    config_len += (size_t)snprintf(config_content + config_len, sizeof(config_content) - config_len,
        "%s/System\n"
        "\n"
        "[floppies]\n"
        "# Map floppy drives to directories\n"
        "# DF0 = ~/.lxa/floppy0\n",
        g_lxa_home);
    
    if (!write_file_if_missing(path, config_content)) return false;
    
    /* Create default Startup-Sequence */
    if (!make_path(path, sizeof(path), "/System/S/Startup-Sequence")) return false;
    
    const char *startup_content =
        "; lxa Startup-Sequence\n"
        "; Default startup script generated by lxa\n"
        ";\n"
        "; This file is executed when the system boots.\n"
        "; Add your startup commands here.\n"
        "\n"
        "; Set up command search path\n"
        "; Path SYS:C ADD\n"
        "\n"
        "; Display welcome message\n"
        "; Echo \"Welcome to lxa - Linux Amiga Emulation Layer\"\n"
        "; Echo \"\"\n";
    
    if (!write_file_if_missing(path, startup_content)) return false;
    
    fprintf(stderr, "lxa: Environment created successfully.\n");
    return true;
}
