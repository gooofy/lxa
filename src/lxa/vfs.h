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

#endif
