/*
 * Probe: colorwheel.gadget, gradientslider.gadget and tapedeck.gadget in a
 * window - GM_RENDER (full redraw and updates through SetGadgetAttrs()),
 * the attached gradient slider, disabling, RemoveGList() and disposal.
 * The pixels are not compared (pen allocation differs per machine); the
 * probe checks that the classes draw without trouble and that their
 * attribute values stay right while they are on display.
 */
#include <exec/memory.h>
#include <exec/execbase.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <gadgets/colorwheel.h>
#include <gadgets/gradientslider.h>
#include <gadgets/tapedeck.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/graphics.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;
struct GfxBase *GfxBase;

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

int main(void)
{
    struct Library *cwb, *gsb, *tdb;
    struct Screen *scr;
    struct Window *win;
    Object *wheel, *grad, *deck;
    static UWORD pens[] = { 1, 2, (UWORD)~0 };

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase)
        return 20;
    cwb = OpenLibrary((CONST_STRPTR)"gadgets/colorwheel.gadget", 39);
    gsb = OpenLibrary((CONST_STRPTR)"gadgets/gradientslider.gadget", 39);
    tdb = OpenLibrary((CONST_STRPTR)"gadgets/tapedeck.gadget", 39);
    P_NULL("colorwheel.gadget", cwb);
    P_NULL("gradientslider.gadget", gsb);
    P_NULL("tapedeck.gadget", tdb);
    if (!cwb || !gsb || !tdb)
        return 20;

    scr = LockPubScreen(NULL);
    if (!scr)
        return 20;
    win = OpenWindowTags(NULL, WA_Left, 20, WA_Top, 20, WA_Width, 320, WA_Height, 150,
                         WA_Title, (ULONG)"gadget classes", WA_PubScreen, (ULONG)scr,
                         WA_Flags, WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE,
                         TAG_DONE);
    P_NULL("window", win);
    if (!win)
        return 20;

    grad = NewObject(NULL, (ClassID)"gradientslider.gadget",
                     GA_Left, 100, GA_Top, 20, GA_Width, 16, GA_Height, 90, GA_ID, 2,
                     PGA_Freedom, LORIENT_VERT, GRAD_PenArray, (ULONG)pens, TAG_DONE);
    wheel = NewObject(NULL, (ClassID)"colorwheel.gadget",
                      GA_Left, 10, GA_Top, 20, GA_Width, 80, GA_Height, 80, GA_ID, 1,
                      GA_Previous, (ULONG)grad,
                      WHEEL_Screen, (ULONG)scr, WHEEL_GradientSlider, (ULONG)grad,
                      WHEEL_Red, 0x40000000, WHEEL_Green, 0xc0000000, WHEEL_Blue, 0x80000000,
                      TAG_DONE);
    deck = NewObject(NULL, (ClassID)"tapedeck.gadget",
                     GA_Left, 110, GA_Top, 120, GA_ID, 3, GA_Previous, (ULONG)wheel,
                     TDECK_Frames, 50, TDECK_CurrentFrame, 10, TAG_DONE);
    P_NULL("wheel", wheel);
    P_NULL("slider", grad);
    P_NULL("deck", deck);
    if (!wheel || !grad || !deck)
        return 20;

    P_SECTION("added and refreshed");
    AddGList(win, (struct Gadget *)grad, -1, -1, NULL);
    RefreshGList((struct Gadget *)grad, win, NULL, -1);
    attr("wheel Hue", wheel, WHEEL_Hue);
    attr("wheel Brightness", wheel, WHEEL_Brightness);
    attr("slider CurVal", grad, GRAD_CurVal);
    attr("deck CurrentFrame", deck, TDECK_CurrentFrame);

    P_SECTION("SetGadgetAttrs while displayed");
    SetGadgetAttrs((struct Gadget *)wheel, win, NULL, WHEEL_Hue, 0x20000000, WHEEL_Saturation, 0x80000000, TAG_DONE);
    SetGadgetAttrs((struct Gadget *)wheel, win, NULL, WHEEL_Brightness, 0x60000000, TAG_DONE);
    SetGadgetAttrs((struct Gadget *)grad, win, NULL, GRAD_CurVal, 0x2000, TAG_DONE);
    SetGadgetAttrs((struct Gadget *)deck, win, NULL, TDECK_Mode, BUT_PLAY, TDECK_CurrentFrame, 20, TAG_DONE);
    attr("wheel Hue", wheel, WHEEL_Hue);
    attr("wheel Saturation", wheel, WHEEL_Saturation);
    attr("wheel Brightness", wheel, WHEEL_Brightness);
    attr("wheel Red", wheel, WHEEL_Red);
    attr("slider CurVal", grad, GRAD_CurVal);
    attr("deck Mode", deck, TDECK_Mode);
    attr("deck CurrentFrame", deck, TDECK_CurrentFrame);

    P_SECTION("disabled");
    SetGadgetAttrs((struct Gadget *)wheel, win, NULL, GA_Disabled, TRUE, TAG_DONE);
    SetGadgetAttrs((struct Gadget *)grad, win, NULL, GA_Disabled, TRUE, TAG_DONE);
    SetGadgetAttrs((struct Gadget *)deck, win, NULL, GA_Disabled, TRUE, TAG_DONE);
    P_HEX("wheel Flags", ((struct Gadget *)wheel)->Flags);
    P_HEX("slider Flags", ((struct Gadget *)grad)->Flags);
    P_HEX("deck Flags", ((struct Gadget *)deck)->Flags);
    RefreshGList((struct Gadget *)grad, win, NULL, -1);

    P_SECTION("removed");
    P_LONG("RemoveGList", RemoveGList(win, (struct Gadget *)grad, 3));
    CloseWindow(win);
    DisposeObject(deck);
    DisposeObject(wheel);
    DisposeObject(grad);
    UnlockPubScreen(NULL, scr);
    P_SECTION("done");

    CloseLibrary(tdb);
    CloseLibrary(gsb);
    CloseLibrary(cwb);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
