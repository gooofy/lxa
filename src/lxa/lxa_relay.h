/*
 * lxa_relay.h - relay trace of library calls (Phase 233, like WINE +relay)
 *
 * Every call through a traced library's jump table is logged with its
 * registers and, when the call returns, with d0.  The JSON-lines format is
 * the one the reference agent writes (lxaprobe TRACE_DUMP), so
 * `python3 -m rdd tracediff` can align both.
 *
 * Spec: "graphics.library:-60,-66;intuition.library:*" (LVOs or * = all).
 * Enabled by LXA_TRACE=<spec> (+ LXA_TRACE_FILE=<path>, default
 * lxa-trace.jsonl) or lxa_trace_start().  LXA_TRACE_INJECT="lib:lvo:delta"
 * adds delta to every return value of that function (a deliberate bug for
 * testing tracediff; the function must be traced).
 */
#ifndef LXA_RELAY_H
#define LXA_RELAY_H

#include <stdbool.h>
#include <stdint.h>

extern bool g_relay_active;

void lxa_relay_init_from_env(void);
bool lxa_relay_start(const char *spec, const char *path);
void lxa_relay_stop(void);
void lxa_relay_check_slow(uint32_t pc);

static inline void lxa_relay_check(uint32_t pc)
{
    if (__builtin_expect(g_relay_active, 0))
        lxa_relay_check_slow(pc);
}

#endif
