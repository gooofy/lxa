/*
 * lxa_copper.c — Minimal Amiga Copper list interpreter.
 *
 * Implements the subset of copper operations that productivity apps and
 * simple demos rely on:
 *
 *   - MOVE: write a 16-bit value to a custom-chip register.
 *   - WAIT: wait for a beam position. We do not model beam timing —
 *     because we run the entire copper list once per VBlank in zero
 *     emulated time, every WAIT is treated as immediately satisfied
 *     (the beam has "already passed" any position by the time the next
 *     VBlank arrives). This is sufficient for the common case where
 *     copper lists are used to set up palette entries and bitplane
 *     pointers per scanline; the cumulative effect after one full pass
 *     matches the final state of the screen, which is what the host
 *     framebuffer reflects.
 *   - SKIP: conditional skip. Like WAIT, the comparison would normally
 *     depend on beam position; with no beam tracking the conditional
 *     is treated as never-skipping (we proceed to the following
 *     instruction).
 *
 * Copper instructions are 32-bit words pairs:
 *
 *   MOVE: word0 = RA (register address, bits 8:1, lower addr bit 0)
 *         word1 = data
 *         RA bit 0 == 0 => MOVE
 *
 *   WAIT: word0 = VP/HP (vertical/horizontal beam position) | 0x0001
 *         word1 = VE/HE (mask) | 0x0000  (BFD bit, blitter-finished, ignored)
 *
 *   SKIP: word0 = VP/HP | 0x0001
 *         word1 = VE/HE | 0x0001
 *
 * End-of-list is the canonical CWAIT(0xFFFF, 0xFFFE) — VP=0xFF, HP=0xFE,
 * VE=0xFF, HE=0xFE — encoded as (0xFFFF, 0xFFFE). We treat that
 * specific pattern as a halt to avoid running away when an app forgets
 * a real terminator.
 *
 * MOVE writes that target a custom register go through the host's
 * existing _handle_custom_write() path (declared in lxa.c) so that all
 * side effects (DMACON, INTENA, blitter triggers, color register
 * tracking, etc.) are honored exactly as if the m68k had performed the
 * write itself.
 *
 * To enforce the real-hardware restriction on which registers the
 * copper is permitted to touch, we apply the OCS/ECS "danger bit"
 * mask: by default the copper may only write to registers >= 0x80
 * (CDANG=0). Writes to lower registers are silently dropped. CDANG=1
 * (set via COPCON=0x02) opens up the full custom-chip register space.
 */

#include "lxa_copper.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "m68k.h"
#include "util.h"

/* Forward declarations from lxa.c — keep the boundary explicit. */
extern void _handle_custom_write_ext(uint16_t reg, uint16_t value);

/* Copper state. Public-ish but only via the API below. */
static uint32_t g_cop1lc = 0;       /* Copper 1 location register (long pointer) */
static uint32_t g_cop2lc = 0;       /* Copper 2 location register (long pointer) */
static uint16_t g_copcon = 0;       /* Copper control: bit 1 (0x02) = CDANG */

/* Statistics — useful for tests and debugging. */
static uint32_t g_copper_runs       = 0;
static uint32_t g_copper_moves      = 0;
static uint32_t g_copper_waits      = 0;
static uint32_t g_copper_skips      = 0;

/*
 * Maximum number of instructions we will execute per copper run.
 * A real copper list is bounded by its terminator; this guards against
 * runaway loops if an app's copper list is malformed.
 */
#define COPPER_MAX_INSTRUCTIONS 8192

void copper_reset(void)
{
    g_cop1lc = 0;
    g_cop2lc = 0;
    g_copcon = 0;
    g_copper_runs   = 0;
    g_copper_moves  = 0;
    g_copper_waits  = 0;
    g_copper_skips  = 0;
}

void copper_set_cop1lc(uint32_t addr) { g_cop1lc = addr; }
void copper_set_cop2lc(uint32_t addr) { g_cop2lc = addr; }
uint32_t copper_get_cop1lc(void)      { return g_cop1lc; }
uint32_t copper_get_cop2lc(void)      { return g_cop2lc; }
void copper_set_copcon(uint16_t val)  { g_copcon = val; }
uint16_t copper_get_copcon(void)      { return g_copcon; }

uint32_t copper_get_run_count(void)   { return g_copper_runs; }
uint32_t copper_get_move_count(void)  { return g_copper_moves; }
uint32_t copper_get_wait_count(void)  { return g_copper_waits; }
uint32_t copper_get_skip_count(void)  { return g_copper_skips; }

/*
 * Execute the copper list starting at `start_addr` for one frame.
 *
 * Returns the number of instructions executed (each instruction is two
 * 16-bit words, so 4 bytes).
 */
static uint32_t copper_run_at(uint32_t start_addr)
{
    if (start_addr == 0) {
        return 0;
    }

    /* Sanity-check the starting address is in RAM. */
    if (start_addr >= 0x00A00000) {
        DPRINTF(LOG_DEBUG, "copper: refusing to run from non-RAM addr 0x%08x\n",
                start_addr);
        return 0;
    }

    uint32_t pc = start_addr;
    uint32_t executed = 0;

    /*
     * Track whether the previous instruction was a SKIP whose condition
     * fired. With no beam emulation we never satisfy the SKIP condition,
     * so this flag is currently always false — but the structure is here
     * for the day we add coarse beam tracking.
     */
    bool skip_next = false;

    for (executed = 0; executed < COPPER_MAX_INSTRUCTIONS; executed++)
    {
        uint16_t w0 = (uint16_t)m68k_read_memory_16(pc);
        uint16_t w1 = (uint16_t)m68k_read_memory_16(pc + 2);
        pc += 4;

        if (skip_next) {
            skip_next = false;
            continue;
        }

        if ((w0 & 0x0001) == 0)
        {
            /*
             * MOVE: w0 bits 8:1 are the register address (in bytes,
             * relative to 0xDFF000). Bit 0 is 0. Upper bits are
             * reserved on OCS but apps sometimes leave them set; mask
             * to a 9-bit register address (the chip address space is
             * 0x000..0x1FE).
             */
            uint16_t reg = w0 & 0x01FE;

            /*
             * CDANG enforcement: registers below 0x80 (or 0x40 on OCS)
             * are protected. We use the ECS rule (>=0x40 always allowed)
             * because lxa identifies as ECS. CDANG=1 unlocks 0x00..0x3E
             * as well.
             */
            if (reg < 0x40 && !(g_copcon & 0x0002)) {
                DPRINTF(LOG_DEBUG,
                        "copper: MOVE to protected reg 0x%03x suppressed (CDANG=0)\n",
                        reg);
                continue;
            }

            DPRINTF(LOG_DEBUG, "copper: MOVE 0x%04x -> 0x%03x\n", w1, reg);
            _handle_custom_write_ext(reg, w1);
            g_copper_moves++;
        }
        else if ((w1 & 0x0001) == 0)
        {
            /*
             * WAIT. With no beam emulation we treat the wait as
             * already satisfied: by the time we run again at the next
             * VBlank, the beam has been everywhere. End-of-list
             * (CWAIT 0xFFFF, 0xFFFE) terminates execution.
             */
            g_copper_waits++;
            if (w0 == 0xFFFF && w1 == 0xFFFE) {
                DPRINTF(LOG_DEBUG, "copper: end-of-list at 0x%08x\n", pc - 4);
                break;
            }
            DPRINTF(LOG_DEBUG, "copper: WAIT vp=0x%02x hp=0x%02x (passed)\n",
                    (w0 >> 8) & 0xFF, w0 & 0xFE);
            /* Continue to next instruction immediately. */
        }
        else
        {
            /*
             * SKIP. With no beam tracking we cannot meaningfully
             * evaluate the comparison; treat it as never-skip so the
             * following instruction always executes. This matches the
             * conservative behavior most apps rely on (they place the
             * "fallback" instruction right after the SKIP).
             */
            g_copper_skips++;
            DPRINTF(LOG_DEBUG, "copper: SKIP vp=0x%02x hp=0x%02x (not taken)\n",
                    (w0 >> 8) & 0xFF, w0 & 0xFE);
        }
    }

    if (executed >= COPPER_MAX_INSTRUCTIONS) {
        DPRINTF(LOG_DEBUG,
                "copper: hit instruction cap (%u) starting at 0x%08x\n",
                COPPER_MAX_INSTRUCTIONS, start_addr);
    }

    return executed;
}

/*
 * Called once per VBlank. Honors the COPPER DMA enable bit in DMACON.
 *
 * We start from COP1LC unconditionally; COP2LC is only entered via an
 * explicit COPJMP2 trigger (handled in lxa.c) which calls
 * copper_jump_to(2) — which we do not chain here. For a basic
 * interpreter, a single pass over the COP1 list per VBlank covers
 * essentially every productivity-app use case.
 */
extern uint16_t g_dmacon;
#define DMAF_COPPER 0x0080
#define DMAF_MASTER 0x0200

void copper_run_frame(void)
{
    if (!(g_dmacon & DMAF_MASTER) || !(g_dmacon & DMAF_COPPER)) {
        return;
    }
    if (g_cop1lc == 0) {
        return;
    }

    g_copper_runs++;
    (void)copper_run_at(g_cop1lc);
}

/*
 * COPJMP1/COPJMP2 strobe: in real hardware this restarts the copper at
 * COP1LC/COP2LC immediately. With our once-per-VBlank model we simply
 * run the requested list synchronously. This keeps apps that do
 * mid-frame copper restarts working even though their list will never
 * actually be in mid-execution under our model.
 */
void copper_jump(int which)
{
    if (!(g_dmacon & DMAF_MASTER) || !(g_dmacon & DMAF_COPPER)) {
        return;
    }
    uint32_t addr = (which == 2) ? g_cop2lc : g_cop1lc;
    if (addr == 0) {
        return;
    }
    g_copper_runs++;
    (void)copper_run_at(addr);
}
