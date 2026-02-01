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

/* Common minterms - Amiga ABC notation (A=source, B=dest, C=pattern) */
#define MINTERM_COPY    0xF0    /* A = copy source to dest (bits 4,5,6,7 set) */
#define MINTERM_INVERT  0x0F    /* !A = invert source */
#define MINTERM_OR      0xFC    /* A | B = source OR dest */
#define MINTERM_AND     0xC0    /* A & B = source AND dest */

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

    /* Test 1: Simple copy (MINTERM_COPY = 0xC0) */
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
    blitted = BltBitMap(&srcBm, 0, 0, &destBm, 8, 0, 8, 8, MINTERM_COPY, 0xFF, NULL);

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
    blitted = BltBitMap(NULL, 0, 0, &destBm, 0, 0, 8, 8, MINTERM_COPY, 0xFF, NULL);
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
    blitted = BltBitMap(&srcBm, 0, 0, &destBm, 0, 0, 0, 0, MINTERM_COPY, 0xFF, NULL);
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
    blitted = BltBitMap(&srcBm, 0, 0, &destBm, 0, 0, -8, 8, MINTERM_COPY, 0xFF, NULL);
    if (blitted != 0)
    {
        print("FAIL: Negative size blit should return 0\n");
        errors++;
    }
    else
    {
        print("OK: Negative size blit returns 0\n");
    }

    /* Clean up */
    FreeRaster(srcPlane, 32, 8);
    FreeRaster(destPlane, 32, 8);

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
