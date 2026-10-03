/*
 * idump.h - Intuition structure dumps for the intuition/gadtools probes
 * (roadmap Phase 222d/e).  One line per object, built on probe.h; never
 * prints pointers, only NULL/non-NULL and the fields a program can see.
 */
#ifndef LXA_PROBE_IDUMP_H
#define LXA_PROBE_IDUMP_H

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/imageclass.h>
#include <intuition/sghooks.h>
#include <graphics/text.h>
#include "probe.h"

static void ps_kv(const char *k, LONG v) { probe_ch(' '); probe_s(k); probe_ch('='); probe_dec(v); }
static void ps_kx(const char *k, ULONG v, int d) { probe_ch(' '); probe_s(k); probe_ch('='); probe_hex(v, d); }
static void ps_box(LONG l, LONG t, LONG w, LONG h)
{
    probe_s(" box="); probe_dec(l); probe_ch(','); probe_dec(t); probe_ch(',');
    probe_dec(w); probe_ch('x'); probe_dec(h);
}
static void ps_str(const char *k, const char *s)
{
    probe_ch(' '); probe_s(k); probe_ch('=');
    if (s) { probe_ch('"'); probe_s(s); probe_ch('"'); } else probe_s("NULL");
}

static void dump_itext(const char *pfx, struct IntuiText *it)
{
    int n = 0;
    for (; it && n < 8; it = it->NextText, n++)
    {
        probe_s(pfx); probe_s("itext"); probe_dec(n);
        ps_str("t", (const char *)it->IText);
        ps_kv("l", it->LeftEdge); ps_kv("tp", it->TopEdge);
        ps_kv("fp", it->FrontPen); ps_kv("bp", it->BackPen); ps_kv("dm", it->DrawMode);
        probe_s(" font=");
        if (it->ITextFont) { probe_s(it->ITextFont->ta_Name ? (const char *)it->ITextFont->ta_Name : "?"); probe_ch('/'); probe_dec(it->ITextFont->ta_YSize); }
        else probe_s("NULL");
        probe_ch('\n');
    }
}

static void dump_image(const char *pfx, struct Image *im)
{
    int n = 0;
    for (; im && n < 4; im = im->NextImage, n++)
    {
        probe_s(pfx); probe_s("image"); probe_dec(n);
        ps_box(im->LeftEdge, im->TopEdge, im->Width, im->Height);
        ps_kv("depth", im->Depth);
        if (im->Depth != CUSTOMIMAGEDEPTH)
        {
            probe_s(im->ImageData ? " data" : " nodata");
            ps_kx("pick", im->PlanePick, 2); ps_kx("onoff", im->PlaneOnOff, 2);
        }
        probe_ch('\n');
        if (im->Depth == CUSTOMIMAGEDEPTH)
            break;  /* a BOOPSI image: NextImage is the object's, not a list */
    }
}

static void dump_border(const char *pfx, struct Border *b)
{
    int n = 0;
    for (; b && n < 6; b = b->NextBorder, n++)
    {
        probe_s(pfx); probe_s("border"); probe_dec(n);
        ps_kv("l", b->LeftEdge); ps_kv("t", b->TopEdge);
        ps_kv("fp", b->FrontPen); ps_kv("bp", b->BackPen); ps_kv("dm", b->DrawMode);
        ps_kv("count", b->Count);
        if (b->XY && b->Count > 0 && b->Count <= 16)
        {
            int i;
            probe_s(" xy");
            for (i = 0; i < b->Count * 2; i++) { probe_ch(i ? ',' : '='); probe_dec(b->XY[i]); }
        }
        probe_ch('\n');
    }
}

static void dump_gadget(const char *pfx, int idx, struct Gadget *g)
{
    UWORD t = g->GadgetType;

    probe_s(pfx); probe_s("gad"); probe_dec(idx);
    ps_kv("id", g->GadgetID);
    ps_kx("type", t, 4); ps_kx("flags", g->Flags, 4); ps_kx("act", g->Activation, 4);
    ps_box(g->LeftEdge, g->TopEdge, g->Width, g->Height);
    probe_s(g->GadgetRender ? " render" : " norender");
    probe_s(g->SelectRender ? " select" : " noselect");
    probe_s(g->GadgetText ? " text" : " notext");
    probe_s(g->SpecialInfo ? " special" : " nospecial");
    if ((t & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET)   /* a hook pointer */
        probe_s(g->MutualExclude ? " mx=set" : " mx=0");
    else
        ps_kv("mx", (LONG)g->MutualExclude);
    probe_ch('\n');
    if (g->Flags & GFLG_EXTENDED)
    {
        struct ExtGadget *eg = (struct ExtGadget *)g;
        probe_s(pfx); probe_s("ext");
        ps_kx("more", eg->MoreFlags, 8);
        if (eg->MoreFlags & GMORE_BOUNDS)
            ps_box(eg->BoundsLeftEdge, eg->BoundsTopEdge, eg->BoundsWidth, eg->BoundsHeight);
        probe_ch('\n');
    }

    if ((g->Flags & GFLG_LABELMASK) == GFLG_LABELITEXT)
        dump_itext(pfx, g->GadgetText);
    else if ((g->Flags & GFLG_LABELMASK) == GFLG_LABELSTRING && g->GadgetText)
    {
        probe_s(pfx); ps_str("labelstring", (const char *)g->GadgetText); probe_ch('\n');
    }
    /* gadget imagery (only for non-BOOPSI gadgets: a custom gadget's
     * render fields belong to its class) */
    if ((t & GTYP_GTYPEMASK) != GTYP_CUSTOMGADGET)
    {
        if (g->Flags & GFLG_GADGIMAGE)
            dump_image(pfx, (struct Image *)g->GadgetRender);
        else
            dump_border(pfx, (struct Border *)g->GadgetRender);
        if ((g->Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHIMAGE)
        {
            probe_s(pfx); probe_s("select:\n");
            if (g->Flags & GFLG_GADGIMAGE)
                dump_image(pfx, (struct Image *)g->SelectRender);
            else
                dump_border(pfx, (struct Border *)g->SelectRender);
        }
    }
    if ((t & GTYP_GTYPEMASK) == GTYP_PROPGADGET && g->SpecialInfo)
    {
        struct PropInfo *pi = (struct PropInfo *)g->SpecialInfo;
        probe_s(pfx); probe_s("prop");
        ps_kx("flags", pi->Flags, 4);
        ps_kx("hpot", pi->HorizPot, 4); ps_kx("vpot", pi->VertPot, 4);
        ps_kx("hbody", pi->HorizBody, 4); ps_kx("vbody", pi->VertBody, 4);
        probe_ch('\n');
    }
    if ((t & GTYP_GTYPEMASK) == GTYP_STRGADGET && g->SpecialInfo)
    {
        struct StringInfo *si = (struct StringInfo *)g->SpecialInfo;
        probe_s(pfx); probe_s("str");
        ps_str("buf", (const char *)si->Buffer);
        ps_kv("max", si->MaxChars); ps_kv("num", si->NumChars);
        ps_kv("pos", si->BufferPos); ps_kv("disp", si->DispPos);
        if (g->Activation & GACT_LONGINT)
            ps_kv("long", si->LongInt);
        probe_s(si->UndoBuffer ? " undo" : " noundo");
        if (g->Activation & GACT_STRINGEXTEND)
        {
            struct StringExtend *se = si->Extension;
            probe_s(" ext");
            if (se)
            {
                probe_s(se->Font ? " font" : " nofont");
                ps_kv("pen0", se->Pens[0]); ps_kv("pen1", se->Pens[1]);
                ps_kv("apen0", se->ActivePens[0]); ps_kv("apen1", se->ActivePens[1]);
                ps_kx("modes", se->InitialModes, 8);
                probe_s(se->EditHook ? " hook" : " nohook");
                probe_s(se->WorkBuffer ? " work" : " nowork");
            }
        }
        probe_ch('\n');
    }
}

/* every gadget of a list, numbered from 0 */
static int dump_glist(const char *pfx, struct Gadget *g)
{
    int n = 0;
    for (; g && n < 64; g = g->NextGadget)
        dump_gadget(pfx, n++, g);
    return n;
}

static void dump_window(const char *pfx, struct Window *w)
{
    probe_s(pfx); probe_s("window");
    ps_box(w->LeftEdge, w->TopEdge, w->Width, w->Height);
    ps_kv("bl", w->BorderLeft); ps_kv("bt", w->BorderTop);
    ps_kv("br", w->BorderRight); ps_kv("bb", w->BorderBottom);
    probe_ch('\n');
    probe_s(pfx); probe_s("  ");
    ps_kv("minw", w->MinWidth); ps_kv("minh", w->MinHeight);
    ps_kv("maxw", (UWORD)w->MaxWidth); ps_kv("maxh", (UWORD)w->MaxHeight);
    ps_kv("gzzw", w->GZZWidth); ps_kv("gzzh", w->GZZHeight);
    probe_ch('\n');
    probe_s(pfx); probe_s("  ");
    ps_kx("flags", w->Flags, 8); ps_kx("idcmp", w->IDCMPFlags, 8);
    ps_str("title", (const char *)w->Title);
    probe_s(w->ScreenTitle ? " stitle" : " nostitle");
    ps_kv("dpen", w->DetailPen); ps_kv("bpen", w->BlockPen);
    probe_ch('\n');
    probe_s(pfx); probe_s("  ");
    probe_s(w->RPort ? " rport" : " norport");
    if (w->RPort && w->RPort->Font)
    {
        ps_kv("rpfont", w->RPort->Font->tf_YSize);
    }
    probe_s(w->BorderRPort ? " borderrp" : " noborderrp");
    probe_s(w->WLayer ? " layer" : " nolayer");
    if (w->WLayer)
    {
        ps_box(w->WLayer->bounds.MinX, w->WLayer->bounds.MinY,
               w->WLayer->bounds.MaxX - w->WLayer->bounds.MinX + 1,
               w->WLayer->bounds.MaxY - w->WLayer->bounds.MinY + 1);
        ps_kx("lflags", w->WLayer->Flags & 0x47, 4);
    }
    probe_ch('\n');
}

#endif
