/*
 * Test: graphics/set_rast
 * Tests SetRast() clears bitmap to various colors
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
    int rast_ok;

    print("Testing SetRast()...\n");

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

    /* Test SetRast(0) - clear to color 0 */
    SetRast(&rp, 0);

    rast_ok = 1;
    for (y = 0; y < 64; y += 16) {
        for (x = 0; x < 64; x += 16) {
            color = ReadPixel(&rp, x, y);
            if (color != 0) {
                print("FAIL: SetRast(0) pixel != 0\n");
                errors++;
                rast_ok = 0;
                break;
            }
        }
        if (!rast_ok) break;
    }
    if (rast_ok) {
        print("OK: SetRast(0) clears bitmap to color 0\n");
    }

    /* Test SetRast(1) - fill with color 1 */
    SetRast(&rp, 1);

    rast_ok = 1;
    for (y = 0; y < 64; y += 16) {
        for (x = 0; x < 64; x += 16) {
            color = ReadPixel(&rp, x, y);
            if (color != 1) {
                print("FAIL: SetRast(1) pixel != 1\n");
                errors++;
                rast_ok = 0;
                break;
            }
        }
        if (!rast_ok) break;
    }
    if (rast_ok) {
        print("OK: SetRast(1) fills bitmap with color 1\n");
    }

    /* Test SetRast(2) */
    SetRast(&rp, 2);

    rast_ok = 1;
    for (y = 0; y < 64; y += 16) {
        for (x = 0; x < 64; x += 16) {
            color = ReadPixel(&rp, x, y);
            if (color != 2) {
                print("FAIL: SetRast(2) pixel != 2\n");
                errors++;
                rast_ok = 0;
                break;
            }
        }
        if (!rast_ok) break;
    }
    if (rast_ok) {
        print("OK: SetRast(2) fills bitmap with color 2\n");
    }

    /* Test SetRast(3) */
    SetRast(&rp, 3);

    rast_ok = 1;
    for (y = 0; y < 64; y += 16) {
        for (x = 0; x < 64; x += 16) {
            color = ReadPixel(&rp, x, y);
            if (color != 3) {
                print("FAIL: SetRast(3) pixel != 3\n");
                errors++;
                rast_ok = 0;
                break;
            }
        }
        if (!rast_ok) break;
    }
    if (rast_ok) {
        print("OK: SetRast(3) fills bitmap with color 3\n");
    }

    /* Test corner pixels after SetRast */
    SetRast(&rp, 1);
    color = ReadPixel(&rp, 0, 0);
    if (color != 1) {
        print("FAIL: Corner (0,0) != 1\n");
        errors++;
    }
    color = ReadPixel(&rp, 63, 0);
    if (color != 1) {
        print("FAIL: Corner (63,0) != 1\n");
        errors++;
    }
    color = ReadPixel(&rp, 0, 63);
    if (color != 1) {
        print("FAIL: Corner (0,63) != 1\n");
        errors++;
    }
    color = ReadPixel(&rp, 63, 63);
    if (color != 1) {
        print("FAIL: Corner (63,63) != 1\n");
        errors++;
    } else {
        print("OK: All four corners correct after SetRast\n");
    }

    /* Draw something then clear it */
    SetAPen(&rp, 3);
    RectFill(&rp, 20, 20, 40, 40);
    SetRast(&rp, 0);

    color = ReadPixel(&rp, 30, 30);
    if (color != 0) {
        print("FAIL: SetRast did not clear drawn rect\n");
        errors++;
    } else {
        print("OK: SetRast clears previously drawn content\n");
    }

    /* Cleanup */
    FreeRaster(bm.Planes[0], 64, 64);
    FreeRaster(bm.Planes[1], 64, 64);

    /* Final result */
    if (errors == 0) {
        print("PASS: SetRast all tests passed\n");
        return 0;
    } else {
        print("FAIL: SetRast had errors\n");
        return 20;
    }
}
