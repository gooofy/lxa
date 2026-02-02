/*
 * Test: graphics/init_rastport
 * Tests InitRastPort() function to verify correct defaults
 *
 * Per AROS/RKRM specification:
 * - Mask = 0xFF (all planes enabled)
 * - FgPen = -1 (0xFF)
 * - BgPen = 0
 * - AOlPen = -1 (0xFF)
 * - DrawMode = JAM2
 * - LinePtrn = 0xFFFF (solid line)
 * - Flags = FRST_DOT
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

/* RastPort flags */
#ifndef FRST_DOT
#define FRST_DOT    0x01
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

    print("Testing InitRastPort()...\n");

    /* Initialize the RastPort */
    InitRastPort(&rp);

    /* Test default Mask value */
    if (rp.Mask != 0xFF) {
        print("FAIL: Mask != 0xFF\n");
        errors++;
    } else {
        print("OK: Mask = 0xFF\n");
    }

    /* Test default FgPen (foreground pen) - per AROS/RKRM should be -1 (0xFF) */
    if (rp.FgPen != -1) {
        print("FAIL: FgPen != -1\n");
        errors++;
    } else {
        print("OK: FgPen = -1\n");
    }

    /* Test default BgPen (background pen) */
    if (rp.BgPen != 0) {
        print("FAIL: BgPen != 0\n");
        errors++;
    } else {
        print("OK: BgPen = 0\n");
    }

    /* Test default AOlPen (outline pen for area fills) - per AROS/RKRM should be -1 */
    if (rp.AOlPen != -1) {
        print("FAIL: AOlPen != -1\n");
        errors++;
    } else {
        print("OK: AOlPen = -1\n");
    }

    /* Test default DrawMode */
    if (rp.DrawMode != JAM2) {
        print("FAIL: DrawMode != JAM2\n");
        errors++;
    } else {
        print("OK: DrawMode = JAM2\n");
    }

    /* Test default LinePtrn (solid line pattern) */
    if (rp.LinePtrn != 0xFFFF) {
        print("FAIL: LinePtrn != 0xFFFF\n");
        errors++;
    } else {
        print("OK: LinePtrn = 0xFFFF\n");
    }

    /* Test Flags contains FRST_DOT */
    if (!(rp.Flags & FRST_DOT)) {
        print("FAIL: Flags missing FRST_DOT\n");
        errors++;
    } else {
        print("OK: Flags contains FRST_DOT\n");
    }

    /* Test default cp_x, cp_y (cursor position) */
    if (rp.cp_x != 0 || rp.cp_y != 0) {
        print("FAIL: cp_x/cp_y != 0\n");
        errors++;
    } else {
        print("OK: cp_x=0, cp_y=0\n");
    }

    /* Test BitMap is NULL initially */
    if (rp.BitMap != NULL) {
        print("FAIL: BitMap != NULL\n");
        errors++;
    } else {
        print("OK: BitMap = NULL\n");
    }

    /* Final result */
    if (errors == 0) {
        print("PASS: InitRastPort() all tests passed\n");
        return 0;
    } else {
        print("FAIL: InitRastPort() had errors\n");
        return 20;
    }
}
