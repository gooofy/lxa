/*
 * bcpl_entry.S - machine level part of lxa's BCPL support
 *
 *  - lxa_bcpl_jsr / lxa_bcpl_rts: the call and return routines BCPL code
 *    reaches through a5 and a6 (see lxa_bcpl.h for the register model);
 *    compiled code also multiplies with "jsr 16(a5)" and divides with
 *    "jsr 18(a5)";
 *  - lxa_bcpl_enter: start a BCPL routine from C;
 *  - the entry stubs of the globals implemented in C: each loads its
 *    global vector offset and goes through one common path into
 *    lxa_bcpl_dispatch() (bcpl_globals.c);
 *  - the globals that change the control flow (stop, longcall);
 *  - lxa_bcpl_lvo: library calls for the execLib/libcall globals;
 *  - the console segment that KS 1.3 Run/NewCLI start.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#include "lxa_bcpl.h"

        .text

/* ------------------------------------------------------------------------
 * Call: the caller has a4 = routine, d0 = its own frame size, d1-d4 =
 * arguments, the return address on the stack.  The new frame starts d0
 * bytes above the current one; the three longs below it link back.
 * ---------------------------------------------------------------------- */
        .balign 4
        .globl  _lxa_bcpl_jsr
_lxa_bcpl_jsr:
        bra.w   call_frame                  | +0x00: call
        .skip   0x10 - 4                    | (never executed)
        bra.s   arith_mul                   | +0x10: d1 = d1 * d2
        bra.s   arith_div                   | +0x12: d1 = d1 / d2, d2 = d1 rem d2
        .if (. - _lxa_bcpl_jsr) != 0x14
        .error "BCPL arithmetic entries moved"
        .endif

call_frame:
        movea.l (%sp)+,%a3                  | return address
        movem.l %a1/%a3/%a4,-12(%a1,%d0.l)  | link: caller frame, return, base
        adda.l  %d0,%a1                     | the new frame
        movem.l %d1-%d4,(%a1)               | arguments 1-4 into it
        jmp     (%a4)

arith_mul:
        muls.l  %d2,%d1
        rts

arith_div:
        move.l  %d2,%d0
        divsl.l %d0,%d2:%d1
        rts

/* ------------------------------------------------------------------------
 * Return: result in d1; back to the caller's frame and base.
 * ---------------------------------------------------------------------- */
        .balign 4
        .globl  _lxa_bcpl_rts
_lxa_bcpl_rts:
        movem.l -12(%a1),%a1/%a3            | caller frame, return address
        movea.l -4(%a1),%a4                 | caller's base
        jmp     (%a3)

/* ------------------------------------------------------------------------
 * LONG lxa_bcpl_enter(entry, gv, frame, exitslot, d1, d2, d3, d4)
 *
 * Top level calls (exitslot != NULL) leave this on the C stack:
 *     exitslot -> [BCPL stack size]
 *                 [address of enter_unwound]   <- popped by stop's rts
 * so that "sp = pr_ReturnAddr - 4; rts" (the stop global, dos Exit())
 * comes back here with the return code in d0.
 * ---------------------------------------------------------------------- */
        .balign 4
        .globl  _lxa_bcpl_enter
_lxa_bcpl_enter:
        movem.l %d2-%d7/%a2-%a6,-(%sp)      | 11 registers, 44 bytes
        movea.l 48(%sp),%a4                 | entry
        movea.l 52(%sp),%a2                 | global vector
        movea.l 56(%sp),%a1                 | first frame
        movea.l 60(%sp),%a3                 | exit slot
        movem.l 64(%sp),%d1-%d4             | arguments
        lea     enter_nested,%a0
        move.l  %a3,%d0
        beq.s   enter_go
        move.l  %sp,%d0
        sub.l   %a1,%d0
        move.l  %d0,-(%sp)                  | the stack size, as RunCommand has
        move.l  %sp,(%a3)
        pea     enter_unwound
        lea     enter_toplevel,%a0
enter_go:
        move.l  %a1,(%a1)+                  | link of the first frame
        move.l  %a0,(%a1)+
        move.l  %a4,(%a1)+
        movem.l %d1-%d4,(%a1)
        suba.l  %a0,%a0
        lea     _lxa_bcpl_jsr,%a5
        lea     _lxa_bcpl_rts,%a6
        jmp     (%a4)

enter_toplevel:                             | normal return of a top level call
        addq.l  #8,%sp                      | landing address, stack size
enter_nested:
        move.l  %d1,%d0
        movem.l (%sp)+,%d2-%d7/%a2-%a6
        rts

enter_unwound:                              | stop / Exit(): d0 = return code
        addq.l  #4,%sp                      | stack size
        movem.l (%sp)+,%d2-%d7/%a2-%a6
        rts

/* ------------------------------------------------------------------------
 * Entry stubs: one per global vector slot, LXA_BCPL_STUB_BYTES each.
 * ---------------------------------------------------------------------- */
        .balign 4
        .globl  _lxa_bcpl_stubs
_lxa_bcpl_stubs:
        .set    gvoff, -LXA_BCPL_NEG_BYTES
        .rept   LXA_BCPL_SLOTS
        move.w  #gvoff,%d0
        bra.w   stub_common
        .set    gvoff, gvoff + 4
        .endr
        .if (. - _lxa_bcpl_stubs) != LXA_BCPL_SLOTS * LXA_BCPL_STUB_BYTES
        .error "BCPL stub size"
        .endif

stub_common:
        movem.l %d2-%d7/%a0-%a6,-(%sp)      | BCPL keeps everything but d0/d1
        ext.l   %d0
        move.l  %a1,-(%sp)                  | the arguments (this frame)
        move.l  %a2,-(%sp)                  | the global vector
        move.l  %d0,-(%sp)                  | the slot
        jsr     _lxa_bcpl_dispatch
        lea     12(%sp),%sp
        movem.l (%sp)+,%d2-%d7/%a0-%a6
        move.l  %d0,%d1
        jmp     (%a6)

/* ------------------------------------------------------------------------
 * stop(code) (global 2, also remProcess): unwind to pr_ReturnAddr like
 * dos.library Exit().
 * ---------------------------------------------------------------------- */
        .balign 4
        .globl  _lxa_bcpl_g_stop
_lxa_bcpl_g_stop:
        move.l  %d1,%d7
        movea.l 4.w,%a6
        suba.l  %a1,%a1
        jsr     LXA_BCPL_LVO_FINDTASK(%a6)
        movea.l %d0,%a0
        move.l  LXA_BCPL_PR_RETURNADDR(%a0),%d0
        beq.s   stop_no_frame
        movea.l %d0,%sp
        subq.l  #4,%sp
        move.l  %d7,%d0
        move.l  %d7,%d1
        rts
stop_no_frame:
        move.l  %d7,%d1
        movea.l _DOSBase,%a6
        jmp     LXA_BCPL_LVO_EXIT(%a6)

/* ------------------------------------------------------------------------
 * longcall(frame, label): continue at label with the given frame (a frame
 * that level() returned).
 * ---------------------------------------------------------------------- */
        .balign 4
        .globl  _lxa_bcpl_g_longcall
_lxa_bcpl_g_longcall:
        movea.l %d1,%a1
        movea.l -4(%a1),%a4
        movea.l %d2,%a3
        jmp     (%a3)

/* ------------------------------------------------------------------------
 * LONG lxa_bcpl_lvo(base, lvo, regs): regs = d0, d1, a0, a1, a2
 * ---------------------------------------------------------------------- */
        .balign 4
        .globl  _lxa_bcpl_lvo
_lxa_bcpl_lvo:
        movem.l %d2-%d7/%a2-%a6,-(%sp)
        movea.l 48(%sp),%a6
        move.l  52(%sp),%d2
        movea.l 56(%sp),%a3
        movem.l (%a3),%d0-%d1/%a0-%a2
        jsr     0(%a6,%d2.l)
        movem.l (%sp)+,%d2-%d7/%a2-%a6
        rts

/* ------------------------------------------------------------------------
 * The console segment (RootNode rn_ConsoleSegment): a BCPL hunk in ROM.
 * Layout as loaded: [size] [next] [length in longs] code ... and at its
 * end the global table, read backwards: highest global, then
 * (global, offset) pairs, then 0.  Global 1 runs lxa_bcpl_cli_main(packet,
 * free BCPL stack).
 * ---------------------------------------------------------------------- */
        .balign 4
cliseg_size:
        .long   cliseg_end - cliseg_size
        .globl  _lxa_bcpl_cliseg
_lxa_bcpl_cliseg:
        .long   0
cliseg_data:
        .long   (cliseg_top - _lxa_bcpl_cliseg) / 4
cliseg_start:
        movem.l %d2-%d7/%a0-%a6,-(%sp)
        pea     64(%a1)
        move.l  %d1,-(%sp)
        jsr     _lxa_bcpl_cli_main
        addq.l  #8,%sp
        movem.l (%sp)+,%d2-%d7/%a0-%a6
        move.l  %d0,%d1
        jmp     (%a6)
        .balign 4
        .long   0                           | end of the global table
        .long   1                           | global 1 ...
        .long   cliseg_start - cliseg_data  | ... at this offset
cliseg_top:
        .long   1                           | highest global
cliseg_end:
