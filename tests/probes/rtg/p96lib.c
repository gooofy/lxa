/*
 * Probe (Phase 242): Picasso96API.library on the uaegfx board - library
 * identity, board data, p96AllocBitMap/p96GetBitMapAttr for every pixel
 * format, p96GetModeIDAttr and p96EncodeColor (Picasso96 2.0, rtg profile).
 *
 * rdd: profile rtg
 */
#define LXA_P96_INLINES
#include <exec/types.h>
#include <exec/execbase.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/displayinfo.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <lxa_p96.h>
#include "probe.h"

struct Library *P96Base;

static void dumpbm(const char *n, struct BitMap *bm)
{
    int i;
    P_SECTION(n);
    if (!bm) { probe_s("NULL\n"); return; }
    P_LONG("bpr", bm->BytesPerRow);
    P_LONG("rows", bm->Rows);
    P_HEX("flags", bm->Flags);
    P_LONG("depth", bm->Depth);
    P_HEX("pad", bm->pad);
    for (i = 0; i < 8; i++) { probe_s("plane"); probe_dec(i); probe_s(bm->Planes[i] ? " set\n" : " 0\n"); }
    P_LONG("BMA_W", GetBitMapAttr(bm, BMA_WIDTH));
    P_LONG("BMA_H", GetBitMapAttr(bm, BMA_HEIGHT));
    P_LONG("BMA_D", GetBitMapAttr(bm, BMA_DEPTH));
    P_HEX("BMA_F", GetBitMapAttr(bm, BMA_FLAGS));
    for (i = 0; i <= 12; i++) {
        if (i == 3 || i >= 10) continue;
        probe_s("p96bma"); probe_dec(i); probe_s(" = "); probe_dec(p96GetBitMapAttr(bm, i)); probe_ch('\n');
    }
}

int main(void)
{
    struct Library *cgx;
    struct Screen *wb;
    ULONG v;
    P96Base = OpenLibrary(P96NAME, 0);
    P_NULL("p96", P96Base);
    if (!P96Base) return 0;
    P_LONG("ver", P96Base->lib_Version);
    P_LONG("rev", P96Base->lib_Revision);
    P_LONG("neg", P96Base->lib_NegSize);
    P_LONG("pos", P96Base->lib_PosSize);
    P_STR("id", (char *)P96Base->lib_IdString);
    cgx = OpenLibrary("cybergraphics.library", 0);
    P_NULL("cgx", cgx);
    if (cgx) { P_LONG("cgxver", cgx->lib_Version); P_LONG("cgxneg", cgx->lib_NegSize); CloseLibrary(cgx); }
    {
        LONG nb = -1;
        struct TagItem t[] = {{P96RD_NumberOfBoards, (ULONG)&nb}, {TAG_DONE, 0}};
        P_LONG("rtgdata", p96GetRTGDataTagList(t));
        P_LONG("boards", nb);
    }
    {
        STRPTR bn = 0, cn = 0; ULONG tm = 0, fm = 0, lf = 0, ms = 0, rf = 0, mc = 0;
        struct TagItem t[] = {{P96BD_BoardName, (ULONG)&bn}, {P96BD_ChipName, (ULONG)&cn},
                              {P96BD_TotalMemory, (ULONG)&tm}, {P96BD_FreeMemory, (ULONG)&fm},
                              {P96BD_LargestFreeMemory, (ULONG)&lf}, {P96BD_MonitorSwitch, (ULONG)&ms},
                              {P96BD_RGBFormats, (ULONG)&rf}, {P96BD_MemoryClock, (ULONG)&mc}, {TAG_DONE, 0}};
        P_LONG("boarddata", p96GetBoardDataTagList(0, t));
        P_STR("bname", bn); P_STR("cname", cn); P_LONG("tmem", tm); P_LONG("fmem", fm); P_LONG("lfmem", lf);
        P_LONG("monsw", ms); P_HEX("rgbf", rf); P_LONG("mclk", mc);
        P_LONG("boarddata1", p96GetBoardDataTagList(1, t));
    }
    wb = LockPubScreen(NULL);
    dumpbm("wbbm", wb->RastPort.BitMap);
    P_HEX("wbid", GetVPModeID(&wb->ViewPort));
    P_LONG("wbdepth", wb->BitMap.Depth);
    {
        static const ULONG fmts[] = {RGBFB_CLUT, RGBFB_R5G6B5PC, RGBFB_B8G8R8A8, RGBFB_R8G8B8, RGBFB_A8R8G8B8, RGBFB_R5G6B5, RGBFB_NONE};
        static const ULONG deps[] = {8, 16, 32, 24, 32, 16, 24};
        int i;
        for (i = 0; i < 7; i++) {
            struct BitMap *bm = p96AllocBitMap(33, 7, deps[i], BMF_CLEAR, NULL, fmts[i]);
            probe_s("p96alloc fmt="); probe_dec(fmts[i]); probe_ch('\n');
            dumpbm("bm", bm);
            if (bm) p96FreeBitMap(bm);
            bm = p96AllocBitMap(33, 7, deps[i], BMF_CLEAR | BMF_USERPRIVATE, NULL, fmts[i]);
            probe_s("userprivate\n");
            dumpbm("bm", bm);
            if (bm) p96FreeBitMap(bm);
        }
        {
            struct BitMap *bm = p96AllocBitMap(33, 7, 8, BMF_CLEAR|BMF_DISPLAYABLE, wb->RastPort.BitMap, RGBFB_NONE);
            dumpbm("p96friend", bm); if (bm) p96FreeBitMap(bm);
            bm = AllocBitMap(33, 7, 8, BMF_CLEAR|BMF_DISPLAYABLE, wb->RastPort.BitMap);
            dumpbm("gfxfriend8", bm); if (bm) FreeBitMap(bm);
            bm = AllocBitMap(33, 7, 3, BMF_CLEAR, wb->RastPort.BitMap);
            dumpbm("gfxfriend3", bm); if (bm) FreeBitMap(bm);
            bm = AllocBitMap(33, 7, 24, BMF_CLEAR, NULL);
            dumpbm("gfx24", bm); if (bm) FreeBitMap(bm);
            bm = AllocBitMap(33, 7, 16, BMF_CLEAR, wb->RastPort.BitMap);
            dumpbm("gfx16friend", bm); if (bm) FreeBitMap(bm);
            bm = AllocBitMap(33, 7, 8, BMF_CLEAR, NULL);
            dumpbm("gfx8nofriend", bm); if (bm) FreeBitMap(bm);
            bm = AllocBitMap(33, 7, 24, BMF_CLEAR|0x80, (struct BitMap *)0); /* no SPECIALFMT fmt */
            dumpbm("gfx24special0", bm); if (bm) FreeBitMap(bm);
            bm = AllocBitMap(33, 7, 24, BMF_CLEAR|0x80|(11UL<<24), (struct BitMap *)0);
            dumpbm("gfx24specialARGB", bm); if (bm) FreeBitMap(bm);
        }
    }
    {
        static const ULONG ids[] = {0x50041000, 0x50041100, 0x50041303, 0xfff01000, 0x29000, 0x12345678};
        int i, a;
        for (i = 0; i < 6; i++) {
            probe_s("mode "); probe_hex(ids[i], 8); probe_ch('\n');
            for (a = 0; a <= 13; a++) {
                if (a == 9) {
                    STRPTR s = (STRPTR)p96GetModeIDAttr(ids[i], a);
                    P_STR(" boardname", s);
                    continue;
                }
                probe_s(" ida"); probe_dec(a); probe_s(" = "); probe_dec(p96GetModeIDAttr(ids[i], a)); probe_ch('\n');
            }
        }
    }
    {
        static const ULONG cols[] = {0x00ff0000, 0x0000ff00, 0x000000ff, 0x00123456, 0xff808080};
        int f, c;
        for (f = 0; f < 16; f++) {
            probe_s("enc "); probe_dec(f);
            for (c = 0; c < 5; c++) { probe_ch(' '); probe_hex(p96EncodeColor(f, cols[c]), 8); }
            probe_ch('\n');
        }
    }
    UnlockPubScreen(NULL, wb);
    CloseLibrary(P96Base);
    return 0;
}
