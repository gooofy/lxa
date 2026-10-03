/*
 * layerprobe.h - helpers for the layers.library conformance probes
 * (roadmap Phase 222c).
 *
 * How a layer's area is cut into ClipRects is an implementation detail;
 * what programs rely on is which parts of the layer are visible, which
 * are obscured with or without backing store, and what is damaged.  The
 * probes print those as canonical regions (horizontal bands of x runs, in
 * layer coordinates) built from the ClipRect list and the DamageList, so
 * the comparison does not depend on the decomposition.
 */
#ifndef LXA_LAYERPROBE_H
#define LXA_LAYERPROBE_H

#include <graphics/layers.h>
#include <graphics/clip.h>
#include <graphics/regions.h>
#include <clib/layers_protos.h>
#include <inline/layers.h>
#include "graphics/gfxprobe.h"

struct Library *LayersBase;

#define LP_MAX 8
static struct Layer *lp_layers[LP_MAX];

#define LP_W 200
#define LP_H 128
static UBYTE lp_mask[LP_H][LP_W / 8];

static int lp_index(struct Layer *l)
{
    int i;
    for (i = 0; i < LP_MAX; i++)
        if (lp_layers[i] && lp_layers[i] == l)
            return i;
    return -1;
}

static void lp_clear(void)
{
    WORD y, x;
    for (y = 0; y < LP_H; y++)
        for (x = 0; x < LP_W / 8; x++)
            lp_mask[y][x] = 0;
}

/* add a rectangle (layer coordinates, clipped to the mask) */
static void lp_add(WORD x0, WORD y0, WORD x1, WORD y1)
{
    WORD x, y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= LP_W) x1 = LP_W - 1;
    if (y1 >= LP_H) y1 = LP_H - 1;
    if (x0 > x1)
        return;
    for (y = y0; y <= y1; y++) {
        WORD b0 = (WORD)(x0 >> 3), b1 = (WORD)(x1 >> 3);
        UBYTE m0 = (UBYTE)(0xff >> (x0 & 7)), m1 = (UBYTE)(0xff << (7 - (x1 & 7)));
        if (b0 == b1)
            lp_mask[y][b0] |= (UBYTE)(m0 & m1);
        else {
            lp_mask[y][b0] |= m0;
            for (x = (WORD)(b0 + 1); x < b1; x++)
                lp_mask[y][x] = 0xff;
            lp_mask[y][b1] |= m1;
        }
    }
}

static int lp_bit(WORD x, WORD y)
{
    return (lp_mask[y][x >> 3] >> (7 - (x & 7))) & 1;
}

static int lp_rows_equal(WORD a, WORD b)
{
    WORD x;
    for (x = 0; x < LP_W / 8; x++)
        if (lp_mask[a][x] != lp_mask[b][x])
            return 0;
    return 1;
}

/* print the mask as bands: " y0-y1:x0-x1,x2-x3" */
static void lp_print_region(const char *label)
{
    WORD y = 0;
    probe_s("   ");
    probe_s(label);
    probe_ch(':');
    while (y < LP_H) {
        WORD y1 = y, x = 0;
        int any = 0;
        for (x = 0; x < LP_W / 8; x++)
            if (lp_mask[y][x])
                any = 1;
        if (!any) {
            y++;
            continue;
        }
        while (y1 + 1 < LP_H && lp_rows_equal(y, (WORD)(y1 + 1)))
            y1++;
        probe_ch(' ');
        probe_dec(y);
        probe_ch('-');
        probe_dec(y1);
        probe_ch(':');
        any = 0;
        for (x = 0; x < LP_W; x++) {
            if (!(x & 7) && !lp_mask[y][x >> 3]) {
                x += 7;
                continue;
            }
            if (lp_bit(x, y) && (x == 0 || !lp_bit((WORD)(x - 1), y))) {
                WORD xe = x;
                while (xe + 1 < LP_W && lp_bit((WORD)(xe + 1), y))
                    xe++;
                if (any)
                    probe_ch(',');
                probe_dec(x);
                probe_ch('-');
                probe_dec(xe);
                any = 1;
            }
        }
        y = (WORD)(y1 + 1);
    }
    probe_ch('\n');
}

/* layer line, then its visible area, the obscured area with backing
 * store (ClipRect.BitMap) and without, and its damage region */
static void lp_dump(const char *label, struct Layer *l)
{
    struct ClipRect *cr;
    struct RegionRectangle *rr;
    WORD lx = l->bounds.MinX, ly = l->bounds.MinY;
    int pass;

    probe_s(label);
    probe_s(" L");
    probe_dec(lp_index(l));
    probe_s(" bounds ");
    probe_dec(l->bounds.MinX); probe_ch(',');
    probe_dec(l->bounds.MinY); probe_ch('-');
    probe_dec(l->bounds.MaxX); probe_ch(',');
    probe_dec(l->bounds.MaxY);
    probe_s(" refresh ");
    probe_s((l->Flags & LAYERREFRESH) ? "yes" : "no");
    probe_s(" front L");
    probe_dec(l->front ? lp_index(l->front) : -1);
    probe_s(" back L");
    probe_dec(l->back ? lp_index(l->back) : -1);
    probe_ch('\n');
    for (pass = 0; pass < 3; pass++) {
        lp_clear();
        for (cr = l->ClipRect; cr; cr = cr->Next) {
            int kind = !cr->obscured ? 0 : cr->BitMap ? 1 : 2;
            if (kind == pass)
                lp_add((WORD)(cr->bounds.MinX - lx), (WORD)(cr->bounds.MinY - ly),
                       (WORD)(cr->bounds.MaxX - lx), (WORD)(cr->bounds.MaxY - ly));
        }
        lp_print_region(pass == 0 ? "visible" : pass == 1 ? "backing store" : "obscured");
    }
    lp_clear();
    if (l->DamageList)
        for (rr = l->DamageList->RegionRectangle; rr; rr = rr->Next)
            lp_add((WORD)(rr->bounds.MinX + l->DamageList->bounds.MinX),
                   (WORD)(rr->bounds.MinY + l->DamageList->bounds.MinY),
                   (WORD)(rr->bounds.MaxX + l->DamageList->bounds.MinX),
                   (WORD)(rr->bounds.MaxY + l->DamageList->bounds.MinY));
    lp_print_region("damage");
}

static void lp_dump_all(const char *label)
{
    int i;
    P_SECTION(label);
    for (i = 0; i < LP_MAX; i++)
        if (lp_layers[i])
            lp_dump(" ", lp_layers[i]);
}

#endif
