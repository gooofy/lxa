/*
 * lxa_coverage.h - ROM code coverage (Phase 202)
 */

#ifndef LXA_COVERAGE_H
#define LXA_COVERAGE_H

#include <stdint.h>

/* NULL unless LXA_ROM_COVERAGE is set; checked on every instruction */
extern uint8_t *g_rom_cov_bitmap;

void lxa_coverage_init(void);
void lxa_coverage_mark_slow(uint32_t pc);
void lxa_coverage_flush(void);

static inline void lxa_coverage_mark(uint32_t pc)
{
    if (__builtin_expect(g_rom_cov_bitmap != 0, 0))
        lxa_coverage_mark_slow(pc);
}

#endif /* LXA_COVERAGE_H */
