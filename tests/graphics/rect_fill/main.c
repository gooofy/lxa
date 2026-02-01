/*
 * Test: graphics/rect_fill
 * Tests RectFill() including COMPLEMENT mode
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
    int rect_ok;

    print("Testing RectFill()...\n");

    /* Setup: 64x64 bitmap with 2 planes (4 colors) */
    InitBitMap(&bm, 2, 64, 64);
    bm.Planes[0] = AllocRaster(64, 64);
    bm.Planes[1] = AllocRaster(64, 64);

    if (!bm.Planes[0] || !bm.Planes[1]) {
        print("FAIL: Could not allocate raster planes\n");
        return 20;
    }

    InitRastPort(&rp);
    rp.BitMap = &bm;

    /* Clear bitmap */
    SetRast(&rp, 0);

    /* Test basic RectFill */
    SetAPen(&rp, 1);
    RectFill(&rp, 10, 10, 30, 30);

    /* Verify all pixels inside rect are set */
    rect_ok = 1;
    for (y = 10; y <= 30; y++) {
        for (x = 10; x <= 30; x++) {
            color = ReadPixel(&rp, x, y);
            if (color != 1) {
                print("FAIL: Inside rect pixel != 1\n");
                errors++;
                rect_ok = 0;
                break;
            }
        }
        if (!rect_ok) break;
    }
    if (rect_ok) {
        print("OK: RectFill (10,10)-(30,30) filled with color 1\n");
    }

    /* Verify outside rect is still 0 */
    color = ReadPixel(&rp, 9, 10);
    if (color != 0) {
        print("FAIL: Outside rect left != 0\n");
        errors++;
    }
    color = ReadPixel(&rp, 31, 10);
    if (color != 0) {
        print("FAIL: Outside rect right != 0\n");
        errors++;
    }
    color = ReadPixel(&rp, 10, 9);
    if (color != 0) {
        print("FAIL: Outside rect top != 0\n");
        errors++;
    }
    color = ReadPixel(&rp, 10, 31);
    if (color != 0) {
        print("FAIL: Outside rect bottom != 0\n");
        errors++;
    } else {
        print("OK: Outside rect area is clear\n");
    }

    /* Test COMPLEMENT mode */
    SetDrMd(&rp, COMPLEMENT);
    SetAPen(&rp, 1);  /* XOR with 1 */
    RectFill(&rp, 15, 15, 25, 25);  /* XOR inner area */

    /* Inner area should now be 0 (1 XOR 1 = 0) */
    color = ReadPixel(&rp, 20, 20);
    if (color != 0) {
        print("FAIL: COMPLEMENT mode at (20,20) != 0\n");
        errors++;
    } else {
        print("OK: COMPLEMENT mode toggled inner area to 0\n");
    }

    /* Outer area of original rect should still be 1 */
    color = ReadPixel(&rp, 11, 11);
    if (color != 1) {
        print("FAIL: Outer area (11,11) != 1\n");
        errors++;
    } else {
        print("OK: Outer area still has color 1\n");
    }

    /* Reset mode and test with different color */
    SetDrMd(&rp, JAM1);
    SetRast(&rp, 0);
    SetAPen(&rp, 3);
    RectFill(&rp, 0, 0, 10, 10);

    color = ReadPixel(&rp, 5, 5);
    if (color != 3) {
        print("FAIL: RectFill color 3 at (5,5) failed\n");
        errors++;
    } else {
        print("OK: RectFill with color 3 works\n");
    }

    /* Test single pixel rect */
    SetRast(&rp, 0);
    SetAPen(&rp, 2);
    RectFill(&rp, 32, 32, 32, 32);

    color = ReadPixel(&rp, 32, 32);
    if (color != 2) {
        print("FAIL: Single pixel rect at (32,32) != 2\n");
        errors++;
    } else {
        print("OK: Single pixel RectFill works\n");
    }

    /* Verify neighbors are not affected */
    if (ReadPixel(&rp, 31, 32) != 0 || ReadPixel(&rp, 33, 32) != 0 ||
        ReadPixel(&rp, 32, 31) != 0 || ReadPixel(&rp, 32, 33) != 0) {
        print("FAIL: Single pixel rect leaked to neighbors\n");
        errors++;
    } else {
        print("OK: Single pixel rect neighbors are clear\n");
    }

    /* Test rect at edge of bitmap */
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    RectFill(&rp, 0, 0, 63, 0);  /* Top row */

    color = ReadPixel(&rp, 0, 0);
    if (color != 1) {
        print("FAIL: Edge rect start (0,0) != 1\n");
        errors++;
    }
    color = ReadPixel(&rp, 63, 0);
    if (color != 1) {
        print("FAIL: Edge rect end (63,0) != 1\n");
        errors++;
    } else {
        print("OK: RectFill at bitmap edge works\n");
    }

    /* Cleanup */
    FreeRaster(bm.Planes[0], 64, 64);
    FreeRaster(bm.Planes[1], 64, 64);

    /* Final result */
    if (errors == 0) {
        print("PASS: RectFill all tests passed\n");
        return 0;
    } else {
        print("FAIL: RectFill had errors\n");
        return 20;
    }
}
