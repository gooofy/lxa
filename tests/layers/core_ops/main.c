/*
 * Test: layers/core_ops
 * Tests the remaining core layers create/delete/move/size/z-order paths.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/layers.h>
#include <graphics/clip.h>
#include <graphics/regions.h>
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
    struct ClipRect *cr = layer ? layer->ClipRect : NULL;

    while (cr)
    {
        count++;
        cr = cr->Next;
    }

    return count;
}

static int count_free_cliprects(struct Layer_Info *li)
{
    int count = 0;
    struct ClipRect *cr = li ? li->FreeClipRects : NULL;

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

static struct Layer *backmost_layer(struct Layer_Info *li)
{
    struct Layer *layer = li ? li->top_layer : NULL;

    while (layer && layer->back)
        layer = layer->back;

    return layer;
}

int main(void)
{
    struct Layer_Info *li;
    struct BitMap *bm;
    struct BitMap *super_bm;
    struct Layer *behind = NULL;
    struct Layer *upfront = NULL;
    struct Layer *hook_front = NULL;
    struct Layer *hook_super = NULL;
    struct Hook hook_a;
    struct Hook hook_b;
    int errors = 0;

    print("Testing layers.library core operations...\n");

    if (!LayersBase || !GfxBase)
    {
        print("FAIL: Required library base missing\n");
        return 20;
    }

    li = NewLayerInfo();
    if (!li)
    {
        print("FAIL: NewLayerInfo() returned NULL\n");
        return 20;
    }

    bm = AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
    super_bm = AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
    if (!bm || !super_bm)
    {
        print("FAIL: Could not allocate BitMap structures\n");
        if (super_bm)
            FreeMem(super_bm, sizeof(struct BitMap));
        if (bm)
            FreeMem(bm, sizeof(struct BitMap));
        DisposeLayerInfo(li);
        return 20;
    }

    InitBitMap(bm, 1, 320, 200);
    InitBitMap(super_bm, 1, 320, 200);
    bm->Planes[0] = AllocRaster(320, 200);
    super_bm->Planes[0] = AllocRaster(320, 200);
    if (!bm->Planes[0] || !super_bm->Planes[0])
    {
        print("FAIL: Could not allocate raster planes\n");
        if (super_bm->Planes[0])
            FreeRaster(super_bm->Planes[0], 320, 200);
        if (bm->Planes[0])
            FreeRaster(bm->Planes[0], 320, 200);
        FreeMem(super_bm, sizeof(struct BitMap));
        FreeMem(bm, sizeof(struct BitMap));
        DisposeLayerInfo(li);
        return 20;
    }

    hook_a.h_Entry = NULL;
    hook_a.h_SubEntry = NULL;
    hook_a.h_Data = (APTR)0x1111;
    hook_b.h_Entry = NULL;
    hook_b.h_SubEntry = NULL;
    hook_b.h_Data = (APTR)0x2222;

    print("\nTest 1: Core layer creation paths...\n");
    behind = CreateBehindLayer(li, bm, 0, 0, 99, 99, LAYERSIMPLE, NULL);
    upfront = CreateUpfrontLayer(li, bm, 20, 20, 79, 79, LAYERSIMPLE, NULL);
    hook_front = CreateUpfrontHookLayer(li, bm, 120, 20, 179, 79, LAYERSIMPLE, &hook_a, NULL);
    hook_super = CreateBehindHookLayer(li, bm, 180, 20, 239, 79,
                                       LAYERSUPER | LAYERSMART, &hook_b, super_bm);

    if (!behind || !upfront || !hook_front || !hook_super)
    {
        print("FAIL: One or more layer creation calls failed\n");
        errors++;
    }
    else if (li->top_layer == hook_front &&
             hook_front->BackFill == &hook_a &&
             hook_super->BackFill == &hook_b &&
             hook_super->SuperBitMap == super_bm &&
             hook_front->rp && hook_front->rp->Layer == hook_front &&
             upfront->rp && upfront->rp->Layer == upfront)
    {
        print("OK: upfront/behind/hook creation semantics verified\n");
    }
    else
    {
        print("FAIL: Layer creation state incorrect\n");
        errors++;
    }

    print("\nTest 2: MoveLayer and SizeLayer update geometry and ClipRects...\n");
    if (upfront)
    {
        LONG move_ok = MoveLayer(0, upfront, 10, 5);
        LONG size_ok = SizeLayer(0, upfront, 10, 15);

        if (move_ok && size_ok &&
            upfront->bounds.MinX == 30 && upfront->bounds.MinY == 25 &&
            upfront->bounds.MaxX == 99 && upfront->bounds.MaxY == 99 &&
            upfront->Width == 70 && upfront->Height == 75 &&
            upfront->ClipRect != NULL)
        {
            print("OK: MoveLayer/SizeLayer updated bounds and ClipRects\n");
        }
        else
        {
            print("FAIL: MoveLayer/SizeLayer geometry incorrect\n");
            errors++;
        }
    }
    else
    {
        print("FAIL: No upfront layer available for geometry test\n");
        errors++;
    }

    print("\nTest 3: UpfrontLayer and BehindLayer reorder z-order...\n");
    if (behind && UpfrontLayer(0, behind) && li->top_layer == behind)
    {
        print("OK: UpfrontLayer moved layer to front\n");
    }
    else
    {
        print("FAIL: UpfrontLayer did not move layer to front\n");
        errors++;
    }

    if (behind && BehindLayer(0, behind) && backmost_layer(li) == behind)
    {
        print("OK: BehindLayer moved layer to back\n");
    }
    else
    {
        print("FAIL: BehindLayer did not move layer to back\n");
        errors++;
    }

    print("\nTest 3b: Backdrop layers stay behind normal layers and reject move/size...\n");
    {
        struct Layer *backdrop = CreateBehindLayer(li, bm, 0, 120, 79, 179,
                                                   LAYERSIMPLE | LAYERBACKDROP, NULL);
        if (!backdrop)
        {
            print("FAIL: Could not create backdrop layer\n");
            errors++;
        }
        else if (backdrop->back == NULL && backdrop->front == behind &&
                 !MoveLayer(0, backdrop, 5, 5) &&
                 !SizeLayer(0, backdrop, 10, 10) &&
                 !MoveSizeLayer(backdrop, 1, 1, 1, 1) &&
                 UpfrontLayer(0, backdrop) && backdrop->back == NULL &&
                 BehindLayer(0, behind) && behind->back == backdrop)
        {
            print("OK: Backdrop z-order and immobility semantics verified\n");
        }
        else
        {
            print("FAIL: Backdrop layer semantics incorrect\n");
            errors++;
        }

        if (backdrop)
            DeleteLayer(0, backdrop);
    }

    print("\nTest 3c: BeginUpdate preserves disjoint damage rectangles...\n");
    if (hook_front)
    {
        struct Rectangle damage_a;
        struct Rectangle damage_b;

        hook_front->DamageList = NewRegion();
        if (!hook_front->DamageList)
        {
            print("FAIL: Could not allocate damage region\n");
            errors++;
        }
        else
        {
            damage_a.MinX = 125;
            damage_a.MinY = 25;
            damage_a.MaxX = 132;
            damage_a.MaxY = 32;
            damage_b.MinX = 165;
            damage_b.MinY = 60;
            damage_b.MaxX = 172;
            damage_b.MaxY = 68;

            if (!OrRectRegion(hook_front->DamageList, &damage_a) ||
                !OrRectRegion(hook_front->DamageList, &damage_b))
            {
                print("FAIL: Could not seed damage rectangles\n");
                errors++;
                DisposeRegion(hook_front->DamageList);
                hook_front->DamageList = NULL;
            }
            else if (!BeginUpdate(hook_front))
            {
                print("FAIL: BeginUpdate() rejected valid damage region\n");
                errors++;
                DisposeRegion(hook_front->DamageList);
                hook_front->DamageList = NULL;
            }
            else if (count_cliprects(hook_front) == 2 &&
                     count_region_rects(hook_front->DamageList) == 2)
            {
                EndUpdate(hook_front, TRUE);
                if (count_cliprects(hook_front) == 1 && hook_front->DamageList == NULL)
                {
                    print("OK: BeginUpdate/EndUpdate kept split damage bookkeeping\n");
                }
                else
                {
                    print("FAIL: EndUpdate() did not restore cliprects/damage state\n");
                    errors++;
                }
            }
            else
            {
                print("FAIL: BeginUpdate() collapsed split damage into one cliprect\n");
                errors++;
                EndUpdate(hook_front, TRUE);
            }
        }
    }
    else
    {
        print("FAIL: No hook layer available for damage test\n");
        errors++;
    }

    print("\nTest 3d: SyncSBitMap copies visible layer content into the SuperBitMap...\n");
    if (hook_super && hook_super->rp)
    {
        struct RastPort screen_rp;
        struct RastPort super_rp;

        InitRastPort(&screen_rp);
        screen_rp.BitMap = bm;
        SetRast(&screen_rp, 0);

        InitRastPort(&super_rp);
        super_rp.BitMap = super_bm;
        SetRast(&super_rp, 0);

        hook_super->Scroll_X = 2;
        hook_super->Scroll_Y = 1;

        SetAPen(&screen_rp, 1);
        WritePixel(&screen_rp, 185, 26);

        SyncSBitMap(hook_super);

        if (ReadPixel(&super_rp, 3, 5) == 1 && ReadPixel(&super_rp, 5, 6) == 0)
        {
            print("OK: SyncSBitMap mirrored the visible super layer into backing storage\n");
        }
        else
        {
            print("FAIL: SyncSBitMap did not update the expected SuperBitMap pixels\n");
            errors++;
        }

        hook_super->Scroll_X = 0;
        hook_super->Scroll_Y = 0;
    }
    else
    {
        print("FAIL: No super layer available for SyncSBitMap test\n");
        errors++;
    }

    print("\nTest 3e: CopySBitMap copies backing SuperBitMap content into the visible layer...\n");
    if (hook_super && hook_super->rp)
    {
        struct RastPort screen_rp;
        struct RastPort super_rp;

        InitRastPort(&screen_rp);
        screen_rp.BitMap = bm;
        SetRast(&screen_rp, 0);

        InitRastPort(&super_rp);
        super_rp.BitMap = super_bm;
        SetRast(&super_rp, 0);

        hook_super->Scroll_X = 2;
        hook_super->Scroll_Y = 1;

        SetAPen(&super_rp, 1);
        WritePixel(&super_rp, 3, 5);

        CopySBitMap(hook_super);

        if (ReadPixel(&screen_rp, 185, 26) == 1 && ReadPixel(&screen_rp, 187, 27) == 0)
        {
            print("OK: CopySBitMap mirrored the backing super layer into visible pixels\n");
        }
        else
        {
            print("FAIL: CopySBitMap did not update the expected visible layer pixels\n");
            errors++;
        }

        hook_super->Scroll_X = 0;
        hook_super->Scroll_Y = 0;
    }
    else
    {
        print("FAIL: No super layer available for CopySBitMap test\n");
        errors++;
    }

    print("\nTest 4: DeleteLayer unlinks layers, damages exposed areas, and pools ClipRects...\n");
    if (upfront)
    {
        int free_before = count_free_cliprects(li);

        if (DeleteLayer(0, upfront))
        {
            upfront = NULL;
            if ((behind->Flags & LAYERREFRESH) && behind->DamageList &&
                count_free_cliprects(li) > free_before && count_cliprects(behind) > 0)
            {
                print("OK: DeleteLayer exposed damage and returned ClipRects to pool\n");
            }
            else
            {
                print("FAIL: DeleteLayer did not update damage/pool state correctly\n");
                errors++;
            }
        }
        else
        {
            print("FAIL: DeleteLayer failed\n");
            errors++;
        }
    }

    print("\nCleaning up...\n");
    if (hook_front)
        DeleteLayer(0, hook_front);
    if (hook_super)
        DeleteLayer(0, hook_super);
    if (behind)
        DeleteLayer(0, behind);
    FreeRaster(super_bm->Planes[0], 320, 200);
    FreeRaster(bm->Planes[0], 320, 200);
    FreeMem(super_bm, sizeof(struct BitMap));
    FreeMem(bm, sizeof(struct BitMap));
    DisposeLayerInfo(li);
    print("OK: Cleanup complete\n");

    if (errors == 0)
    {
        print("\nPASS: layers/core_ops all tests passed\n");
        return 0;
    }

    print("\nFAIL: layers/core_ops had ");
    print_num(errors);
    print(" errors\n");
    return 20;
}
