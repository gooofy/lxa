/*
 * Test: graphics/pen_state
 * Tests pen/mode functions (SetAPen, SetBPen, SetDrMd, GetAPen, GetBPen, GetDrMd, SetABPenDrMd, Move)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;

/* Drawing modes */
#ifndef JAM1
#define JAM1        0
#define JAM2        1
#define COMPLEMENT  2
#define INVERSVID   4
#endif

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

int main(void)
{
    struct RastPort rp;
    int errors = 0;
    ULONG val;

    print("Testing pen/mode functions...\n");

    InitRastPort(&rp);

    /* Test SetAPen/GetAPen */
    SetAPen(&rp, 5);
    val = GetAPen(&rp);
    if (val != 5) {
        print("FAIL: GetAPen() != 5 after SetAPen(5)\n");
        errors++;
    } else {
        print("OK: SetAPen(5)/GetAPen() = 5\n");
    }

    /* Test SetBPen/GetBPen */
    SetBPen(&rp, 3);
    val = GetBPen(&rp);
    if (val != 3) {
        print("FAIL: GetBPen() != 3 after SetBPen(3)\n");
        errors++;
    } else {
        print("OK: SetBPen(3)/GetBPen() = 3\n");
    }

    /* Test SetDrMd/GetDrMd with JAM1 */
    SetDrMd(&rp, JAM1);
    val = GetDrMd(&rp);
    if (val != JAM1) {
        print("FAIL: GetDrMd() != JAM1 after SetDrMd(JAM1)\n");
        errors++;
    } else {
        print("OK: SetDrMd(JAM1)/GetDrMd() = JAM1\n");
    }

    /* Test SetDrMd with COMPLEMENT */
    SetDrMd(&rp, COMPLEMENT);
    val = GetDrMd(&rp);
    if (val != COMPLEMENT) {
        print("FAIL: GetDrMd() != COMPLEMENT\n");
        errors++;
    } else {
        print("OK: SetDrMd(COMPLEMENT)/GetDrMd() = COMPLEMENT\n");
    }

    /* Test SetABPenDrMd */
    SetABPenDrMd(&rp, 7, 2, JAM2);
    val = GetAPen(&rp);
    if (val != 7) {
        print("FAIL: GetAPen() != 7 after SetABPenDrMd\n");
        errors++;
    } else {
        print("OK: SetABPenDrMd APen = 7\n");
    }

    val = GetBPen(&rp);
    if (val != 2) {
        print("FAIL: GetBPen() != 2 after SetABPenDrMd\n");
        errors++;
    } else {
        print("OK: SetABPenDrMd BPen = 2\n");
    }

    val = GetDrMd(&rp);
    if (val != JAM2) {
        print("FAIL: GetDrMd() != JAM2 after SetABPenDrMd\n");
        errors++;
    } else {
        print("OK: SetABPenDrMd DrMd = JAM2\n");
    }

    /* Test Move */
    Move(&rp, 100, 50);
    if (rp.cp_x != 100 || rp.cp_y != 50) {
        print("FAIL: Move(100,50) failed\n");
        errors++;
    } else {
        print("OK: Move(100,50) -> cp_x=100, cp_y=50\n");
    }

    /* Test Move with negative coords */
    Move(&rp, -10, -20);
    if (rp.cp_x != -10 || rp.cp_y != -20) {
        print("FAIL: Move(-10,-20) failed\n");
        errors++;
    } else {
        print("OK: Move(-10,-20) -> cp_x=-10, cp_y=-20\n");
    }

    /* Test pen value 255 (maximum) */
    SetAPen(&rp, 255);
    val = GetAPen(&rp);
    if (val != 255) {
        print("FAIL: SetAPen(255)/GetAPen() != 255\n");
        errors++;
    } else {
        print("OK: SetAPen(255)/GetAPen() = 255\n");
    }

    /* Final result */
    if (errors == 0) {
        print("PASS: Pen/mode functions all tests passed\n");
        return 0;
    } else {
        print("FAIL: Pen/mode functions had errors\n");
        return 20;
    }
}
