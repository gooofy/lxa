/*
 * Probe: gradientslider.gadget - the public BOOPSI class
 * "gradientslider.gadget" (OM_NEW with GRAD_* attributes, defaults,
 * OM_GET/OM_SET incl. GRAD_MaxVal rescaling, OM_DISPOSE).  No rendering:
 * the objects are never added to a window.
 */
#include <exec/memory.h>
#include <exec/execbase.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <gadgets/gradientslider.h>
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
    attr("  MaxVal", o, GRAD_MaxVal);
    attr("  CurVal", o, GRAD_CurVal);
    attr("  SkipVal", o, GRAD_SkipVal);
}

static void gadget(Object *o)
{
    struct Gadget *g = (struct Gadget *)o;
    P_LONG("  LeftEdge", g->LeftEdge);
    P_LONG("  TopEdge", g->TopEdge);
    P_LONG("  Width", g->Width);
    P_LONG("  Height", g->Height);
    P_LONG("  GadgetID", g->GadgetID);
    P_HEX("  GadgetType", g->GadgetType);
    P_HEX("  Flags", g->Flags);
    P_HEX("  Activation", g->Activation);
}

int main(void)
{
    struct Library *GradBase;
    Object *o;
    static UWORD pens[] = { 1, 2, 3, (UWORD)~0 };

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (!IntuitionBase)
        return 20;

    P_SECTION("library");
    GradBase = OpenLibrary((CONST_STRPTR)"gadgets/gradientslider.gadget", 39);
    P_NULL("OpenLibrary gadgets/gradientslider.gadget", GradBase);
    if (!GradBase)
        return 20;
    P_LONG("lib_Version", GradBase->lib_Version);
    P_STR("ln_Name", GradBase->lib_Node.ln_Name);

    P_SECTION("NewObject defaults");
    o = NewObject(NULL, (ClassID)"gradientslider.gadget",
                  GA_Left, 5, GA_Top, 7, GA_Width, 100, GA_Height, 12, GA_ID, 3, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        P_STR("class", (char *)OCLASS(o)->cl_ID);
        P_STR("superclass", (char *)OCLASS(o)->cl_Super->cl_ID);
        gadget(o);
        dump(o);
        attr("  GA_ID", o, GA_ID);

        P_SECTION("SetAttrs GRAD_CurVal");
        P_LONG("SetAttrs", SetAttrs(o, GRAD_CurVal, 0x8000, TAG_DONE));
        dump(o);
        P_SECTION("SetAttrs GRAD_SkipVal");
        P_LONG("SetAttrs", SetAttrs(o, GRAD_SkipVal, 0x100, TAG_DONE));
        dump(o);
        P_SECTION("SetAttrs GRAD_MaxVal 0x100");
        P_LONG("SetAttrs", SetAttrs(o, GRAD_MaxVal, 0x100, TAG_DONE));
        dump(o);
        P_SECTION("SetAttrs GRAD_CurVal 0x80");
        P_LONG("SetAttrs", SetAttrs(o, GRAD_CurVal, 0x80, TAG_DONE));
        dump(o);
        P_SECTION("SetAttrs GRAD_MaxVal 1000");
        P_LONG("SetAttrs", SetAttrs(o, GRAD_MaxVal, 1000, TAG_DONE));
        dump(o);
        P_SECTION("SetAttrs GRAD_PenArray");
        P_LONG("SetAttrs", SetAttrs(o, GRAD_PenArray, pens, TAG_DONE));
        dump(o);
        DisposeObject(o);
    }

    P_SECTION("NewObject with values");
    o = NewObject(NULL, (ClassID)"gradientslider.gadget",
                  GA_Width, 16, GA_Height, 80,
                  GRAD_MaxVal, 255, GRAD_CurVal, 100, GRAD_SkipVal, 16,
                  GRAD_KnobPixels, 8, GRAD_PenArray, pens,
                  PGA_Freedom, LORIENT_VERT, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        gadget(o);
        dump(o);
        DisposeObject(o);
    }

    P_SECTION("NewObject CurVal before MaxVal");
    o = NewObject(NULL, (ClassID)"gradientslider.gadget",
                  GA_Width, 50, GA_Height, 10,
                  GRAD_CurVal, 50, GRAD_MaxVal, 100, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        dump(o);
        DisposeObject(o);
    }

    P_SECTION("clamping and OM_SET return value");
    o = NewObject(NULL, (ClassID)"gradientslider.gadget", GA_Width, 50, GA_Height, 10,
                  GRAD_MaxVal, 100, GRAD_CurVal, 300, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        attr("CurVal (MaxVal 100, CurVal 300)", o, GRAD_CurVal);
        P_LONG("set CurVal 500", SetAttrs(o, GRAD_CurVal, 500, TAG_DONE));
        attr("CurVal", o, GRAD_CurVal);
        P_LONG("set MaxVal 0", SetAttrs(o, GRAD_MaxVal, 0, TAG_DONE));
        attr("CurVal", o, GRAD_CurVal);
        P_LONG("set MaxVal 0x20000", SetAttrs(o, GRAD_MaxVal, 0x20000, TAG_DONE));
        attr("CurVal", o, GRAD_CurVal);
        P_LONG("set CurVal 0x18000", SetAttrs(o, GRAD_CurVal, 0x18000, TAG_DONE));
        attr("CurVal", o, GRAD_CurVal);
        P_LONG("set CurVal 0x18000 again", SetAttrs(o, GRAD_CurVal, 0x18000, TAG_DONE));
        P_LONG("set GRAD_KnobPixels", SetAttrs(o, GRAD_KnobPixels, 3, TAG_DONE));
        P_LONG("set GA_Left", SetAttrs(o, GA_Left, 3, TAG_DONE));
        P_LONG("set no tags", SetAttrs(o, TAG_DONE));
        P_LONG("set GRAD_PenArray NULL", SetAttrs(o, GRAD_PenArray, NULL, TAG_DONE));
        P_LONG("set GA_Disabled", SetAttrs(o, GA_Disabled, TRUE, TAG_DONE));
        P_HEX("Flags", ((struct Gadget *)o)->Flags);
        attr("GRAD_KnobPixels", o, GRAD_KnobPixels);
        attr("GRAD_PenArray", o, GRAD_PenArray);
        attr("PGA_Freedom", o, PGA_Freedom);
        DisposeObject(o);
    }
    o = NewObject(NULL, (ClassID)"gradientslider.gadget", GA_Width, 50, GA_Height, 10,
                  GRAD_CurVal, 0x20000, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        attr("CurVal 0x20000 with the default MaxVal", o, GRAD_CurVal);
        DisposeObject(o);
    }
    o = NewObject(NULL, (ClassID)"gradientslider.gadget", GA_Width, 50, GA_Height, 10,
                  GRAD_CurVal, 300, GRAD_MaxVal, 100, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        attr("CurVal (CurVal 300, MaxVal 100)", o, GRAD_CurVal);
        DisposeObject(o);
    }

    CloseLibrary(GradBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
