/*
 * lxa_unimpl.h - stub telemetry (Phase 203)
 */

#ifndef LXA_UNIMPL_H
#define LXA_UNIMPL_H

#include <stdbool.h>

/* Exit code of a run stopped by --strict-unimplemented */
#define LXA_EXIT_UNIMPLEMENTED 125

typedef struct lxa_unimpl_entry {
    char lib[32];
    char function[48];
    char detail[128];
    char first_task[64];
    int  count;
} lxa_unimpl_entry_t;

void lxa_unimpl_reset(void);
void lxa_unimpl_set_strict(bool strict);
bool lxa_unimpl_strict(void);
bool lxa_unimpl_tripped(void);

/* Record a hit; returns true if the run must stop (strict mode). */
bool lxa_unimpl_record(const char *lib, const char *fn, const char *detail, const char *task);

/* Copy up to `max` records; returns the number of distinct records. */
int  lxa_unimpl_get(lxa_unimpl_entry_t *out, int max);

/* Write the per-run summary to lxa.log. */
void lxa_unimpl_summary(void);

#endif /* LXA_UNIMPL_H */
