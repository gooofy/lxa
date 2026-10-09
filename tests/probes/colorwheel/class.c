/*
 * Probe: colorwheel.gadget - the library (version, ConvertHSBToRGB(),
 * ConvertRGBToHSB()) and the public BOOPSI class "colorwheel.gadget"
 * (OM_NEW with WHEEL_* attributes, OM_GET/OM_SET, attached gradient
 * slider, OM_DISPOSE).  No rendering: the objects are never added to a
 * window.
 */
#include <exec/memory.h>
#include <exec/execbase.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <gadgets/colorwheel.h>
#include <gadgets/gradientslider.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/colorwheel.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;
struct Library *ColorWheelBase;

static void hex3(ULONG a, ULONG b, ULONG c)
{
    probe_hex(a, 8); probe_ch(' ');
    probe_hex(b, 8); probe_ch(' ');
    probe_hex(c, 8);
}

static void hsb2rgb(ULONG h, ULONG s, ULONG b)
{
    struct ColorWheelHSB hsb;
    struct ColorWheelRGB rgb;
    hsb.cw_Hue = h; hsb.cw_Saturation = s; hsb.cw_Brightness = b;
    rgb.cw_Red = rgb.cw_Green = rgb.cw_Blue = 0xdeadbeef;
    ConvertHSBToRGB(&hsb, &rgb);
    probe_s("HSB ");
    hex3(h, s, b);
    probe_s(" -> RGB ");
    hex3(rgb.cw_Red, rgb.cw_Green, rgb.cw_Blue);
    probe_ch('\n');
}

static void rgb2hsb(ULONG r, ULONG g, ULONG b)
{
    struct ColorWheelHSB hsb;
    struct ColorWheelRGB rgb;
    rgb.cw_Red = r; rgb.cw_Green = g; rgb.cw_Blue = b;
    hsb.cw_Hue = hsb.cw_Saturation = hsb.cw_Brightness = 0xdeadbeef;
    ConvertRGBToHSB(&rgb, &hsb);
    probe_s("RGB ");
    hex3(r, g, b);
    probe_s(" -> HSB ");
    /* the hue of a grey (saturation 0) is undefined: 3.1 leaves a random value */
    if (hsb.cw_Saturation == 0)
        probe_s("(undefined)");
    else
        probe_hex(hsb.cw_Hue, 8);
    probe_ch(' ');
    probe_hex(hsb.cw_Saturation, 8); probe_ch(' ');
    probe_hex(hsb.cw_Brightness, 8);
    probe_ch('\n');
}

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
    struct ColorWheelHSB hsb;
    struct ColorWheelRGB rgb;
    attr("  Hue", o, WHEEL_Hue);
    attr("  Saturation", o, WHEEL_Saturation);
    attr("  Brightness", o, WHEEL_Brightness);
    attr("  Red", o, WHEEL_Red);
    attr("  Green", o, WHEEL_Green);
    attr("  Blue", o, WHEEL_Blue);
    hsb.cw_Hue = hsb.cw_Saturation = hsb.cw_Brightness = 0xdeadbeef;
    GetAttr(WHEEL_HSB, o, (ULONG *)&hsb);
    probe_s("  HSB = "); hex3(hsb.cw_Hue, hsb.cw_Saturation, hsb.cw_Brightness); probe_ch('\n');
    rgb.cw_Red = rgb.cw_Green = rgb.cw_Blue = 0xdeadbeef;
    GetAttr(WHEEL_RGB, o, (ULONG *)&rgb);
    probe_s("  RGB = "); hex3(rgb.cw_Red, rgb.cw_Green, rgb.cw_Blue); probe_ch('\n');
}

int main(void)
{
    struct Screen *scr;
    struct Library *GradBase;
    Object *o, *grad;

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (!IntuitionBase)
        return 20;

    P_SECTION("library");
    ColorWheelBase = OpenLibrary((CONST_STRPTR)"gadgets/colorwheel.gadget", 39);
    P_NULL("OpenLibrary gadgets/colorwheel.gadget", ColorWheelBase);
    if (!ColorWheelBase)
        return 20;
    P_LONG("lib_Version", ColorWheelBase->lib_Version);
    P_STR("ln_Name", ColorWheelBase->lib_Node.ln_Name);
    {
        /* opened again by its path or its name: the same library */
        struct Library *b2 = OpenLibrary((CONST_STRPTR)"gadgets/colorwheel.gadget", 39);
        struct Library *b3 = OpenLibrary((CONST_STRPTR)"colorwheel.gadget", 39);
        P_BOOL("2nd OpenLibrary gadgets/colorwheel.gadget same base", b2 == ColorWheelBase);
        P_BOOL("OpenLibrary colorwheel.gadget same base", b3 == ColorWheelBase);
        if (b2) CloseLibrary(b2);
        if (b3) CloseLibrary(b3);
    }

    P_SECTION("ConvertHSBToRGB");
    hsb2rgb(0x00000000, 0xffffffff, 0xffffffff);
    hsb2rgb(0x2aaaaaaa, 0xffffffff, 0xffffffff);
    hsb2rgb(0x55555555, 0xffffffff, 0xffffffff);
    hsb2rgb(0x80000000, 0xffffffff, 0xffffffff);
    hsb2rgb(0xaaaaaaaa, 0xffffffff, 0xffffffff);
    hsb2rgb(0xd5555555, 0xffffffff, 0xffffffff);
    hsb2rgb(0xffffffff, 0xffffffff, 0xffffffff);
    hsb2rgb(0x00000000, 0x00000000, 0xffffffff);
    hsb2rgb(0x40000000, 0x00000000, 0x80000000);
    hsb2rgb(0x40000000, 0x80000000, 0x80000000);
    hsb2rgb(0x40000000, 0xffffffff, 0x00000000);
    hsb2rgb(0x12345678, 0x9abcdef0, 0xfedcba98);
    hsb2rgb(0xc0000000, 0x40000000, 0xc0000000);
    hsb2rgb(0x0fff0fff, 0xffffffff, 0xffffffff);
    hsb2rgb(0x2aa82aa8, 0xffffffff, 0xffffffff);
    hsb2rgb(0x2aab0000, 0xffffffff, 0xffffffff);
    hsb2rgb(0x40000000, 0x7fffffff, 0x80000000);
    hsb2rgb(0x854d854d, 0x80000000, 0xc0000000);
    hsb2rgb(0xaff5aff5, 0xffffffff, 0xffffffff);
    hsb2rgb(0xfff0fff0, 0x12345678, 0x9abcdef0);

    P_SECTION("ConvertRGBToHSB");
    rgb2hsb(0xffffffff, 0x00000000, 0x00000000);
    rgb2hsb(0x00000000, 0xffffffff, 0x00000000);
    rgb2hsb(0x00000000, 0x00000000, 0xffffffff);
    rgb2hsb(0xffffffff, 0xffffffff, 0x00000000);
    rgb2hsb(0x00000000, 0xffffffff, 0xffffffff);
    rgb2hsb(0xffffffff, 0x00000000, 0xffffffff);
    rgb2hsb(0xffffffff, 0xffffffff, 0xffffffff);
    rgb2hsb(0x00000000, 0x00000000, 0x00000000);
    rgb2hsb(0x80808080, 0x80808080, 0x80808080);
    rgb2hsb(0xffffffff, 0x80000000, 0x00000000);
    rgb2hsb(0x12345678, 0x9abcdef0, 0xfedcba98);
    rgb2hsb(0x66666666, 0x88888888, 0xbbbbbbbb);
    rgb2hsb(0x9380f57d, 0xe2d6f703, 0xc385e89f);
    rgb2hsb(0x714f6f00, 0x78115fd0, 0xf1737cb0);
    rgb2hsb(0x74ef6bd7, 0x6bd7f927, 0xf927aaaa);
    rgb2hsb(0xc0910000, 0x3c970000, 0xb2030000);
    rgb2hsb(0xbc6e0000, 0x98b50000, 0x00000000);
    rgb2hsb(0x90dc0000, 0xf7c80000, 0x9df00000);
    rgb2hsb(0x00010000, 0x00000000, 0x00000000);

    scr = LockPubScreen(NULL);
    if (!scr)
        return 20;

    P_SECTION("NewObject defaults");
    o = NewObject(NULL, (ClassID)"colorwheel.gadget",
                  GA_Left, 10, GA_Top, 10, GA_Width, 60, GA_Height, 60,
                  WHEEL_Screen, scr, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        P_STR("class", (char *)OCLASS(o)->cl_ID);
        P_STR("superclass", (char *)OCLASS(o)->cl_Super->cl_ID);
        P_LONG("Width", ((struct Gadget *)o)->Width);
        P_LONG("Height", ((struct Gadget *)o)->Height);
        P_HEX("GadgetType", ((struct Gadget *)o)->GadgetType);
        P_HEX("Flags", ((struct Gadget *)o)->Flags);
        P_HEX("Activation", ((struct Gadget *)o)->Activation);
        dump(o);
        P_SECTION("SetAttrs WHEEL_Hue/Saturation");
        P_LONG("SetAttrs", SetAttrs(o, WHEEL_Hue, 0x55555555, WHEEL_Saturation, 0x80000000, TAG_DONE));
        dump(o);
        P_SECTION("SetAttrs WHEEL_Brightness");
        P_LONG("SetAttrs", SetAttrs(o, WHEEL_Brightness, 0x40000000, TAG_DONE));
        dump(o);
        P_SECTION("SetAttrs WHEEL_Red/Green/Blue");
        P_LONG("SetAttrs", SetAttrs(o, WHEEL_Red, 0x20000000, WHEEL_Green, 0xc0000000, WHEEL_Blue, 0x60000000, TAG_DONE));
        dump(o);
        P_SECTION("SetAttrs WHEEL_RGB");
        {
            struct ColorWheelRGB rgb = { 0xffffffff, 0x80000000, 0x00000000 };
            P_LONG("SetAttrs", SetAttrs(o, WHEEL_RGB, &rgb, TAG_DONE));
        }
        dump(o);
        P_SECTION("SetAttrs WHEEL_HSB");
        {
            struct ColorWheelHSB hsb = { 0xaaaaaaaa, 0xc0000000, 0xe0000000 };
            P_LONG("SetAttrs", SetAttrs(o, WHEEL_HSB, &hsb, TAG_DONE));
        }
        dump(o);

        P_SECTION("OM_SET return value");
        P_LONG("Hue/Sat/Bri 0", SetAttrs(o, WHEEL_Hue, 0, WHEEL_Saturation, 0x0000ffff, WHEEL_Brightness, 0, TAG_DONE));
        P_LONG("same values", SetAttrs(o, WHEEL_Hue, 0x1000, WHEEL_Saturation, 0, WHEEL_Brightness, 0, TAG_DONE));
        P_LONG("Red unchanged", SetAttrs(o, WHEEL_Red, 0x1000, TAG_DONE));
        P_LONG("Green", SetAttrs(o, WHEEL_Green, 0x80000000, TAG_DONE));
        P_LONG("Blue unchanged", SetAttrs(o, WHEEL_Blue, 0x1000, TAG_DONE));
        P_LONG("Hue + same Brightness", SetAttrs(o, WHEEL_Hue, 0x1000, WHEEL_Brightness, 0x80000000, TAG_DONE));
        P_LONG("Brightness + same Hue", SetAttrs(o, WHEEL_Brightness, 0x90000000, WHEEL_Hue, 0x2000, TAG_DONE));
        P_LONG("Brightness twice", SetAttrs(o, WHEEL_Brightness, 0x90000000, WHEEL_Brightness, 0x2000, TAG_DONE));
        P_LONG("Hue there and back", SetAttrs(o, WHEEL_Hue, 0x90000000, WHEEL_Hue, 0x2000, WHEEL_Hue, 3, TAG_DONE));
        P_LONG("GA_Left", SetAttrs(o, GA_Left, 3, TAG_DONE));
        P_LONG("no tags", SetAttrs(o, TAG_DONE));
        P_LONG("WHEEL_Abbrv", SetAttrs(o, WHEEL_Abbrv, "abcdef", TAG_DONE));
        P_LONG("GA_Disabled", SetAttrs(o, GA_Disabled, TRUE, TAG_DONE));
        P_HEX("Flags", ((struct Gadget *)o)->Flags);
        dump(o);

        P_SECTION("OM_GET of I-only attributes");
        attr("WHEEL_Screen", o, WHEEL_Screen);
        attr("WHEEL_Abbrv", o, WHEEL_Abbrv);
        attr("WHEEL_MaxPens", o, WHEEL_MaxPens);
        attr("WHEEL_GradientSlider", o, WHEEL_GradientSlider);
        attr("WHEEL_BevelBox", o, WHEEL_BevelBox);
        attr("WHEEL_Donation", o, WHEEL_Donation);
        DisposeObject(o);
    }

    P_SECTION("HSB attributes before RGB attributes");
    o = NewObject(NULL, (ClassID)"colorwheel.gadget", GA_Width, 60, GA_Height, 60, WHEEL_Screen, scr,
                  WHEEL_Hue, 0x40000000, WHEEL_Red, 0x12345678, WHEEL_Brightness, 0x80000000, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        dump(o);
        DisposeObject(o);
    }

    P_SECTION("NewObject without WHEEL_Screen");
    o = NewObject(NULL, (ClassID)"colorwheel.gadget", GA_Width, 60, GA_Height, 60, WHEEL_Blue, 0xffffffff, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        dump(o);
        DisposeObject(o);
    }

    P_SECTION("NewObject with initial values");
    o = NewObject(NULL, (ClassID)"colorwheel.gadget",
                  GA_Width, 40, GA_Height, 40, WHEEL_Screen, scr,
                  WHEEL_Red, 0x10000000, WHEEL_Green, 0x80000000, WHEEL_Blue, 0xf0000000,
                  WHEEL_Abbrv, "GCBMRY", WHEEL_MaxPens, 8, WHEEL_BevelBox, TRUE, TAG_DONE);
    P_NULL("object", o);
    if (o) {
        dump(o);
        DisposeObject(o);
    }

    P_SECTION("attached gradient slider");
    GradBase = OpenLibrary((CONST_STRPTR)"gadgets/gradientslider.gadget", 39);
    P_NULL("OpenLibrary gadgets/gradientslider.gadget", GradBase);
    grad = !GradBase ? NULL : NewObject(NULL, (ClassID)"gradientslider.gadget",
                     GA_Width, 20, GA_Height, 60, PGA_Freedom, LORIENT_VERT, TAG_DONE);
    P_NULL("slider", grad);
    o = grad ? NewObject(NULL, (ClassID)"colorwheel.gadget",
                         GA_Width, 60, GA_Height, 60, WHEEL_Screen, scr,
                         WHEEL_GradientSlider, grad,
                         WHEEL_Brightness, 0xc0000000, TAG_DONE) : NULL;
    P_NULL("object", o);
    if (o) {
        ULONG v = 0xdeadbeef;
        GetAttr(GRAD_CurVal, grad, &v);
        P_HEX("slider CurVal after OM_NEW", v);
        dump(o);
        SetAttrs(o, WHEEL_Brightness, 0x20000000, TAG_DONE);
        v = 0xdeadbeef;
        GetAttr(GRAD_CurVal, grad, &v);
        P_HEX("slider CurVal after WHEEL_Brightness", v);
        SetAttrs(grad, GRAD_CurVal, 0x1000, TAG_DONE);
        attr("Brightness after GRAD_CurVal 0x1000", o, WHEEL_Brightness);
        attr("Red after GRAD_CurVal 0x1000", o, WHEEL_Red);
        DisposeObject(o);
    }
    if (grad)
        DisposeObject(grad);
    if (GradBase)
        CloseLibrary(GradBase);

    UnlockPubScreen(NULL, scr);
    CloseLibrary(ColorWheelBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
