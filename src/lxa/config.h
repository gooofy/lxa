#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H

#include <stdbool.h>

bool config_load(const char *path);
void config_reset(void);
const char *config_get_rom_path(void);
int config_get_ram_size(void);

/*
 * Phase 15: Rootless Mode Configuration
 * When enabled, each Amiga window gets its own SDL window instead of
 * all windows being rendered on a single screen surface.
 */
bool config_get_rootless_mode(void);
void config_set_rootless_mode(bool enable);

/*
 * Host sound output (the emulated audio hardware runs either way).
 * Command line > config.ini [audio] enabled > LXA_AUDIO env (0/1, on/off) >
 * default (on for interactive lxa; liblxa sets it off: tests run silently).
 */
void config_set_audio(bool enable);
void config_set_audio_default(bool enable);
bool config_get_audio_enabled(void);

#endif
