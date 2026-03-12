/*
 * lxa_layers.c - layers.library implementation for lxa
 *
 * Implements the AmigaOS layers.library for window clipping and refresh handling.
 * This library manages layers (drawing regions) that can overlap and be reordered.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
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
#include <graphics/layersext.h>
#include <graphics/regions.h>

#include <utility/tagitem.h>

#include <clib/graphics_protos.h>
#include <clib/utility_protos.h>
#include <inline/graphics.h>
#include <inline/utility.h>

#include <clib/layers_protos.h>

//#define ENABLE_DEBUG
#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "layers"
#define EXLIBVER   " 40.1 (2026/02/01)"

char __aligned _g_layers_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_layers_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_layers_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_layers_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct GfxBase  *GfxBase;
extern struct UtilityBase *UtilityBase;

/*
 * LayersBase structure - library base for layers.library
 */
struct LayersBase
{
    struct Library lib;
    /* Add any global state here if needed */
};

static struct LayersBase *LayersBase;

static BOOL IntersectRectangles(const struct Rectangle *r1, const struct Rectangle *r2,
                                struct Rectangle *result);
static void RebuildClipRects(struct Layer *layer);
static void RebuildClipRectsFrom(struct Layer *layer);
static void DamageExposedAreas(struct Layer_Info *li,
                               struct Layer *moved_layer,
                               const struct Rectangle *old_bounds,
                               const struct Rectangle *new_bounds);
static struct Layer *CreateLayerTagListInternal(struct LayersBase *LayersBase,
                                                struct Layer_Info *li,
                                                struct BitMap *bm,
                                                LONG x0,
                                                LONG y0,
                                                LONG x1,
                                                LONG y1,
                                                LONG flags,
                                                struct TagItem *tagList);

/* ========================================================================
 * Library management functions
 * ======================================================================== */

struct LayersBase * __g_lxa_layers_InitLib ( register struct LayersBase *layersb __asm("d0"),
                                              register BPTR               seglist __asm("a0"),
                                              register struct ExecBase   *sysb    __asm("a6"))
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

static BOOL IsBackdropLayer(const struct Layer *layer)
{
    return layer && ((layer->Flags & LAYERBACKDROP) != 0);
}

static struct Layer *FindBackmostLayer(struct Layer_Info *li)
{
    struct Layer *layer;

    if (!li)
        return NULL;

    layer = li->top_layer;
    while (layer && layer->back)
        layer = layer->back;

    return layer;
}

static struct Layer *FindFrontmostBackdrop(struct Layer_Info *li)
{
    struct Layer *layer;

    if (!li)
        return NULL;

    layer = li->top_layer;
    while (layer)
    {
        if (IsBackdropLayer(layer))
            return layer;
        layer = layer->back;
    }

    return NULL;
}

static void UnlinkLayerFromInfo(struct Layer_Info *li, struct Layer *layer)
{
    if (!li || !layer)
        return;

    if (layer->front)
        layer->front->back = layer->back;
    else
        li->top_layer = layer->back;

    if (layer->back)
        layer->back->front = layer->front;

    layer->front = NULL;
    layer->back = NULL;
}

static void InsertLayerAtFront(struct Layer_Info *li, struct Layer *layer)
{
    if (!li || !layer)
        return;

    layer->front = NULL;
    layer->back = li->top_layer;
    if (li->top_layer)
        li->top_layer->front = layer;
    li->top_layer = layer;
}

static void InsertLayerInFrontOf(struct Layer_Info *li, struct Layer *layer, struct Layer *other)
{
    struct Layer *backmost;

    if (!li || !layer)
        return;

    if (!other)
    {
        backmost = FindBackmostLayer(li);
        if (!backmost)
        {
            li->top_layer = layer;
            layer->front = NULL;
            layer->back = NULL;
            return;
        }

        layer->front = backmost;
        layer->back = NULL;
        backmost->back = layer;
        return;
    }

    layer->back = other;
    layer->front = other->front;
    if (other->front)
        other->front->back = layer;
    else
        li->top_layer = layer;
    other->front = layer;
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
    struct Rectangle bounds;
    struct Rectangle clipped;
    struct ClipRect *cr = AllocClipRect(li);
    if (!cr)
        return NULL;

    bounds = layer->bounds;
    if (li)
    {
        if (!IntersectRectangles(&bounds, &li->bounds, &clipped))
        {
            FreeClipRect(li, cr);
            return NULL;
        }

        bounds = clipped;
    }

    cr->bounds = bounds;
    cr->obscured = 0;  /* Visible */
    cr->BitMap = NULL; /* Draw directly to screen */
    cr->Next = NULL;

    return cr;
}

/*
 * Check if rectangle A completely contains rectangle B
 */
static BOOL RectContainsRect(const struct Rectangle *outer, const struct Rectangle *inner)
{
    return (inner->MinX >= outer->MinX && inner->MaxX <= outer->MaxX &&
            inner->MinY >= outer->MinY && inner->MaxY <= outer->MaxY);
}

/*
 * Split a rectangle around an obscuring rectangle.
 * This creates up to 4 new rectangles: top, bottom, left, right portions
 * that are not covered by the obscuring rectangle.
 * Returns a linked list of ClipRects.
 */
static struct ClipRect *SplitRectAroundObscurer(struct Layer_Info *li,
                                                  struct Rectangle *rect,
                                                  struct Rectangle *obscurer)
{
    struct ClipRect *head = NULL;
    struct ClipRect *tail = NULL;
    struct ClipRect *cr;

    /* Calculate the intersection */
    WORD isect_minx = (rect->MinX > obscurer->MinX) ? rect->MinX : obscurer->MinX;
    WORD isect_miny = (rect->MinY > obscurer->MinY) ? rect->MinY : obscurer->MinY;
    WORD isect_maxx = (rect->MaxX < obscurer->MaxX) ? rect->MaxX : obscurer->MaxX;
    WORD isect_maxy = (rect->MaxY < obscurer->MaxY) ? rect->MaxY : obscurer->MaxY;

    /* No intersection? Return NULL */
    if (isect_minx > isect_maxx || isect_miny > isect_maxy)
        return NULL;

    /* Top strip: from rect top to obscurer top */
    if (rect->MinY < isect_miny)
    {
        cr = AllocClipRect(li);
        if (cr)
        {
            cr->bounds.MinX = rect->MinX;
            cr->bounds.MinY = rect->MinY;
            cr->bounds.MaxX = rect->MaxX;
            cr->bounds.MaxY = isect_miny - 1;
            cr->obscured = 0;
            if (tail)
            {
                tail->Next = cr;
                tail = cr;
            }
            else
            {
                head = tail = cr;
            }
        }
    }

    /* Bottom strip: from obscurer bottom to rect bottom */
    if (rect->MaxY > isect_maxy)
    {
        cr = AllocClipRect(li);
        if (cr)
        {
            cr->bounds.MinX = rect->MinX;
            cr->bounds.MinY = isect_maxy + 1;
            cr->bounds.MaxX = rect->MaxX;
            cr->bounds.MaxY = rect->MaxY;
            cr->obscured = 0;
            if (tail)
            {
                tail->Next = cr;
                tail = cr;
            }
            else
            {
                head = tail = cr;
            }
        }
    }

    /* Left strip: from rect left to obscurer left (only in intersection Y range) */
    if (rect->MinX < isect_minx)
    {
        cr = AllocClipRect(li);
        if (cr)
        {
            cr->bounds.MinX = rect->MinX;
            cr->bounds.MinY = isect_miny;
            cr->bounds.MaxX = isect_minx - 1;
            cr->bounds.MaxY = isect_maxy;
            cr->obscured = 0;
            if (tail)
            {
                tail->Next = cr;
                tail = cr;
            }
            else
            {
                head = tail = cr;
            }
        }
    }

    /* Right strip: from obscurer right to rect right (only in intersection Y range) */
    if (rect->MaxX > isect_maxx)
    {
        cr = AllocClipRect(li);
        if (cr)
        {
            cr->bounds.MinX = isect_maxx + 1;
            cr->bounds.MinY = isect_miny;
            cr->bounds.MaxX = rect->MaxX;
            cr->bounds.MaxY = isect_maxy;
            cr->obscured = 0;
            if (tail)
            {
                tail->Next = cr;
                tail = cr;
            }
            else
            {
                head = tail = cr;
            }
        }
    }

    return head;
}

static void RebuildAllClipRects(struct Layer_Info *li)
{
    if (!li)
        return;

    RebuildClipRectsFrom(li->top_layer);
}

/*
 * Check if two rectangles overlap
 */
static BOOL RectsOverlap(const struct Rectangle *r1, const struct Rectangle *r2)
{
    return (r1->MinX <= r2->MaxX && r1->MaxX >= r2->MinX &&
            r1->MinY <= r2->MaxY && r1->MaxY >= r2->MinY);
}

static BOOL IntersectLayerBounds(const struct Layer *layer,
                                 const struct Rectangle *rect,
                                 struct Rectangle *result)
{
    if (!layer || !rect || !result)
        return FALSE;

    return IntersectRectangles(&layer->bounds, rect, result);
}

static struct ClipRect *AppendClipRectIntersection(struct Layer_Info *li,
                                                   struct ClipRect **head,
                                                   struct ClipRect **tail,
                                                   const struct ClipRect *source,
                                                   const struct Rectangle *bounds)
{
    struct ClipRect *copy;

    if (!head || !tail || !source || !bounds)
        return NULL;

    copy = AllocClipRect(li);
    if (!copy)
        return NULL;

    copy->bounds = *bounds;
    copy->obscured = source->obscured;
    copy->BitMap = source->BitMap;
    copy->Next = NULL;

    if (*tail)
        (*tail)->Next = copy;
    else
        *head = copy;

    *tail = copy;
    return copy;
}

static struct ClipRect *ClipClipRectListToRegion(struct Layer_Info *li,
                                                 const struct ClipRect *source_list,
                                                 const struct Region *region)
{
    struct ClipRect *head = NULL;
    struct ClipRect *tail = NULL;
    struct RegionRectangle *rr;
    const struct ClipRect *source;

    if (!source_list || !region || !region->RegionRectangle)
        return NULL;

    for (rr = region->RegionRectangle; rr != NULL; rr = rr->Next)
    {
        for (source = source_list; source != NULL; source = source->Next)
        {
            struct Rectangle intersection;

            if (!IntersectRectangles(&source->bounds, &rr->bounds, &intersection))
                continue;

            if (!AppendClipRectIntersection(li, &head, &tail, source, &intersection))
            {
                FreeClipRectList(li, head);
                return NULL;
            }
        }
    }

    return head;
}

static struct ClipRect *ApplyLayerClipRegion(struct Layer *layer,
                                             struct ClipRect *visible)
{
    struct ClipRect *clipped;

    if (!layer || !visible)
        return visible;

    if (!layer->ClipRegion)
        return visible;

    clipped = ClipClipRectListToRegion(layer->LayerInfo, visible, layer->ClipRegion);
    FreeClipRectList(layer->LayerInfo, visible);
    return clipped;
}

static void RebuildClipRectsFrom(struct Layer *layer)
{
    while (layer)
    {
        RebuildClipRects(layer);
        layer = layer->back;
    }
}

static void RefreshLayerGeometry(struct Layer *layer,
                                 const struct Rectangle *old_bounds,
                                 const struct Rectangle *new_bounds)
{
    struct Layer_Info *li;

    if (!layer)
        return;

    li = layer->LayerInfo;
    if (li && old_bounds)
        DamageExposedAreas(li, layer, old_bounds, new_bounds);

    RebuildClipRectsFrom(layer);
}

/*
 * Rebuild ClipRects for a layer based on overlapping layers.
 * This creates ClipRects for visible portions of the layer,
 * split around any layers that are in front of this one.
 */
static void RebuildClipRects(struct Layer *layer)
{
    struct Layer_Info *li;

    if (!layer)
        return;

    li = layer->LayerInfo;

    /* Free existing ClipRects */
    FreeClipRectList(li, layer->ClipRect);
    layer->ClipRect = NULL;

    DPRINTF(LOG_DEBUG, "_layers: RebuildClipRects() layer bounds [%d,%d]-[%d,%d]\n",
            layer->bounds.MinX, layer->bounds.MinY,
            layer->bounds.MaxX, layer->bounds.MaxY);

    if (layer->Flags & LAYERHIDDEN)
        return;

    /* Start with a single ClipRect covering the whole layer */
    struct ClipRect *initial = CreateSimpleClipRect(li, layer);
    if (!initial)
        return;

    /* Build a list of visible ClipRects by splitting around obscuring layers */
    struct ClipRect *visible = initial;

    /* Check each layer in front of this one */
    struct Layer *front_layer = layer->front;
    while (front_layer)
    {
        /* Skip hidden layers */
        if (front_layer->Flags & LAYERHIDDEN)
        {
            front_layer = front_layer->front;
            continue;
        }

        /* For each visible cliprect, check if it overlaps with this front layer */
        struct ClipRect *cr = visible;
        struct ClipRect *new_visible = NULL;
        struct ClipRect *new_tail = NULL;

        while (cr)
        {
            struct ClipRect *next = cr->Next;

            if (RectsOverlap(&cr->bounds, &front_layer->bounds))
            {
                /* This cliprect is partially or fully obscured */
                
                /* Check if fully obscured */
                if (RectContainsRect(&front_layer->bounds, &cr->bounds))
                {
                    /* Completely obscured - remove this cliprect */
                    FreeClipRect(li, cr);
                }
                else
                {
                    /* Partially obscured - split around the obscurer */
                    struct ClipRect *split = SplitRectAroundObscurer(li, &cr->bounds, &front_layer->bounds);
                    
                    /* Free the original cliprect */
                    FreeClipRect(li, cr);
                    
                    /* Add split pieces to new_visible */
                    if (split)
                    {
                        if (new_tail)
                        {
                            new_tail->Next = split;
                        }
                        else
                        {
                            new_visible = split;
                        }
                        /* Find end of split list */
                        while (split->Next)
                            split = split->Next;
                        new_tail = split;
                    }
                }
            }
            else
            {
                /* Not obscured by this layer - keep it */
                if (new_tail)
                {
                    new_tail->Next = cr;
                }
                else
                {
                    new_visible = cr;
                }
                cr->Next = NULL;
                new_tail = cr;
            }

            cr = next;
        }

        visible = new_visible;

        front_layer = front_layer->front;
    }

    layer->ClipRect = ApplyLayerClipRegion(layer, visible);
    
    DPRINTF(LOG_DEBUG, "_layers: RebuildClipRects() done, ClipRect=0x%08lx\n", (ULONG)layer->ClipRect);
}

/*
 * Add a rectangle to a layer's damage list.
 * This marks the area as needing to be refreshed.
 * For SIMPLE_REFRESH layers, this will trigger an IDCMP_REFRESHWINDOW message.
 */
static void AddDamageToLayer(struct Layer *layer, struct Rectangle *rect)
{
    struct Rectangle clipped;

    DPRINTF(LOG_DEBUG, "_layers: AddDamageToLayer() layer=0x%08lx rect=[%d,%d]-[%d,%d]\n",
            (ULONG)layer, rect->MinX, rect->MinY, rect->MaxX, rect->MaxY);

    if (!layer || !rect)
        return;

    if (!IntersectLayerBounds(layer, rect, &clipped))
        return;

    /* Create damage list if it doesn't exist */
    if (!layer->DamageList)
    {
        layer->DamageList = NewRegion();
        if (!layer->DamageList)
            return;
    }

    /* Add the rectangle to the damage region */
    OrRectRegion(layer->DamageList, &clipped);

    /* Set the LAYERREFRESH flag to indicate this layer needs refresh */
    layer->Flags |= LAYERREFRESH;
}

/*
 * Calculate the intersection of two rectangles.
 * Returns TRUE if they intersect, FALSE otherwise.
 * The intersection is stored in 'result'.
 */
static BOOL IntersectRectangles(const struct Rectangle *r1, const struct Rectangle *r2,
                                struct Rectangle *result)
{
    if (r1->MinX > r2->MaxX || r1->MaxX < r2->MinX ||
        r1->MinY > r2->MaxY || r1->MaxY < r2->MinY)
    {
        return FALSE;  /* No intersection */
    }

    /* Calculate intersection */
    result->MinX = (r1->MinX > r2->MinX) ? r1->MinX : r2->MinX;
    result->MinY = (r1->MinY > r2->MinY) ? r1->MinY : r2->MinY;
    result->MaxX = (r1->MaxX < r2->MaxX) ? r1->MaxX : r2->MaxX;
    result->MaxY = (r1->MaxY < r2->MaxY) ? r1->MaxY : r2->MaxY;

    return TRUE;
}

/*
 * Add damage to a layer for the areas that were previously obscured.
 * This is called when a layer is moved/resized/deleted and exposes
 * areas of layers behind it.
 * 'old_bounds' is the previous position, 'new_bounds' is the new position.
 * For deletion, pass NULL for new_bounds.
 */
static void DamageExposedAreas(struct Layer_Info *li, struct Layer *moved_layer,
                               const struct Rectangle *old_bounds,
                               const struct Rectangle *new_bounds)
{
    DPRINTF(LOG_DEBUG, "_layers: DamageExposedAreas() checking for exposed areas\n");

    if (!li || !old_bounds)
        return;

    /* For each layer behind the moved layer, check if any of their area
     * was previously obscured by old_bounds but is now exposed */
    struct Layer *layer = li->top_layer;
    while (layer)
    {
        /* Skip the moved layer itself and layers in front of it */
        if (layer == moved_layer)
        {
            layer = layer->back;
            continue;
        }

        /* Check if this layer's bounds intersect with the old position */
        struct Rectangle intersection;
        if (IntersectRectangles(&layer->bounds, old_bounds, &intersection))
        {
            /* There was an intersection with the old position.
             * If new_bounds is NULL (layer deleted) or doesn't cover this area,
             * add damage to the exposed layer. */
            if (!new_bounds)
            {
                /* Layer was deleted - entire intersection is exposed */
                AddDamageToLayer(layer, &intersection);
            }
            else
            {
                /* Check if the new position still covers this area */
                struct Rectangle new_intersection;
                if (!IntersectRectangles(&layer->bounds, new_bounds, &new_intersection))
                {
                    /* New position doesn't intersect at all - entire old area is exposed */
                    AddDamageToLayer(layer, &intersection);
                }
                else
                {
                    /* Partial overlap - only damage the areas not covered by new position
                     * This is complex, so for simplicity we damage the entire old intersection
                     * TODO: Optimize to only damage truly exposed areas */
                    if (intersection.MinX != new_intersection.MinX ||
                        intersection.MinY != new_intersection.MinY ||
                        intersection.MaxX != new_intersection.MaxX ||
                        intersection.MaxY != new_intersection.MaxY)
                    {
                        AddDamageToLayer(layer, &intersection);
                    }
                }
            }
        }

        layer = layer->back;
    }
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
    struct Layer *first_backdrop;

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

    first_backdrop = FindFrontmostBackdrop(li);
    if (!li->top_layer)
    {
        li->top_layer = layer;
        layer->front = NULL;
        layer->back = NULL;
    }
    else if (in_front)
    {
        if (IsBackdropLayer(layer))
        {
            if (first_backdrop)
                InsertLayerInFrontOf(li, layer, first_backdrop);
            else
                InsertLayerInFrontOf(li, layer, NULL);
        }
        else
        {
            InsertLayerAtFront(li, layer);
        }
    }
    else
    {
        if (IsBackdropLayer(layer))
            InsertLayerInFrontOf(li, layer, NULL);
        else if (first_backdrop)
            InsertLayerInFrontOf(li, layer, first_backdrop);
        else
            InsertLayerInFrontOf(li, layer, NULL);
    }

    /* Build initial ClipRects */
    RebuildClipRects(layer);

    /* Rebuild ClipRects for all layers behind this one (they may be obscured now) */
    RebuildClipRectsFrom(layer->back);

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

    /* Save bounds for damage tracking before removing */
    struct Rectangle old_bounds = layer->bounds;

    /* Damage exposed areas on layers behind this one */
    DamageExposedAreas(li, layer, &old_bounds, NULL);

    /* Remove from layer list */
    UnlinkLayerFromInfo(li, layer);

    /* Remove from semaphore list */
    Remove((struct Node *)&layer->Lock);

    RebuildAllClipRects(li);

    ReleaseSemaphore(&li->Lock);

    /* Free DamageList if any */
    if (layer->DamageList)
    {
        DisposeRegion(layer->DamageList);
        layer->DamageList = NULL;
    }

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
    struct Layer *target;

    DPRINTF(LOG_DEBUG, "_layers: UpfrontLayer() called layer=0x%08lx\n", (ULONG)layer);

    if (!layer || !layer->LayerInfo)
        return FALSE;

    struct Layer_Info *li = layer->LayerInfo;

    ObtainSemaphore(&li->Lock);

    target = FindFrontmostBackdrop(li);

    if (!IsBackdropLayer(layer))
    {
        if (layer->front == NULL)
        {
            ReleaseSemaphore(&li->Lock);
            return TRUE;
        }

        UnlinkLayerFromInfo(li, layer);
        InsertLayerAtFront(li, layer);
    }
    else
    {
        if (target == layer)
        {
            ReleaseSemaphore(&li->Lock);
            return TRUE;
        }

        UnlinkLayerFromInfo(li, layer);
        InsertLayerInFrontOf(li, layer, target);
    }

    /* Rebuild ClipRects for all layers (z-order changed) */
    RebuildAllClipRects(li);

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
    struct Layer *target;

    DPRINTF(LOG_DEBUG, "_layers: BehindLayer() called layer=0x%08lx\n", (ULONG)layer);

    if (!layer || !layer->LayerInfo)
        return FALSE;

    struct Layer_Info *li = layer->LayerInfo;

    ObtainSemaphore(&li->Lock);

    target = IsBackdropLayer(layer) ? NULL : FindFrontmostBackdrop(li);

    if ((target && layer->back == target) ||
        (!target && FindBackmostLayer(li) == layer))
    {
        ReleaseSemaphore(&li->Lock);
        return TRUE;
    }

    UnlinkLayerFromInfo(li, layer);
    InsertLayerInFrontOf(li, layer, target);

    /* Rebuild ClipRects for all layers (z-order changed) */
    RebuildAllClipRects(li);

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

    if (IsBackdropLayer(layer))
        return FALSE;

    struct Layer_Info *li = layer->LayerInfo;

    ObtainSemaphore(&layer->Lock);

    /* Save old bounds for damage tracking */
    struct Rectangle old_bounds = layer->bounds;

    /* Update bounds */
    layer->bounds.MinX += dx;
    layer->bounds.MinY += dy;
    layer->bounds.MaxX += dx;
    layer->bounds.MaxY += dy;

    RefreshLayerGeometry(layer, li ? &old_bounds : NULL, li ? &layer->bounds : NULL);

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

    if (IsBackdropLayer(layer))
        return FALSE;

    struct Layer_Info *li = layer->LayerInfo;

    ObtainSemaphore(&layer->Lock);

    /* Save old bounds for damage tracking */
    struct Rectangle old_bounds = layer->bounds;

    /* Update bounds */
    layer->bounds.MaxX += dx;
    layer->bounds.MaxY += dy;
    layer->Width = layer->bounds.MaxX - layer->bounds.MinX + 1;
    layer->Height = layer->bounds.MaxY - layer->bounds.MinY + 1;

    RefreshLayerGeometry(layer,
                         (li && (dx < 0 || dy < 0)) ? &old_bounds : NULL,
                         li ? &layer->bounds : NULL);

    ReleaseSemaphore(&layer->Lock);

    return TRUE;
}

/*
 * ScrollLayer - Scroll layer contents (offset -0x48)
 *
 * Per AROS/RKRM: For non-SuperBitMap layers, the scroll offsets are
 * accumulated and the bitmap contents are scrolled via ScrollRaster
 * so the visible content moves. For SuperBitMap layers, we'd need
 * SyncSBitMap/CopySBitMap which we don't implement yet.
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

    if (dx == 0 && dy == 0)
        return;

    ObtainSemaphore(&layer->Lock);

    if (layer->Flags & LAYERSUPER)
    {
        /* SuperBitMap layer: adjust scroll offsets (opposite sign per AROS) */
        layer->Scroll_X -= dx;
        layer->Scroll_Y -= dy;

        /* TODO: SyncSBitMap/CopySBitMap for full SuperBitMap support */
    }
    else
    {
        /* Simple/Smart refresh layer:
         * Scroll the visible bitmap contents via ScrollRaster,
         * then update the scroll offsets */
        if (layer->rp && GfxBase)
        {
            ScrollRaster(layer->rp, dx, dy,
                         layer->bounds.MinX, layer->bounds.MinY,
                         layer->bounds.MaxX, layer->bounds.MaxY);
        }

        layer->Scroll_X += dx;
        layer->Scroll_Y += dy;
    }

    ReleaseSemaphore(&layer->Lock);
}

/* ========================================================================
 * Refresh handling (-0x4e to -0x54)
 * ======================================================================== */

/*
 * BeginUpdate - Begin rendering to damage regions (offset -0x4e)
 *
 * Sets up ClipRects for rendering only to damaged areas.
 * Returns TRUE if there are areas to refresh, FALSE otherwise.
 *
 * Per RKRM/AROS: saves the current ClipRect list and replaces it with
 * ClipRects clipped to the damaged region. The application then draws
 * normally, but rendering is automatically clipped to damaged areas.
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

    /* If there's a damage list, build ClipRects clipped to the damaged region */
    if (layer->DamageList && layer->DamageList->RegionRectangle)
    {
        struct ClipRect *new_head = ClipClipRectListToRegion(layer->LayerInfo,
                                                             layer->ClipRect,
                                                             layer->DamageList);

        if (!new_head)
            return FALSE;

        layer->ClipRect = new_head;
        return TRUE;
    }

    /* Even with no explicit damage, return TRUE to allow drawing */
    return TRUE;
}

/*
 * EndUpdate - End rendering, restore ClipRects (offset -0x54)
 *
 * Per RKRM/AROS: restores the original ClipRect list saved by BeginUpdate,
 * frees the damage-specific ClipRects, and optionally clears the DamageList.
 */
static VOID _layers_EndUpdate ( register struct LayersBase *LayersBase __asm("a6"),
                                register struct Layer      *layer      __asm("a0"),
                                register UWORD              flag       __asm("d0"))
{
    DPRINTF(LOG_DEBUG, "_layers: EndUpdate() called layer=0x%08lx flag=%u\n", (ULONG)layer, flag);

    if (!layer)
        return;

    layer->Flags &= ~LAYERUPDATING;

    /* Free damage ClipRects created by BeginUpdate and restore originals */
    if (layer->Undamaged)
    {
        /* Free the damage-specific ClipRects (if different from the saved ones) */
        if (layer->ClipRect != layer->Undamaged)
        {
            FreeClipRectList(layer->LayerInfo, layer->ClipRect);
        }

        layer->ClipRect = layer->Undamaged;
        layer->Undamaged = NULL;
    }

    /* Clear damage if requested */
    if (flag)
    {
        if (layer->DamageList)
        {
            /* Free DamageList Region via graphics.library DisposeRegion */
            DisposeRegion(layer->DamageList);
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
 *
 * Per RKRM/AROS: For LAYERSMART layers, swaps the bitmap contents between
 * the screen (rp->BitMap at cr->bounds position) and the ClipRect's
 * off-screen backing store (cr->BitMap). This is used during layer
 * operations to save/restore obscured content.
 *
 * For our implementation without full backing store support, this is
 * a no-op for layers without cr->BitMap. When cr->BitMap exists,
 * we swap the contents using BltBitMap.
 */
static VOID _layers_SwapBitsRastPortClipRect ( register struct LayersBase *LayersBase __asm("a6"),
                                               register struct RastPort   *rp         __asm("a0"),
                                               register struct ClipRect   *cr         __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_layers: SwapBitsRastPortClipRect() rp=0x%08lx cr=0x%08lx\n",
            (ULONG)rp, (ULONG)cr);

    if (!rp || !cr || !rp->BitMap)
        return;

    /* Only meaningful if ClipRect has a backing store bitmap */
    if (!cr->BitMap)
        return;

    /* Swap: screen -> temp, backing -> screen, temp -> backing
     * We do this with two BltBitMaps through a temporary allocation */
    {
        WORD width = cr->bounds.MaxX - cr->bounds.MinX + 1;
        WORD height = cr->bounds.MaxY - cr->bounds.MinY + 1;
        WORD depth = rp->BitMap->Depth;
        struct BitMap *temp;

        temp = AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
        if (!temp)
            return;

        InitBitMap(temp, depth, width, height);

        /* Allocate planes for temp bitmap */
        {
            WORD i;
            WORD bytesPerRow = ((width + 15) >> 4) << 1;
            BOOL alloc_ok = TRUE;

            for (i = 0; i < depth; i++)
            {
                temp->Planes[i] = AllocMem(bytesPerRow * height, MEMF_CHIP | MEMF_CLEAR);
                if (!temp->Planes[i])
                {
                    alloc_ok = FALSE;
                    break;
                }
            }

            if (!alloc_ok)
            {
                /* Clean up partial allocation */
                {
                    WORD j;
                    for (j = 0; j < i; j++)
                    {
                        FreeMem(temp->Planes[j], bytesPerRow * height);
                    }
                }
                FreeMem(temp, sizeof(struct BitMap));
                return;
            }

            /* Step 1: screen -> temp */
            BltBitMap(rp->BitMap, cr->bounds.MinX, cr->bounds.MinY,
                      temp, 0, 0, width, height, 0xC0, 0xFF, NULL);

            /* Step 2: backing -> screen */
            BltBitMap(cr->BitMap, 0, 0,
                      rp->BitMap, cr->bounds.MinX, cr->bounds.MinY,
                      width, height, 0xC0, 0xFF, NULL);

            /* Step 3: temp -> backing */
            BltBitMap(temp, 0, 0,
                      cr->BitMap, 0, 0, width, height, 0xC0, 0xFF, NULL);

            /* Free temp bitmap */
            for (i = 0; i < depth; i++)
            {
                FreeMem(temp->Planes[i], bytesPerRow * height);
            }
            FreeMem(temp, sizeof(struct BitMap));
        }
    }
}

/*
 * WhichLayer - Find the layer at coordinates (x, y) (offset -0x84)
 */
static struct Layer * _layers_WhichLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                           register struct Layer_Info *li         __asm("a0"),
                                           register WORD               x          __asm("d0"),
                                           register WORD               y          __asm("d1"))
{
    struct Layer *layer;

    DPRINTF(LOG_DEBUG, "_layers: WhichLayer() x=%d y=%d\n", x, y);

    if (!li)
        return NULL;

    /* Search from front (top) to back (bottom) */
    layer = li->top_layer;
    while (layer)
    {
        /* Skip hidden layers */
        if (!(layer->Flags & LAYERHIDDEN))
        {
            struct ClipRect *cr = layer->ClipRect;

            while (cr)
            {
                if (!cr->obscured &&
                    x >= cr->bounds.MinX && x <= cr->bounds.MaxX &&
                    y >= cr->bounds.MinY && y <= cr->bounds.MaxY)
                {
                    return layer;
                }

                cr = cr->Next;
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
    DPRINTF(LOG_DEBUG, "_layers: FattenLayerInfo() called li=0x%08lx\n", (ULONG)li);

    if (!li)
        return FALSE;

    li->Flags |= NEWLAYERINFO_CALLED;
    return TRUE;
}

/*
 * ThinLayerInfo - Obsolete, for compatibility (offset -0xa2)
 */
static VOID _layers_ThinLayerInfo ( register struct LayersBase *LayersBase __asm("a6"),
                                    register struct Layer_Info *li         __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_layers: ThinLayerInfo() called li=0x%08lx\n", (ULONG)li);

    if (!li)
        return;

    li->Flags &= ~NEWLAYERINFO_CALLED;
}

/* ========================================================================
 * Advanced functions (-0xa8 to -0xf0)
 * ======================================================================== */

/*
 * MoveLayerInFrontOf - Move layer in front of another (offset -0xa8)
 *
 * Per RKRM: Moves layer_to_move so it appears directly in front of other_layer
 * in the z-ordering. Both layers must belong to the same Layer_Info.
 */
static LONG _layers_MoveLayerInFrontOf ( register struct LayersBase *LayersBase    __asm("a6"),
                                         register struct Layer      *layer_to_move __asm("a0"),
                                         register struct Layer      *other_layer   __asm("a1"))
{
    struct Layer_Info *li;
    struct Layer *target;

    DPRINTF(LOG_DEBUG, "_layers: MoveLayerInFrontOf() layer=0x%08lx other=0x%08lx\n",
            (ULONG)layer_to_move, (ULONG)other_layer);

    if (!layer_to_move || !other_layer)
        return FALSE;

    if (layer_to_move == other_layer)
        return TRUE;

    li = layer_to_move->LayerInfo;
    if (!li || li != other_layer->LayerInfo)
        return FALSE;

    if (IsBackdropLayer(layer_to_move) && !IsBackdropLayer(other_layer))
        return FALSE;

    target = other_layer;
    if (!IsBackdropLayer(layer_to_move) && IsBackdropLayer(other_layer))
        target = FindFrontmostBackdrop(li);

    /* Already in the right position? */
    if (layer_to_move->back == target)
        return TRUE;

    ObtainSemaphore(&li->Lock);

    /* Save old bounds for damage tracking */
    struct Rectangle old_bounds = layer_to_move->bounds;

    UnlinkLayerFromInfo(li, layer_to_move);
    InsertLayerInFrontOf(li, layer_to_move, target);

    /* Damage exposed areas */
    DamageExposedAreas(li, layer_to_move, &old_bounds, &layer_to_move->bounds);

    /* Rebuild ClipRects for all layers (z-order changed) */
    RebuildAllClipRects(li);

    ReleaseSemaphore(&li->Lock);

    return TRUE;
}

/*
 * InstallClipRegion - Install user clipping region (offset -0xae)
 *
 * Per RKRM: Installs a user-defined clipping region on a layer.
 * Returns the previously installed region (or NULL).
 * The region further restricts drawing to only the areas within it.
 * After installation, ClipRects are rebuilt to reflect the new clipping.
 */
static struct Region * _layers_InstallClipRegion ( register struct LayersBase *LayersBase __asm("a6"),
                                                   register struct Layer      *layer      __asm("a0"),
                                                   register struct Region     *region     __asm("a1"))
{
    struct Region *old;

    DPRINTF(LOG_DEBUG, "_layers: InstallClipRegion() called layer=0x%08lx region=0x%08lx\n",
            (ULONG)layer, (ULONG)region);

    if (!layer)
        return NULL;

    ObtainSemaphore(&layer->Lock);

    old = layer->ClipRegion;
    layer->ClipRegion = region;

    /* Rebuild ClipRects to incorporate the new region */
    RebuildClipRects(layer);

    ReleaseSemaphore(&layer->Lock);

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

    if (IsBackdropLayer(layer))
        return FALSE;

    struct Layer_Info *li = layer->LayerInfo;

    ObtainSemaphore(&layer->Lock);

    /* Save old bounds for damage tracking */
    struct Rectangle old_bounds = layer->bounds;

    /* Update bounds */
    layer->bounds.MinX += dx;
    layer->bounds.MinY += dy;
    layer->bounds.MaxX += dx + dw;
    layer->bounds.MaxY += dy + dh;
    layer->Width = layer->bounds.MaxX - layer->bounds.MinX + 1;
    layer->Height = layer->bounds.MaxY - layer->bounds.MinY + 1;

    RefreshLayerGeometry(layer, li ? &old_bounds : NULL, li ? &layer->bounds : NULL);

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
 *
 * Per AROS: Sorts the layer's ClipRect linked list based on scroll direction
 * (dx, dy) so that blits happen in the correct order (back-to-front relative
 * to scroll direction) to prevent overwriting source data.
 *
 * Uses insertion sort on the singly-linked list.
 */
static VOID _layers_SortLayerCR ( register struct LayersBase *LayersBase __asm("a6"),
                                  register struct Layer      *layer      __asm("a0"),
                                  register WORD               dx         __asm("d0"),
                                  register WORD               dy         __asm("d1"))
{
    struct ClipRect *sorted_head;
    struct ClipRect *cur;

    DPRINTF(LOG_DEBUG, "_layers: SortLayerCR() layer=0x%08lx dx=%d dy=%d\n",
            (ULONG)layer, dx, dy);

    if (!layer || !layer->ClipRect || !layer->ClipRect->Next)
        return;  /* 0 or 1 element — already sorted */

    /* Insertion sort */
    sorted_head = NULL;
    cur = layer->ClipRect;

    while (cur)
    {
        struct ClipRect *next = cur->Next;
        struct ClipRect *prev = NULL;
        struct ClipRect *scan = sorted_head;
        BOOL insert_before;

        /* Find insertion point */
        while (scan)
        {
            insert_before = FALSE;

            if (dy > 0)
            {
                /* Scrolling down: process top ClipRects first (ascending MinY) */
                if (cur->bounds.MinY < scan->bounds.MinY)
                    insert_before = TRUE;
                else if (cur->bounds.MinY == scan->bounds.MinY && dx > 0 &&
                         cur->bounds.MinX < scan->bounds.MinX)
                    insert_before = TRUE;
                else if (cur->bounds.MinY == scan->bounds.MinY && dx < 0 &&
                         cur->bounds.MaxX > scan->bounds.MaxX)
                    insert_before = TRUE;
            }
            else if (dy < 0)
            {
                /* Scrolling up: process bottom ClipRects first (descending MaxY) */
                if (cur->bounds.MaxY > scan->bounds.MaxY)
                    insert_before = TRUE;
                else if (cur->bounds.MaxY == scan->bounds.MaxY && dx > 0 &&
                         cur->bounds.MinX < scan->bounds.MinX)
                    insert_before = TRUE;
                else if (cur->bounds.MaxY == scan->bounds.MaxY && dx < 0 &&
                         cur->bounds.MaxX > scan->bounds.MaxX)
                    insert_before = TRUE;
            }
            else
            {
                /* dy == 0: horizontal only */
                if (dx > 0 && cur->bounds.MinX < scan->bounds.MinX)
                    insert_before = TRUE;
                else if (dx < 0 && cur->bounds.MaxX > scan->bounds.MaxX)
                    insert_before = TRUE;
            }

            if (insert_before)
                break;

            prev = scan;
            scan = scan->Next;
        }

        /* Insert cur before scan (after prev) */
        if (prev)
        {
            cur->Next = prev->Next;
            prev->Next = cur;
        }
        else
        {
            cur->Next = sorted_head;
            sorted_head = cur;
        }

        cur = next;
    }

    layer->ClipRect = sorted_head;
}

/*
 * DoHookClipRects - Call hook for each ClipRect (offset -0xd8)
 *
 * Per RKRM/AROS: Iterates over every ClipRect in the layer, clips the
 * requested rectangle to each ClipRect's bounds, and calls the hook
 * for each non-empty intersection.
 *
 * The hook receives a layerhookmsg structure containing:
 *   - l_Layer: the layer pointer
 *   - l_bounds: the clipped rectangle (in screen coordinates)
 *   - l_OffsetX/l_OffsetY: logical coordinates for the hook
 *
 * Special hooks:
 *   - LAYERS_NOBACKFILL: do nothing
 *   - LAYERS_BACKFILL: clear the area with BltBitMap (minterm 0 = clear)
 */
static VOID _layers_DoHookClipRects ( register struct LayersBase    *LayersBase __asm("a6"),
                                      register struct Hook          *hook       __asm("a0"),
                                      register struct RastPort      *rp         __asm("a1"),
                                      register const struct Rectangle *rect     __asm("a2"))
{
    struct Layer *layer;
    struct ClipRect *cr;
    struct Rectangle boundrect;

    DPRINTF(LOG_DEBUG, "_layers: DoHookClipRects() hook=0x%08lx rp=0x%08lx\n",
            (ULONG)hook, (ULONG)rp);

    if (!rp || !rect)
        return;

    /* LAYERS_NOBACKFILL means do nothing */
    if (hook == LAYERS_NOBACKFILL)
        return;

    layer = rp->Layer;

    if (!layer)
    {
        /* No layer — apply hook to the entire rectangle directly */
        if (hook == LAYERS_BACKFILL || hook == NULL)
        {
            /* Clear the area */
            if (rp->BitMap)
            {
                WORD width = rect->MaxX - rect->MinX + 1;
                WORD height = rect->MaxY - rect->MinY + 1;
                if (width > 0 && height > 0)
                {
                    BltBitMap(rp->BitMap, rect->MinX, rect->MinY,
                              rp->BitMap, rect->MinX, rect->MinY,
                              width, height, 0x00, 0xFF, NULL);  /* minterm 0 = clear */
                }
            }
        }
        return;
    }

    ObtainSemaphore(&layer->Lock);

    /* Convert rect from layer-relative to screen coordinates */
    boundrect.MinX = rect->MinX + layer->bounds.MinX - layer->Scroll_X;
    boundrect.MinY = rect->MinY + layer->bounds.MinY - layer->Scroll_Y;
    boundrect.MaxX = rect->MaxX + layer->bounds.MinX - layer->Scroll_X;
    boundrect.MaxY = rect->MaxY + layer->bounds.MinY - layer->Scroll_Y;

    /* Clip to layer bounds */
    if (boundrect.MinX < layer->bounds.MinX) boundrect.MinX = layer->bounds.MinX;
    if (boundrect.MinY < layer->bounds.MinY) boundrect.MinY = layer->bounds.MinY;
    if (boundrect.MaxX > layer->bounds.MaxX) boundrect.MaxX = layer->bounds.MaxX;
    if (boundrect.MaxY > layer->bounds.MaxY) boundrect.MaxY = layer->bounds.MaxY;

    /* Check if anything visible */
    if (boundrect.MinX > boundrect.MaxX || boundrect.MinY > boundrect.MaxY)
    {
        ReleaseSemaphore(&layer->Lock);
        return;
    }

    /* Walk all ClipRects */
    cr = layer->ClipRect;
    while (cr)
    {
        struct Rectangle intersection;

        if (IntersectRectangles(&boundrect, &cr->bounds, &intersection))
        {
            /* Skip hidden ClipRects for LAYERSIMPLE layers (no backing store) */
            if (cr->obscured && (layer->Flags & LAYERSIMPLE))
            {
                cr = cr->Next;
                continue;
            }

            /* Apply the hook to this intersection */
            if (hook == LAYERS_BACKFILL || hook == NULL)
            {
                /* Default backfill: clear the area */
                if (rp->BitMap)
                {
                    WORD w = intersection.MaxX - intersection.MinX + 1;
                    WORD h = intersection.MaxY - intersection.MinY + 1;
                    if (w > 0 && h > 0)
                    {
                        BltBitMap(rp->BitMap, intersection.MinX, intersection.MinY,
                                  rp->BitMap, intersection.MinX, intersection.MinY,
                                  w, h, 0x00, 0xFF, NULL);
                    }
                }
            }
            /* Custom hooks would be called here, but we don't support the
             * full hook calling convention in ROM code yet */
        }

        cr = cr->Next;
    }

    ReleaseSemaphore(&layer->Lock);
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
    struct Layer_Info *li;

    DPRINTF(LOG_DEBUG, "_layers: HideLayer() called\n");

    if (!layer)
        return FALSE;

    li = layer->LayerInfo;
    layer->Flags |= LAYERHIDDEN;

    if (li)
        RebuildAllClipRects(li);

    return TRUE;
}

/*
 * ShowLayer - Make layer visible (offset -0xea)
 *
 * Per RKRM: Makes a hidden layer visible. If in_front_of is not NULL,
 * the layer is repositioned directly in front of that layer.
 */
static LONG _layers_ShowLayer ( register struct LayersBase *LayersBase __asm("a6"),
                                register struct Layer      *layer      __asm("a0"),
                                register struct Layer      *in_front_of __asm("a1"))
{
    struct Layer_Info *li;

    DPRINTF(LOG_DEBUG, "_layers: ShowLayer() called layer=0x%08lx in_front_of=0x%08lx\n",
            (ULONG)layer, (ULONG)in_front_of);

    if (!layer)
        return FALSE;

    li = layer->LayerInfo;
    layer->Flags &= ~LAYERHIDDEN;

    /* If in_front_of is specified, reposition the layer */
    if (in_front_of == LAYER_FRONTMOST)
    {
        return _layers_UpfrontLayer(LayersBase, 0, layer);
    }
    else if (in_front_of == LAYER_BACKMOST)
    {
        return _layers_BehindLayer(LayersBase, 0, layer);
    }
    else if (in_front_of && li)
    {
        _layers_MoveLayerInFrontOf(LayersBase, layer, in_front_of);
    }
    else if (li)
    {
        /* Just rebuild ClipRects since visibility changed */
        RebuildAllClipRects(li);
    }

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
    RebuildAllClipRects(li);

    return TRUE;
}

static struct Layer *CreateLayerTagListInternal(struct LayersBase *LayersBase,
                                                struct Layer_Info *li,
                                                struct BitMap *bm,
                                                LONG x0,
                                                LONG y0,
                                                LONG x1,
                                                LONG y1,
                                                LONG flags,
                                                struct TagItem *tagList)
{
    struct Hook *hook;
    struct BitMap *super_bitmap = NULL;
    struct Layer *in_front_of = NULL;
    struct Layer *behind = NULL;
    struct Layer *layer;
    struct TagItem *tag;
    struct TagItem *state = tagList;
    APTR window = NULL;
    BOOL hidden = FALSE;

    if (!li || !bm)
        return NULL;

    hook = (struct Hook *)li->resPtr1;
    if (!hook)
        hook = LAYERS_BACKFILL;

    while ((tag = NextTagItem(&state)))
    {
        switch (tag->ti_Tag)
        {
            case LA_BackfillHook:
                hook = (struct Hook *)tag->ti_Data;
                break;

            case LA_SuperBitMap:
                super_bitmap = (struct BitMap *)tag->ti_Data;
                break;

            case LA_WindowPtr:
                window = (APTR)tag->ti_Data;
                break;

            case LA_Hidden:
                hidden = (BOOL)tag->ti_Data;
                break;

            case LA_InFrontOf:
                in_front_of = (struct Layer *)tag->ti_Data;
                break;

            case LA_Behind:
                behind = (struct Layer *)tag->ti_Data;
                break;

            case LA_ChildOf:
            case LA_ShapeRegion:
            case LA_ShapeHook:
                if (tag->ti_Data)
                    return NULL;
                break;

            default:
                break;
        }
    }

    if (in_front_of && behind)
        return NULL;

    if ((flags & LAYERSUPER) && !super_bitmap)
        return NULL;

    layer = CreateLayerInternal(LayersBase, li, bm, x0, y0, x1, y1, flags,
                                hook, super_bitmap, TRUE);
    if (!layer)
        return NULL;

    layer->Window = window;

    if (in_front_of)
    {
        if (!_layers_MoveLayerInFrontOf(LayersBase, layer, in_front_of))
            goto failed;
    }
    else if (behind)
    {
        if (behind->back)
        {
            if (!_layers_MoveLayerInFrontOf(LayersBase, layer, behind->back))
                goto failed;
        }
        else if (!_layers_BehindLayer(LayersBase, 0, layer))
        {
            goto failed;
        }
    }

    if (hidden && !_layers_HideLayer(LayersBase, layer))
        goto failed;

    return layer;

failed:
    _layers_DeleteLayer(LayersBase, 0, layer);
    return NULL;
}

static struct Layer * _layers_CreateLayerTagList ( register struct LayersBase *LayersBase __asm("a6"),
                                                   register struct Layer_Info *li         __asm("a0"),
                                                   register struct BitMap     *bm         __asm("a1"),
                                                   register LONG               x0         __asm("d0"),
                                                   register LONG               y0         __asm("d1"),
                                                   register LONG               x1         __asm("d2"),
                                                   register LONG               y1         __asm("d3"),
                                                   register LONG               flags      __asm("d4"),
                                                   register struct TagItem    *tagList    __asm("a2"))
{
    DPRINTF(LOG_DEBUG, "_layers: CreateLayerTagList() called\n");
    return CreateLayerTagListInternal(LayersBase, li, bm, x0, y0, x1, y1, flags, tagList);
}

static LONG _layers_AddLayerInfoTag ( register struct LayersBase *LayersBase __asm("a6"),
                                      register struct Layer_Info *li         __asm("a0"),
                                      register Tag                tag        __asm("d0"),
                                      register ULONG              data       __asm("d1"))
{
    DPRINTF(LOG_DEBUG, "_layers: AddLayerInfoTag() called tag=0x%08lx\n", (ULONG)tag);

    if (!li)
        return FALSE;

    switch (tag)
    {
        case TAG_DONE:
        case TAG_IGNORE:
            return TRUE;

        case LA_BackfillHook:
            li->resPtr1 = (APTR)data;
            return TRUE;

        default:
            return FALSE;
    }
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
    _layers_CreateLayerTagList,       // -246 (0xf6)
    _layers_AddLayerInfoTag,          // -252 (0xfc)

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
