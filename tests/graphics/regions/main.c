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
#include <inline/macros.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

#ifndef RectInRegion
#define RectInRegion(region, rectangle) \
    LP2(642, BOOL, RectInRegion, struct Region *, (region), a0, CONST struct Rectangle *, (rectangle), a1, struct GfxBase *, GfxBase)
#endif

#ifndef PointInRegion
#define PointInRegion(region, x, y) \
    LP3(648, BOOL, PointInRegion, struct Region *, (region), a0, WORD, (x), d0, WORD, (y), d1, struct GfxBase *, GfxBase)
#endif

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

static int count_region_rects(struct Region *region)
{
    int count = 0;
    struct RegionRectangle *rr;

    if (!region)
        return 0;

    for (rr = region->RegionRectangle; rr != NULL; rr = rr->Next)
        count++;

    return count;
}

int main(void)
{
    struct Region *region;
    struct Region *src;
    struct Region *dest;
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

    /* Test 7: ClearRectRegion() split behaviour */
    print("\nTest 7: ClearRectRegion() splitting...\n");
    ClearRegion(region);
    rect.MinX = 0;
    rect.MinY = 0;
    rect.MaxX = 100;
    rect.MaxY = 100;
    OrRectRegion(region, &rect);

    rect.MinX = 20;
    rect.MinY = 30;
    rect.MaxX = 80;
    rect.MaxY = 70;
    if (!ClearRectRegion(region, &rect))
    {
        print("FAIL: ClearRectRegion() returned FALSE\n");
        errors++;
    }
    else if (count_region_rects(region) == 4 &&
             region->bounds.MinX == 0 && region->bounds.MinY == 0 &&
             region->bounds.MaxX == 100 && region->bounds.MaxY == 100)
    {
        print("OK: ClearRectRegion split the rectangle into four pieces\n");
    }
    else
    {
        print("FAIL: ClearRectRegion did not split as expected\n");
        errors++;
    }

    /* Test 8: XorRectRegion() toggles overlapping area */
    print("\nTest 8: XorRectRegion() toggle...\n");
    ClearRegion(region);
    rect.MinX = 0;
    rect.MinY = 0;
    rect.MaxX = 40;
    rect.MaxY = 40;
    OrRectRegion(region, &rect);

    rect.MinX = 20;
    rect.MinY = 10;
    rect.MaxX = 60;
    rect.MaxY = 30;
    if (!XorRectRegion(region, &rect))
    {
        print("FAIL: XorRectRegion() returned FALSE\n");
        errors++;
    }
    else if (PointInRegion(region, 10, 10) && !PointInRegion(region, 25, 20) &&
             PointInRegion(region, 50, 20))
    {
        print("OK: XorRectRegion toggled overlap and added outside area\n");
    }
    else
    {
        print("FAIL: XorRectRegion() produced wrong membership\n");
        errors++;
    }

    /* Test 9: Region-to-region operations */
    print("\nTest 9: Region-to-region operations...\n");
    src = NewRegion();
    dest = NewRegion();
    if (!src || !dest)
    {
        print("FAIL: Could not allocate helper regions\n");
        errors++;
    }
    else
    {
        struct Rectangle src_rect = { 20, 20, 80, 80 };
        struct Rectangle dest_rect = { 0, 0, 40, 40 };
        struct Rectangle extra_rect = { 90, 90, 110, 110 };

        OrRectRegion(src, &src_rect);
        OrRectRegion(dest, &dest_rect);
        OrRectRegion(dest, &extra_rect);

        if (!AndRegionRegion(src, dest))
        {
            print("FAIL: AndRegionRegion() returned FALSE\n");
            errors++;
        }
        else if (PointInRegion(dest, 30, 30) && !PointInRegion(dest, 10, 10) &&
                 !PointInRegion(dest, 100, 100))
        {
            print("OK: AndRegionRegion kept only the shared area\n");
        }
        else
        {
            print("FAIL: AndRegionRegion() produced wrong result\n");
            errors++;
        }

        ClearRegion(dest);
        OrRectRegion(dest, &dest_rect);
        if (!OrRegionRegion(src, dest))
        {
            print("FAIL: OrRegionRegion() returned FALSE\n");
            errors++;
        }
        else if (PointInRegion(dest, 10, 10) && PointInRegion(dest, 70, 70))
        {
            print("OK: OrRegionRegion preserved both input regions\n");
        }
        else
        {
            print("FAIL: OrRegionRegion() produced wrong result\n");
            errors++;
        }

        ClearRegion(dest);
        OrRectRegion(dest, &dest_rect);
        if (!XorRegionRegion(src, dest))
        {
            print("FAIL: XorRegionRegion() returned FALSE\n");
            errors++;
        }
        else if (PointInRegion(dest, 10, 10) && !PointInRegion(dest, 30, 30) &&
                 PointInRegion(dest, 70, 70))
        {
            print("OK: XorRegionRegion toggled the shared area\n");
        }
        else
        {
            print("FAIL: XorRegionRegion() produced wrong result\n");
            errors++;
        }
    }

    if (src)
        DisposeRegion(src);
    if (dest)
        DisposeRegion(dest);

    /* Test 10: RectInRegion()/PointInRegion() */
    print("\nTest 10: RectInRegion()/PointInRegion()...\n");
    ClearRegion(region);
    rect.MinX = 5;
    rect.MinY = 5;
    rect.MaxX = 25;
    rect.MaxY = 25;
    OrRectRegion(region, &rect);
    rect.MinX = 40;
    rect.MinY = 5;
    rect.MaxX = 60;
    rect.MaxY = 25;
    OrRectRegion(region, &rect);

    {
        struct Rectangle inside = { 8, 8, 20, 20 };
        struct Rectangle spanning_gap = { 20, 8, 45, 20 };

        if (RectInRegion(region, &inside) && !RectInRegion(region, &spanning_gap) &&
            PointInRegion(region, 10, 10) && !PointInRegion(region, 30, 10))
        {
            print("OK: RectInRegion()/PointInRegion() membership matches region pieces\n");
        }
        else
        {
            print("FAIL: RectInRegion()/PointInRegion() membership incorrect\n");
            errors++;
        }
    }

    /* Test 11: DisposeRegion() */
    print("\nTest 11: DisposeRegion()...\n");
    DisposeRegion(region);
    print("OK: DisposeRegion() completed\n");

    /* Test 12: Handle NULL gracefully */
    print("\nTest 12: NULL handling...\n");
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
