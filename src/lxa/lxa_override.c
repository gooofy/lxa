/*
 * lxa_override.c - library override mode (Phase 235), see lxa_override.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "lxa_override.h"

/* libraries AmigaOS 3.1 loads from LIBS: (hardware independent) */
static const char *const s_allowed[] = {
    "amigaguide", "asl", "bullet", "commodities", "datatypes", "diskfont", "iffparse",
    "locale", "lowlevel", "mathieeedoubbas", "mathieeedoubtrans", "mathieeesingtrans",
    "mathtrans", "nonvolatile", "realtime", "rexxsupport", "rexxsyslib", "version", NULL
};

#define MAX_OVERRIDES 24
static char s_names[MAX_OVERRIDES][48];
static char s_paths[MAX_OVERRIDES][512];
static int  s_count;

static bool allowed(const char *base)
{
    for (int i = 0; s_allowed[i]; i++)
        if (!strcasecmp(base, s_allowed[i]))
            return true;
    return false;
}

void lxa_override_init(void)
{
    const char *spec = getenv("LXA_OVERRIDE");
    const char *dir = getenv("LXA_OVERRIDE_DIR");
    char defdir[512], buf[1024], *tok, *save = NULL;

    s_count = 0;
    if (!spec || !*spec)
        return;
    if (!dir || !*dir) {
        snprintf(defdir, sizeof(defdir), "%s/.cache/lxa/refsys/SYS-aga/Libs", getenv("HOME") ? getenv("HOME") : "");
        dir = defdir;
    }
    snprintf(buf, sizeof(buf), "%s", spec);
    for (tok = strtok_r(buf, ", ", &save); tok && s_count < MAX_OVERRIDES; tok = strtok_r(NULL, ", ", &save)) {
        char base[48];
        snprintf(base, sizeof(base), "%s", tok);
        char *dot = strstr(base, ".library");
        if (dot)
            *dot = 0;
        if (!allowed(base)) {
            fprintf(stderr, "LXA_OVERRIDE: %s is not a disk library of AmigaOS 3.1 - ignored\n", tok);
            continue;
        }
        snprintf(s_names[s_count], sizeof(s_names[0]), "%s.library", base);
        snprintf(s_paths[s_count], sizeof(s_paths[0]), "%s/%s", dir, s_names[s_count]);
        if (access(s_paths[s_count], R_OK) != 0) {
            fprintf(stderr, "LXA_OVERRIDE: %s not found - ignored\n", s_paths[s_count]);
            continue;
        }
        fprintf(stderr, "LXA_OVERRIDE: %s from %s (diagnostic mode)\n", s_names[s_count], s_paths[s_count]);
        s_count++;
    }
}

bool lxa_override_lookup(const char *name, char *path, size_t maxlen)
{
    for (int i = 0; i < s_count; i++) {
        if (!strcasecmp(name, s_names[i])) {
            if (path)
                snprintf(path, maxlen, "%s", s_paths[i]);
            return true;
        }
    }
    return false;
}

int lxa_override_count(void)
{
    return s_count;
}
