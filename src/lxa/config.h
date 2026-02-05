#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H

#include <stdbool.h>

bool config_load(const char *path);
const char *config_get_rom_path(void);
int config_get_ram_size(void);

/*
 * Phase 15: Rootless Mode Configuration
 * When enabled, each Amiga window gets its own SDL window instead of
 * all windows being rendered on a single screen surface.
 */
bool config_get_rootless_mode(void);
void config_set_rootless_mode(bool enable);

#endif
