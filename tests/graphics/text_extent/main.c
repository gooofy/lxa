/*
 * Test: graphics/text_extent
 * Tests TextExtent() and TextFit() functions
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
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

int main(void)
{
    struct RastPort rp;
    struct BitMap bm;
    struct TextAttr ta;
    struct TextFont *font;
    struct TextExtent te;
    PLANEPTR plane;
    ULONG numFit;
    int errors = 0;

    print("Testing TextExtent and TextFit...\n");

    /* Allocate a small bitmap (64x16 pixels, 1 bitplane) */
    plane = AllocRaster(64, 16);
    if (!plane)
    {
        print("FAIL: Could not allocate raster\n");
        return 20;
    }

    /* Initialize bitmap */
    InitBitMap(&bm, 1, 64, 16);
    bm.Planes[0] = plane;

    /* Initialize rastport */
    InitRastPort(&rp);
    rp.BitMap = &bm;

    /* Set up TextAttr for topaz 8 */
    ta.ta_Name = (STRPTR)"topaz.font";
    ta.ta_YSize = 8;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;

    /* Open font */
    print("Opening topaz.font 8...\n");
    font = OpenFont(&ta);
    if (!font)
    {
        print("FAIL: OpenFont() returned NULL\n");
        FreeRaster(plane, 64, 16);
        return 20;
    }

    SetFont(&rp, font);
    print("OK: Font set\n");

    /* Test 1: TextExtent with "Hello" */
    print("\nTest 1: TextExtent(Hello, 5)...\n");
    TextExtent(&rp, (CONST_STRPTR)"Hello", 5, &te);

    print("  te_Width = ");
    print_num(te.te_Width);
    print(" (expected 40)\n");
    if (te.te_Width != 40)
    {
        print("  FAIL: Width incorrect\n");
        errors++;
    }

    print("  te_Height = ");
    print_num(te.te_Height);
    print(" (expected 8)\n");
    if (te.te_Height != 8)
    {
        print("  FAIL: Height incorrect\n");
        errors++;
    }

    print("  te_Extent.MinX = ");
    print_num(te.te_Extent.MinX);
    print(" (expected 0)\n");
    if (te.te_Extent.MinX != 0)
    {
        print("  FAIL: MinX incorrect\n");
        errors++;
    }

    print("  te_Extent.MaxX = ");
    print_num(te.te_Extent.MaxX);
    print(" (expected 39)\n");
    if (te.te_Extent.MaxX != 39)
    {
        print("  FAIL: MaxX incorrect\n");
        errors++;
    }

    print("  te_Extent.MinY = ");
    print_num(te.te_Extent.MinY);
    print(" (expected -6)\n");
    if (te.te_Extent.MinY != -6)
    {
        print("  FAIL: MinY incorrect\n");
        errors++;
    }

    print("  te_Extent.MaxY = ");
    print_num(te.te_Extent.MaxY);
    print(" (expected 1)\n");
    if (te.te_Extent.MaxY != 1)
    {
        print("  FAIL: MaxY incorrect\n");
        errors++;
    }

    if (errors == 0)
        print("  OK: TextExtent dimensions correct\n");

    /* Test 2: TextExtent with single character */
    print("\nTest 2: TextExtent(A, 1)...\n");
    TextExtent(&rp, (CONST_STRPTR)"A", 1, &te);

    print("  te_Width = ");
    print_num(te.te_Width);
    print(" (expected 8)\n");
    if (te.te_Width != 8)
    {
        print("  FAIL: Width incorrect\n");
        errors++;
    }

    print("  te_Extent.MaxX = ");
    print_num(te.te_Extent.MaxX);
    print(" (expected 7)\n");
    if (te.te_Extent.MaxX != 7)
    {
        print("  FAIL: MaxX incorrect\n");
        errors++;
    }

    if (te.te_Width == 8 && te.te_Extent.MaxX == 7)
        print("  OK: Single character dimensions correct\n");

    /* Test 3: TextExtent with empty string */
    print("\nTest 3: TextExtent(empty, 0)...\n");
    TextExtent(&rp, (CONST_STRPTR)"", 0, &te);

    print("  te_Width = ");
    print_num(te.te_Width);
    print(" (expected 0)\n");
    if (te.te_Width != 0)
    {
        print("  FAIL: Width should be 0\n");
        errors++;
    }
    else
        print("  OK: Empty string width is 0\n");

    /* Test 4: TextFit - all characters fit */
    print("\nTest 4: TextFit(ABC, 3, width=50)...\n");
    numFit = TextFit(&rp, (CONST_STRPTR)"ABC", 3, &te, NULL, 1, 50, 16);

    print("  numFit = ");
    print_num(numFit);
    print(" (expected 3)\n");
    if (numFit != 3)
    {
        print("  FAIL: Should fit 3 characters\n");
        errors++;
    }
    else
        print("  OK: All 3 characters fit\n");

    print("  te_Width = ");
    print_num(te.te_Width);
    print(" (expected 24)\n");
    if (te.te_Width != 24)
    {
        print("  FAIL: Width incorrect\n");
        errors++;
    }

    /* Test 5: TextFit - only some characters fit */
    print("\nTest 5: TextFit(ABCDE, 5, width=25)...\n");
    numFit = TextFit(&rp, (CONST_STRPTR)"ABCDE", 5, &te, NULL, 1, 25, 16);

    print("  numFit = ");
    print_num(numFit);
    print(" (expected 3)\n");
    if (numFit != 3)
    {
        print("  FAIL: Should fit 3 characters\n");
        errors++;
    }
    else
        print("  OK: 3 characters fit in width 25\n");

    /* Test 6: TextFit - no characters fit (width too small) */
    print("\nTest 6: TextFit(ABC, 3, width=5)...\n");
    numFit = TextFit(&rp, (CONST_STRPTR)"ABC", 3, &te, NULL, 1, 5, 16);

    print("  numFit = ");
    print_num(numFit);
    print(" (expected 0)\n");
    if (numFit != 0)
    {
        print("  FAIL: Should fit 0 characters\n");
        errors++;
    }
    else
        print("  OK: No characters fit in width 5\n");

    /* Test 7: TextFit - height too small */
    print("\nTest 7: TextFit(ABC, 3, height=5)...\n");
    numFit = TextFit(&rp, (CONST_STRPTR)"ABC", 3, &te, NULL, 1, 100, 5);

    print("  numFit = ");
    print_num(numFit);
    print(" (expected 0)\n");
    if (numFit != 0)
    {
        print("  FAIL: Should fit 0 characters (height too small)\n");
        errors++;
    }
    else
        print("  OK: No characters fit when height < font height\n");

    /* Clean up */
    CloseFont(font);
    FreeRaster(plane, 64, 16);

    /* Final result */
    print("\n");
    if (errors == 0)
    {
        print("PASS: text_extent all tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: text_extent had ");
        print_num(errors);
        print(" errors\n");
        return 20;
    }
}
