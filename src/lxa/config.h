#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H

#include <stdbool.h>

bool config_load(const char *path);
const char *config_get_rom_path(void);
int config_get_ram_size(void);

#endif
