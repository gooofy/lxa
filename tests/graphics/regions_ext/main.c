/*
 * Test: graphics/regions_ext
 *
 * lxa-only coverage (tests/ref_suite.yaml): RectInRegion()/PointInRegion()
 * are AROS extensions that lxa provides in graphics.library's private LVO
 * slots -642/-648; they do not exist in AmigaOS 3.1, so this program cannot
 * run on the reference machine.  Also checks lxa's NULL tolerance of
 * DisposeRegion()/ClearRegion().
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

int main(void)
{
    struct Region *region;
    struct Rectangle rect;
    int errors = 0;

    print("Testing lxa region extensions...\n");

    region = NewRegion();
    if (!region)
    {
        print("FAIL: NewRegion() returned NULL\n");
        return 20;
    }

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

    DisposeRegion(region);

    DisposeRegion(NULL);
    ClearRegion(NULL);
    print("OK: NULL parameters handled gracefully\n");

    if (errors == 0)
    {
        print("PASS: graphics/regions_ext all tests passed\n");
        return 0;
    }

    print("FAIL: graphics/regions_ext had errors\n");
    return 20;
}
