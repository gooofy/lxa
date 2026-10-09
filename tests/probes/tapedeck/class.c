/*
 * Probe: tapedeck.gadget - the public BOOPSI class "tapedeck.gadget"
 * (OM_NEW with TDECK_* attributes for animation and tape controls,
 * OM_GET/OM_SET, OM_DISPOSE).  No rendering: the objects are never added
 * to a window.
 */
#include <exec/memory.h>
#include <exec/execbase.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <gadgets/tapedeck.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;

static void attr(const char *label, Object *o, ULONG id)
{
    ULONG v = 0xdeadbeef;
    ULONG r = GetAttr(id, o, &v);
    probe_s(label);
    probe_s(" = ");
    probe_hex(v, 8);
    probe_s(" (GetAttr ");
    probe_dec(r != 0);
    probe_s(")\n");
}

static void dump(Object *o)
{
    struct Gadget *g = (struct Gadget *)o;
    attr("  TDECK_Mode", o, TDECK_Mode);
    attr("  TDECK_Paused", o, TDECK_Paused);
    attr("  TDECK_Frames", o, TDECK_Frames);
    attr("  TDECK_CurrentFrame", o, TDECK_CurrentFrame);
    P_LONG("  LeftEdge", g->LeftEdge);
    P_LONG("  TopEdge", g->TopEdge);
    P_LONG("  Width", g->Width);
    P_LONG("  Height", g->Height);
    P_HEX("  GadgetType", g->GadgetType);
    P_HEX("  Flags", g->Flags);
    P_HEX("  Activation", g->Activation);
}

int main(void)
{
    struct Library *TapeBase;
    Object *o;

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (!IntuitionBase)
        return 20;

    P_SECTION("library");
    TapeBase = OpenLibrary((CONST_STRPTR)"gadgets/tapedeck.gadget", 39);
    P_NULL("OpenLibrary gadgets/tapedeck.gadget", TapeBase);
    if (!TapeBase)
        return 20;
    P_LONG("lib_Version", TapeBase->lib_Version);
    P_STR("ln_Name", TapeBase->lib_Node.ln_Name);

    P_SECTION("animation controls");
    o = NewObject(NULL, (ClassID)"tapedeck.gadget",
                  GA_Left, 10, GA_Top, 20, GA_Width, 200, GA_Height, 15,
                  TDECK_Frames, 100, TDECK_CurrentFrame, 5, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        P_STR("class", (char *)OCLASS(o)->cl_ID);
        P_STR("superclass", (char *)OCLASS(o)->cl_Super->cl_ID);
        dump(o);
        P_SECTION("SetAttrs TDECK_Mode BUT_PLAY");
        P_LONG("SetAttrs", SetAttrs(o, TDECK_Mode, BUT_PLAY, TAG_DONE));
        dump(o);
        P_SECTION("SetAttrs TDECK_CurrentFrame 42");
        P_LONG("SetAttrs", SetAttrs(o, TDECK_CurrentFrame, 42, TAG_DONE));
        dump(o);
        P_SECTION("SetAttrs TDECK_Frames 10");
        P_LONG("SetAttrs", SetAttrs(o, TDECK_Frames, 10, TAG_DONE));
        dump(o);
        P_SECTION("SetAttrs TDECK_Paused TRUE");
        P_LONG("SetAttrs", SetAttrs(o, TDECK_Paused, TRUE, TAG_DONE));
        dump(o);
        DisposeObject(o);
    }

    P_SECTION("tape controls");
    o = NewObject(NULL, (ClassID)"tapedeck.gadget",
                  GA_Width, 201, GA_Height, 15,
                  TDECK_Tape, TRUE, TDECK_Mode, BUT_STOP, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        dump(o);
        P_SECTION("SetAttrs TDECK_Mode BUT_FORWARD");
        P_LONG("SetAttrs", SetAttrs(o, TDECK_Mode, BUT_FORWARD, TAG_DONE));
        dump(o);
        DisposeObject(o);
    }

    P_SECTION("two objects, sizes, invalid values");
    {
        Object *o2;
        o = NewObject(NULL, (ClassID)"tapedeck.gadget", GA_Width, 50, GA_Height, 5, TDECK_Frames, 7, TAG_DONE);
        o2 = NewObject(NULL, (ClassID)"tapedeck.gadget", GA_Width, 300, GA_Height, 30,
                       TDECK_Frames, 9, TDECK_Tape, TRUE, TAG_DONE);
        P_NULL("object 1", o);
        P_NULL("object 2", o2);
        if (o && o2) {
            dump(o);
            dump(o2);
            attr("1 TDECK_Tape", o, TDECK_Tape);
            attr("2 TDECK_Tape", o2, TDECK_Tape);
            attr("2 GA_Width", o2, GA_Width);
            P_LONG("2 set TDECK_CurrentFrame 3", SetAttrs(o2, TDECK_CurrentFrame, 3, TAG_DONE));
            attr("1 TDECK_CurrentFrame", o, TDECK_CurrentFrame);
            P_LONG("1 set TDECK_Mode 9", SetAttrs(o, TDECK_Mode, 9, TAG_DONE));
            attr("1 TDECK_Mode", o, TDECK_Mode);
            P_LONG("1 set TDECK_Mode BUT_END", SetAttrs(o, TDECK_Mode, BUT_END, TAG_DONE));
            attr("1 TDECK_Mode", o, TDECK_Mode);
            SetAttrs(o, TDECK_Mode, BUT_FRAME, TAG_DONE);
            attr("1 TDECK_Mode after BUT_FRAME", o, TDECK_Mode);
            SetAttrs(o, TDECK_Mode, BUT_BEGIN, TAG_DONE);
            attr("1 TDECK_Mode after BUT_BEGIN", o, TDECK_Mode);
            SetAttrs(o, TDECK_Mode, BUT_PAUSE, TAG_DONE);
            attr("1 TDECK_Mode after BUT_PAUSE", o, TDECK_Mode);
            SetAttrs(o, TDECK_Mode, BUT_REWIND, TAG_DONE);
            attr("1 TDECK_Mode after BUT_REWIND", o, TDECK_Mode);
            P_LONG("1 set GA_Width/GA_Height", SetAttrs(o, GA_Width, 120, GA_Height, 40, TAG_DONE));
            P_LONG("1 Width", ((struct Gadget *)o)->Width);
            P_LONG("1 Height", ((struct Gadget *)o)->Height);
            P_LONG("1 set TDECK_Paused 5", SetAttrs(o, TDECK_Paused, 5, TAG_DONE));
            attr("1 TDECK_Paused", o, TDECK_Paused);
            attr("2 TDECK_Paused", o2, TDECK_Paused);
        }
        if (o) DisposeObject(o);
        if (o2) DisposeObject(o2);
    }
    o = NewObject(NULL, (ClassID)"tapedeck.gadget", TAG_DONE);
    P_NULL("object without tags", o);
    if (o) {
        dump(o);
        DisposeObject(o);
    }

    CloseLibrary(TapeBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
