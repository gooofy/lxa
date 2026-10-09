/*
 * lxa interface to the BCPL support ported from AROS (Phase 239).
 *
 * BCPL programs (1.x C: commands, BCPL handlers) are entered with
 * a2 = the process's global vector (pr_GlobVec), a5 = BCPL_jsr and
 * a6 = BCPL_rts, and keep their stack frames upwards from a1.
 */
#ifndef LXA_BCPL_SUPPORT_H
#define LXA_BCPL_SUPPORT_H

#include <exec/types.h>
#include <dos/dosextens.h>

/* BCPL call/return routines (bcpl.S) */
extern void BCPL_jsr(void);
extern void BCPL_rts(void);

/* Enter a BCPL routine; returns its result (bcpl.S) */
LONG lxa_bcpl_call(APTR entry, APTR globvec, APTR splower, APTR *returnaddr,
                   LONG d1, LONG d2, LONG d3, LONG d4);

/* Set up dl_GV / dl_A5 / dl_A6 at dos.library init */
BOOL lxa_bcpl_init(struct DosLibrary *dosbase);

/* Install the globals of a BCPL segment list into a global vector */
ULONG BCPL_InstallSeg(BPTR seg, ULONG *globvec);

#endif
