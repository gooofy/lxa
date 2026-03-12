/*
 * Test: layers/visibility
 * Tests layer visibility, layer-info bounds, and hit testing.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/layers.h>
#include <graphics/clip.h>
#include <graphics/regions.h>
#include <utility/hooks.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/layers_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/layers.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct Library *LayersBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;
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

static int count_cliprects(struct Layer *layer)
{
    int count = 0;
    struct ClipRect *cr;

    if (!layer)
        return 0;

    cr = layer->ClipRect;
    while (cr)
    {
        count++;
        cr = cr->Next;
    }

    return count;
}

static int count_region_rects(struct Region *region)
{
    int count = 0;
    struct RegionRectangle *rr = region ? region->RegionRectangle : NULL;

    while (rr)
    {
        count++;
        rr = rr->Next;
    }

    return count;
}

int main(void)
{
    struct Layer_Info *li;
    struct BitMap *bm;
    struct Layer *back;
    struct Layer *front;
    struct Hook hook_a;
    struct Hook hook_b;
    struct Rectangle bounds;
    int errors = 0;

    print("Testing layers.library visibility and bounds...\n");

    if (!LayersBase)
    {
        print("FAIL: LayersBase is NULL\n");
        return 20;
    }

    li = NewLayerInfo();
    if (!li)
    {
        print("FAIL: NewLayerInfo() returned NULL\n");
        return 20;
    }

    bm = AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
    if (!bm)
    {
        print("FAIL: Could not allocate BitMap\n");
        DisposeLayerInfo(li);
        return 20;
    }

    InitBitMap(bm, 1, 320, 200);
    bm->Planes[0] = AllocRaster(320, 200);
    if (!bm->Planes[0])
    {
        print("FAIL: Could not allocate raster\n");
        FreeMem(bm, sizeof(struct BitMap));
        DisposeLayerInfo(li);
        return 20;
    }

    back = CreateBehindLayer(li, bm, 10, 10, 110, 110, LAYERSIMPLE, NULL);
    front = CreateUpfrontLayer(li, bm, 40, 40, 90, 90, LAYERSIMPLE, NULL);
    if (!back || !front)
    {
        print("FAIL: Could not create test layers\n");
        if (front)
            DeleteLayer(0, front);
        if (back)
            DeleteLayer(0, back);
        FreeRaster(bm->Planes[0], 320, 200);
        FreeMem(bm, sizeof(struct BitMap));
        DisposeLayerInfo(li);
        return 20;
    }

    print("OK: Test layers created\n");

    print("\nTest 1: WhichLayer respects visible ClipRects...\n");
    if (WhichLayer(li, 50, 50) == front)
    {
        print("OK: Overlapped point resolves to front layer\n");
    }
    else
    {
        print("FAIL: Overlapped point did not resolve to front layer\n");
        errors++;
    }

    print("\nTest 2: HideLayer updates occlusion and hit-testing...\n");
    if (!LayerOccluded(back))
    {
        print("OK: Back layer initially visible\n");
    }
    else
    {
        print("FAIL: Back layer unexpectedly occluded\n");
        errors++;
    }

    if (HideLayer(front))
    {
        print("OK: HideLayer(front) succeeded\n");
    }
    else
    {
        print("FAIL: HideLayer(front) failed\n");
        errors++;
    }

    if (LayerOccluded(front) && count_cliprects(front) == 0)
    {
        print("OK: Hidden layer is occluded and has no ClipRects\n");
    }
    else
    {
        print("FAIL: Hidden layer still reports visible ClipRects\n");
        errors++;
    }

    if (WhichLayer(li, 50, 50) == back)
    {
        print("OK: Hidden front layer no longer wins hit test\n");
    }
    else
    {
        print("FAIL: Hidden front layer still affects hit testing\n");
        errors++;
    }

    print("\nTest 3: ShowLayer restores layer and supports front/back constants...\n");
    if (ShowLayer(front, LAYER_BACKMOST))
    {
        print("OK: ShowLayer(front, LAYER_BACKMOST) succeeded\n");
    }
    else
    {
        print("FAIL: ShowLayer(front, LAYER_BACKMOST) failed\n");
        errors++;
    }

    if (li->top_layer == back && WhichLayer(li, 50, 50) == back)
    {
        print("OK: Back layer moved to front of z-order\n");
    }
    else
    {
        print("FAIL: LAYER_BACKMOST handling incorrect\n");
        errors++;
    }

    if (ShowLayer(front, LAYER_FRONTMOST))
    {
        print("OK: ShowLayer(front, LAYER_FRONTMOST) succeeded\n");
    }
    else
    {
        print("FAIL: ShowLayer(front, LAYER_FRONTMOST) failed\n");
        errors++;
    }

    if (li->top_layer == front && WhichLayer(li, 50, 50) == front)
    {
        print("OK: Front layer restored to top\n");
    }
    else
    {
        print("FAIL: LAYER_FRONTMOST handling incorrect\n");
        errors++;
    }

    print("\nTest 4: SetLayerInfoBounds clips new ClipRects...\n");
    bounds.MinX = 0;
    bounds.MinY = 0;
    bounds.MaxX = 60;
    bounds.MaxY = 60;
    if (SetLayerInfoBounds(li, &bounds))
    {
        print("OK: SetLayerInfoBounds succeeded\n");
    }
    else
    {
        print("FAIL: SetLayerInfoBounds failed\n");
        errors++;
    }

    if (front->ClipRect && front->ClipRect->bounds.MaxX <= 60 && front->ClipRect->bounds.MaxY <= 60)
    {
        print("OK: Front layer ClipRects clipped to Layer_Info bounds\n");
    }
    else
    {
        print("FAIL: Front layer ClipRects not clipped by bounds\n");
        errors++;
    }

    if (WhichLayer(li, 80, 80) == NULL)
    {
        print("OK: Out-of-bounds point returns NULL\n");
    }
    else
    {
        print("FAIL: Out-of-bounds point still resolves to layer\n");
        errors++;
    }

    print("\nTest 4b: InstallClipRegion constrains cliprects and hit testing...\n");
    {
        struct Region *clip = NewRegion();
        struct Region *old_clip;
        struct Rectangle clip_rect;

        if (!clip)
        {
            print("FAIL: Could not allocate clip region\n");
            errors++;
        }
        else
        {
            clip_rect.MinX = 45;
            clip_rect.MinY = 45;
            clip_rect.MaxX = 55;
            clip_rect.MaxY = 55;
            OrRectRegion(clip, &clip_rect);

            old_clip = InstallClipRegion(front, clip);
            if (old_clip != NULL)
            {
                print("FAIL: InstallClipRegion returned unexpected previous region\n");
                errors++;
            }
            else if (count_cliprects(front) == 1 && count_region_rects(clip) == 1 &&
                     front->ClipRect->bounds.MinX == 45 && front->ClipRect->bounds.MaxX == 55 &&
                     WhichLayer(li, 50, 50) == front && WhichLayer(li, 70, 70) == NULL)
            {
                print("OK: InstallClipRegion tightened cliprects and hit testing\n");
            }
            else
            {
                print("FAIL: InstallClipRegion did not constrain visibility correctly\n");
                errors++;
            }

            old_clip = InstallClipRegion(front, NULL);
            if (old_clip != clip)
            {
                print("FAIL: InstallClipRegion(NULL) did not return previous region\n");
                errors++;
            }
            else if (WhichLayer(li, 58, 58) == front)
            {
                print("OK: Removing clip region restored full visibility\n");
            }
            else
            {
                print("FAIL: Removing clip region did not restore visibility\n");
                errors++;
            }

            DisposeRegion(clip);
        }
    }

    print("\nTest 5: Hook installation helpers and fatten/thin flags...\n");
    hook_a.h_Entry = NULL;
    hook_a.h_SubEntry = NULL;
    hook_a.h_Data = (APTR)1;
    hook_b.h_Entry = NULL;
    hook_b.h_SubEntry = NULL;
    hook_b.h_Data = (APTR)2;

    if (InstallLayerHook(front, &hook_a) == LAYERS_BACKFILL && front->BackFill == &hook_a)
    {
        print("OK: InstallLayerHook updates BackFill\n");
    }
    else
    {
        print("FAIL: InstallLayerHook behavior incorrect\n");
        errors++;
    }

    if (InstallLayerInfoHook(li, &hook_b) == LAYERS_BACKFILL && li->BlankHook == &hook_b)
    {
        print("OK: InstallLayerInfoHook updates BlankHook\n");
    }
    else
    {
        print("FAIL: InstallLayerInfoHook behavior incorrect\n");
        errors++;
    }

    if (ThinLayerInfo(li), !(li->Flags & NEWLAYERINFO_CALLED))
    {
        print("OK: ThinLayerInfo clears NEWLAYERINFO_CALLED\n");
    }
    else
    {
        print("FAIL: ThinLayerInfo did not clear NEWLAYERINFO_CALLED\n");
        errors++;
    }

    if (FattenLayerInfo(li) && (li->Flags & NEWLAYERINFO_CALLED))
    {
        print("OK: FattenLayerInfo restores NEWLAYERINFO_CALLED\n");
    }
    else
    {
        print("FAIL: FattenLayerInfo did not restore NEWLAYERINFO_CALLED\n");
        errors++;
    }

    print("\nCleaning up...\n");
    DeleteLayer(0, front);
    DeleteLayer(0, back);
    FreeRaster(bm->Planes[0], 320, 200);
    FreeMem(bm, sizeof(struct BitMap));
    DisposeLayerInfo(li);
    print("OK: Cleanup complete\n");

    if (errors == 0)
    {
        print("\nPASS: layers/visibility all tests passed\n");
        return 0;
    }

    print("\nFAIL: layers/visibility had ");
    print_num(errors);
    print(" errors\n");
    return 20;
}
