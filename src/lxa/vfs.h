#ifndef HAVE_VFS_H
#define HAVE_VFS_H

#include <stddef.h>
#include <stdbool.h>

void vfs_init(void);
bool vfs_add_drive(const char *amiga_name, const char *linux_path);
bool vfs_resolve_path(const char *amiga_path, char *linux_path, size_t maxlen);

/*
 * Phase 3: Automatic Environment Setup
 * 
 * Check if ~/.lxa exists and create it with the default structure if not.
 * Returns the path to the lxa home directory.
 * 
 * Structure created:
 *   ~/.lxa/
 *     config.ini         - Default configuration file
 *     System/            - Main system drive (SYS:)
 *       S/               - Startup scripts
 *         Startup-Sequence
 *       C/               - Commands
 *       Libs/            - Libraries
 *       Devs/            - Devices
 *       T/               - Temporary files
 */
bool vfs_setup_environment(void);

/* Get the lxa home directory path (~/.lxa) */
const char *vfs_get_home_dir(void);

/* Check if a SYS: drive is already configured */
bool vfs_has_sys_drive(void);

/* Set up automatic HOME: and CWD: drives */
void vfs_setup_dynamic_drives(void);

/*
 * Phase 7: Assignment System
 *
 * Assigns are logical name mappings that can point to:
 * - A single directory (AssignLock)
 * - A late-binding path resolved at access time (AssignLate)
 * - Multiple directories searched in order (AssignAdd for multi-assigns)
 *
 * Assigns are stored separately from drives and take precedence during
 * path resolution. An assign name does NOT include the trailing colon.
 */

/* Assign types */
typedef enum {
    ASSIGN_LOCK,    /* Points to a specific lock/directory */
    ASSIGN_LATE,    /* Late-binding: path resolved when accessed */
    ASSIGN_PATH     /* Non-binding path (like AssignPath) */
} assign_type_t;

/* Create an assign pointing to a Linux path (like AssignLock with resolved path) */
bool vfs_assign_add(const char *name, const char *linux_path, assign_type_t type);

/* Remove an assign */
bool vfs_assign_remove(const char *name);

/* Add a path to a multi-assign (creates if doesn't exist) */
bool vfs_assign_add_path(const char *name, const char *linux_path);

/* Remove a specific path from a multi-assign */
bool vfs_assign_remove_path(const char *name, const char *linux_path);

/* List all assigns (returns count, fills names array up to max_count) */
int vfs_assign_list(const char **names, const char **paths, int max_count);

/* Check if an assign exists */
bool vfs_assign_exists(const char *name);

/* Get the path(s) for an assign (returns first path, or NULL if not found) */
const char *vfs_assign_get_path(const char *name);

#endif
