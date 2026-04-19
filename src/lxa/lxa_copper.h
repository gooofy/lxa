/*
 * lxa_copper.h — Public interface for the copper interpreter.
 *
 * See lxa_copper.c for design notes and limitations.
 */

#ifndef LXA_COPPER_H
#define LXA_COPPER_H

#include <stdint.h>

/* Reset all copper state (called from coldstart). */
void copper_reset(void);

/* Register pointer accessors (driven by writes to COP1LC/COP2LC). */
void     copper_set_cop1lc(uint32_t addr);
void     copper_set_cop2lc(uint32_t addr);
uint32_t copper_get_cop1lc(void);
uint32_t copper_get_cop2lc(void);

/* COPCON register (only bit 1 / CDANG is honored). */
void     copper_set_copcon(uint16_t val);
uint16_t copper_get_copcon(void);

/*
 * Run the copper list for one frame. Honors DMACON COPPER+MASTER bits
 * and starts from COP1LC. Safe to call when DMA is disabled or the
 * pointer is zero (it is then a no-op).
 *
 * Called from the VBlank handler in lxa_api.c / lxa.c.
 */
void copper_run_frame(void);

/*
 * Synchronous "jump and execute" used to model the COPJMP1/COPJMP2
 * strobes. `which` must be 1 or 2.
 */
void copper_jump(int which);

/* Statistics — useful for tests and diagnostics. */
uint32_t copper_get_run_count(void);
uint32_t copper_get_move_count(void);
uint32_t copper_get_wait_count(void);
uint32_t copper_get_skip_count(void);

#endif /* LXA_COPPER_H */
