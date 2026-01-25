#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "vfs.h"
#include "util.h"

static char *g_rom_path = NULL;
static int g_ram_size = 10 * 1024 * 1024;

static char *trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

bool config_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[1024];
    char section[64] = "";

    while (fgets(line, sizeof(line), f)) {
        char *l = trim(line);
        if (l[0] == '#' || l[0] == ';' || l[0] == '\0') continue;

        if (l[0] == '[' && l[strlen(l)-1] == ']') {
            strncpy(section, l + 1, sizeof(section) - 1);
            section[strlen(section)-1] = '\0';
            continue;
        }

        char *eq = strchr(l, '=');
        if (eq) {
            *eq = '\0';
            char *key = trim(l);
            char *val = trim(eq + 1);

            if (strcmp(section, "system") == 0) {
                if (strcmp(key, "rom_path") == 0) {
                    if (g_rom_path) free(g_rom_path);
                    g_rom_path = strdup(val);
                } else if (strcmp(key, "ram_size") == 0) {
                    g_ram_size = atoi(val);
                }
            } else if (strcmp(section, "drives") == 0 || strcmp(section, "floppies") == 0) {
                vfs_add_drive(key, val);
            }
        }
    }

    fclose(f);
    return true;
}

const char *config_get_rom_path(void) {
    return g_rom_path;
}

int config_get_ram_size(void) {
    return g_ram_size;
}
