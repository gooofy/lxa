/*
 * gfxprobe.h - helpers for the graphics/layers conformance probes
 * (roadmap Phase 222b/c).
 *
 * Off-screen bitmaps are built with InitBitMap() + AllocRaster() so the
 * planar layout (BytesPerRow = width rounded up to 16 bits) is the same on
 * every system; only the visible width of each row is hashed.  Running a
 * probe with an argument prints every plane as rows of hex in the sections
 * whose name contains it - that is how a divergence is located.
 */
#ifndef LXA_GFXPROBE_H
#define LXA_GFXPROBE_H

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/gfxmacros.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include "probe.h"

extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;

static int gp_verbose;
static const char *gp_filter;   /* argv[1]: dump the sections containing it */

static int gp_contains(const char *s, const char *sub)
{
    const char *a, *b;
    for (; *s; s++) {
        for (a = s, b = sub; *a && *b && *a == *b; a++, b++)
            ;
        if (!*b)
            return 1;
    }
    return !*sub;
}

/* section header; enables hex dumps when it matches the filter argument */
static void gp_section(const char *name)
{
    P_SECTION(name);
    gp_verbose = gp_filter && gp_contains(name, gp_filter);
}

static void gp_args(int argc, char **argv)
{
    gp_filter = argc > 1 ? argv[1] : NULL;
}

struct gp_bm
{
    struct BitMap bm;
    WORD width, height;
};

/* blitter sources (templates, masks, patterns) must be in chip memory:
 * on the reference the program's data hunk is in fast memory */
static APTR gp_chip_mem[16];
static ULONG gp_chip_size[16];

static APTR gp_chip(const void *src, ULONG size)
{
    WORD i;
    for (i = 0; i < 16; i++)
        if (!gp_chip_mem[i]) {
            UBYTE *p = AllocMem(size, MEMF_CHIP | MEMF_CLEAR);
            ULONG k;
            if (p && src)
                for (k = 0; k < size; k++)
                    p[k] = ((const UBYTE *)src)[k];
            gp_chip_mem[i] = p;
            gp_chip_size[i] = size;
            return p;
        }
    return NULL;
}

static void gp_chip_free(void)
{
    WORD i;
    for (i = 0; i < 16; i++)
        if (gp_chip_mem[i]) {
            FreeMem(gp_chip_mem[i], gp_chip_size[i]);
            gp_chip_mem[i] = NULL;
        }
}

static void gp_clear(struct gp_bm *g);

static BOOL gp_alloc(struct gp_bm *g, WORD w, WORD h, WORD depth)
{
    WORD i;
    InitBitMap(&g->bm, depth, w, h);
    g->width = w;
    g->height = h;
    for (i = 0; i < 8; i++)
        g->bm.Planes[i] = NULL;
    for (i = 0; i < depth; i++) {
        g->bm.Planes[i] = AllocRaster(w, h);
        if (!g->bm.Planes[i])
            return FALSE;
    }
    gp_clear(g);
    return TRUE;
}

static void gp_clear(struct gp_bm *g)
{
    WORD i;
    LONG n = (LONG)g->bm.BytesPerRow * g->bm.Rows;
    for (i = 0; i < g->bm.Depth; i++) {
        UBYTE *p = g->bm.Planes[i];
        LONG k;
        if (p)
            for (k = 0; k < n; k++)
                p[k] = 0;
    }
}

static void gp_free(struct gp_bm *g)
{
    WORD i;
    for (i = 0; i < g->bm.Depth; i++)
        if (g->bm.Planes[i])
            FreeRaster(g->bm.Planes[i], g->width, g->height);
}

static void gp_rp(struct RastPort *rp, struct gp_bm *g)
{
    InitRastPort(rp);
    rp->BitMap = &g->bm;
}

static ULONG gp_plane_hash(struct gp_bm *g, WORD plane)
{
    ULONG h = 2166136261UL;
    WORD y, x, bytes = (g->width + 7) >> 3;
    for (y = 0; y < g->height; y++) {
        const UBYTE *row = g->bm.Planes[plane] + (LONG)y * g->bm.BytesPerRow;
        for (x = 0; x < bytes; x++) {
            UBYTE b = row[x];
            /* mask the bits beyond the width in the last byte */
            if (x == bytes - 1 && (g->width & 7))
                b &= (UBYTE)(0xff << (8 - (g->width & 7)));
            h ^= b;
            h *= 16777619UL;
        }
    }
    return h;
}

static void gp_dump(struct gp_bm *g)
{
    WORD p, y, x, bytes = (g->width + 7) >> 3;
    static const char hx[] = "0123456789abcdef";
    for (p = 0; p < g->bm.Depth; p++) {
        probe_s("  plane ");
        probe_dec(p);
        probe_ch('\n');
        for (y = 0; y < g->height; y++) {
            const UBYTE *row = g->bm.Planes[p] + (LONG)y * g->bm.BytesPerRow;
            probe_s("   ");
            if (y < 10)
                probe_ch(' ');
            probe_dec(y);
            probe_ch(' ');
            for (x = 0; x < bytes; x++) {
                probe_ch(hx[row[x] >> 4]);
                probe_ch(hx[row[x] & 15]);
            }
            probe_ch('\n');
        }
    }
}

/* one line: "<label> = p0 0x.. p1 0x.." (+ hex dump when verbose) */
static void gp_hash(const char *label, struct gp_bm *g)
{
    WORD p;
    probe_s(label);
    probe_s(" =");
    for (p = 0; p < g->bm.Depth; p++) {
        probe_s(" p");
        probe_dec(p);
        probe_ch(' ');
        probe_hex(gp_plane_hash(g, p), 8);
    }
    probe_ch('\n');
    if (gp_verbose)
        gp_dump(g);
}

/* pixel values of a row segment, as a string of pen digits */
static void gp_row(const char *label, struct RastPort *rp, WORD y, WORD x0, WORD x1)
{
    WORD x;
    probe_s(label);
    probe_s(" = ");
    for (x = x0; x <= x1; x++) {
        LONG v = ReadPixel(rp, x, y);
        probe_ch(v < 0 ? '-' : v < 10 ? (char)('0' + v) : (char)('a' + v - 10));
    }
    probe_ch('\n');
}

#endif
