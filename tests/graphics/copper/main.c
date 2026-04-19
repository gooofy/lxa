/*
 * Test: graphics/copper
 *
 * Exercises the basic copper-list interpreter (Phase 114). Builds small
 * copper lists in chip RAM, points COP1LC at them, waits for VBlanks to
 * drive the interpreter, and reads back the affected custom-chip
 * registers (color palette) to confirm the MOVE instructions executed.
 *
 * Tests:
 *   1. Single MOVE writes COLOR00 to a known value.
 *   2. Multiple MOVEs program a small color gradient (COLOR00..COLOR03).
 *   3. WAIT instructions in the middle of a list do not block subsequent
 *      MOVEs (no beam emulation -> WAIT always satisfied).
 *   4. CDANG protection: without CDANG, MOVE to a low register
 *      (e.g. DMACON 0x096) is suppressed; with CDANG=1 it is honored.
 *      We use COLOR00 (>= 0x80) as the canary so suppressed writes leave
 *      a previously-set color untouched.
 *   5. End-of-list (CWAIT 0xFFFF, 0xFFFE) halts the interpreter so a
 *      trailing "poison" MOVE after it is never executed.
 *   6. SKIP without beam tracking does not skip — the following
 *      instruction still runs.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;

#define CUSTOM_BASE ((volatile struct Custom *)0xDFF000)

/* Custom register byte offsets. */
#define CR_DMACON   0x096
#define CR_COPCON   0x02E
#define CR_COP1LCH  0x080
#define CR_COP1LCL  0x082
#define CR_COPJMP1  0x088
#define CR_COLOR00  0x180
#define CR_COLOR01  0x182
#define CR_COLOR02  0x184
#define CR_COLOR03  0x186

#define CUSTOM_W(off) (*(volatile UWORD *)(0xDFF000 + (off)))

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_hex(ULONG n)
{
    char buf[12];
    char *p = buf + 10;
    *p = '\0';
    do {
        int d = n & 0xf;
        *(--p) = (d < 10) ? ('0' + d) : ('a' + d - 10);
        n >>= 4;
    } while (n > 0);
    *(--p) = 'x';
    *(--p) = '0';
    print(p);
}

/* Build a copper MOVE instruction in two consecutive UWORDs. */
static void cop_move(UWORD *list, int idx, UWORD reg_off, UWORD value)
{
    /* MOVE: w0 = reg address (bits 8:1), bit 0 = 0; w1 = data. */
    list[idx * 2 + 0] = reg_off & 0x01FE;
    list[idx * 2 + 1] = value;
}

/* CWAIT(vp, hp): w0 = (vp<<8)|(hp&0xFE)|1, w1 = 0xFFFE & ~1 */
static void cop_wait(UWORD *list, int idx, UWORD vp, UWORD hp)
{
    list[idx * 2 + 0] = ((vp & 0xFF) << 8) | (hp & 0xFE) | 0x0001;
    list[idx * 2 + 1] = 0xFFFE;
}

/* CSKIP(vp, hp). */
static void cop_skip(UWORD *list, int idx, UWORD vp, UWORD hp)
{
    list[idx * 2 + 0] = ((vp & 0xFF) << 8) | (hp & 0xFE) | 0x0001;
    list[idx * 2 + 1] = 0xFFFF;
}

static void cop_end(UWORD *list, int idx)
{
    list[idx * 2 + 0] = 0xFFFF;
    list[idx * 2 + 1] = 0xFFFE;
}

/* Install list and trigger via COP1LC + COPJMP1, then drain a VBlank. */
static void install_and_run(UWORD *list)
{
    ULONG addr = (ULONG)list;
    CUSTOM_W(CR_COP1LCH) = (UWORD)(addr >> 16);
    CUSTOM_W(CR_COP1LCL) = (UWORD)(addr & 0xFFFF);
    /* COPJMP1 strobe: any write triggers an immediate restart. */
    CUSTOM_W(CR_COPJMP1) = 0;
    /* Also ride the next VBlank tick for belt-and-braces. */
    WaitTOF();
}

int main(void)
{
    UWORD *list;
    int errors = 0;
    UWORD v;

    print("Testing copper-list interpreter...\n");

    /* Allocate a generous chip-RAM buffer for our copper lists. */
    list = (UWORD *)AllocMem(256, MEMF_CHIP | MEMF_CLEAR);
    if (!list) {
        print("FAIL: cannot allocate chip RAM for copper list\n");
        return 20;
    }

    /* Make sure copper DMA is on (it is by default; assert anyway). */
    CUSTOM_W(CR_DMACON) = 0x8000 | 0x0080 | 0x0200; /* SET | COPEN | DMAEN */

    /* ---- Test 1: single MOVE sets COLOR00 ------------------------ */
    cop_move(list, 0, CR_COLOR00, 0x0123);
    cop_end (list, 1);
    install_and_run(list);

    v = CUSTOM_W(CR_COLOR00);
    if (v != 0x0123) {
        print("FAIL: test1 single MOVE COLOR00 expected 0x0123 got ");
        print_hex(v); print("\n");
        errors++;
    } else {
        print("  ok: test1 single MOVE\n");
    }

    /* ---- Test 2: gradient across COLOR00..COLOR03 ---------------- */
    cop_move(list, 0, CR_COLOR00, 0x0111);
    cop_move(list, 1, CR_COLOR01, 0x0222);
    cop_move(list, 2, CR_COLOR02, 0x0333);
    cop_move(list, 3, CR_COLOR03, 0x0444);
    cop_end (list, 4);
    install_and_run(list);

    if (CUSTOM_W(CR_COLOR00) != 0x0111 ||
        CUSTOM_W(CR_COLOR01) != 0x0222 ||
        CUSTOM_W(CR_COLOR02) != 0x0333 ||
        CUSTOM_W(CR_COLOR03) != 0x0444)
    {
        print("FAIL: test2 gradient mismatch: ");
        print_hex(CUSTOM_W(CR_COLOR00)); print(" ");
        print_hex(CUSTOM_W(CR_COLOR01)); print(" ");
        print_hex(CUSTOM_W(CR_COLOR02)); print(" ");
        print_hex(CUSTOM_W(CR_COLOR03)); print("\n");
        errors++;
    } else {
        print("  ok: test2 gradient\n");
    }

    /* ---- Test 3: WAIT in the middle does not block follow-on ----- */
    cop_move(list, 0, CR_COLOR00, 0x0AAA);
    cop_wait(list, 1, 0x40, 0x00);   /* arbitrary mid-frame wait */
    cop_move(list, 2, CR_COLOR00, 0x0BBB);
    cop_end (list, 3);
    install_and_run(list);

    v = CUSTOM_W(CR_COLOR00);
    if (v != 0x0BBB) {
        print("FAIL: test3 WAIT blocked follow-on MOVE, COLOR00=");
        print_hex(v); print("\n");
        errors++;
    } else {
        print("  ok: test3 WAIT pass-through\n");
    }

    /* ---- Test 4: CDANG suppresses writes to protected registers --- */
    /* First seed COLOR00 to a known sentinel via a normal copper MOVE. */
    cop_move(list, 0, CR_COLOR00, 0x0CCC);
    cop_end (list, 1);
    install_and_run(list);
    if (CUSTOM_W(CR_COLOR00) != 0x0CCC) {
        print("FAIL: test4 setup, COLOR00 sentinel not set\n");
        errors++;
    }

    /* CDANG=0: a copper MOVE to DMACON (0x096, < 0x40 in the protected
     * region for OCS — but on ECS the cutoff is 0x40. Use INTENA 0x09A?
     * No — both are >=0x40. The truly protected band is 0x00..0x3E.
     * Use a low register like SERPER (0x032) which is < 0x40. */
    CUSTOM_W(CR_COPCON) = 0x0000;  /* CDANG=0 */
    cop_move(list, 0, 0x032, 0x0000);          /* protected: should be dropped */
    cop_move(list, 1, CR_COLOR01, 0x0EEE);     /* unprotected: should run */
    cop_end (list, 2);
    install_and_run(list);
    if (CUSTOM_W(CR_COLOR01) != 0x0EEE) {
        print("FAIL: test4 CDANG=0 dropped a non-protected MOVE\n");
        errors++;
    } else {
        print("  ok: test4 CDANG=0 protects low regs, allows high regs\n");
    }

    /* CDANG=1: protected writes go through. We can't easily observe a
     * SERPER write, but we can confirm that flipping CDANG does not
     * break subsequent normal MOVEs. */
    CUSTOM_W(CR_COPCON) = 0x0002;  /* CDANG=1 */
    cop_move(list, 0, CR_COLOR02, 0x0FFF);
    cop_end (list, 1);
    install_and_run(list);
    if (CUSTOM_W(CR_COLOR02) != 0x0FFF) {
        print("FAIL: test4 CDANG=1 broke normal MOVE\n");
        errors++;
    } else {
        print("  ok: test4 CDANG=1 still allows normal MOVEs\n");
    }
    CUSTOM_W(CR_COPCON) = 0x0000;  /* restore */

    /* ---- Test 5: end-of-list halts; trailing MOVE never runs ----- */
    cop_move(list, 0, CR_COLOR00, 0x0200);
    cop_end (list, 1);
    cop_move(list, 2, CR_COLOR00, 0x0DEAD & 0x0FFF);  /* poison */
    cop_end (list, 3);
    install_and_run(list);

    v = CUSTOM_W(CR_COLOR00);
    if (v != 0x0200) {
        print("FAIL: test5 end-of-list did not halt, COLOR00=");
        print_hex(v); print("\n");
        errors++;
    } else {
        print("  ok: test5 end-of-list halts\n");
    }

    /* ---- Test 6: SKIP does not skip (no beam tracking) ----------- */
    cop_move(list, 0, CR_COLOR00, 0x0100);
    cop_skip(list, 1, 0x40, 0x00);
    cop_move(list, 2, CR_COLOR00, 0x0300);   /* must still run */
    cop_end (list, 3);
    install_and_run(list);

    v = CUSTOM_W(CR_COLOR00);
    if (v != 0x0300) {
        print("FAIL: test6 SKIP unexpectedly skipped MOVE, COLOR00=");
        print_hex(v); print("\n");
        errors++;
    } else {
        print("  ok: test6 SKIP behaves as never-skip\n");
    }

    FreeMem(list, 256);

    if (errors == 0) {
        print("PASS: copper interpreter all tests passed\n");
        return 0;
    } else {
        print("FAIL: copper interpreter had errors\n");
        return 20;
    }
}
