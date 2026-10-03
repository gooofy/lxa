/*
 * Probe (Phase 222c): layers.library refresh behaviour - backfill hooks
 * (per layer and Layer_Info, LAYERS_NOBACKFILL), SIMPLE/SMART/SUPER
 * layers through move/depth/size/delete, BeginUpdate/EndUpdate,
 * EraseRect/ScrollRaster in layers, ScrollLayer, SyncSBitMap/CopySBitMap.
 * Prints the backfill hook calls, the damage state and bitmap hashes.
 */
#include <utility/hooks.h>
#include "layerprobe.h"

#define W 160
#define H 100

static struct gp_bm g, sb;
static struct RastPort srp;

struct BackFillMsg
{
    struct Layer    *Layer;
    struct Rectangle Bounds;
    LONG             OffsetX;
    LONG             OffsetY;
};

#define LOGMAX 64
static struct {
    WORD layer, minx, miny, maxx, maxy;
    LONG ox, oy;
    WORD hook;
} hooklog[LOGMAX];
static WORD nlog;

static ULONG hookfunc(register struct Hook *h __asm("a0"), register struct RastPort *rp __asm("a2"),
                      register struct BackFillMsg *m __asm("a1"))
{
    struct RastPort tmp;
    if (nlog < LOGMAX) {
        hooklog[nlog].layer = (WORD)lp_index(m->Layer);
        hooklog[nlog].minx = m->Bounds.MinX;
        hooklog[nlog].miny = m->Bounds.MinY;
        hooklog[nlog].maxx = m->Bounds.MaxX;
        hooklog[nlog].maxy = m->Bounds.MaxY;
        hooklog[nlog].ox = m->OffsetX;
        hooklog[nlog].oy = m->OffsetY;
        hooklog[nlog].hook = (WORD)(ULONG)h->h_Data;
        nlog++;
    }
    /* fill with a pen derived from the hook, on the rastport's bitmap
     * without its layer (as the RKRM example does) */
    tmp = *rp;
    tmp.Layer = NULL;
    SetAPen(&tmp, (UBYTE)((ULONG)h->h_Data & 3));
    SetDrMd(&tmp, JAM2);
    RectFill(&tmp, m->Bounds.MinX, m->Bounds.MinY, m->Bounds.MaxX, m->Bounds.MaxY);
    return 0;
}

static struct Hook hook1 = {{0, 0}, (ULONG (*)())hookfunc, 0, (APTR)1};
static struct Hook hook2 = {{0, 0}, (ULONG (*)())hookfunc, 0, (APTR)2};

/* the hook calls of each hook, as the region they cover in layer
 * coordinates (OffsetX/Y + size): how 3.1 splits the area into calls and
 * which bitmap (screen or backing store) each call targets is not compared */
static void flush_log(const char *label)
{
    WORD i, h;
    probe_s(label);
    probe_ch('\n');
    for (h = 1; h <= 2; h++) {
        lp_clear();
        for (i = 0; i < nlog; i++)
            if (hooklog[i].hook == h)
                lp_add((WORD)hooklog[i].ox, (WORD)hooklog[i].oy,
                       (WORD)(hooklog[i].ox + hooklog[i].maxx - hooklog[i].minx),
                       (WORD)(hooklog[i].oy + hooklog[i].maxy - hooklog[i].miny));
        lp_print_region(h == 1 ? "hook1" : "hook2");
    }
    nlog = 0;
}

static void paint(int i, UBYTE pen)
{
    struct RastPort *rp = lp_layers[i]->rp;
    WORD w = (WORD)(lp_layers[i]->bounds.MaxX - lp_layers[i]->bounds.MinX);
    WORD h = (WORD)(lp_layers[i]->bounds.MaxY - lp_layers[i]->bounds.MinY);
    SetAPen(rp, pen);
    SetDrMd(rp, JAM2);
    RectFill(rp, 0, 0, w, h);
    SetAPen(rp, (UBYTE)(pen ^ 3));
    Move(rp, 0, 0);
    Draw(rp, w, h);
    Move(rp, w, 0);
    Draw(rp, 0, h);
}

static void state(const char *label)
{
    flush_log(label);
    gp_verbose = gp_filter && gp_contains(label, gp_filter);
    gp_hash("  screen", &g);
    lp_dump_all(label);
}

int main(int argc, char **argv)
{
    struct Layer_Info *li;
    struct Hook *old;
    LONG rc;

    gp_args(argc, argv);
    LayersBase = OpenLibrary((STRPTR)"layers.library", 39);
    if (!LayersBase || !gp_alloc(&g, W, H, 2) || !gp_alloc(&sb, 80, 60, 2))
        return 20;
    gp_rp(&srp, &g);
    SetRast(&srp, 3);
    li = NewLayerInfo();
    if (!li)
        return 20;

    lp_layers[0] = CreateUpfrontHookLayer(li, &g.bm, 0, 0, W - 1, H - 1, LAYERSIMPLE | LAYERBACKDROP, &hook1, NULL);
    state("backdrop created");
    lp_layers[1] = CreateUpfrontLayer(li, &g.bm, 10, 10, 89, 59, LAYERSIMPLE, NULL);
    state("simple created");
    lp_layers[2] = CreateUpfrontHookLayer(li, &g.bm, 40, 30, 139, 79, LAYERSMART, &hook2, NULL);
    state("smart created");
    lp_layers[3] = CreateUpfrontLayer(li, &g.bm, 100, 5, 150, 40, LAYERSUPER, &sb.bm);
    state("super created");

    paint(1, 1);
    paint(2, 2);
    paint(3, 1);
    paint(0, 0);
    state("painted");

    rc = MoveLayer(0, lp_layers[2], -25, 15);
    P_LONG("MoveLayer smart", rc);
    state("smart moved");

    rc = BehindLayer(0, lp_layers[1]);
    P_LONG("BehindLayer simple", rc);
    state("simple behind");

    rc = UpfrontLayer(0, lp_layers[1]);
    P_LONG("UpfrontLayer simple", rc);
    state("simple upfront");

    rc = BeginUpdate(lp_layers[1]);
    P_LONG("BeginUpdate", rc);
    paint(1, 3);
    EndUpdate(lp_layers[1], TRUE);
    state("after update");

    rc = MoveLayer(0, lp_layers[1], 40, 20);
    P_LONG("MoveLayer simple", rc);
    state("simple moved");

    rc = SizeLayer(0, lp_layers[2], 20, 4);
    P_LONG("SizeLayer smart bigger", rc);
    state("smart sized");

    EraseRect(lp_layers[1]->rp, 2, 2, 30, 20);
    EraseRect(lp_layers[2]->rp, 5, 5, 40, 30);
    state("EraseRect");

    SetBPen(lp_layers[1]->rp, 2);
    ScrollRaster(lp_layers[1]->rp, 7, 3, 0, 0, 79, 49);
    SetBPen(lp_layers[2]->rp, 1);
    ScrollRaster(lp_layers[2]->rp, -5, 4, 0, 0, 119, 59);
    state("ScrollRaster");

    old = InstallLayerHook(lp_layers[1], LAYERS_NOBACKFILL);
    P_BOOL("InstallLayerHook old default", old == LAYERS_BACKFILL);
    rc = MoveLayer(0, lp_layers[3], -30, 30);
    P_LONG("MoveLayer super", rc);
    state("super moved");

    ScrollLayer(0, lp_layers[3], 10, 5);
    state("ScrollLayer");
    SyncSBitMap(lp_layers[3]);
    gp_hash("  super bitmap after SyncSBitMap", &sb);
    SetRast(&srp, 3);
    CopySBitMap(lp_layers[3]);
    gp_hash("  screen after SetRast+CopySBitMap", &g);

    /* (3.1 hangs walking the super layer's ClipRects after the next
     * DeleteLayer, so it goes first) */
    rc = DeleteLayer(0, lp_layers[3]);
    lp_layers[3] = NULL;
    P_LONG("DeleteLayer super", rc);
    state("super deleted");

    old = InstallLayerInfoHook(li, &hook2);
    P_BOOL("InstallLayerInfoHook old default", old == LAYERS_BACKFILL);
    rc = DeleteLayer(0, lp_layers[2]);
    lp_layers[2] = NULL;
    P_LONG("DeleteLayer smart", rc);
    state("smart deleted");

    {
        int i;
        for (i = LP_MAX - 1; i >= 0; i--)
            if (lp_layers[i]) {
                DeleteLayer(0, lp_layers[i]);
                lp_layers[i] = NULL;
            }
    }
    flush_log("all deleted");
    DisposeLayerInfo(li);
    gp_free(&sb);
    gp_free(&g);
    CloseLibrary(LayersBase);
    return 0;
}
