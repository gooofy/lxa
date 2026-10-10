/*
 * lxa_bcpl.h - BCPL program support (shared by bcpl_entry.S and the C files)
 *
 * BCPL programs (the 1.x C: commands, BCPL handlers, KS 1.3 Run/NewCLI)
 * run with
 *   a2 = the global vector (pr_GlobVec), a pointer to slot 0,
 *   a1 = the current BCPL frame (frames grow upwards; the arguments are at
 *        0(a1), 4(a1), ...; the three longs below a frame hold the caller's
 *        frame, the return address and the routine's own base address),
 *   a5 = the call routine, a6 = the return routine,
 *   a0 = 0 (compiled code addresses memory as (a0,dN.l)).
 * A call is "movea.l G(a2),a4; moveq #framesize,d0; jsr (a5)" with the
 * first four arguments in d1-d4; the result comes back in d1.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#ifndef LXA_BCPL_H
#define LXA_BCPL_H

/* Global vector geometry: slot 0 holds the number of slots (as on 3.1);
 * the system also keeps entries at negative offsets. */
#define LXA_BCPL_NEG_BYTES      0xb0
#define LXA_BCPL_POS_BYTES      0x21c
#define LXA_BCPL_SLOTS          ((LXA_BCPL_NEG_BYTES + LXA_BCPL_POS_BYTES) / 4)
#define LXA_BCPL_POS_SLOTS      (LXA_BCPL_POS_BYTES / 4)

/* Every C-implemented global has an 8 byte entry stub in bcpl_entry.S */
#define LXA_BCPL_STUB_BYTES     8

/* Longs left free above a frame before C code calls back into BCPL */
#define LXA_BCPL_CALLBACK_GAP   32

/* Offsets bcpl_entry.S needs (checked with _Static_assert in bcpl_globals.c) */
#define LXA_BCPL_PR_RETURNADDR  176     /* struct Process, pr_ReturnAddr */
#define LXA_BCPL_LVO_FINDTASK   -294    /* exec.library */
#define LXA_BCPL_LVO_EXIT       -144    /* dos.library */

#ifndef __ASSEMBLER__

#include <exec/types.h>
#include <dos/dosextens.h>

/* BCPL call and return routines (dl_A5 / dl_A6) */
extern void lxa_bcpl_jsr(void);
extern void lxa_bcpl_rts(void);

/* The global entry stubs: one per slot, indexed from the lowest negative
 * offset */
extern const UBYTE lxa_bcpl_stubs[];

/* Globals implemented in assembler */
extern void lxa_bcpl_g_stop(void);
extern void lxa_bcpl_g_longcall(void);

/* The console segment (rn_ConsoleSegment), a ROM-resident BCPL hunk */
extern ULONG lxa_bcpl_cliseg[];

/*
 * Run BCPL routine 'entry' with global vector 'gv' and its first frame at
 * 'frame' (d1-d4 as arguments); returns its result.  With 'exitslot' set
 * (&pr_ReturnAddr) the call is a top level one that the stop global and
 * dos.library Exit() unwind to; the caller saves and restores the old
 * value.
 */
LONG lxa_bcpl_enter(APTR entry, ULONG *gv, LONG *frame, APTR *exitslot,
                    LONG d1, LONG d2, LONG d3, LONG d4);

/* Call library function at 'lvo' of 'base' with regs = d0, d1, a0, a1, a2 */
LONG lxa_bcpl_lvo(APTR base, LONG lvo, const LONG *regs);

/* bcpl_globals.c */
BOOL  lxa_bcpl_init(struct DosLibrary *dosbase);
LONG  lxa_bcpl_dispatch(LONG off, ULONG *gv, LONG *args);
void  lxa_bcpl_install(BPTR seg, ULONG *gv);
ULONG lxa_bcpl_highest_global(BPTR seg);

/* bcpl_cli.c */
struct MsgPort *lxa_bcpl_create_proc(CONST_STRPTR name, BPTR *segarray,
                                     ULONG stackbytes, LONG pri);
LONG lxa_bcpl_cli_main(BPTR pkt, LONG *freestack);

#endif /* __ASSEMBLER__ */

#endif
