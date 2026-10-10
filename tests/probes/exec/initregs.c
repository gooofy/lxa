/*
 * Probe (Phase 237b): library vectors that do not preserve D2-D7/A2-A5.
 * InitResident() must still add the library (Fred Fish midi.library's Manx
 * C init trashes registers; lxa's InitResident lost its list pointer and
 * called AddResource() through a NULL base), and OpenLibrary() of a
 * library whose Open vector leaves A5 changed (Fish voice.library) keeps
 * the caller's registers.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/nodes.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

__asm__(
    "    .text\n"
    "    .globl _rg_open\n"
    "_rg_open:\n"                       /* as Fish voice.library: A5 = base */
    "    move.l a6,a5\n"
    "    addq.w #1,32(a6)\n"
    "    move.l a6,d0\n"
    "    rts\n"
    /* APTR rg_initresident(res): InitResident() with every register saved
     * (the probe only checks that the library is added) */
    "    .globl _rg_initresident\n"
    "_rg_initresident:\n"
    "    movem.l d2-d7/a2-a6,-(sp)\n"
    "    move.l 48(sp),a1\n"
    "    moveq #0,d1\n"
    "    move.l 4.w,a6\n"
    "    jsr -102(a6)\n"
    "    movem.l (sp)+,d2-d7/a2-a6\n"
    "    rts\n"
    /* ULONG rg_call_open(name): OpenLibrary with known D2-D7/A2-A5,
     * returns 1 when they all survived (0 when not, 2 when it failed) */
    "    .globl _rg_call_open\n"
    "_rg_call_open:\n"
    "    movem.l d2-d7/a2-a6,-(sp)\n"
    "    move.l 48(sp),a1\n"
    "    moveq #0,d0\n"
    "    move.l #0x11110002,d2\n"
    "    move.l #0x11110003,d3\n"
    "    move.l #0x11110004,d4\n"
    "    move.l #0x11110005,d5\n"
    "    move.l #0x11110006,d6\n"
    "    move.l #0x11110007,d7\n"
    "    move.l #0x2222000a,a2\n"
    "    move.l #0x2222000b,a3\n"
    "    move.l #0x2222000c,a4\n"
    "    move.l #0x2222000d,a5\n"
    "    move.l 4.w,a6\n"
    "    jsr -552(a6)\n"
    "    tst.l d0\n"
    "    beq.s 9f\n"
    "    move.l d0,a1\n"
    "    moveq #0,d0\n"
    "    cmp.l #0x11110002,d2\n"
    "    bne.s 8f\n"
    "    cmp.l #0x11110003,d3\n"
    "    bne.s 8f\n"
    "    cmp.l #0x11110004,d4\n"
    "    bne.s 8f\n"
    "    cmp.l #0x11110005,d5\n"
    "    bne.s 8f\n"
    "    cmp.l #0x11110006,d6\n"
    "    bne.s 8f\n"
    "    cmp.l #0x11110007,d7\n"
    "    bne.s 8f\n"
    "    cmp.l #0x2222000a,a2\n"
    "    bne.s 8f\n"
    "    cmp.l #0x2222000b,a3\n"
    "    bne.s 8f\n"
    "    cmp.l #0x2222000c,a4\n"
    "    bne.s 8f\n"
    "    cmp.l #0x2222000d,a5\n"
    "    bne.s 8f\n"
    "    moveq #1,d0\n"
    "8:  move.l d0,-(sp)\n"
    "    jsr -414(a6)\n"                   /* CloseLibrary(a1) */
    "    move.l (sp)+,d0\n"
    "    bra.s 7f\n"
    "9:  moveq #2,d0\n"
    "7:  movem.l (sp)+,d2-d7/a2-a6\n"
    "    rts\n"
    "    .globl _rg_null\n"
    "_rg_null:\n"
    "    moveq #0,d0\n"
    "    rts\n"
    "    .globl _rg_init\n"
    "_rg_init:\n"                       /* d0 = library: returned, all else trashed */
    "    move.l #0x5a5a0001,d1\n"
    "    move.l d1,d2\n"
    "    move.l d1,d3\n"
    "    move.l d1,d4\n"
    "    move.l d1,d5\n"
    "    move.l d1,d6\n"
    "    move.l d1,d7\n"
    "    move.l d1,a0\n"
    "    move.l d1,a1\n"
    "    move.l d1,a2\n"
    "    move.l d1,a3\n"
    "    move.l d1,a4\n"
    "    move.l d1,a5\n"
    "    rts\n");

extern void rg_open(void);
extern void rg_null(void);
extern void rg_init(void);
extern ULONG rg_call_open(char *name);
extern APTR rg_initresident(struct Resident *r);

static APTR functab[] = { (APTR)rg_open, (APTR)rg_null, (APTR)rg_null, (APTR)rg_null, (APTR)-1 };

static char name[] = "probeinitregs.library";
static char id[] = "probeinitregs 1.0 (1.1.24)\r\n";

struct RgInitTable { ULONG size; APTR *functions; APTR data; APTR init; };
static struct RgInitTable init = { sizeof(struct Library), functab, NULL, (APTR)rg_init };
static struct Resident res = {
    RTC_MATCHWORD, &res, &res + 1, RTF_AUTOINIT, 1, NT_LIBRARY, 0, name, id, &init
};

int main(void)
{
    struct Library *lib = (struct Library *)rg_initresident(&res);
    struct Node *n;

    probe_s("result ");
    probe_s(lib ? "library" : "NULL");
    probe_ch('\n');
    if (!lib)
        return 0;
    Forbid();
    n = FindName(&SysBase->LibList, (STRPTR)name);
    Permit();
    probe_s("in LibList ");
    probe_s(n == (struct Node *)lib ? "yes" : "no");
    probe_ch('\n');
    if (n != (struct Node *)lib)
        return 0;
    {
        ULONG r = rg_call_open(name);
        probe_s("OpenLibrary with an Open vector that changes A5: ");
        probe_s(r == 1 ? "registers kept" : (r == 0 ? "registers changed" : "failed"));
        probe_ch('\n');
    }
    Forbid();
    if (n)
        Remove(n);
    Permit();
    return 0;
}
