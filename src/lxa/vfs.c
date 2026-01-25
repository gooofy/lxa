#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>
#include "vfs.h"
#include "util.h"

typedef struct drive_map_s drive_map_t;
struct drive_map_s {
    drive_map_t *next;
    char *amiga_name;
    char *linux_path;
};

static drive_map_t *g_drive_maps = NULL;

void vfs_init(void) {
    // Initialization if needed
}

bool vfs_add_drive(const char *amiga_name, const char *linux_path) {
    drive_map_t *map = malloc(sizeof(drive_map_t));
    if (!map) return false;

    map->amiga_name = strdup(amiga_name);
    
    // Convert to absolute path if needed
    char abs_path[PATH_MAX];
    if (linux_path[0] != '/') {
        if (!realpath(linux_path, abs_path)) {
            // If realpath fails, just use the path as-is
            map->linux_path = strdup(linux_path);
        } else {
            map->linux_path = strdup(abs_path);
        }
    } else {
        map->linux_path = strdup(linux_path);
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
