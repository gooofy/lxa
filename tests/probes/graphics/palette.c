/*
 * Probe (Phase 222b remainder): PaletteExtra pen sharing (AttachPalExtra/
 * ObtainPen/ObtainBestPenA/ReleasePen and the pe_RefCnt/pe_AllocList
 * arrays programs can see), VideoControl() on a fresh ColorMap (GET tags
 * of values never set, VTAG_IMMEDIATE) and CalcIVG() for long copper
 * lists.  The ColorMap belongs to an off-screen ViewPort.  AmigaOS 3.1
 * keeps pe_RefCnt as an array of UWORDs and pe_AllocList as an array of
 * UBYTEs (255 ends a list), whatever view.h declares.
 */
#include <exec/memory.h>
#include <graphics/view.h>
#include <graphics/videocontrol.h>
#include <graphics/copper.h>
#include <graphics/modeid.h>
#include <utility/tagitem.h>
#include <intuition/screens.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>
#include "gfxprobe.h"

struct IntuitionBase *IntuitionBase;

#define NCOLORS 16

static struct BitMap pe_bm;
static struct RasInfo pe_ri;

static void p_pe(const char *label, struct ColorMap *cm)
{
    struct PaletteExtra *pe = cm->PalExtra;
    WORD i;

    probe_s(label);
    if (!pe) {
        probe_s(" = no PalExtra\n");
        return;
    }
    probe_s(" = free ");
    probe_dec(pe->pe_NFree);
    probe_s(" firstfree ");
    probe_dec(pe->pe_FirstFree);
    probe_s(" shared ");
    probe_dec(pe->pe_NShared);
    probe_s(" firstshared ");
    probe_dec(pe->pe_FirstShared);
    probe_s(" sharable ");
    probe_dec(pe->pe_SharableColors);
    probe_ch('\n');
    probe_s("  refcnt (words) =");
    for (i = 0; i < NCOLORS; i++) {
        probe_ch(' ');
        probe_dec(((UWORD *)pe->pe_RefCnt)[i]);
    }
    probe_ch('\n');
    probe_s("  alloc (bytes)  =");
    for (i = 0; i < NCOLORS; i++) {
        probe_ch(' ');
        probe_dec(pe->pe_AllocList[i]);
    }
    probe_ch('\n');
}

static void p_rgb(struct ColorMap *cm, LONG pen)
{
    ULONG rgb[3];
    if (pen < 0 || pen >= NCOLORS)
        return;
    GetRGB32(cm, pen, 1, rgb);
    probe_s("  rgb ");
    probe_dec(pen);
    probe_s(" = ");
    probe_hex(rgb[0] >> 24, 2);
    probe_ch(' ');
    probe_hex(rgb[1] >> 24, 2);
    probe_ch(' ');
    probe_hex(rgb[2] >> 24, 2);
    probe_ch('\n');
}

static void pens(void)
{
    struct ViewPort vp;
    struct ColorMap *cm;
    struct TagItem tags[4];
    LONG p[8], r, i;

    gp_section("PaletteExtra");
    InitVPort(&vp);
    InitBitMap(&pe_bm, 4, 320, 200);
    pe_ri.BitMap = &pe_bm;
    pe_ri.RxOffset = pe_ri.RyOffset = 0;
    pe_ri.Next = NULL;
    vp.RasInfo = &pe_ri;
    vp.DWidth = 320;
    vp.DHeight = 200;
    cm = GetColorMap(NCOLORS);
    if (!cm)
        return;
    vp.ColorMap = cm;
    for (i = 0; i < NCOLORS; i++)
        SetRGB32CM(cm, i, (i * 0x11) << 24, (0xff - i * 0x10) << 24, (i * 0x07) << 24);
    P_LONG("AttachPalExtra", AttachPalExtra(cm, &vp));
    P_BOOL("pe_ViewPort", cm->PalExtra && cm->PalExtra->pe_ViewPort == &vp);
    p_pe("attached", cm);

    gp_section("ObtainPen shared");
    p[0] = ObtainPen(cm, -1, 0x12345678, 0x9abcdef0, 0x0fedcba9, 0);
    P_LONG("ObtainPen(-1)", p[0]);
    p_rgb(cm, p[0]);
    p[1] = ObtainPen(cm, -1, 0x55555555, 0x66666666, 0x77777777, 0);
    P_LONG("ObtainPen(-1) #2", p[1]);
    p[2] = ObtainPen(cm, 5, 0x11111111, 0x22222222, 0x33333333, 0);
    P_LONG("ObtainPen(5)", p[2]);
    p_rgb(cm, 5);
    p[3] = ObtainPen(cm, 5, 0x11111111, 0x22222222, 0x33333333, 0);
    P_LONG("ObtainPen(5) again", p[3]);
    p_pe("after shared", cm);

    gp_section("ObtainPen exclusive");
    p[4] = ObtainPen(cm, -1, 0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, PENF_EXCLUSIVE);
    P_LONG("exclusive(-1)", p[4]);
    p[5] = ObtainPen(cm, 6, 0, 0, 0, PENF_EXCLUSIVE | PENF_NO_SETCOLOR);
    P_LONG("exclusive(6) no setcolor", p[5]);
    p_rgb(cm, 6);
    P_LONG("shared on exclusive 6", ObtainPen(cm, 6, 0, 0, 0, 0));
    P_LONG("exclusive on shared 5", ObtainPen(cm, 5, 0, 0, 0, PENF_EXCLUSIVE));
    p_pe("after exclusive", cm);

    gp_section("ObtainBestPenA");
    tags[0].ti_Tag = OBP_Precision;
    tags[0].ti_Data = PRECISION_EXACT;
    tags[1].ti_Tag = TAG_DONE;
    p[6] = ObtainBestPenA(cm, 0x33333333, 0xcccccccc, 0x15151515, tags);
    P_LONG("exact (color 3)", p[6]);
    tags[0].ti_Data = PRECISION_IMAGE;
    p[7] = ObtainBestPenA(cm, 0x34000000, 0xcb000000, 0x16000000, tags);
    P_LONG("image near 3", p[7]);
    r = ObtainBestPenA(cm, 0x12345678, 0x9abcdef0, 0x0fedcba9, NULL);
    P_LONG("best for pen 0 color", r);
    p_pe("after best", cm);
    if (r >= 0)
        ReleasePen(cm, r);

    gp_section("ReleasePen");
    for (i = 0; i < 8; i++)
        if (p[i] >= 0)
            ReleasePen(cm, p[i]);
    p_pe("after release", cm);
    ReleasePen(cm, 5);
    p_pe("release 5 again", cm);

    gp_section("exhaust");
    for (i = 0; i < 20; i++) {
        r = ObtainPen(cm, -1, i << 24, i << 24, i << 24, PENF_EXCLUSIVE);
        probe_s(" ");
        probe_dec(r);
    }
    probe_ch('\n');
    p_pe("exhausted", cm);
    FreeColorMap(cm);
}

/* which colour distances ObtainBestPenA() accepts: one shared pen of grey
 * 0x40, requests that differ in red (or in all guns) by each delta, for
 * several numbers of free pens and precisions; s = shared, n = new pen */
static void tolerance(WORD depth, WORD all)
{
    static const LONG precs[] = {-1, 0, 16, 32, 64};
    static const LONG deltas[] = {1, 2, 3, 4, 5, 6, 8, 12, 15, 16, 17, 24, 31, 32, 33, 48, 63,
                                  64, 96, 128};
    static const LONG nfrees[] = {31, 20, 15, 8, 6, 3, 2, 1};
    struct BitMap tbm;
    struct RasInfo tri;
    LONG fi, pi, di, i;

    for (fi = 0; fi < 8; fi++) {
        if (nfrees[fi] >= (1 << depth))
            continue;
        for (pi = 0; pi < 5; pi++) {
            struct ViewPort vp;
            struct ColorMap *cm;
            struct TagItem tags[2];
            LONG pen, held[32], nh = 0;

            InitVPort(&vp);
            InitBitMap(&tbm, depth, 320, 200);
            tri.BitMap = &tbm;
            tri.Next = NULL;
            tri.RxOffset = tri.RyOffset = 0;
            vp.RasInfo = &tri;
            cm = GetColorMap(1 << depth);
            if (!cm)
                return;
            vp.ColorMap = cm;
            for (i = 0; i < (1 << depth); i++)
                SetRGB32CM(cm, i, 0, 0, 0);
            AttachPalExtra(cm, &vp);
            pen = ObtainPen(cm, -1, 0x40000000, 0x40000000, 0x40000000, 0);
            while (cm->PalExtra->pe_NFree > nfrees[fi] && nh < 32)
                held[nh++] = ObtainPen(cm, -1, 0xffffffff, 0, 0xffffffff, PENF_EXCLUSIVE);
            probe_s("depth ");
            probe_dec(depth);
            probe_s(all ? " rgb" : " red");
            probe_s(" nfree ");
            probe_dec(cm->PalExtra->pe_NFree);
            probe_s(" prec ");
            probe_dec(precs[pi]);
            probe_s(" = ");
            tags[0].ti_Tag = OBP_Precision;
            tags[0].ti_Data = precs[pi];
            tags[1].ti_Tag = TAG_DONE;
            for (di = 0; di < (LONG)(sizeof(deltas) / sizeof(deltas[0])); di++) {
                ULONG v = (ULONG)(0x40 + deltas[di]) << 24;
                LONG r = ObtainBestPenA(cm, v, all ? v : 0x40000000, all ? v : 0x40000000, tags);
                probe_ch(r == pen ? 's' : r < 0 ? '-' : 'n');
                if (r >= 0)
                    ReleasePen(cm, r);
            }
            probe_ch('\n');
            while (nh)
                ReleasePen(cm, held[--nh]);
            ReleasePen(cm, pen);
            FreeColorMap(cm);
        }
    }
}

static void vctl(void)
{
    struct ColorMap *cm;
    struct TagItem t[24];
    LONG imm;
    WORD i;
    static const ULONG get_tags[] = {
        VTAG_ATTACH_CM_GET, VTAG_VIEWPORTEXTRA_GET, VTAG_NORMAL_DISP_GET, VTAG_COERCE_DISP_GET,
        VTAG_PF1_BASE_GET, VTAG_PF2_BASE_GET, VTAG_SPEVEN_BASE_GET, VTAG_SPODD_BASE_GET,
        VTAG_BORDERSPRITE_GET, VTAG_SPRITERESN_GET, VTAG_DEFSPRITERESN_GET, VTAG_BORDERBLANK_GET,
        VTAG_BORDERNOTRANS_GET, VTAG_FULLPALETTE_GET, VTAG_USERCLIP_GET, VTAG_BATCH_CM_GET,
        VTAG_BATCH_ITEMS_GET, VTAG_VPMODEID_GET, 0
    };

    gp_section("VideoControl fresh GET");
    cm = GetColorMap(32);
    if (!cm)
        return;
    for (i = 0; get_tags[i]; i++) {
        t[i].ti_Tag = get_tags[i];
        t[i].ti_Data = 0xdeadbeef;
    }
    t[i].ti_Tag = TAG_DONE;
    P_LONG("result", VideoControl(cm, t));
    for (i = 0; get_tags[i]; i++) {
        probe_hex(get_tags[i], 8);
        probe_s(" -> ");
        probe_hex(t[i].ti_Tag, 8);
        probe_ch(' ');
        if (get_tags[i] == VTAG_ATTACH_CM_GET || get_tags[i] == VTAG_VIEWPORTEXTRA_GET ||
            get_tags[i] == VTAG_NORMAL_DISP_GET || get_tags[i] == VTAG_COERCE_DISP_GET ||
            get_tags[i] == VTAG_BATCH_ITEMS_GET)
            probe_s(t[i].ti_Data == 0xdeadbeef ? "unchanged" : t[i].ti_Data ? "non-NULL" : "NULL");
        else
            probe_hex(t[i].ti_Data, 8);
        probe_ch('\n');
    }

    gp_section("fresh ColorMap fields");
    P_LONG("Type", cm->Type);
    P_LONG("Flags", cm->Flags);
    P_LONG("Count", cm->Count);
    P_HEX("Bp_0_base/Bp_1_base", ((ULONG)cm->Bp_0_base << 16) | cm->Bp_1_base);
    P_HEX("SpriteBase_Even/Odd", ((ULONG)cm->SpriteBase_Even << 16) | cm->SpriteBase_Odd);
    P_LONG("SpriteResolution", cm->SpriteResolution);
    P_LONG("SpriteResDefault", cm->SpriteResDefault);
    P_LONG("AuxFlags", cm->AuxFlags);
    P_HEX("VPModeID", cm->VPModeID);

    gp_section("VTAG_*_BASE_SET fields");
    t[0].ti_Tag = VTAG_PF1_BASE_SET;
    t[0].ti_Data = 0x11;
    t[1].ti_Tag = VTAG_PF2_BASE_SET;
    t[1].ti_Data = 0x22;
    t[2].ti_Tag = VTAG_SPEVEN_BASE_SET;
    t[2].ti_Data = 0x33;
    t[3].ti_Tag = VTAG_SPODD_BASE_SET;
    t[3].ti_Data = 0x44;
    t[4].ti_Tag = TAG_DONE;
    VideoControl(cm, t);
    P_HEX("Bp_0_base/Bp_1_base", ((ULONG)cm->Bp_0_base << 16) | cm->Bp_1_base);
    P_HEX("SpriteBase_Even/Odd", ((ULONG)cm->SpriteBase_Even << 16) | cm->SpriteBase_Odd);

    gp_section("VTAG_FULLPALETTE / VPMODEID");
    t[0].ti_Tag = VTAG_FULLPALETTE_SET;
    t[1].ti_Tag = VTAG_VPMODEID_SET;
    t[1].ti_Data = 0x00029004;
    t[2].ti_Tag = TAG_DONE;
    VideoControl(cm, t);
    P_LONG("AuxFlags", cm->AuxFlags);
    P_HEX("VPModeID", cm->VPModeID);
    t[0].ti_Tag = VTAG_FULLPALETTE_GET;
    t[0].ti_Data = 0xdeadbeef;
    t[1].ti_Tag = VTAG_VPMODEID_GET;
    t[1].ti_Data = 0xdeadbeef;
    VideoControl(cm, t);
    P_HEX("FULLPALETTE_GET tag", t[0].ti_Tag);
    P_HEX("FULLPALETTE_GET data", t[0].ti_Data);
    P_HEX("VPMODEID_GET tag", t[1].ti_Tag);
    P_HEX("VPMODEID_GET data", t[1].ti_Data);
    t[0].ti_Tag = VTAG_FULLPALETTE_CLR;
    t[1].ti_Tag = VTAG_VPMODEID_CLR;
    VideoControl(cm, t);
    P_LONG("AuxFlags", cm->AuxFlags);
    P_HEX("VPModeID", cm->VPModeID);
    t[0].ti_Tag = VTAG_FULLPALETTE_GET;
    t[0].ti_Data = 0xdeadbeef;
    t[1].ti_Tag = VTAG_VPMODEID_GET;
    t[1].ti_Data = 0xdeadbeef;
    VideoControl(cm, t);
    P_HEX("FULLPALETTE_GET tag", t[0].ti_Tag);
    P_HEX("FULLPALETTE_GET data", t[0].ti_Data);
    P_HEX("VPMODEID_GET tag", t[1].ti_Tag);
    P_HEX("VPMODEID_GET data", t[1].ti_Data);

    gp_section("VTAG_IMMEDIATE");
    imm = 0x1234;
    t[0].ti_Tag = VTAG_IMMEDIATE;
    t[0].ti_Data = (ULONG)&imm;
    t[1].ti_Tag = TAG_DONE;
    P_LONG("result", VideoControl(cm, t));
    P_HEX("imm", imm);
    imm = 0x1234;
    t[0].ti_Tag = VTAG_BORDERBLANK_SET;
    t[1].ti_Tag = VTAG_IMMEDIATE;
    t[1].ti_Data = (ULONG)&imm;
    t[2].ti_Tag = TAG_DONE;
    P_LONG("result with SET", VideoControl(cm, t));
    P_HEX("imm", imm);
    imm = 0x1234;
    t[0].ti_Tag = VTAG_IMMEDIATE;
    t[0].ti_Data = (ULONG)&imm;
    t[1].ti_Tag = VTAG_SPRITERESN_SET;
    t[1].ti_Data = SPRITERESN_70NS;
    t[2].ti_Tag = TAG_DONE;
    P_LONG("result IMMEDIATE first", VideoControl(cm, t));
    P_HEX("imm", imm);
    {
        struct ViewPort vp;
        InitVPort(&vp);
        t[0].ti_Tag = VTAG_ATTACH_CM_SET;
        t[0].ti_Data = (ULONG)&vp;
        t[1].ti_Tag = TAG_DONE;
        VideoControl(cm, t);
        imm = 0x1234;
        t[0].ti_Tag = VTAG_IMMEDIATE;
        t[0].ti_Data = (ULONG)&imm;
        P_LONG("attached result", VideoControl(cm, t));
        P_HEX("imm", imm);
        imm = 0x1234;
        t[1].ti_Tag = VTAG_BORDERBLANK_CLR;
        t[2].ti_Tag = TAG_DONE;
        P_LONG("attached result CLR", VideoControl(cm, t));
        P_HEX("imm", imm);
    }
    FreeColorMap(cm);

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    if (IntuitionBase) {
        struct Screen *scr = LockPubScreen(NULL);
        if (scr) {
            imm = 0x1234;
            t[0].ti_Tag = VTAG_IMMEDIATE;
            t[0].ti_Data = (ULONG)&imm;
            t[1].ti_Tag = TAG_DONE;
            P_LONG("Workbench result", VideoControl(scr->ViewPort.ColorMap, t));
            P_HEX("imm", imm);
            imm = 0x1234;
            t[1].ti_Tag = VTAG_BORDERBLANK_GET;
            t[2].ti_Tag = TAG_DONE;
            P_LONG("Workbench result GET", VideoControl(scr->ViewPort.ColorMap, t));
            P_HEX("imm", imm);
            UnlockPubScreen(NULL, scr);
        }
        CloseLibrary((struct Library *)IntuitionBase);
    }
}

static void calcivg(void)
{
    static const WORD counts[] = {0, 1, 2, 10, 20, 30, 39, 40, 41, 42, 45, 49, 50, 51, 52, 53, 54, 55,
                                  56, 57, 58, 59, 60, 70, 80, 99, 100, 101, 110, 111, 112, 113, 114,
                                  115, 116, 117, 118, 119,
                                  120, 150, 200, 300, -1};
    struct View view;
    struct ViewPort vp;
    struct RasInfo ri;
    struct BitMap bm;
    struct CopList cl;
    struct CopIns *ins;
    WORD i, k, lace, wait;

    ins = AllocMem(300 * sizeof(struct CopIns), MEMF_PUBLIC | MEMF_CLEAR);
    if (!ins)
        return;
    for (wait = 0; wait < 2; wait++)
        for (lace = 0; lace < 2; lace++) {
            gp_section(wait ? (lace ? "CalcIVG WAIT+MOVE lace" : "CalcIVG WAIT+MOVE")
                            : (lace ? "CalcIVG MOVE lace" : "CalcIVG MOVE"));
            for (k = 0; counts[k] >= 0; k++) {
                UBYTE *p;
                LONG j;
                p = (UBYTE *)&view;
                for (j = 0; j < (LONG)sizeof(view); j++) p[j] = 0;
                p = (UBYTE *)&vp;
                for (j = 0; j < (LONG)sizeof(vp); j++) p[j] = 0;
                p = (UBYTE *)&ri;
                for (j = 0; j < (LONG)sizeof(ri); j++) p[j] = 0;
                p = (UBYTE *)&cl;
                for (j = 0; j < (LONG)sizeof(cl); j++) p[j] = 0;
                InitBitMap(&bm, 2, 320, 200);
                for (i = 0; i < counts[k]; i++) {
                    ins[i].OpCode = (wait && (i & 1)) ? COPPER_WAIT : COPPER_MOVE;
                    ins[i].u3.u4.u1.DestAddr = (wait && (i & 1)) ? i : 0x180;
                    ins[i].u3.u4.u2.DestData = 0;
                }
                cl.CopIns = ins;
                cl.Count = counts[k];
                cl.MaxCount = counts[k];
                ri.BitMap = &bm;
                view.ViewPort = &vp;
                view.Modes = lace ? LACE : 0;
                vp.DspIns = &cl;
                vp.RasInfo = &ri;
                vp.DWidth = 320;
                vp.DHeight = 200;
                vp.Modes = lace ? LACE : 0;
                probe_s("count ");
                probe_dec(counts[k]);
                probe_s(" = ");
                probe_dec(CalcIVG(&view, &vp));
                vp.Modes |= HIRES;
                vp.DWidth = 640;
                bm.Depth = 4;
                probe_s(" hires4 ");
                probe_dec(CalcIVG(&view, &vp));
                cl.Count = counts[k] / 2;
                probe_s(" Count/2 ");
                probe_dec(CalcIVG(&view, &vp));
                probe_ch('\n');
            }
        }
    FreeMem(ins, 300 * sizeof(struct CopIns));
}

int main(int argc, char **argv)
{
    gp_args(argc, argv);
    pens();
    gp_section("ObtainBestPenA tolerance");
    tolerance(3, FALSE);
    tolerance(3, TRUE);
    tolerance(5, FALSE);
    tolerance(5, TRUE);
    vctl();
    calcivg();
    return 0;
}
