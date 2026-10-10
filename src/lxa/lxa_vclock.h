/*
 * lxa_vclock.h - Virtual clock (Phase 201: deterministic virtual time)
 *
 * All emulated notions of time (VBlank cadence, timer.device, DateStamp,
 * GetSysTime) are read through this module.
 *
 * Real-time mode (interactive lxa): time is host wall-clock time and
 * VBlank comes from a SIGALRM timer, as before.
 *
 * Deterministic mode (liblxa default, `lxa --deterministic`): time is
 * derived only from emulated CPU cycles:
 *
 *     now = epoch + total_cycles / cpu_hz
 *
 * VBlank fires every `cycles_per_frame` emulated cycles.  When every task
 * is blocked in Wait() the clock skips ahead to the end of the current
 * execution slice instead of sleeping, so idle time costs no wall time.
 * Two runs of the same scenario see exactly the same time values.
 */

#ifndef LXA_VCLOCK_H
#define LXA_VCLOCK_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Default emulated CPU clock: 25 MHz, i.e. the canonical reference machine
 * (A4000 class, see roadmap "Canonical Reference Configuration"); Musashi
 * counts 68030 cycles.  One 50 Hz PAL frame is 500000 cycles.  The E-clock
 * (709379 Hz) is independent of this, as on real hardware.
 */
#define VCLOCK_DEFAULT_CPU_HZ           25000000u
#define VCLOCK_DEFAULT_FRAME_HZ         50u
/* Default epoch for deterministic runs: 2024-01-01 00:00:00 */
#define VCLOCK_DEFAULT_EPOCH            1704067200ull

typedef struct vclock_config {
    bool     deterministic;
    uint32_t cpu_hz;            /* 0 = default */
    uint32_t cycles_per_frame;  /* 0 = cpu_hz / 50 */
    uint64_t epoch_secs;        /* 0 = default epoch (deterministic only) */
} vclock_config_t;

void     vclock_init(const vclock_config_t *cfg);
bool     vclock_deterministic(void);

/* Current emulated time in microseconds since the Unix epoch. */
uint64_t vclock_now_us(void);

/* Total emulated cycles (including the currently executing slice). */
uint64_t vclock_cycles(void);
uint64_t vclock_frames(void);
/* Cycles skipped because every task was waiting (deterministic mode). */
uint64_t vclock_idle_cycles(void);
uint32_t vclock_cycles_per_frame(void);

/* Cycles until the next VBlank frame boundary (deterministic mode). */
uint32_t vclock_cycles_to_next_frame(void);

/*
 * Run the CPU for `cycles` and account the elapsed virtual time.
 * Returns the number of virtual cycles that elapsed.
 */
int      vclock_execute(int cycles);

/*
 * Returns true (once per boundary) when the cycle counter has crossed a
 * VBlank frame boundary since the last call.
 */
bool     vclock_frame_due(void);

/*
 * Called from EMU_CALL_WAIT (scheduler idle loop and polling loops).
 * Real-time mode: sleeps 1 ms.  Deterministic mode: if `system_idle` (no
 * task ready), ends the current slice and lets the clock skip to its end;
 * otherwise does nothing.
 */
void     vclock_idle(bool system_idle);

/* End the current execution slice early (wraps m68k_end_timeslice()). */
void     vclock_end_timeslice(void);

/* Advance virtual time without executing (deterministic mode only). */
void     vclock_advance_us(uint64_t us);

/* Charge the running m68k code for work done on the host on its behalf
 * (deterministic mode only): the CPU is busy for `cycles` more cycles,
 * e.g. while it waits for the blitter (Phase 237b). */
void     vclock_consume(uint32_t cycles);

/* The virtual CPU clock in Hz. */
uint32_t vclock_cpu_hz(void);

#endif /* LXA_VCLOCK_H */
