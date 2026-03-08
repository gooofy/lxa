/*
 * Test: graphics/line_draw
 * Tests Draw() for horizontal, vertical, diagonal lines
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
    struct BitMap bm;
    struct RastPort rp;
    LONG color;
    int errors = 0;
    int x, y;
    int line_ok;

    print("Testing Draw()...\n");

    /* Setup: 64x64 bitmap with 1 plane */
    InitBitMap(&bm, 1, 64, 64);
    bm.Planes[0] = AllocRaster(64, 64);

    if (!bm.Planes[0]) {
        print("FAIL: Could not allocate raster plane\n");
        return 20;
    }

    InitRastPort(&rp);
    rp.BitMap = &bm;

    /* Clear bitmap */
    SetRast(&rp, 0);

    /* Test horizontal line */
    SetAPen(&rp, 1);
    Move(&rp, 0, 10);
    Draw(&rp, 63, 10);

    line_ok = 1;
    for (x = 0; x <= 63; x++) {
        color = ReadPixel(&rp, x, 10);
        if (color != 1) {
            print("FAIL: Horizontal line pixel missing\n");
            errors++;
            line_ok = 0;
            break;
        }
    }
    if (line_ok) {
        print("OK: Horizontal line (0,10)-(63,10) all pixels set\n");
    }

    /* Verify current position updated */
    if (rp.cp_x != 63 || rp.cp_y != 10) {
        print("FAIL: After Draw, cp not updated to (63,10)\n");
        errors++;
    } else {
        print("OK: Current position updated to (63,10)\n");
    }

    /* Clear and test vertical line */
    SetRast(&rp, 0);
    Move(&rp, 32, 0);
    Draw(&rp, 32, 63);

    line_ok = 1;
    for (y = 0; y <= 63; y++) {
        color = ReadPixel(&rp, 32, y);
        if (color != 1) {
            print("FAIL: Vertical line pixel missing\n");
            errors++;
            line_ok = 0;
            break;
        }
    }
    if (line_ok) {
        print("OK: Vertical line (32,0)-(32,63) all pixels set\n");
    }

    /* Clear and test diagonal line */
    SetRast(&rp, 0);
    Move(&rp, 0, 0);
    Draw(&rp, 31, 31);

    line_ok = 1;
    for (x = 0; x <= 31; x++) {
        color = ReadPixel(&rp, x, x);
        if (color != 1) {
            print("FAIL: Diagonal line pixel missing\n");
            errors++;
            line_ok = 0;
            break;
        }
    }
    if (line_ok) {
        print("OK: Diagonal line (0,0)-(31,31) all pixels set\n");
    }

    /* Test anti-diagonal line */
    SetRast(&rp, 0);
    Move(&rp, 31, 0);
    Draw(&rp, 0, 31);

    line_ok = 1;
    for (x = 0; x <= 31; x++) {
        color = ReadPixel(&rp, 31 - x, x);
        if (color != 1) {
            print("FAIL: Anti-diagonal line pixel missing\n");
            errors++;
            line_ok = 0;
            break;
        }
    }
    if (line_ok) {
        print("OK: Anti-diagonal line (31,0)-(0,31) all pixels set\n");
    }

    /* Test single point (Draw to same location) */
    SetRast(&rp, 0);
    Move(&rp, 20, 20);
    Draw(&rp, 20, 20);

    color = ReadPixel(&rp, 20, 20);
    if (color != 1) {
        print("FAIL: Single point Draw at (20,20) failed\n");
        errors++;
    } else {
        print("OK: Single point Draw at (20,20) works\n");
    }

    /* Test short line (3 pixels) */
    SetRast(&rp, 0);
    Move(&rp, 10, 5);
    Draw(&rp, 12, 5);

    color = ReadPixel(&rp, 10, 5);
    if (color != 1) {
        print("FAIL: Short line start (10,5) missing\n");
        errors++;
    }
    color = ReadPixel(&rp, 11, 5);
    if (color != 1) {
        print("FAIL: Short line middle (11,5) missing\n");
        errors++;
    }
    color = ReadPixel(&rp, 12, 5);
    if (color != 1) {
        print("FAIL: Short line end (12,5) missing\n");
        errors++;
    } else {
        print("OK: Short horizontal line (10,5)-(12,5) works\n");
    }

    /* Test COMPLEMENT mode toggles pixels */
    SetRast(&rp, 0);
    SetDrMd(&rp, COMPLEMENT);
    SetAPen(&rp, 1);
    Move(&rp, 5, 40);
    Draw(&rp, 7, 40);

    if (ReadPixel(&rp, 6, 40) != 1) {
        print("FAIL: COMPLEMENT Draw did not set pixel\n");
        errors++;
    } else {
        print("OK: COMPLEMENT Draw sets pixels on first pass\n");
    }

    Move(&rp, 5, 40);
    Draw(&rp, 7, 40);
    if (ReadPixel(&rp, 6, 40) != 0) {
        print("FAIL: COMPLEMENT Draw did not toggle pixel off\n");
        errors++;
    } else {
        print("OK: COMPLEMENT Draw toggles pixels on second pass\n");
    }

    /* Test INVERSVID uses background pen */
    SetDrMd(&rp, JAM1 | INVERSVID);
    SetRast(&rp, 0);
    SetAPen(&rp, 0);
    SetBPen(&rp, 1);
    Move(&rp, 5, 45);
    Draw(&rp, 7, 45);

    if (ReadPixel(&rp, 6, 45) != 1) {
        print("FAIL: INVERSVID Draw did not use background pen\n");
        errors++;
    } else {
        print("OK: INVERSVID Draw uses background pen\n");
    }

    SetDrMd(&rp, JAM1);

    /* Cleanup */
    FreeRaster(bm.Planes[0], 64, 64);

    /* Final result */
    if (errors == 0) {
        print("PASS: Draw() all tests passed\n");
        return 0;
    } else {
        print("FAIL: Draw() had errors\n");
        return 20;
    }
}
