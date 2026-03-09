/*
 * Test: layers/tag_layers
 * Tests CreateLayerTagList() and AddLayerInfoTag() hook-layer tag semantics.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/layers.h>
#include <graphics/clip.h>
#include <graphics/layersext.h>
#include <utility/hooks.h>
#include <utility/tagitem.h>
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

int main(void)
{
    struct Layer_Info *li;
    struct BitMap *bm;
    struct BitMap *super_bm;
    struct Layer *base = NULL;
    struct Layer *behind = NULL;
    struct Layer *super_layer = NULL;
    struct Layer *hidden = NULL;
    struct Hook default_hook;
    struct Hook override_hook;
    int errors = 0;
    APTR window_ptr = (APTR)0x1234;
    struct TagItem base_tags[] = {
        { LA_WindowPtr, (ULONG)window_ptr },
        { TAG_DONE, 0 }
    };
    struct TagItem behind_tags[] = {
        { LA_BackfillHook, 0 },
        { LA_Behind, 0 },
        { TAG_DONE, 0 }
    };
    struct TagItem super_tags[] = {
        { LA_SuperBitMap, 0 },
        { TAG_DONE, 0 }
    };
    struct TagItem hidden_tags[] = {
        { LA_Hidden, TRUE },
        { LA_InFrontOf, 0 },
        { TAG_DONE, 0 }
    };
    struct TagItem conflict_tags[] = {
        { LA_InFrontOf, 0 },
        { LA_Behind, 0 },
        { TAG_DONE, 0 }
    };

    print("Testing layers.library tag-based layer creation...\n");

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

    default_hook.h_Entry = NULL;
    default_hook.h_SubEntry = NULL;
    default_hook.h_Data = (APTR)0x1111;
    override_hook.h_Entry = NULL;
    override_hook.h_SubEntry = NULL;
    override_hook.h_Data = (APTR)0x2222;

    print("\nTest 1: AddLayerInfoTag installs default hook for tag-created layers...\n");
    if (AddLayerInfoTag(li, LA_BackfillHook, (ULONG)&default_hook))
    {
        print("OK: AddLayerInfoTag accepted LA_BackfillHook\n");
    }
    else
    {
        print("FAIL: AddLayerInfoTag rejected LA_BackfillHook\n");
        errors++;
    }

    base = CreateLayerTagList(li, bm, 10, 10, 110, 110, LAYERSIMPLE, base_tags);
    if (!base)
    {
        print("FAIL: CreateLayerTagList could not create base layer\n");
        errors++;
    }
    else if (base->BackFill == &default_hook && base->Window == window_ptr)
    {
        print("OK: Base layer inherited default hook and window pointer tag\n");
    }
    else
    {
        print("FAIL: Base layer did not honor default hook/window tags\n");
        errors++;
    }

    print("\nTest 2: Explicit hook and behind tags override defaults and z-order...\n");
    behind_tags[0].ti_Data = (ULONG)&override_hook;
    behind_tags[1].ti_Data = (ULONG)base;
    behind = CreateLayerTagList(li, bm, 20, 20, 90, 90, LAYERSIMPLE, behind_tags);
    if (!behind)
    {
        print("FAIL: CreateLayerTagList could not create behind layer\n");
        errors++;
    }
    else if (behind->BackFill == &override_hook && li->top_layer == base && base->back == behind)
    {
        print("OK: Explicit hook overrides default and LA_Behind preserves order\n");
    }
    else
    {
        print("FAIL: Explicit hook or LA_Behind semantics incorrect\n");
        errors++;
    }

    print("\nTest 3: SuperBitMap tag is required for LAYERSUPER...\n");
    if (CreateLayerTagList(li, bm, 30, 30, 70, 70, LAYERSUPER | LAYERSMART, NULL) == NULL)
    {
        print("OK: LAYERSUPER without LA_SuperBitMap fails\n");
    }
    else
    {
        print("FAIL: LAYERSUPER without LA_SuperBitMap unexpectedly succeeded\n");
        errors++;
    }

    super_tags[0].ti_Data = (ULONG)super_bm;
    super_layer = CreateLayerTagList(li, bm, 120, 20, 180, 80, LAYERSUPER | LAYERSMART, super_tags);
    if (!super_layer)
    {
        print("FAIL: CreateLayerTagList could not create SuperBitMap layer\n");
        errors++;
    }
    else if (super_layer->SuperBitMap == super_bm)
    {
        print("OK: LA_SuperBitMap assigned SuperBitMap layer backing store\n");
    }
    else
    {
        print("FAIL: SuperBitMap tag was not applied\n");
        errors++;
    }

    print("\nTest 4: Hidden and in-front-of tags affect visibility and ordering...\n");
    hidden_tags[1].ti_Data = (ULONG)behind;
    hidden = CreateLayerTagList(li, bm, 25, 25, 85, 85, LAYERSIMPLE, hidden_tags);
    if (!hidden)
    {
        print("FAIL: CreateLayerTagList could not create hidden layer\n");
        errors++;
    }
    else if (hidden->back == behind && LayerOccluded(hidden) && count_cliprects(hidden) == 0 && WhichLayer(li, 30, 30) == base)
    {
        print("OK: LA_Hidden and LA_InFrontOf keep layer hidden without changing hit testing\n");
    }
    else
    {
        print("FAIL: Hidden/in-front-of tag semantics incorrect\n");
        errors++;
    }

    print("\nTest 5: Conflicting LA_InFrontOf and LA_Behind tags are rejected...\n");
    conflict_tags[0].ti_Data = (ULONG)base;
    conflict_tags[1].ti_Data = (ULONG)behind;
    if (CreateLayerTagList(li, bm, 5, 5, 15, 15, LAYERSIMPLE, conflict_tags) == NULL)
    {
        print("OK: Conflicting position tags rejected\n");
    }
    else
    {
        print("FAIL: Conflicting position tags unexpectedly succeeded\n");
        errors++;
    }

    print("\nCleaning up...\n");
    if (hidden)
        DeleteLayer(0, hidden);
    if (super_layer)
        DeleteLayer(0, super_layer);
    if (behind)
        DeleteLayer(0, behind);
    if (base)
        DeleteLayer(0, base);
    FreeRaster(super_bm->Planes[0], 320, 200);
    FreeRaster(bm->Planes[0], 320, 200);
    FreeMem(super_bm, sizeof(struct BitMap));
    FreeMem(bm, sizeof(struct BitMap));
    DisposeLayerInfo(li);
    print("OK: Cleanup complete\n");

    if (errors == 0)
    {
        print("\nPASS: layers/tag_layers all tests passed\n");
        return 0;
    }

    print("\nFAIL: layers/tag_layers had errors\n");
    return 20;
}
