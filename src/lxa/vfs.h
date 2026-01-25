#ifndef HAVE_VFS_H
#define HAVE_VFS_H

#include <stddef.h>
#include <stdbool.h>

void vfs_init(void);
bool vfs_add_drive(const char *amiga_name, const char *linux_path);
bool vfs_resolve_path(const char *amiga_path, char *linux_path, size_t maxlen);

#endif
