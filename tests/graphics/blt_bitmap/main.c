/*
 * Test: graphics/blt_bitmap
 * Tests BltBitMap() function for bitmap copying
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

static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + 15;
    BOOL neg = FALSE;
    *p = '\0';
    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }
    do
    {
        *(--p) = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    if (neg) *(--p) = '-';
    print(p);
}

static ULONG read_plane_pixel(struct BitMap *bm, WORD x, WORD y)
{
    struct RastPort rp;

    InitRastPort(&rp);
    rp.BitMap = bm;
    return ReadPixel(&rp, x, y);
}

/* Common blit minterms with A=rectangle, B=source, C=destination */
#define MINTERM_COPY_SOURCE 0xC0
#define MINTERM_INVERT_DEST 0x50

int main(void)
{
    struct BitMap srcBm, destBm;
    PLANEPTR srcPlane, destPlane;
    LONG blitted;
    int errors = 0;
    int i;

    print("Testing BltBitMap...\n");

    /* Allocate source and destination bitmaps (32x8 pixels, 1 bitplane) */
    srcPlane = AllocRaster(32, 8);
    destPlane = AllocRaster(32, 8);

    if (!srcPlane || !destPlane)
    {
        print("FAIL: Could not allocate rasters\n");
        if (srcPlane) FreeRaster(srcPlane, 32, 8);
        if (destPlane) FreeRaster(destPlane, 32, 8);
        return 20;
    }

    /* Initialize bitmaps */
    InitBitMap(&srcBm, 1, 32, 8);
    srcBm.Planes[0] = srcPlane;

    InitBitMap(&destBm, 1, 32, 8);
    destBm.Planes[0] = destPlane;

    /* Test 1: Simple copy (B -> D) */
    print("Test 1: Simple copy...\n");

    /* Clear both bitmaps */
    for (i = 0; i < 32; i++)  /* 32 bytes = 32 pixels wide * 8 rows / 8 bits */
    {
        srcPlane[i] = 0;
        destPlane[i] = 0;
    }

    /* Set a pattern in source: first byte of each row = 0xAA */
    for (i = 0; i < 8; i++)
    {
        srcPlane[i * 4] = 0xAA;  /* 4 bytes per row for 32 pixels */
    }

    /* Copy 8x8 pixels from (0,0) in source to (8,0) in dest */
    blitted = BltBitMap(&srcBm, 0, 0, &destBm, 8, 0, 8, 8, MINTERM_COPY_SOURCE, 0xFF, NULL);

    /* Check that dest now has pattern at offset 8 */
    if (destPlane[1] != 0xAA)  /* byte 1 = pixels 8-15 */
    {
        print("FAIL: BltBitMap copy did not work, got ");
        print_num(destPlane[1]);
        print(" expected 170 (0xAA)\n");
        errors++;
    }
    else
    {
        print("OK: BltBitMap copy worked\n");
    }

    /* Test 2: Verify source wasn't modified */
    print("Test 2: Source unchanged...\n");
    if (srcPlane[0] != 0xAA)
    {
        print("FAIL: Source was modified\n");
        errors++;
    }
    else
    {
        print("OK: Source unchanged\n");
    }

    /* Test 3: NULL bitmap handling */
    print("Test 3: NULL bitmap handling...\n");
    blitted = BltBitMap(NULL, 0, 0, &destBm, 0, 0, 8, 8, MINTERM_COPY_SOURCE, 0xFF, NULL);
    if (blitted != 0)
    {
        print("FAIL: BltBitMap with NULL src should return 0\n");
        errors++;
    }
    else
    {
        print("OK: NULL src returns 0\n");
    }

    /* Test 4: Zero size blit */
    print("Test 4: Zero size blit...\n");
    blitted = BltBitMap(&srcBm, 0, 0, &destBm, 0, 0, 0, 0, MINTERM_COPY_SOURCE, 0xFF, NULL);
    if (blitted != 0)
    {
        print("FAIL: Zero size blit should return 0\n");
        errors++;
    }
    else
    {
        print("OK: Zero size blit returns 0\n");
    }

    /* Test 5: Negative size blit */
    print("Test 5: Negative size blit...\n");
    blitted = BltBitMap(&srcBm, 0, 0, &destBm, 0, 0, -8, 8, MINTERM_COPY_SOURCE, 0xFF, NULL);
    if (blitted != 0)
    {
        print("FAIL: Negative size blit should return 0\n");
        errors++;
    }
    else
    {
        print("OK: Negative size blit returns 0\n");
    }

    /* Test 6: Overlapping copy within the same bitmap */
    print("Test 6: Overlapping self-copy shifts data correctly...\n");
    for (i = 0; i < 32; i++)
        destPlane[i] = 0;
    for (i = 0; i < 8; i++)
        destPlane[i * 4] = 0xF0;

    blitted = BltBitMap(&destBm, 0, 0, &destBm, 4, 0, 8, 8, MINTERM_COPY_SOURCE, 0x01, NULL);
    if (blitted != 1 || destPlane[0] != 0xFF || destPlane[4] != 0xFF) {
        print("FAIL: Overlapping self-copy did not preserve source bits\n");
        errors++;
    } else {
        print("OK: Overlapping self-copy preserved source bits\n");
    }

    /* Test 7: Minterm 0x50 inverts destination when source is ignored */
    print("Test 7: Minterm 0x50 inverts destination bits...\n");
    for (i = 0; i < 32; i++) {
        srcPlane[i] = 0x00;
        destPlane[i] = 0x00;
    }
    for (i = 0; i < 8; i++)
        destPlane[i * 4] = 0x3C;

    blitted = BltBitMap(&srcBm, 0, 0, &destBm, 0, 0, 8, 8, MINTERM_INVERT_DEST, 0x01, NULL);
    if (blitted != 1 || destPlane[0] != 0xC3 || destPlane[4] != 0xC3) {
        print("FAIL: Minterm 0x50 did not invert destination bits\n");
        errors++;
    } else {
        print("OK: Minterm 0x50 inverted destination bits\n");
    }

    /* Test 8: Plane mask limits affected planes */
    print("Test 8: Plane mask updates only selected planes...\n");
    FreeRaster(srcPlane, 32, 8);
    FreeRaster(destPlane, 32, 8);

    {
        PLANEPTR srcPlane0 = AllocRaster(16, 8);
        PLANEPTR srcPlane1 = AllocRaster(16, 8);
        PLANEPTR destPlane0 = AllocRaster(16, 8);
        PLANEPTR destPlane1 = AllocRaster(16, 8);

        if (!srcPlane0 || !srcPlane1 || !destPlane0 || !destPlane1) {
            print("FAIL: Could not allocate rasters for plane-mask test\n");
            errors++;
        } else {
            struct BitMap src2;
            struct BitMap dest2;

            for (i = 0; i < 16; i++) {
                srcPlane0[i] = 0xFF;
                srcPlane1[i] = 0x00;
                destPlane0[i] = 0x00;
                destPlane1[i] = 0xFF;
            }

            InitBitMap(&src2, 2, 16, 8);
            InitBitMap(&dest2, 2, 16, 8);
            src2.Planes[0] = srcPlane0;
            src2.Planes[1] = srcPlane1;
            dest2.Planes[0] = destPlane0;
            dest2.Planes[1] = destPlane1;

            blitted = BltBitMap(&src2, 0, 0, &dest2, 0, 0, 8, 8, MINTERM_COPY_SOURCE, 0x01, NULL);
            if (blitted != 1 || read_plane_pixel(&dest2, 1, 1) != 3 || read_plane_pixel(&dest2, 9, 1) != 2) {
                print("FAIL: Plane mask did not leave the second plane untouched\n");
                errors++;
            } else {
                print("OK: Plane mask limited the blit to plane 0\n");
            }
        }

        if (srcPlane0) FreeRaster(srcPlane0, 16, 8);
        if (srcPlane1) FreeRaster(srcPlane1, 16, 8);
        if (destPlane0) FreeRaster(destPlane0, 16, 8);
        if (destPlane1) FreeRaster(destPlane1, 16, 8);
    }

    /* Clean up */
    srcPlane = NULL;
    destPlane = NULL;

    /* Final result */
    if (errors == 0)
    {
        print("PASS: blt_bitmap all tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: blt_bitmap had errors\n");
        return 20;
    }
}
