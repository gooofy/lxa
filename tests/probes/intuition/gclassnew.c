/*
 * Probe (Phase 252): what OM_NEW of intuition's public gadget classes
 * leaves in the Gadget (type, flags, activation, box, imagery, SpecialInfo)
 * for typical tag sets, and which attributes OM_GET answers.
 * gadgetclass, buttongclass, frbuttonclass, propgclass, strgclass and
 * groupgclass (OM_ADDMEMBER / box growth).
 */
#include <exec/memory.h>
#include <exec/execbase.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include "idump.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;

static struct IntuiText itext = { 1, 0, JAM1, 2, 3, NULL, (UBYTE *)"IText", NULL };
static UBYTE strbuf[64] = "hello";
static UBYTE undobuf[64];

static void show_special(struct Gadget *g, const char *cls)
{
    if (!g->SpecialInfo)
        return;
    if (cls[0] == 'p')
    {
        struct PropInfo *pi = (struct PropInfo *)g->SpecialInfo;
        probe_s("  propinfo");
        ps_kx("flags", pi->Flags, 4);
        ps_kx("hpot", pi->HorizPot, 4); ps_kx("vpot", pi->VertPot, 4);
        ps_kx("hbody", pi->HorizBody, 4); ps_kx("vbody", pi->VertBody, 4);
        probe_ch('\n');
    }
    else if (cls[0] == 's')
    {
        struct StringInfo *si = (struct StringInfo *)g->SpecialInfo;
        probe_s("  strinfo");
        ps_kv("max", si->MaxChars); ps_kv("num", si->NumChars);
        ps_kv("pos", si->BufferPos); ps_kv("disp", si->DispPos);
        ps_kv("long", si->LongInt);
        probe_s(si->Buffer == strbuf ? " buf=caller" : (si->Buffer ? " buf=own" : " buf=NULL"));
        if (si->Buffer) ps_str("text", (const char *)si->Buffer);
        probe_s(si->UndoBuffer == undobuf ? " undo=caller" : (si->UndoBuffer ? " undo=own" : " undo=NULL"));
        probe_s(si->Extension ? " ext" : " noext");
        probe_ch('\n');
    }
}

static void show_render(struct Gadget *g)
{
    if (g->GadgetRender && (g->Flags & GFLG_GADGIMAGE))
        dump_image("  render ", (struct Image *)g->GadgetRender);
    if (g->SelectRender && (g->Flags & GFLG_GADGIMAGE))
        dump_image("  select ", (struct Image *)g->SelectRender);
    if (g->GadgetText && (g->Flags & GFLG_LABELMASK) == GFLG_LABELITEXT)
        dump_itext("  ", (struct IntuiText *)g->GadgetText);
}

static const ULONG get_attrs[] = {
    GA_Left, GA_Top, GA_Width, GA_Height, GA_ID, GA_UserData, GA_Disabled,
    GA_Selected, GA_Highlight, GA_Image, GA_Text, GA_IntuiText,
    PGA_Freedom, PGA_Top, PGA_Total, PGA_Visible, PGA_HorizPot, PGA_VertPot,
    PGA_HorizBody, PGA_VertBody,
    STRINGA_LongVal, STRINGA_TextVal, STRINGA_MaxChars, STRINGA_BufferPos,
    0
};
static const char *get_names[] = {
    "GA_Left", "GA_Top", "GA_Width", "GA_Height", "GA_ID", "GA_UserData", "GA_Disabled",
    "GA_Selected", "GA_Highlight", "GA_Image", "GA_Text", "GA_IntuiText",
    "PGA_Freedom", "PGA_Top", "PGA_Total", "PGA_Visible", "PGA_HorizPot", "PGA_VertPot",
    "PGA_HorizBody", "PGA_VertBody",
    "STRINGA_LongVal", "STRINGA_TextVal", "STRINGA_MaxChars", "STRINGA_BufferPos",
};

static void show_get(Object *o)
{
    int i;
    probe_s("  get");
    for (i = 0; get_attrs[i]; i++)
    {
        ULONG v = 0xdeadbeef;
        ULONG r = GetAttr(get_attrs[i], o, &v);
        if (!r)
            continue;
        probe_ch(' ');
        probe_s(get_names[i]);
        probe_ch('=');
        if (get_attrs[i] == GA_Image || get_attrs[i] == GA_Text || get_attrs[i] == GA_IntuiText ||
            get_attrs[i] == STRINGA_TextVal || get_attrs[i] == GA_UserData)
            probe_s(v ? (v == 0xdeadbeef ? "unset" : "ptr") : "0");
        else if (v == 0xdeadbeef)
            probe_s("unset");
        else
            probe_dec((LONG)v);
    }
    probe_ch('\n');
}

static Object *mk(const char *label, const char *cls, struct TagItem *tags)
{
    Object *o = NewObjectA(NULL, (UBYTE *)cls, tags);
    probe_s("== ");
    probe_s(label);
    probe_s(o ? "\n" : " = NULL\n");
    if (o)
    {
        dump_gadget("  ", 0, (struct Gadget *)o);
        show_render((struct Gadget *)o);
        show_special((struct Gadget *)o, cls);
        show_get(o);
    }
    return o;
}

int main(void)
{
    Object *frame, *o, *group, *m1, *m2, *knob;
    struct Screen *scr;
    struct DrawInfo *dri;

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 20;
    scr = LockPubScreen(NULL);
    dri = scr ? GetScreenDrawInfo(scr) : NULL;
    if (!dri)
        return 20;

    P_SECTION("gadgetclass");
    {
        struct TagItem t0[] = { { TAG_DONE, 0 } };
        struct TagItem t1[] = { { GA_Left, 10 }, { GA_Top, 20 }, { GA_Width, 30 }, { GA_Height, 40 },
                                { GA_ID, 7 }, { GA_RelVerify, TRUE }, { GA_Immediate, TRUE }, { TAG_DONE, 0 } };
        struct TagItem t2[] = { { GA_RelRight, -20 }, { GA_RelBottom, -10 }, { GA_RelWidth, -40 },
                                { GA_RelHeight, -30 }, { GA_RightBorder, TRUE }, { GA_FollowMouse, TRUE },
                                { GA_ToggleSelect, TRUE }, { GA_Selected, TRUE }, { GA_Disabled, TRUE },
                                { GA_EndGadget, TRUE }, { TAG_DONE, 0 } };
        struct TagItem t3[] = { { GA_Text, (ULONG)"Label" }, { GA_TabCycle, TRUE }, { GA_GadgetHelp, TRUE },
                                { GA_LeftBorder, TRUE }, { GA_BottomBorder, TRUE }, { GA_Highlight, GFLG_GADGHNONE },
                                { TAG_DONE, 0 } };
        struct TagItem t4[] = { { GA_IntuiText, (ULONG)&itext }, { GA_TopBorder, TRUE }, { GA_SysGType, GTYP_WDRAGGING },
                                { GA_Bounds, 0 }, { TAG_DONE, 0 } };
        struct IBox bounds = { 1, 2, 300, 400 };
        t4[3].ti_Data = (ULONG)&bounds;
        if ((o = mk("gadgetclass default", "gadgetclass", t0))) DisposeObject(o);
        if ((o = mk("gadgetclass box id verify immediate", "gadgetclass", t1))) DisposeObject(o);
        if ((o = mk("gadgetclass rel border follow toggle sel dis end", "gadgetclass", t2))) DisposeObject(o);
        if ((o = mk("gadgetclass text tabcycle help border hnone", "gadgetclass", t3))) DisposeObject(o);
        if ((o = mk("gadgetclass itext topborder sysgtype bounds", "gadgetclass", t4))) DisposeObject(o);
    }

    P_SECTION("buttongclass");
    {
        struct TagItem t0[] = { { TAG_DONE, 0 } };
        struct TagItem t1[] = { { GA_Left, 5 }, { GA_Top, 6 }, { GA_Width, 50 }, { GA_Height, 14 },
                                { GA_Text, (ULONG)"Push" }, { GA_RelVerify, TRUE }, { TAG_DONE, 0 } };
        struct TagItem t2[] = { { GA_RelRight, -17 }, { GA_RelBottom, -30 }, { GA_RightBorder, TRUE },
                                { GA_Immediate, TRUE }, { GA_RelVerify, TRUE }, { GA_Image, 0 },
                                { GA_SelectRender, 0 }, { TAG_DONE, 0 } };
        struct TagItem t3[] = { { GA_ToggleSelect, TRUE }, { GA_Selected, TRUE }, { GA_Highlight, GFLG_GADGHBOX },
                                { GA_IntuiText, (ULONG)&itext }, { TAG_DONE, 0 } };
        struct TagItem ti[] = { { SYSIA_Which, UPIMAGE }, { SYSIA_Size, SYSISIZE_MEDRES },
                                { SYSIA_DrawInfo, (ULONG)dri }, { TAG_DONE, 0 } };
        Object *img = NewObjectA(NULL, (UBYTE *)"sysiclass", ti);
        Object *img2 = NewObjectA(NULL, (UBYTE *)"sysiclass", ti);
        t2[5].ti_Data = (ULONG)img;
        t2[6].ti_Data = (ULONG)img2;
        if ((o = mk("buttongclass default", "buttongclass", t0))) DisposeObject(o);
        if ((o = mk("buttongclass box text verify", "buttongclass", t1))) DisposeObject(o);
        if ((o = mk("buttongclass arrow: rel border image select", "buttongclass", t2))) DisposeObject(o);
        if ((o = mk("buttongclass toggle selected hbox itext", "buttongclass", t3))) DisposeObject(o);
        DisposeObject(img);
        DisposeObject(img2);
    }

    P_SECTION("frbuttonclass");
    {
        struct TagItem tf[] = { { IA_FrameType, FRAME_BUTTON }, { TAG_DONE, 0 } };
        struct TagItem t0[] = { { TAG_DONE, 0 } };
        struct TagItem t1[] = { { GA_Left, 20 }, { GA_Top, 30 }, { GA_Image, 0 }, { GA_Text, (ULONG)"Frame" },
                                { GA_RelVerify, TRUE }, { TAG_DONE, 0 } };
        struct TagItem t2[] = { { GA_Left, 20 }, { GA_Top, 30 }, { GA_Width, 120 }, { GA_Height, 20 },
                                { GA_Image, 0 }, { GA_IntuiText, (ULONG)&itext }, { TAG_DONE, 0 } };
        struct TagItem t3[] = { { GA_Left, 4 }, { GA_Top, 4 }, { GA_Image, 0 }, { TAG_DONE, 0 } };
        frame = NewObjectA(NULL, (UBYTE *)"frameiclass", tf);
        t1[2].ti_Data = (ULONG)frame;
        t2[4].ti_Data = (ULONG)frame;
        t3[2].ti_Data = (ULONG)frame;
        if ((o = mk("frbuttonclass default", "frbuttonclass", t0))) DisposeObject(o);
        if ((o = mk("frbuttonclass frame text", "frbuttonclass", t1))) DisposeObject(o);
        if ((o = mk("frbuttonclass frame box itext", "frbuttonclass", t2))) DisposeObject(o);
        if ((o = mk("frbuttonclass frame only", "frbuttonclass", t3))) DisposeObject(o);
        if (frame)
        {
            struct Image *fi = (struct Image *)frame;
            probe_s("frame after"); ps_box(fi->LeftEdge, fi->TopEdge, fi->Width, fi->Height); probe_ch('\n');
        }
        DisposeObject(frame);
    }

    P_SECTION("propgclass");
    {
        struct TagItem t0[] = { { TAG_DONE, 0 } };
        struct TagItem t1[] = { { GA_Left, 10 }, { GA_Top, 10 }, { GA_Width, 16 }, { GA_Height, 80 },
                                { PGA_Freedom, FREEVERT }, { PGA_Total, 100 }, { PGA_Visible, 10 }, { PGA_Top, 25 },
                                { PGA_NewLook, TRUE }, { GA_RelVerify, TRUE }, { TAG_DONE, 0 } };
        struct TagItem t2[] = { { GA_RelRight, -15 }, { GA_Top, 12 }, { GA_Width, 10 }, { GA_RelHeight, -40 },
                                { GA_RightBorder, TRUE }, { PGA_Borderless, TRUE }, { PGA_Freedom, FREEVERT },
                                { PGA_NewLook, TRUE }, { PGA_Total, 50 }, { PGA_Visible, 50 }, { TAG_DONE, 0 } };
        struct TagItem t3[] = { { GA_Left, 0 }, { GA_Top, 0 }, { GA_Width, 100 }, { GA_Height, 10 },
                                { PGA_Freedom, FREEHORIZ }, { PGA_HorizPot, 0x4000 }, { PGA_HorizBody, 0x2000 },
                                { GA_Immediate, TRUE }, { GA_FollowMouse, TRUE }, { TAG_DONE, 0 } };
        struct TagItem tk[] = { { IA_Width, 8 }, { IA_Height, 8 }, { TAG_DONE, 0 } };
        struct TagItem t4[] = { { GA_Width, 50 }, { GA_Height, 50 }, { PGA_Freedom, FREEHORIZ | FREEVERT },
                                { PGA_KnobImage, 0 }, { TAG_DONE, 0 } };
        knob = NewObjectA(NULL, (UBYTE *)"fillrectclass", tk);
        t4[3].ti_Data = (ULONG)knob;
        if ((o = mk("propgclass default", "propgclass", t0))) DisposeObject(o);
        if ((o = mk("propgclass vert total visible top newlook", "propgclass", t1))) DisposeObject(o);
        if ((o = mk("propgclass border scroller borderless", "propgclass", t2))) DisposeObject(o);
        if ((o = mk("propgclass horiz pot body immediate follow", "propgclass", t3))) DisposeObject(o);
        if ((o = mk("propgclass both knobimage", "propgclass", t4))) DisposeObject(o);
        DisposeObject(knob);
    }

    P_SECTION("strgclass");
    {
        struct TagItem t0[] = { { TAG_DONE, 0 } };
        struct TagItem t1[] = { { GA_Left, 10 }, { GA_Top, 10 }, { GA_Width, 100 }, { GA_Height, 12 },
                                { STRINGA_MaxChars, 40 }, { STRINGA_TextVal, (ULONG)"init" }, { GA_RelVerify, TRUE },
                                { TAG_DONE, 0 } };
        struct TagItem t2[] = { { STRINGA_Buffer, (ULONG)strbuf }, { STRINGA_UndoBuffer, (ULONG)undobuf },
                                { STRINGA_MaxChars, 64 }, { STRINGA_BufferPos, 2 }, { TAG_DONE, 0 } };
        struct TagItem t3[] = { { STRINGA_LongVal, 1234 }, { STRINGA_MaxChars, 10 },
                                { STRINGA_Justification, GACT_STRINGRIGHT }, { TAG_DONE, 0 } };
        if ((o = mk("strgclass default", "strgclass", t0))) DisposeObject(o);
        if ((o = mk("strgclass maxchars textval", "strgclass", t1))) DisposeObject(o);
        if ((o = mk("strgclass caller buffers bufferpos", "strgclass", t2))) DisposeObject(o);
        if ((o = mk("strgclass longval right", "strgclass", t3))) DisposeObject(o);
    }

    P_SECTION("groupgclass");
    {
        struct TagItem t0[] = { { GA_Left, 10 }, { GA_Top, 20 }, { TAG_DONE, 0 } };
        struct TagItem tm1[] = { { GA_Left, 5 }, { GA_Top, 6 }, { GA_Width, 40 }, { GA_Height, 12 },
                                 { GA_Text, (ULONG)"A" }, { GA_ID, 1 }, { TAG_DONE, 0 } };
        struct TagItem tm2[] = { { GA_Left, 60 }, { GA_Top, 30 }, { GA_Width, 30 }, { GA_Height, 15 },
                                 { GA_Text, (ULONG)"B" }, { GA_ID, 2 }, { TAG_DONE, 0 } };
        group = mk("groupgclass at 10,20", "groupgclass", t0);
        if (group)
        {
            struct Gadget *g = (struct Gadget *)group;
            m1 = NewObjectA(NULL, (UBYTE *)"buttongclass", tm1);
            m2 = NewObjectA(NULL, (UBYTE *)"buttongclass", tm2);
            P_LONG("addmember 1", DoMethod(group, OM_ADDMEMBER, (ULONG)m1));
            probe_s("group"); ps_box(g->LeftEdge, g->TopEdge, g->Width, g->Height); probe_ch('\n');
            dump_gadget("  m1 ", 0, (struct Gadget *)m1);
            P_LONG("addmember 2", DoMethod(group, OM_ADDMEMBER, (ULONG)m2));
            probe_s("group"); ps_box(g->LeftEdge, g->TopEdge, g->Width, g->Height); probe_ch('\n');
            dump_gadget("  m2 ", 0, (struct Gadget *)m2);
            probe_s(g->NextGadget ? "group next set\n" : "group next NULL\n");
            probe_s(((struct Gadget *)m1)->NextGadget ? "m1 next set\n" : "m1 next NULL\n");
            {
                struct TagItem ts[] = { { GA_Left, 100 }, { GA_Top, 100 }, { TAG_DONE, 0 } };
                SetAttrsA(group, ts);
                probe_s("after move group"); ps_box(g->LeftEdge, g->TopEdge, g->Width, g->Height); probe_ch('\n');
                dump_gadget("  m1 ", 0, (struct Gadget *)m1);
                dump_gadget("  m2 ", 0, (struct Gadget *)m2);
            }
            P_LONG("remmember 2", DoMethod(m2, OM_REMOVE));
            probe_s("group"); ps_box(g->LeftEdge, g->TopEdge, g->Width, g->Height); probe_ch('\n');
            DisposeObject(m2);
            DisposeObject(group);     /* disposes m1 too */
        }
    }

    FreeScreenDrawInfo(scr, dri);
    UnlockPubScreen(NULL, scr);
    CloseLibrary(IntuitionBase);
    return 0;
}
