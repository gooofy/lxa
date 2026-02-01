/*
 * Test: graphics/regions
 * Tests Region functions: NewRegion, DisposeRegion, OrRectRegion, etc.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/regions.h>
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
    char *p = buf + sizeof(buf) - 1;
    BOOL neg = FALSE;
    
    *p = '\0';
    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }
    if (n == 0)
    {
        *--p = '0';
    }
    else
    {
        while (n > 0)
        {
            *--p = '0' + (n % 10);
            n /= 10;
        }
    }
    if (neg)
        *--p = '-';
    print(p);
}

int main(void)
{
    struct Region *region;
    struct Rectangle rect;
    int errors = 0;

    print("Testing Region functions...\n");

    /* Test 1: NewRegion() */
    print("\nTest 1: NewRegion()...\n");
    region = NewRegion();
    if (!region)
    {
        print("FAIL: NewRegion() returned NULL\n");
        return 20;
    }
    print("OK: NewRegion() returned non-NULL\n");

    /* Test 2: Check initial state */
    print("\nTest 2: Initial region state...\n");
    if (region->RegionRectangle != NULL)
    {
        print("FAIL: New region has non-NULL RegionRectangle\n");
        errors++;
    }
    else
    {
        print("OK: New region has NULL RegionRectangle\n");
    }

    if (region->bounds.MinX != 0 || region->bounds.MinY != 0 ||
        region->bounds.MaxX != 0 || region->bounds.MaxY != 0)
    {
        print("FAIL: New region has non-zero bounds\n");
        errors++;
    }
    else
    {
        print("OK: New region has zero bounds\n");
    }

    /* Test 3: OrRectRegion() - add a rectangle */
    print("\nTest 3: OrRectRegion()...\n");
    rect.MinX = 10;
    rect.MinY = 20;
    rect.MaxX = 100;
    rect.MaxY = 80;
    
    if (!OrRectRegion(region, &rect))
    {
        print("FAIL: OrRectRegion() returned FALSE\n");
        errors++;
    }
    else
    {
        print("OK: OrRectRegion() returned TRUE\n");
    }

    /* Verify bounds updated */
    if (region->bounds.MinX == 10 && region->bounds.MinY == 20 &&
        region->bounds.MaxX == 100 && region->bounds.MaxY == 80)
    {
        print("OK: Region bounds updated correctly\n");
    }
    else
    {
        print("FAIL: Region bounds not updated correctly\n");
        print("  MinX="); print_num(region->bounds.MinX);
        print(" MinY="); print_num(region->bounds.MinY);
        print(" MaxX="); print_num(region->bounds.MaxX);
        print(" MaxY="); print_num(region->bounds.MaxY);
        print("\n");
        errors++;
    }

    /* Verify RegionRectangle is non-NULL */
    if (region->RegionRectangle != NULL)
    {
        print("OK: RegionRectangle is non-NULL after OrRectRegion\n");
    }
    else
    {
        print("FAIL: RegionRectangle is still NULL after OrRectRegion\n");
        errors++;
    }

    /* Test 4: Add another rectangle */
    print("\nTest 4: OrRectRegion() second rectangle...\n");
    rect.MinX = 50;
    rect.MinY = 60;
    rect.MaxX = 150;
    rect.MaxY = 120;
    
    if (!OrRectRegion(region, &rect))
    {
        print("FAIL: Second OrRectRegion() returned FALSE\n");
        errors++;
    }
    else
    {
        print("OK: Second OrRectRegion() returned TRUE\n");
    }

    /* Bounds should now encompass both rectangles */
    if (region->bounds.MinX == 10 && region->bounds.MinY == 20 &&
        region->bounds.MaxX == 150 && region->bounds.MaxY == 120)
    {
        print("OK: Region bounds expanded to include both rectangles\n");
    }
    else
    {
        print("FAIL: Region bounds not expanded correctly\n");
        print("  MinX="); print_num(region->bounds.MinX);
        print(" MinY="); print_num(region->bounds.MinY);
        print(" MaxX="); print_num(region->bounds.MaxX);
        print(" MaxY="); print_num(region->bounds.MaxY);
        print("\n");
        errors++;
    }

    /* Test 5: ClearRegion() */
    print("\nTest 5: ClearRegion()...\n");
    ClearRegion(region);
    
    if (region->RegionRectangle == NULL)
    {
        print("OK: RegionRectangle is NULL after ClearRegion\n");
    }
    else
    {
        print("FAIL: RegionRectangle is not NULL after ClearRegion\n");
        errors++;
    }

    if (region->bounds.MinX == 0 && region->bounds.MinY == 0 &&
        region->bounds.MaxX == 0 && region->bounds.MaxY == 0)
    {
        print("OK: Bounds reset to zero after ClearRegion\n");
    }
    else
    {
        print("FAIL: Bounds not reset after ClearRegion\n");
        errors++;
    }

    /* Test 6: AndRectRegion() - clip to a rectangle */
    print("\nTest 6: AndRectRegion()...\n");
    
    /* Add a rectangle first */
    rect.MinX = 0;
    rect.MinY = 0;
    rect.MaxX = 100;
    rect.MaxY = 100;
    OrRectRegion(region, &rect);

    /* Clip to a smaller rectangle */
    rect.MinX = 20;
    rect.MinY = 30;
    rect.MaxX = 80;
    rect.MaxY = 70;
    AndRectRegion(region, &rect);

    /* Bounds should be clipped */
    if (region->bounds.MinX == 20 && region->bounds.MinY == 30 &&
        region->bounds.MaxX == 80 && region->bounds.MaxY == 70)
    {
        print("OK: AndRectRegion clipped bounds correctly\n");
    }
    else
    {
        print("FAIL: AndRectRegion did not clip bounds correctly\n");
        print("  MinX="); print_num(region->bounds.MinX);
        print(" MinY="); print_num(region->bounds.MinY);
        print(" MaxX="); print_num(region->bounds.MaxX);
        print(" MaxY="); print_num(region->bounds.MaxY);
        print("\n");
        errors++;
    }

    /* Test 7: DisposeRegion() */
    print("\nTest 7: DisposeRegion()...\n");
    DisposeRegion(region);
    print("OK: DisposeRegion() completed\n");

    /* Test 8: Handle NULL gracefully */
    print("\nTest 8: NULL handling...\n");
    DisposeRegion(NULL);  /* Should not crash */
    ClearRegion(NULL);    /* Should not crash */
    print("OK: NULL parameters handled gracefully\n");

    /* Final result */
    if (errors == 0)
    {
        print("\nPASS: graphics/regions all tests passed\n");
        return 0;
    }
    else
    {
        print("\nFAIL: graphics/regions had errors\n");
        return 20;
    }
}
