/*
 * lxa_unimpl.c - stub telemetry (Phase 203)
 *
 * ROM code reports every call into a stub, partial implementation or
 * unfilled LVO slot via LXA_UNIMPLEMENTED() -> EMU_CALL_UNIMPLEMENTED.
 * The host keeps one record per (library, function, detail) with a hit
 * count, logs the first hit, prints a summary at shutdown and, in strict
 * mode, stops the run with LXA_EXIT_UNIMPLEMENTED.
 */

#include <stdio.h>
#include <string.h>

#include "lxa_unimpl.h"
#include "util.h"

#define MAX_UNIMPL_ENTRIES 512

static lxa_unimpl_entry_t s_entries[MAX_UNIMPL_ENTRIES];
static int s_count = 0;
static int s_dropped = 0;
static bool s_strict = false;
static bool s_tripped = false;

static void copy_name(char *dst, size_t len, const char *src)
{
    if (!src)
        src = "";
    /* ROM library tags are written "_graphics": drop the leading '_' */
    while (*src == '_')
        src++;
    snprintf(dst, len, "%s", src);
}

void lxa_unimpl_reset(void)
{
    s_count = 0;
    s_dropped = 0;
    s_tripped = false;
}

void lxa_unimpl_set_strict(bool strict)
{
    s_strict = strict;
}

bool lxa_unimpl_strict(void)
{
    return s_strict;
}

bool lxa_unimpl_tripped(void)
{
    return s_tripped;
}

bool lxa_unimpl_record(const char *lib, const char *fn, const char *detail, const char *task)
{
    char l[sizeof(s_entries[0].lib)], f[sizeof(s_entries[0].function)];
    int i;

    copy_name(l, sizeof(l), lib);
    copy_name(f, sizeof(f), fn);
    if (!detail)
        detail = "";

    for (i = 0; i < s_count; i++)
    {
        if (strcmp(s_entries[i].lib, l) == 0 && strcmp(s_entries[i].function, f) == 0 &&
            strcmp(s_entries[i].detail, detail) == 0)
        {
            s_entries[i].count++;
            goto out;
        }
    }

    if (s_count < MAX_UNIMPL_ENTRIES)
    {
        lxa_unimpl_entry_t *e = &s_entries[s_count++];
        snprintf(e->lib, sizeof(e->lib), "%s", l);
        snprintf(e->function, sizeof(e->function), "%s", f);
        snprintf(e->detail, sizeof(e->detail), "%s", detail);
        snprintf(e->first_task, sizeof(e->first_task), "%s", task ? task : "");
        e->count = 1;
        LPRINTF(LOG_WARNING, "lxa: UNIMPLEMENTED %s.library/%s (%s) called by '%s'\n",
                e->lib, e->function, e->detail, e->first_task);
    }
    else
    {
        s_dropped++;
    }

out:
    /* Strict mode trips on stubs, private slots and empty vectors; partial
     * implementations ("partial: ...") are only recorded, otherwise almost
     * every application would stop in OpenWindow(). */
    if (s_strict && !s_tripped && strncmp(detail, "partial:", 8) != 0)
    {
        s_tripped = true;
        fprintf(stderr, "*** lxa --strict-unimplemented: %s/%s (%s) called, stopping\n", l, f, detail);
        return true;
    }
    return false;
}

int lxa_unimpl_get(lxa_unimpl_entry_t *out, int max)
{
    int i;

    if (out)
        for (i = 0; i < s_count && i < max; i++)
            out[i] = s_entries[i];
    return s_count;
}

void lxa_unimpl_summary(void)
{
    int i;

    if (!s_count)
        return;

    LPRINTF(LOG_INFO, "lxa: %d unimplemented system-library function(s) were called:\n", s_count);
    for (i = 0; i < s_count; i++)
        LPRINTF(LOG_INFO, "lxa:   %5d x %s/%s (%s)\n", s_entries[i].count,
                s_entries[i].lib, s_entries[i].function, s_entries[i].detail);
    if (s_dropped)
        LPRINTF(LOG_INFO, "lxa:   (%d further records dropped)\n", s_dropped);
}
