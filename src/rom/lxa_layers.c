/*
 * lxa_layers.c - layers.library implementation for lxa
 *
 * Implements the AmigaOS layers.library for window clipping and refresh handling.
 * This library manages layers (drawing regions) that can overlap and be reordered.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT license.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/semaphores.h>
#include <exec/lists.h>

#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <graphics/regions.h>

#include <clib/graphics_protos.h>
#include <inline/graphics.h>

#include <clib/layers_protos.h>

//#define ENABLE_DEBUG
#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "layers"
#define EXLIBVER   " 40.1 (2026/02/01)"

char __aligned _g_layers_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_layers_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_layers_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_layers_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct GfxBase  *GfxBase;

/*
 * LayersBase structure - library base for layers.library
 */
struct LayersBase
{
    struct Library lib;
    /* Add any global state here if needed */
};

static struct LayersBase *LayersBase;

/* ========================================================================
 * Library management functions
 * ======================================================================== */

struct LayersBase * __g_lxa_layers_InitLib ( register struct LayersBase *layersb __asm("a6"),
                                              register BPTR               seglist __asm("a0"),
                                              register struct ExecBase   *sysb    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_layers: InitLib() called\n");
    LayersBase = layersb;
    return layersb;
}

struct LayersBase * __g_lxa_layers_OpenLib ( register struct LayersBase *layersb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_layers: OpenLib() called\n");
    layersb->lib.lib_OpenCnt++;
    layersb->lib.lib_Flags &= ~LIBF_DELEXP;
    return layersb;
}

BPTR __g_lxa_layers_CloseLib ( register struct LayersBase *layersb __asm("a6"))
{
    return NULL;
}

BPTR __g_lxa_layers_ExpungeLib ( register struct LayersBase *layersb __asm("a6"))
{
    return NULL;
}

ULONG __g_lxa_layers_ExtFuncLib(void)
{
    return 0;
}

/* ========================================================================
 * Helper functions
 * ======================================================================== */

/*
 * Allocate a ClipRect from the Layer_Info pool or fresh memory
 */
static struct ClipRect *AllocClipRect(struct Layer_Info *li)
{
    struct ClipRect *cr;

    /* Try to reuse from FreeClipRects pool */
    if (li && li->FreeClipRects)
    {
        cr = li->FreeClipRects;
        li->FreeClipRects = cr->Next;
        memset(cr, 0, sizeof(struct ClipRect));
        cr->home = li;
        return cr;
    }

    /* Allocate fresh */
    cr = AllocMem(sizeof(struct ClipRect), MEMF_PUBLIC | MEMF_CLEAR);
    if (cr)
    {
        cr->home = li;
    }
    return cr;
}

/*
 * Return a ClipRect to the Layer_Info pool
 */
static void FreeClipRect(struct Layer_Info *li, struct ClipRect *cr)
{
    if (!cr)
        return;

    /* Free any backing store bitmap */
    if (cr->BitMap && cr->obscured)
    {
        /* Backing store bitmap - would need to free planes here */
        /* For now, we don't allocate backing store bitmaps */
    }

    if (li)
    {
        /* Return to pool */
        cr->Next = li->FreeClipRects;
        li->FreeClipRects = cr;
    }
    else
    {
        FreeMem(cr, sizeof(struct ClipRect));
    }
}

/*
 * Free all ClipRects in a list
 */
static void FreeClipRectList(struct Layer_Info *li, struct ClipRect *cr)
{
    while (cr)
    {
        struct ClipRect *next = cr->Next;
        FreeClipRect(li, cr);
        cr = next;
    }
}

/*
 * Create a simple ClipRect covering the entire layer bounds
 * For a single layer with no overlapping, this is all we need.
 */
static struct ClipRect *CreateSimpleClipRect(struct Layer_Info *li, struct Layer *layer)
{
    struct ClipRect *cr = AllocClipRect(li);
    if (!cr)
        return NULL;

    cr->bounds = layer->bounds;
    cr->obscured = 0;  /* Visible */
    cr->BitMap = NULL; /* Draw directly to screen */
    cr->Next = NULL;

    return cr;
}

/*
 * Rebuild ClipRects for a layer based on overlapping layers.
 * For Phase 1, we just create a single ClipRect covering the whole layer.
 * Full implementation would split based on overlapping layers.
 */
static void RebuildClipRects(struct Layer *layer)
{
    struct Layer_Info *li = layer->LayerInfo;

    /* Free existing ClipRects */
    FreeClipRectList(li, layer->ClipRect);
    layer->ClipRect = NULL;

    /* For now, create a simple single ClipRect covering the whole layer */
    layer->ClipRect = CreateSimpleClipRect(li, layer);

    DPRINTF(LOG_DEBUG, "_layers: RebuildClipRects() layer bounds [%d,%d]-[%d,%d]\n",
            layer->bounds.MinX, layer->bounds.MinY,
            layer->bounds.MaxX, layer->bounds.MaxY);
}

/* ========================================================================
 * Layer_Info management (-0x1e to -0x96)
 * ======================================================================== */

/*
 * InitLayers - Initialize a Layer_Info structure (offset -0x1e)
 *
 * Initializes a pre-allocated Layer_Info structure.
 * Called by NewLayerInfo() after allocation.
 */
static VOID _layers_InitLayers ( register struct LayersBase *LayersBase __asm("a6"),
                                 register struct Layer_Info *li         __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: InitLayers() called li=0x%08lx\n", (ULONG)li);

    if (!li)
        return;

    memset(li, 0, sizeof(struct Layer_Info));

    li->top_layer = NULL;
    li->FreeClipRects = NULL;

    /* Initialize bounds to maximum - no clipping by default */
    li->bounds.MinX = 0;
    li->bounds.MinY = 0;
    li->bounds.MaxX = 32767;  /* Maximum screen size */
    li->bounds.MaxY = 32767;

    /* Initialize the semaphore */
    InitSemaphore(&li->Lock);

    /* Initialize the semaphore list (MinList) */
    li->gs_Head.mlh_Head = (struct MinNode *)&li->gs_Head.mlh_Tail;
    li->gs_Head.mlh_Tail = NULL;
    li->gs_Head.mlh_TailPred = (struct MinNode *)&li->gs_Head.mlh_Head;

    li->Flags = NEWLAYERINFO_CALLED;
    li->LockLayersCount = 0;
    li->BlankHook = LAYERS_BACKFILL;
}

/*
 * NewLayerInfo - Allocate and initialize a new Layer_Info (offset -0x90)
 *
 * Creates a new Layer_Info structure that can hold layers.
 * This is typically called when creating a new Screen.
 */
static struct Layer_Info * _layers_NewLayerInfo ( register struct LayersBase *LayersBase __asm("a6"))
{
    DPRINTF(LOG_DEBUG, "_layers: NewLayerInfo() called\n");

    struct Layer_Info *li = AllocMem(sizeof(struct Layer_Info), MEMF_PUBLIC | MEMF_CLEAR);
    if (!li)
        return NULL;

    _layers_InitLayers(LayersBase, li);

    return li;
}

/*
 * DisposeLayerInfo - Free a Layer_Info structure (offset -0x96)
 *
 * Frees the Layer_Info and all associated resources.
 * All layers should be deleted before calling this.
 */
static VOID _layers_DisposeLayerInfo ( register struct LayersBase *LayersBase __asm("a6"),
                                       register struct Layer_Info *li         __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: DisposeLayerInfo() called li=0x%08lx\n", (ULONG)li);

    if (!li)
        return;

    /* Free pooled ClipRects */
    struct ClipRect *cr = li->FreeClipRects;
    while (cr)
    {
        struct ClipRect *next = cr->Next;
        FreeMem(cr, sizeof(struct ClipRect));
        cr = next;
    }

    FreeMem(li, sizeof(struct Layer_Info));
}

/* ========================================================================
 * Layer creation and deletion (-0x24 to -0x5a)
 * ======================================================================== */

/*
 * Internal: Create a layer with specified position in z-order
 */
static struct Layer * CreateLayerInternal ( struct LayersBase  *LayersBase,
                                            struct Layer_Info  *li,
                                            struct BitMap      *bm,
                                            LONG                x0,
                                            LONG                y0,
                                            LONG                x1,
                                            LONG                y1,
                                            LONG                flags,
                                            struct Hook        *hook,
                                            struct BitMap      *bm2,
                                            BOOL                in_front)
{
    DPRINTF(LOG_DEBUG, "_layers: CreateLayerInternal() [%ld,%ld]-[%ld,%ld] flags=0x%lx front=%d\n",
            x0, y0, x1, y1, flags, in_front);

    if (!li || !bm)
        return NULL;

    /* Allocate the layer structure */
    struct Layer *layer = AllocMem(sizeof(struct Layer), MEMF_PUBLIC | MEMF_CLEAR);
    if (!layer)
        return NULL;

    /* Initialize bounds */
    layer->bounds.MinX = x0;
    layer->bounds.MinY = y0;
    layer->bounds.MaxX = x1;
    layer->bounds.MaxY = y1;
    layer->Width = x1 - x0 + 1;
    layer->Height = y1 - y0 + 1;

    /* Set flags */
    layer->Flags = flags & (LAYERSIMPLE | LAYERSMART | LAYERSUPER | LAYERBACKDROP);

    /* Link to Layer_Info */
    layer->LayerInfo = li;

    /* Initialize semaphore */
    InitSemaphore(&layer->Lock);

    /* Set up RastPort for this layer */
    layer->rp = AllocMem(sizeof(struct RastPort), MEMF_PUBLIC | MEMF_CLEAR);
    if (!layer->rp)
    {
        FreeMem(layer, sizeof(struct Layer));
        return NULL;
    }

    /* Initialize the RastPort */
    InitRastPort(layer->rp);
    layer->rp->BitMap = bm;
    layer->rp->Layer = layer;

    /* SuperBitMap support */
    if (flags & LAYERSUPER)
    {
        layer->SuperBitMap = bm2;
    }

    /* BackFill hook */
    layer->BackFill = hook;

    /* Insert into layer list */
    ObtainSemaphore(&li->Lock);

    if (in_front)
    {
        /* Insert at front (top) */
        layer->back = li->top_layer;
        layer->front = NULL;

        if (li->top_layer)
        {
            li->top_layer->front = layer;
        }
        li->top_layer = layer;
    }
    else
    {
        /* Insert at back (bottom) */
        if (!li->top_layer)
        {
            /* First layer */
            li->top_layer = layer;
            layer->front = NULL;
            layer->back = NULL;
        }
        else
        {
            /* Find the backmost layer */
            struct Layer *back = li->top_layer;
            while (back->back)
            {
                back = back->back;
            }

            layer->front = back;
            layer->back = NULL;
            back->back = layer;
        }
    }

    /* Build initial ClipRects */
    RebuildClipRects(layer);

    /* Add layer's semaphore to the gs_Head list */
    AddTail((struct List *)&li->gs_Head, (struct Node *)&layer->Lock);

    ReleaseSemaphore(&li->Lock);

    DPRINTF(LOG_DEBUG, "_layers: CreateLayerInternal() returning layer=0x%08lx\n", (ULONG)layer);

    return layer;
}

/*
 * CreateUpfrontLayer - Create a layer in front of all others (offset -0x24)
 */
static struct Layer * _layers_CreateUpfrontLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                                   register struct Layer_Info *li         __asm("a0"),
                                                   register struct BitMap     *bm         __asm("a1"),
                                                   register LONG               x0         __asm("d0"),
                                                   register LONG               y0         __asm("d1"),
                                                   register LONG               x1         __asm("d2"),
                                                   register LONG               y1         __asm("d3"),
                                                   register LONG               flags      __asm("d4"),
                                                   register struct BitMap     *bm2        __asm("a2"))
{
    DPRINTF(LOG_DEBUG, "_layers: CreateUpfrontLayer() called\n");
    return CreateLayerInternal(LayersBase, li, bm, x0, y0, x1, y1, flags, NULL, bm2, TRUE);
}

/*
 * CreateBehindLayer - Create a layer behind all others (offset -0x2a)
 */
static struct Layer * _layers_CreateBehindLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                                  register struct Layer_Info *li         __asm("a0"),
                                                  register struct BitMap     *bm         __asm("a1"),
                                                  register LONG               x0         __asm("d0"),
                                                  register LONG               y0         __asm("d1"),
                                                  register LONG               x1         __asm("d2"),
                                                  register LONG               y1         __asm("d3"),
                                                  register LONG               flags      __asm("d4"),
                                                  register struct BitMap     *bm2        __asm("a2"))
{
    DPRINTF(LOG_DEBUG, "_layers: CreateBehindLayer() called\n");
    return CreateLayerInternal(LayersBase, li, bm, x0, y0, x1, y1, flags, NULL, bm2, FALSE);
}

/*
 * DeleteLayer - Delete a layer (offset -0x5a)
 *
 * Returns TRUE on success, FALSE on failure.
 */
static LONG _layers_DeleteLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                  register LONG               dummy      __asm("a0"),
                                  register struct Layer      *layer      __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: DeleteLayer() called layer=0x%08lx\n", (ULONG)layer);

    if (!layer)
        return FALSE;

    struct Layer_Info *li = layer->LayerInfo;
    if (!li)
        return FALSE;

    ObtainSemaphore(&li->Lock);

    /* Remove from layer list */
    if (layer->front)
    {
        layer->front->back = layer->back;
    }
    else
    {
        /* This was the top layer */
        li->top_layer = layer->back;
    }

    if (layer->back)
    {
        layer->back->front = layer->front;
    }

    /* Remove from semaphore list */
    Remove((struct Node *)&layer->Lock);

    ReleaseSemaphore(&li->Lock);

    /* Free ClipRects */
    FreeClipRectList(li, layer->ClipRect);

    /* Free the layer's RastPort if we allocated it */
    if (layer->rp)
    {
        FreeMem(layer->rp, sizeof(struct RastPort));
    }

    /* Free the layer */
    FreeMem(layer, sizeof(struct Layer));

    return TRUE;
}

/* ========================================================================
 * Layer manipulation (-0x30 to -0x48)
 * ======================================================================== */

/*
 * UpfrontLayer - Move layer to front (offset -0x30)
 */
static LONG _layers_UpfrontLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                   register LONG               dummy      __asm("a0"),
                                   register struct Layer      *layer      __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: UpfrontLayer() called layer=0x%08lx\n", (ULONG)layer);

    if (!layer || !layer->LayerInfo)
        return FALSE;

    struct Layer_Info *li = layer->LayerInfo;

    ObtainSemaphore(&li->Lock);

    /* Already at front? */
    if (layer->front == NULL)
    {
        ReleaseSemaphore(&li->Lock);
        return TRUE;
    }

    /* Remove from current position */
    if (layer->front)
        layer->front->back = layer->back;
    if (layer->back)
        layer->back->front = layer->front;

    /* Insert at front */
    layer->back = li->top_layer;
    layer->front = NULL;
    if (li->top_layer)
        li->top_layer->front = layer;
    li->top_layer = layer;

    /* Rebuild ClipRects for affected layers */
    /* TODO: In full implementation, rebuild all affected layers */

    ReleaseSemaphore(&li->Lock);

    return TRUE;
}

/*
 * BehindLayer - Move layer to back (offset -0x36)
 */
static LONG _layers_BehindLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                  register LONG               dummy      __asm("a0"),
                                  register struct Layer      *layer      __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: BehindLayer() called layer=0x%08lx\n", (ULONG)layer);

    if (!layer || !layer->LayerInfo)
        return FALSE;

    struct Layer_Info *li = layer->LayerInfo;

    ObtainSemaphore(&li->Lock);

    /* Already at back? */
    if (layer->back == NULL && layer != li->top_layer)
    {
        ReleaseSemaphore(&li->Lock);
        return TRUE;
    }

    /* Remove from current position */
    if (layer->front)
        layer->front->back = layer->back;
    else
        li->top_layer = layer->back;

    if (layer->back)
        layer->back->front = layer->front;

    /* Find backmost layer */
    struct Layer *back = li->top_layer;
    if (back)
    {
        while (back->back)
            back = back->back;

        layer->front = back;
        layer->back = NULL;
        back->back = layer;
    }
    else
    {
        /* Only layer */
        li->top_layer = layer;
        layer->front = NULL;
        layer->back = NULL;
    }

    ReleaseSemaphore(&li->Lock);

    return TRUE;
}

/*
 * MoveLayer - Move a layer by dx, dy (offset -0x3c)
 */
static LONG _layers_MoveLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                register LONG               dummy      __asm("a0"),
                                register struct Layer      *layer      __asm("a1"),
                                register LONG               dx         __asm("d0"),
                                register LONG               dy         __asm("d1"))
{
    DPRINTF(LOG_DEBUG, "_layers: MoveLayer() layer=0x%08lx dx=%ld dy=%ld\n", (ULONG)layer, dx, dy);

    if (!layer)
        return FALSE;

    ObtainSemaphore(&layer->Lock);

    layer->bounds.MinX += dx;
    layer->bounds.MinY += dy;
    layer->bounds.MaxX += dx;
    layer->bounds.MaxY += dy;

    /* Rebuild ClipRects */
    RebuildClipRects(layer);

    ReleaseSemaphore(&layer->Lock);

    return TRUE;
}

/*
 * SizeLayer - Resize a layer (offset -0x42)
 */
static LONG _layers_SizeLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                register LONG               dummy      __asm("a0"),
                                register struct Layer      *layer      __asm("a1"),
                                register LONG               dx         __asm("d0"),
                                register LONG               dy         __asm("d1"))
{
    DPRINTF(LOG_DEBUG, "_layers: SizeLayer() layer=0x%08lx dx=%ld dy=%ld\n", (ULONG)layer, dx, dy);

    if (!layer)
        return FALSE;

    ObtainSemaphore(&layer->Lock);

    layer->bounds.MaxX += dx;
    layer->bounds.MaxY += dy;
    layer->Width = layer->bounds.MaxX - layer->bounds.MinX + 1;
    layer->Height = layer->bounds.MaxY - layer->bounds.MinY + 1;

    /* Rebuild ClipRects */
    RebuildClipRects(layer);

    ReleaseSemaphore(&layer->Lock);

    return TRUE;
}

/*
 * ScrollLayer - Scroll layer contents (offset -0x48)
 */
static VOID _layers_ScrollLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                  register LONG               dummy      __asm("a0"),
                                  register struct Layer      *layer      __asm("a1"),
                                  register LONG               dx         __asm("d0"),
                                  register LONG               dy         __asm("d1"))
{
    DPRINTF(LOG_DEBUG, "_layers: ScrollLayer() layer=0x%08lx dx=%ld dy=%ld\n", (ULONG)layer, dx, dy);

    if (!layer)
        return;

    layer->Scroll_X += dx;
    layer->Scroll_Y += dy;

    /* TODO: Actually scroll the bitmap contents */
}

/* ========================================================================
 * Refresh handling (-0x4e to -0x54)
 * ======================================================================== */

/*
 * BeginUpdate - Begin rendering to damage regions (offset -0x4e)
 *
 * Sets up ClipRects for rendering only to damaged areas.
 * Returns TRUE if there are areas to refresh, FALSE otherwise.
 */
static LONG _layers_BeginUpdate ( register struct LayersBase *LayersBase __asm("a6"),
                                  register struct Layer      *layer      __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: BeginUpdate() called layer=0x%08lx\n", (ULONG)layer);

    if (!layer)
        return FALSE;

    ObtainSemaphore(&layer->Lock);

    layer->Flags |= LAYERUPDATING;

    /* Save current ClipRects for restoration in EndUpdate */
    layer->Undamaged = layer->ClipRect;

    /* TODO: Build ClipRects based on DamageList */
    /* For now, we just keep the full layer ClipRects */

    /* Check if there's damage to refresh */
    if (layer->DamageList && layer->DamageList->RegionRectangle)
    {
        return TRUE;
    }

    /* Even with no explicit damage, return TRUE to allow drawing */
    return TRUE;
}

/*
 * EndUpdate - End rendering, restore ClipRects (offset -0x54)
 */
static VOID _layers_EndUpdate ( register struct LayersBase *LayersBase __asm("a6"),
                                register struct Layer      *layer      __asm("a0"),
                                register UWORD              flag       __asm("d0"))
{
    DPRINTF(LOG_DEBUG, "_layers: EndUpdate() called layer=0x%08lx flag=%u\n", (ULONG)layer, flag);

    if (!layer)
        return;

    layer->Flags &= ~LAYERUPDATING;

    /* Restore original ClipRects if we saved them */
    if (layer->Undamaged)
    {
        layer->ClipRect = layer->Undamaged;
        layer->Undamaged = NULL;
    }

    /* Clear damage if requested */
    if (flag)
    {
        if (layer->DamageList)
        {
            /* Free DamageList Region */
            /* TODO: Implement Region freeing */
            layer->DamageList = NULL;
        }
        layer->Flags &= ~LAYERREFRESH;
    }

    ReleaseSemaphore(&layer->Lock);
}

/* ========================================================================
 * Locking functions (-0x60 to -0x8a)
 * ======================================================================== */

/*
 * LockLayer - Lock a layer for exclusive access (offset -0x60)
 */
static VOID _layers_LockLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                register LONG               dummy      __asm("a0"),
                                register struct Layer      *layer      __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: LockLayer() called layer=0x%08lx\n", (ULONG)layer);

    if (layer)
    {
        ObtainSemaphore(&layer->Lock);
    }
}

/*
 * UnlockLayer - Unlock a layer (offset -0x66)
 */
static VOID _layers_UnlockLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                  register struct Layer      *layer      __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: UnlockLayer() called layer=0x%08lx\n", (ULONG)layer);

    if (layer)
    {
        ReleaseSemaphore(&layer->Lock);
    }
}

/*
 * LockLayers - Lock all layers in a Layer_Info (offset -0x6c)
 */
static VOID _layers_LockLayers ( register struct LayersBase *LayersBase __asm("a6"),
                                 register struct Layer_Info *li         __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: LockLayers() called li=0x%08lx\n", (ULONG)li);

    if (!li)
        return;

    ObtainSemaphore(&li->Lock);

    /* Lock all layers from front to back */
    struct Layer *layer = li->top_layer;
    while (layer)
    {
        ObtainSemaphore(&layer->Lock);
        layer = layer->back;
    }

    li->LockLayersCount++;
}

/*
 * UnlockLayers - Unlock all layers in a Layer_Info (offset -0x72)
 */
static VOID _layers_UnlockLayers ( register struct LayersBase *LayersBase __asm("a6"),
                                   register struct Layer_Info *li         __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: UnlockLayers() called li=0x%08lx\n", (ULONG)li);

    if (!li)
        return;

    /* Unlock all layers from back to front */
    struct Layer *layer = li->top_layer;
    if (layer)
    {
        /* Find backmost */
        while (layer->back)
            layer = layer->back;

        /* Unlock from back to front */
        while (layer)
        {
            ReleaseSemaphore(&layer->Lock);
            layer = layer->front;
        }
    }

    li->LockLayersCount--;
    ReleaseSemaphore(&li->Lock);
}

/*
 * LockLayerInfo - Lock the Layer_Info structure (offset -0x78)
 */
static VOID _layers_LockLayerInfo ( register struct LayersBase *LayersBase __asm("a6"),
                                    register struct Layer_Info *li         __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: LockLayerInfo() called li=0x%08lx\n", (ULONG)li);

    if (li)
    {
        ObtainSemaphore(&li->Lock);
    }
}

/*
 * UnlockLayerInfo - Unlock the Layer_Info structure (offset -0x8a)
 */
static VOID _layers_UnlockLayerInfo ( register struct LayersBase *LayersBase __asm("a6"),
                                      register struct Layer_Info *li         __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: UnlockLayerInfo() called li=0x%08lx\n", (ULONG)li);

    if (li)
    {
        ReleaseSemaphore(&li->Lock);
    }
}

/* ========================================================================
 * Support functions (-0x7e to -0x84)
 * ======================================================================== */

/*
 * SwapBitsRastPortClipRect - Swap backing store with visible (offset -0x7e)
 */
static VOID _layers_SwapBitsRastPortClipRect ( register struct LayersBase *LayersBase __asm("a6"),
                                               register struct RastPort   *rp         __asm("a0"),
                                               register struct ClipRect   *cr         __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: SwapBitsRastPortClipRect() stub called\n");
    /* TODO: Implement for LAYERSMART support */
}

/*
 * WhichLayer - Find the layer at coordinates (x, y) (offset -0x84)
 */
static struct Layer * _layers_WhichLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                           register struct Layer_Info *li         __asm("a0"),
                                           register WORD               x          __asm("d0"),
                                           register WORD               y          __asm("d1"))
{
    DPRINTF(LOG_DEBUG, "_layers: WhichLayer() x=%d y=%d\n", x, y);

    if (!li)
        return NULL;

    /* Search from front (top) to back (bottom) */
    struct Layer *layer = li->top_layer;
    while (layer)
    {
        /* Skip hidden layers */
        if (!(layer->Flags & LAYERHIDDEN))
        {
            if (x >= layer->bounds.MinX && x <= layer->bounds.MaxX &&
                y >= layer->bounds.MinY && y <= layer->bounds.MaxY)
            {
                return layer;
            }
        }
        layer = layer->back;
    }

    return NULL;
}

/* ========================================================================
 * Obsolete functions for compatibility (-0x9c to -0xa2)
 * ======================================================================== */

/*
 * FattenLayerInfo - Obsolete, for compatibility (offset -0x9c)
 */
static LONG _layers_FattenLayerInfo ( register struct LayersBase *LayersBase __asm("a6"),
                                      register struct Layer_Info *li         __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: FattenLayerInfo() obsolete stub called\n");
    return TRUE;
}

/*
 * ThinLayerInfo - Obsolete, for compatibility (offset -0xa2)
 */
static VOID _layers_ThinLayerInfo ( register struct LayersBase *LayersBase __asm("a6"),
                                    register struct Layer_Info *li         __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: ThinLayerInfo() obsolete stub called\n");
}

/* ========================================================================
 * Advanced functions (-0xa8 to -0xf0)
 * ======================================================================== */

/*
 * MoveLayerInFrontOf - Move layer in front of another (offset -0xa8)
 */
static LONG _layers_MoveLayerInFrontOf ( register struct LayersBase *LayersBase    __asm("a6"),
                                         register struct Layer      *layer_to_move __asm("a0"),
                                         register struct Layer      *other_layer   __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: MoveLayerInFrontOf() stub called\n");

    if (!layer_to_move || !other_layer)
        return FALSE;

    /* TODO: Full implementation */
    return TRUE;
}

/*
 * InstallClipRegion - Install user clipping region (offset -0xae)
 */
static struct Region * _layers_InstallClipRegion ( register struct LayersBase *LayersBase __asm("a6"),
                                                   register struct Layer      *layer      __asm("a0"),
                                                   register struct Region     *region     __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: InstallClipRegion() called layer=0x%08lx region=0x%08lx\n",
            (ULONG)layer, (ULONG)region);

    if (!layer)
        return NULL;

    struct Region *old = layer->ClipRegion;
    layer->ClipRegion = region;

    /* TODO: Rebuild ClipRects to incorporate the new region */

    return old;
}

/*
 * MoveSizeLayer - Combined move and size (offset -0xb4)
 */
static LONG _layers_MoveSizeLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                    register struct Layer      *layer      __asm("a0"),
                                    register LONG               dx         __asm("d0"),
                                    register LONG               dy         __asm("d1"),
                                    register LONG               dw         __asm("d2"),
                                    register LONG               dh         __asm("d3"))
{
    DPRINTF(LOG_DEBUG, "_layers: MoveSizeLayer() layer=0x%08lx dx=%ld dy=%ld dw=%ld dh=%ld\n",
            (ULONG)layer, dx, dy, dw, dh);

    if (!layer)
        return FALSE;

    ObtainSemaphore(&layer->Lock);

    layer->bounds.MinX += dx;
    layer->bounds.MinY += dy;
    layer->bounds.MaxX += dx + dw;
    layer->bounds.MaxY += dy + dh;
    layer->Width = layer->bounds.MaxX - layer->bounds.MinX + 1;
    layer->Height = layer->bounds.MaxY - layer->bounds.MinY + 1;

    RebuildClipRects(layer);

    ReleaseSemaphore(&layer->Lock);

    return TRUE;
}

/*
 * CreateUpfrontHookLayer - Create layer with backfill hook (offset -0xba)
 */
static struct Layer * _layers_CreateUpfrontHookLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                                       register struct Layer_Info *li         __asm("a0"),
                                                       register struct BitMap     *bm         __asm("a1"),
                                                       register LONG               x0         __asm("d0"),
                                                       register LONG               y0         __asm("d1"),
                                                       register LONG               x1         __asm("d2"),
                                                       register LONG               y1         __asm("d3"),
                                                       register LONG               flags      __asm("d4"),
                                                       register struct Hook        *hook      __asm("a3"),
                                                       register struct BitMap     *bm2        __asm("a2"))
{
    DPRINTF(LOG_DEBUG, "_layers: CreateUpfrontHookLayer() called\n");
    return CreateLayerInternal(LayersBase, li, bm, x0, y0, x1, y1, flags, hook, bm2, TRUE);
}

/*
 * CreateBehindHookLayer - Create behind layer with backfill hook (offset -0xc0)
 */
static struct Layer * _layers_CreateBehindHookLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                                      register struct Layer_Info *li         __asm("a0"),
                                                      register struct BitMap     *bm         __asm("a1"),
                                                      register LONG               x0         __asm("d0"),
                                                      register LONG               y0         __asm("d1"),
                                                      register LONG               x1         __asm("d2"),
                                                      register LONG               y1         __asm("d3"),
                                                      register LONG               flags      __asm("d4"),
                                                      register struct Hook        *hook      __asm("a3"),
                                                      register struct BitMap     *bm2        __asm("a2"))
{
    DPRINTF(LOG_DEBUG, "_layers: CreateBehindHookLayer() called\n");
    return CreateLayerInternal(LayersBase, li, bm, x0, y0, x1, y1, flags, hook, bm2, FALSE);
}

/*
 * InstallLayerHook - Install backfill hook on layer (offset -0xc6)
 */
static struct Hook * _layers_InstallLayerHook ( register struct LayersBase *LayersBase __asm("a6"),
                                                register struct Layer      *layer      __asm("a0"),
                                                register struct Hook       *hook       __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: InstallLayerHook() called\n");

    if (!layer)
        return NULL;

    struct Hook *old = layer->BackFill;
    layer->BackFill = hook;
    return old;
}

/*
 * InstallLayerInfoHook - Install global backfill hook (offset -0xcc)
 */
static struct Hook * _layers_InstallLayerInfoHook ( register struct LayersBase *LayersBase __asm("a6"),
                                                    register struct Layer_Info *li         __asm("a0"),
                                                    register struct Hook       *hook       __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: InstallLayerInfoHook() called\n");

    if (!li)
        return NULL;

    struct Hook *old = li->BlankHook;
    li->BlankHook = hook;
    return old;
}

/*
 * SortLayerCR - Sort ClipRects for scrolling (offset -0xd2)
 */
static VOID _layers_SortLayerCR ( register struct LayersBase *LayersBase __asm("a6"),
                                  register struct Layer      *layer      __asm("a0"),
                                  register WORD               dx         __asm("d0"),
                                  register WORD               dy         __asm("d1"))
{
    DPRINTF(LOG_DEBUG, "_layers: SortLayerCR() stub called\n");
    /* TODO: Implement for optimized scrolling */
}

/*
 * DoHookClipRects - Call hook for each ClipRect (offset -0xd8)
 */
static VOID _layers_DoHookClipRects ( register struct LayersBase    *LayersBase __asm("a6"),
                                      register struct Hook          *hook       __asm("a0"),
                                      register struct RastPort      *rp         __asm("a1"),
                                      register const struct Rectangle *rect     __asm("a2"))
{
    DPRINTF(LOG_DEBUG, "_layers: DoHookClipRects() stub called\n");
    /* TODO: Implement hook iteration over ClipRects */
}

/* ========================================================================
 * V45+ functions (-0xde to -0xf0)
 * ======================================================================== */

/*
 * LayerOccluded - Check if layer is completely hidden (offset -0xde)
 */
static BOOL _layers_LayerOccluded ( register struct LayersBase *LayersBase __asm("a6"),
                                    register struct Layer      *layer      __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: LayerOccluded() called\n");

    if (!layer)
        return TRUE;

    /* Check if layer is hidden */
    if (layer->Flags & LAYERHIDDEN)
        return TRUE;

    /* Check if all ClipRects are obscured */
    struct ClipRect *cr = layer->ClipRect;
    while (cr)
    {
        if (!cr->obscured)
            return FALSE;  /* At least one visible cliprect */
        cr = cr->Next;
    }

    return TRUE;  /* All obscured or no cliprects */
}

/*
 * HideLayer - Make layer invisible (offset -0xe4)
 */
static LONG _layers_HideLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                register struct Layer      *layer      __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: HideLayer() called\n");

    if (!layer)
        return FALSE;

    layer->Flags |= LAYERHIDDEN;
    return TRUE;
}

/*
 * ShowLayer - Make layer visible (offset -0xea)
 */
static LONG _layers_ShowLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                register struct Layer      *layer      __asm("a0"),
                                register struct Layer      *in_front_of __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: ShowLayer() called\n");

    if (!layer)
        return FALSE;

    layer->Flags &= ~LAYERHIDDEN;

    /* TODO: Handle in_front_of positioning */

    return TRUE;
}

/*
 * SetLayerInfoBounds - Set clipping bounds of Layer_Info (offset -0xf0)
 */
static BOOL _layers_SetLayerInfoBounds ( register struct LayersBase     *LayersBase __asm("a6"),
                                         register struct Layer_Info     *li         __asm("a0"),
                                         register const struct Rectangle *bounds    __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: SetLayerInfoBounds() called\n");

    if (!li || !bounds)
        return FALSE;

    li->bounds = *bounds;
    return TRUE;
}

/* ========================================================================
 * Library data structures
 * ======================================================================== */

struct MyDataInit
{
    UWORD ln_Type_Init     ; UWORD ln_Type_Offset     ; UWORD ln_Type_Content     ;
    UBYTE ln_Name_Init     ; UBYTE ln_Name_Offset     ; ULONG ln_Name_Content     ;
    UWORD lib_Flags_Init   ; UWORD lib_Flags_Offset   ; UWORD lib_Flags_Content   ;
    UWORD lib_Version_Init ; UWORD lib_Version_Offset ; UWORD lib_Version_Content ;
    UWORD lib_Revision_Init; UWORD lib_Revision_Offset; UWORD lib_Revision_Content;
    UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
    ULONG ENDMARK;
};

extern APTR              __g_lxa_layers_FuncTab [];
extern struct MyDataInit __g_lxa_layers_DataTab;
extern struct InitTable  __g_lxa_layers_InitTab;
extern APTR              __g_lxa_layers_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                // UWORD rt_MatchWord
    &ROMTag,                      // struct Resident *rt_MatchTag
    &__g_lxa_layers_EndResident,  // APTR  rt_EndSkip
    RTF_AUTOINIT,                 // UBYTE rt_Flags
    VERSION,                      // UBYTE rt_Version
    NT_LIBRARY,                   // UBYTE rt_Type
    0,                            // BYTE  rt_Pri
    &_g_layers_ExLibName[0],      // char  *rt_Name
    &_g_layers_ExLibID[0],        // char  *rt_IdString
    &__g_lxa_layers_InitTab       // APTR  rt_Init
};

APTR __g_lxa_layers_EndResident;
struct Resident *__lxa_layers_ROMTag = &ROMTag;

struct InitTable __g_lxa_layers_InitTab =
{
    (ULONG)               sizeof(struct LayersBase),    // LibBaseSize
    (APTR              *) &__g_lxa_layers_FuncTab[0],   // FunctionTable
    (APTR)                &__g_lxa_layers_DataTab,      // DataTable
    (APTR)                __g_lxa_layers_InitLib        // InitLibFn
};

/*
 * Function table - offsets from base
 *
 * Standard library functions:
 *   -6   Open
 *   -12  Close
 *   -18  Expunge
 *   -24  Reserved
 *
 * Layers functions start at -30 (0x1e):
 */
APTR __g_lxa_layers_FuncTab [] =
{
    __g_lxa_layers_OpenLib,           // -6
    __g_lxa_layers_CloseLib,          // -12
    __g_lxa_layers_ExpungeLib,        // -18
    __g_lxa_layers_ExtFuncLib,        // -24

    _layers_InitLayers,               // -30  (0x1e)
    _layers_CreateUpfrontLayer,       // -36  (0x24)
    _layers_CreateBehindLayer,        // -42  (0x2a)
    _layers_UpfrontLayer,             // -48  (0x30)
    _layers_BehindLayer,              // -54  (0x36)
    _layers_MoveLayer,                // -60  (0x3c)
    _layers_SizeLayer,                // -66  (0x42)
    _layers_ScrollLayer,              // -72  (0x48)
    _layers_BeginUpdate,              // -78  (0x4e)
    _layers_EndUpdate,                // -84  (0x54)
    _layers_DeleteLayer,              // -90  (0x5a)
    _layers_LockLayer,                // -96  (0x60)
    _layers_UnlockLayer,              // -102 (0x66)
    _layers_LockLayers,               // -108 (0x6c)
    _layers_UnlockLayers,             // -114 (0x72)
    _layers_LockLayerInfo,            // -120 (0x78)
    _layers_SwapBitsRastPortClipRect, // -126 (0x7e)
    _layers_WhichLayer,               // -132 (0x84)
    _layers_UnlockLayerInfo,          // -138 (0x8a)
    _layers_NewLayerInfo,             // -144 (0x90)
    _layers_DisposeLayerInfo,         // -150 (0x96)
    _layers_FattenLayerInfo,          // -156 (0x9c)
    _layers_ThinLayerInfo,            // -162 (0xa2)
    _layers_MoveLayerInFrontOf,       // -168 (0xa8)
    _layers_InstallClipRegion,        // -174 (0xae)
    _layers_MoveSizeLayer,            // -180 (0xb4)
    _layers_CreateUpfrontHookLayer,   // -186 (0xba)
    _layers_CreateBehindHookLayer,    // -192 (0xc0)
    _layers_InstallLayerHook,         // -198 (0xc6)
    _layers_InstallLayerInfoHook,     // -204 (0xcc)
    _layers_SortLayerCR,              // -210 (0xd2)
    _layers_DoHookClipRects,          // -216 (0xd8)
    _layers_LayerOccluded,            // -222 (0xde)
    _layers_HideLayer,                // -228 (0xe4)
    _layers_ShowLayer,                // -234 (0xea)
    _layers_SetLayerInfoBounds,       // -240 (0xf0)

    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_layers_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_layers_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_layers_ExLibID[0],
    (ULONG) 0
};
