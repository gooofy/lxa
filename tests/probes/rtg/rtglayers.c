/*
 * Probe (Phase 241): layers on RTG screens (Picasso96 2.0, rtg profile) -
 * SMART_REFRESH backing store in the screen's pixel format: a window
 * obscured by another, drawn into while obscured, uncovered by closing the
 * other window; a SIMPLE_REFRESH window's damage list after the same; for
 * a 16 bit (R5G6B5PC) and a 32 bit (B8G8R8A8) screen.  Pixels are read raw
 * from the screen bitmap.
 *
 * rdd: profile rtg
 */
#define LXA_P96_INLINES
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/clip.h>
#include <graphics/regions.h>
#include <graphics/layers.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <lxa_p96.h>
#include "probe.h"

struct Library *P96Base = NULL;

static ULONG pix(struct BitMap *bm, int x, int y)
{
    UBYTE *m = (UBYTE *)p96GetBitMapAttr(bm, P96BMA_MEMORY);
    ULONG bpr = p96GetBitMapAttr(bm, P96BMA_BYTESPERROW);
    ULONG bpp = p96GetBitMapAttr(bm, P96BMA_BYTESPERPIXEL);
    UBYTE *p = m + y * bpr + x * bpp;
    if (bpp == 1) return p[0];
    if (bpp == 2) return (p[0] << 8) | p[1];
    if (bpp == 3) return (p[0] << 16) | (p[1] << 8) | p[2];
    return ((ULONG)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static void P_PIX(const char *l, struct BitMap *bm, int x, int y)
{
    probe_s(l); probe_s(" = "); probe_hex(pix(bm, x, y), 8); probe_ch('\n');
}

static struct Window *win(struct Screen *s, int x, int y, int w, int h, ULONG refresh)
{
    return OpenWindowTags(NULL, WA_CustomScreen, s, WA_Left, x, WA_Top, y, WA_Width, w, WA_Height, h,
                          WA_Borderless, TRUE, WA_Activate, FALSE, WA_RMBTrap, TRUE,
                          WA_SmartRefresh, refresh == WFLG_SMART_REFRESH,
                          WA_SimpleRefresh, refresh == WFLG_SIMPLE_REFRESH, TAG_DONE);
}

static void points(const char *n, struct BitMap *bm)
{
    P_SECTION(n);
    P_PIX("a only", bm, 20, 30);        /* window A, never covered */
    P_PIX("overlap", bm, 80, 60);       /* A under B */
    P_PIX("overlap drawn", bm, 100, 80);/* A under B, drawn while covered */
    P_PIX("b only", bm, 150, 90);       /* B outside A */
}

static void run(ULONG id, ULONG depth)
{
    struct Screen *s = OpenScreenTags(NULL, SA_DisplayID, id, SA_Depth, depth, SA_Width, 640, SA_Height, 480,
                                      SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Behind, TRUE, TAG_DONE);
    struct BitMap *bm;
    struct Window *a, *b;

    P_SECTION("screen");
    P_HEX("id", id);
    P_NULL("screen", s);
    if (!s)
        return;
    bm = s->RastPort.BitMap;
    P_LONG("fmt", p96GetBitMapAttr(bm, P96BMA_RGBFORMAT));
    p96RectFill(&s->RastPort, 0, 0, 299, 199, 0x00000000);

    /* SMART_REFRESH: backing store */
    a = win(s, 10, 20, 120, 80, WFLG_SMART_REFRESH);
    P_NULL("win a", a);
    if (a) {
        p96RectFill(a->RPort, 0, 0, 119, 79, 0x00ff0000);
        b = win(s, 60, 50, 120, 60, WFLG_SMART_REFRESH);
        P_NULL("win b", b);
        if (b) {
            p96RectFill(b->RPort, 0, 0, 119, 59, 0x0000ff00);
            /* into A's obscured part: lands in the backing store */
            p96RectFill(a->RPort, 85, 55, 105, 70, 0x000000ff);
            P_HEX("a read obscured", p96ReadPixel(a->RPort, 90, 60));
            points("smart covered", bm);
            CloseWindow(b);
            points("smart uncovered", bm);
        }
        CloseWindow(a);
    }

    /* SIMPLE_REFRESH: no backing store, the uncovered part is damaged */
    p96RectFill(&s->RastPort, 0, 0, 299, 199, 0x00000000);
    a = win(s, 10, 20, 120, 80, WFLG_SIMPLE_REFRESH);
    P_NULL("win a simple", a);
    if (a) {
        p96RectFill(a->RPort, 0, 0, 119, 79, 0x00ff0000);
        b = win(s, 60, 50, 120, 60, WFLG_SMART_REFRESH);
        P_NULL("win b", b);
        if (b) {
            p96RectFill(b->RPort, 0, 0, 119, 59, 0x0000ff00);
            p96RectFill(a->RPort, 85, 55, 105, 70, 0x000000ff);
            points("simple covered", bm);
            CloseWindow(b);
            P_LONG("damaged", (a->WLayer->Flags & LAYERREFRESH) != 0);
            if (a->WLayer->DamageList && a->WLayer->DamageList->RegionRectangle) {
                struct Rectangle *r = &a->WLayer->DamageList->bounds;
                P_LONG("damage minx", r->MinX);
                P_LONG("damage miny", r->MinY);
                P_LONG("damage maxx", r->MaxX);
                P_LONG("damage maxy", r->MaxY);
            }
            /* fill the damage as an application would on IDCMP_REFRESHWINDOW */
            BeginRefresh(a);
            p96RectFill(a->RPort, 0, 0, 119, 79, 0x00ffff00);
            EndRefresh(a, TRUE);
            points("simple refreshed", bm);
        }
        CloseWindow(a);
    }
    CloseScreen(s);
}

int main(void)
{
    P96Base = OpenLibrary(P96NAME, 2);
    if (!P96Base) return 0;
    run(0x50031100, 16);
    run(0x50031303, 24);
    CloseLibrary(P96Base);
    return 0;
}
