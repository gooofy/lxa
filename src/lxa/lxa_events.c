/*
 * lxa_events.c - Intuition event log for lxa emulator.
 *
 * This module owns the circular event log buffer and provides
 * lxa_push_intui_event() (called from lxa_dispatch.c at window/screen
 * open/close sites) and lxa_reset_intui_events() (called from
 * lxa_api.c:lxa_load_program to clear the log between runs).
 *
 * It is compiled into both the standalone `lxa` binary (via
 * LXA_CORE_SOURCES) and into liblxa, so that lxa_dispatch.c can call
 * lxa_push_intui_event() without a linker error.
 *
 * lxa_api.c accesses the storage via the externs declared below.
 *
 * Phase 131.
 */

#include "lxa_internal.h"
#include "lxa_api.h"

#include <string.h>

/* Circular event log storage — accessed by lxa_api.c via extern */
lxa_intui_event_t g_event_log[LXA_EVENT_LOG_SIZE];
int               g_event_log_head;
int               g_event_log_count;

void lxa_push_intui_event(int type, int window_index, const char *title,
                          int x, int y, int w, int h)
{
    lxa_intui_event_t *ev = &g_event_log[g_event_log_head];
    ev->type         = type;
    ev->window_index = window_index;
    ev->x            = x;
    ev->y            = y;
    ev->width        = w;
    ev->height       = h;

    if (title)
    {
        strncpy(ev->title, title, sizeof(ev->title) - 1);
        ev->title[sizeof(ev->title) - 1] = '\0';
    }
    else
    {
        ev->title[0] = '\0';
    }

    g_event_log_head = (g_event_log_head + 1) % LXA_EVENT_LOG_SIZE;
    if (g_event_log_count < LXA_EVENT_LOG_SIZE)
        g_event_log_count++;
}

void lxa_reset_intui_events(void)
{
    g_event_log_head  = 0;
    g_event_log_count = 0;
}
