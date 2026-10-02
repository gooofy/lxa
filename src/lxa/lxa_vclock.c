/*
 * lxa_vclock.c - Virtual clock (Phase 201: deterministic virtual time)
 *
 * See lxa_vclock.h for the model.
 */

#include <sys/time.h>
#include <unistd.h>

#include "lxa_vclock.h"
#include "m68k.h"

static bool     s_deterministic = false;
static uint32_t s_cpu_hz = VCLOCK_DEFAULT_CPU_HZ;
static uint32_t s_cycles_per_frame = VCLOCK_DEFAULT_CPU_HZ / VCLOCK_DEFAULT_FRAME_HZ;
static uint64_t s_epoch_us = 0;

static uint64_t s_total_cycles = 0;     /* cycles accounted so far */
static uint64_t s_next_frame = 0;       /* cycle count of next VBlank */
static uint64_t s_frames = 0;
static uint64_t s_idle_cycles = 0;     /* cycles skipped while idle */

static bool     s_in_execute = false;
static int      s_slice_budget = 0;     /* cycles requested for current slice */
static bool     s_slice_ended = false;  /* vclock_end_timeslice() was called */
static int      s_slice_used = 0;       /* cycles run when the slice ended */
static bool     s_slice_idle = false;   /* slice ended because all tasks wait */

void vclock_init(const vclock_config_t *cfg)
{
    s_deterministic = cfg && cfg->deterministic;
    s_cpu_hz = (cfg && cfg->cpu_hz) ? cfg->cpu_hz : VCLOCK_DEFAULT_CPU_HZ;
    s_cycles_per_frame = (cfg && cfg->cycles_per_frame)
                       ? cfg->cycles_per_frame
                       : s_cpu_hz / VCLOCK_DEFAULT_FRAME_HZ;
    s_epoch_us = ((cfg && cfg->epoch_secs) ? cfg->epoch_secs : VCLOCK_DEFAULT_EPOCH)
               * 1000000ull;
    s_total_cycles = 0;
    s_next_frame = s_cycles_per_frame;
    s_frames = 0;
    s_idle_cycles = 0;
    s_in_execute = false;
    s_slice_budget = 0;
    s_slice_ended = false;
    s_slice_used = 0;
    s_slice_idle = false;
}

bool vclock_deterministic(void)
{
    return s_deterministic;
}

uint64_t vclock_cycles(void)
{
    uint64_t c = s_total_cycles;

    if (s_in_execute && !s_slice_ended)
    {
        int run = m68k_cycles_run();
        if (run > 0)
            c += (uint64_t)run;
    }
    else if (s_in_execute && s_slice_ended)
    {
        c += (uint64_t)s_slice_used;
    }

    return c;
}

uint64_t vclock_idle_cycles(void)
{
    return s_idle_cycles;
}

uint64_t vclock_frames(void)
{
    return s_frames;
}

uint32_t vclock_cycles_per_frame(void)
{
    return s_cycles_per_frame;
}

uint64_t vclock_now_us(void)
{
    if (!s_deterministic)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint64_t)tv.tv_sec * 1000000ull + (uint64_t)tv.tv_usec;
    }

    /* Split to avoid overflow: cycles * 1e6 overflows after ~2.6e13 cycles */
    uint64_t c = vclock_cycles();
    uint64_t secs = c / s_cpu_hz;
    uint64_t rem = c % s_cpu_hz;
    return s_epoch_us + secs * 1000000ull + (rem * 1000000ull) / s_cpu_hz;
}

uint32_t vclock_cycles_to_next_frame(void)
{
    if (s_next_frame <= s_total_cycles)
        return 1;
    uint64_t d = s_next_frame - s_total_cycles;
    return d > 0x7fffffffu ? 0x7fffffffu : (uint32_t)d;
}

int vclock_execute(int cycles)
{
    int used;

    if (cycles <= 0)
        return 0;

    s_in_execute = true;
    s_slice_budget = cycles;
    s_slice_ended = false;
    s_slice_used = 0;
    s_slice_idle = false;

    int ret = m68k_execute(cycles);

    if (s_slice_idle && s_deterministic)
    {
        used = cycles;              /* idle: time passes to the end of the slice */
        s_idle_cycles += (uint64_t)(cycles - s_slice_used);
    }
    else if (s_slice_ended)
        used = s_slice_used;
    else
        used = ret;

    if (used < 0)
        used = 0;

    s_in_execute = false;
    s_slice_ended = false;
    s_slice_idle = false;
    s_total_cycles += (uint64_t)used;
    return used;
}

bool vclock_frame_due(void)
{
    if (s_total_cycles < s_next_frame)
        return false;

    /* Collapse multiple missed boundaries into one VBlank, like real
     * hardware would if the CPU was stalled. */
    while (s_next_frame <= s_total_cycles)
    {
        s_next_frame += s_cycles_per_frame;
        s_frames++;
    }
    return true;
}

void vclock_end_timeslice(void)
{
    if (s_in_execute && !s_slice_ended)
    {
        int run = m68k_cycles_run();
        s_slice_used = run > 0 ? run : 0;
        s_slice_ended = true;
    }
    m68k_end_timeslice();
}

void vclock_idle(bool system_idle)
{
    if (!s_deterministic)
    {
        usleep(1000);   /* 1ms - avoid busy-waiting */
        return;
    }

    if (system_idle && s_in_execute)
    {
        s_slice_idle = true;
        vclock_end_timeslice();
    }
}

void vclock_advance_us(uint64_t us)
{
    if (!s_deterministic)
        return;
    s_total_cycles += (us * s_cpu_hz) / 1000000ull;
}
