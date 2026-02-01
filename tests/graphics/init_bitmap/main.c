/*
 * Test: graphics/init_bitmap
 * Tests InitBitMap() function to verify BytesPerRow calculation
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
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
    int errors = 0;
    int i;

    print("Testing InitBitMap()...\n");

    /* Test 320x200 4-plane (standard lowres) */
    InitBitMap(&bm, 4, 320, 200);
    if (bm.BytesPerRow != 40) {
        print("FAIL: 320 pixels -> BytesPerRow != 40\n");
        errors++;
    } else {
        print("OK: 320 pixels -> BytesPerRow=40\n");
    }
    if (bm.Rows != 200) {
        print("FAIL: Rows != 200\n");
        errors++;
    } else {
        print("OK: Rows=200\n");
    }
    if (bm.Depth != 4) {
        print("FAIL: Depth != 4\n");
        errors++;
    } else {
        print("OK: Depth=4\n");
    }

    /* Test 17 pixels (should round to 4 bytes for word alignment) */
    InitBitMap(&bm, 1, 17, 10);
    if (bm.BytesPerRow != 4) {
        print("FAIL: 17 pixels -> BytesPerRow != 4\n");
        errors++;
    } else {
        print("OK: 17 pixels -> BytesPerRow=4 (word aligned)\n");
    }

    /* Test 16 pixels (exactly 2 bytes) */
    InitBitMap(&bm, 1, 16, 10);
    if (bm.BytesPerRow != 2) {
        print("FAIL: 16 pixels -> BytesPerRow != 2\n");
        errors++;
    } else {
        print("OK: 16 pixels -> BytesPerRow=2\n");
    }

    /* Test 640x400 8-plane (AGA style) */
    InitBitMap(&bm, 8, 640, 400);
    if (bm.BytesPerRow != 80) {
        print("FAIL: 640 pixels -> BytesPerRow != 80\n");
        errors++;
    } else {
        print("OK: 640 pixels -> BytesPerRow=80\n");
    }
    if (bm.Depth != 8) {
        print("FAIL: Depth != 8\n");
        errors++;
    } else {
        print("OK: Depth=8\n");
    }

    /* Test minimum width (1 pixel) */
    InitBitMap(&bm, 1, 1, 1);
    if (bm.BytesPerRow != 2) {
        print("FAIL: 1 pixel -> BytesPerRow != 2\n");
        errors++;
    } else {
        print("OK: 1 pixel -> BytesPerRow=2 (minimum word)\n");
    }

    /* Test that Planes are cleared */
    InitBitMap(&bm, 8, 64, 64);
    for (i = 0; i < 8; i++) {
        if (bm.Planes[i] != NULL) {
            print("FAIL: Planes not NULL after InitBitMap\n");
            errors++;
            break;
        }
    }
    if (i == 8) {
        print("OK: All plane pointers are NULL\n");
    }

    /* Final result */
    if (errors == 0) {
        print("PASS: InitBitMap() all tests passed\n");
        return 0;
    } else {
        print("FAIL: InitBitMap() had errors\n");
        return 20;
    }
}
