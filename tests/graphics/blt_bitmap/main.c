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

    /* Test 9: Non-byte-aligned destination X shift (Phase 112 regression guard).
     * Verifies that a BltBitMap to an odd X offset correctly shifts source
     * bits and does not leave garbage in the leading/trailing sub-byte. */
    print("Test 9: Non-byte-aligned destination shift...\n");
    {
        PLANEPTR s = AllocRaster(32, 4);
        PLANEPTR d = AllocRaster(32, 4);
        struct BitMap sbm, dbm;
        int shift;
        int shift_errors = 0;

        if (!s || !d) {
            print("FAIL: alloc for shift test\n");
            errors++;
        } else {
            InitBitMap(&sbm, 1, 32, 4);
            InitBitMap(&dbm, 1, 32, 4);
            sbm.Planes[0] = s;
            dbm.Planes[0] = d;

            /* Source: all-ones in a known rectangle */
            for (i = 0; i < 16; i++) s[i] = 0xFF;

            for (shift = 1; shift < 8; shift++) {
                int x, y;
                int ok = 1;
                for (i = 0; i < 16; i++) d[i] = 0x00;
                /* Blit 8x4 rect from (0,0) to (shift, 0) */
                BltBitMap(&sbm, 0, 0, &dbm, shift, 0, 8, 4,
                          MINTERM_COPY_SOURCE, 0x01, NULL);
                /* Verify: pixels [shift..shift+7] in each row should be 1,
                 * all others 0. */
                for (y = 0; y < 4; y++) {
                    for (x = 0; x < 32; x++) {
                        ULONG expected = (x >= shift && x < shift + 8) ? 1 : 0;
                        if (read_plane_pixel(&dbm, x, y) != expected) {
                            ok = 0;
                        }
                    }
                }
                if (!ok) shift_errors++;
            }

            if (shift_errors) {
                print("FAIL: Non-byte-aligned shift produced wrong pixels (");
                print_num(shift_errors);
                print(" of 7)\n");
                errors++;
            } else {
                print("OK: Non-byte-aligned shifts (1..7) work correctly\n");
            }
        }
        if (s) FreeRaster(s, 32, 4);
        if (d) FreeRaster(d, 32, 4);
    }

    /* Test 10: Minterm 0x30 (NOT source) — used by DrawImageState IDS_SELECTED */
    print("Test 10: Minterm 0x30 (NOT source)...\n");
    {
        PLANEPTR s = AllocRaster(16, 4);
        PLANEPTR d = AllocRaster(16, 4);
        struct BitMap sbm, dbm;
        if (!s || !d) {
            print("FAIL: alloc for minterm 0x30\n");
            errors++;
        } else {
            InitBitMap(&sbm, 1, 16, 4);
            InitBitMap(&dbm, 1, 16, 4);
            sbm.Planes[0] = s;
            dbm.Planes[0] = d;
            for (i = 0; i < 8; i++) {
                s[i] = 0xAA;
                d[i] = 0x00;
            }
            BltBitMap(&sbm, 0, 0, &dbm, 0, 0, 8, 4, 0x30, 0x01, NULL);
            /* Dest = NOT source = 0x55 */
            if (d[0] != 0x55) {
                print("FAIL: minterm 0x30 got ");
                print_num(d[0]);
                print(" expected 85 (0x55)\n");
                errors++;
            } else {
                print("OK: Minterm 0x30 produces NOT(source)\n");
            }
        }
        if (s) FreeRaster(s, 16, 4);
        if (d) FreeRaster(d, 16, 4);
    }

    /* Test 11: NULL source plane treated as all-zero (DrawImage planeonoff).
     * Per lxa_graphics.c:GetPlaneBit, a NULL plane reads as 0. With
     * minterm 0x30 (NOT src), dest should become all-ones. */
    print("Test 11: NULL source plane behaves as all-zero...\n");
    {
        struct BitMap sbm, dbm;
        PLANEPTR d = AllocRaster(16, 4);
        if (!d) {
            print("FAIL: alloc for NULL plane\n");
            errors++;
        } else {
            InitBitMap(&sbm, 1, 16, 4);
            InitBitMap(&dbm, 1, 16, 4);
            sbm.Planes[0] = NULL;  /* all-zero source */
            dbm.Planes[0] = d;
            for (i = 0; i < 8; i++) d[i] = 0x00;
            BltBitMap(&sbm, 0, 0, &dbm, 0, 0, 8, 4, 0x30, 0x01, NULL);
            if (d[0] != 0xFF) {
                print("FAIL: NULL source + 0x30 got ");
                print_num(d[0]);
                print(" expected 255\n");
                errors++;
            } else {
                print("OK: NULL source plane behaves as all-zero\n");
            }
        }
        if (d) FreeRaster(d, 16, 4);
    }

    /* Test 12: (PLANEPTR)-1 source plane treated as all-one
     * (DrawImage planeonoff bit set). With minterm 0xC0 (copy source),
     * dest should become all-ones. */
    print("Test 12: (PLANEPTR)-1 source plane behaves as all-one...\n");
    {
        struct BitMap sbm, dbm;
        PLANEPTR d = AllocRaster(16, 4);
        if (!d) {
            print("FAIL: alloc for -1 plane\n");
            errors++;
        } else {
            InitBitMap(&sbm, 1, 16, 4);
            InitBitMap(&dbm, 1, 16, 4);
            sbm.Planes[0] = (PLANEPTR)-1;  /* all-one source */
            dbm.Planes[0] = d;
            for (i = 0; i < 8; i++) d[i] = 0x00;
            BltBitMap(&sbm, 0, 0, &dbm, 0, 0, 8, 4,
                      MINTERM_COPY_SOURCE, 0x01, NULL);
            if (d[0] != 0xFF) {
                print("FAIL: -1 source + 0xC0 got ");
                print_num(d[0]);
                print(" expected 255\n");
                errors++;
            } else {
                print("OK: (PLANEPTR)-1 source plane behaves as all-one\n");
            }
        }
        if (d) FreeRaster(d, 16, 4);
    }

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
