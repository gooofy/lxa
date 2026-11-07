/*
 * Test: intuition/gadclass_input (Phase 252)
 *
 * The Workbench 3.1 gadget classes colorwheel.gadget, gradientslider.gadget
 * and tapedeck.gadget driven with the mouse: Intuition must send them
 * GM_HITTEST, GM_GOACTIVE and GM_HANDLEINPUT like any custom gadget.  The
 * scenario tests/scenarios/interactive/gadclass_input.yaml clicks and drags;
 * the IDCMP messages and final attributes are compared with AmigaOS 3.1.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <gadgets/colorwheel.h>
#include <gadgets/gradientslider.h>
#include <gadgets/tapedeck.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/utility_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>
#include <inline/utility.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase = NULL;
struct Library *UtilityBase = NULL;

#define NMSG 128

struct msgent {
    ULONG cls;
    UWORD id, code;
    ULONG tag, data;
};

static struct msgent g_msgs[NMSG];
static int g_nmsg;

static void addmsg(ULONG cls, UWORD id, UWORD code, ULONG tag, ULONG data)
{
    struct msgent *m;

    if (g_nmsg >= NMSG)
        return;
    m = &g_msgs[g_nmsg++];
    m->cls = cls;
    m->id = id;
    m->code = code;
    m->tag = tag;
    m->data = data;
}

/* GRAD_* and TDECK_* share their tag base: names depend on the gadget */
static const char *tagname(UWORD id, ULONG tag)
{
    if (id == 2)
        return tag == GRAD_CurVal ? "GRAD_CurVal" : NULL;
    if (id == 3)
        return tag == TDECK_Mode ? "TDECK_Mode" : tag == TDECK_Paused ? "TDECK_Paused" :
               tag == TDECK_CurrentFrame ? "TDECK_CurrentFrame" : NULL;
    switch (tag) {
    case GA_ID: return "GA_ID";
    case WHEEL_Hue: return "WHEEL_Hue";
    case WHEEL_Saturation: return "WHEEL_Saturation";
    case WHEEL_Brightness: return "WHEEL_Brightness";
    case WHEEL_HSB: return "WHEEL_HSB";
    case WHEEL_Red: return "WHEEL_Red";
    case WHEEL_Green: return "WHEEL_Green";
    case WHEEL_Blue: return "WHEEL_Blue";
    case WHEEL_RGB: return "WHEEL_RGB";
    default: return NULL;
    }
}

int main(void)
{
    struct Library *WheelBase, *GradBase, *TapeBase;
    struct Screen *scr;
    struct Gadget *glist = NULL, *wheel, *grad, *tape;
    struct Window *win;
    BOOL done = FALSE;
    int idle = 0;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    UtilityBase = OpenLibrary((STRPTR)"utility.library", 39);
    WheelBase = OpenLibrary((STRPTR)"gadgets/colorwheel.gadget", 39);
    GradBase = OpenLibrary((STRPTR)"gadgets/gradientslider.gadget", 39);
    TapeBase = OpenLibrary((STRPTR)"gadgets/tapedeck.gadget", 39);
    if (!IntuitionBase || !UtilityBase || !WheelBase || !GradBase || !TapeBase) {
        Printf("libraries missing\n");
        return 20;
    }
    scr = LockPubScreen(NULL);

    grad = (struct Gadget *)NewObject(NULL, (ClassID)"gradientslider.gadget",
                                      GA_Left, 20, GA_Top, 110, GA_Width, 160, GA_Height, 12,
                                      GA_ID, 2, GA_RelVerify, TRUE, GA_Immediate, TRUE,
                                      PGA_Freedom, LORIENT_HORIZ, GRAD_MaxVal, 0xffff,
                                      GRAD_CurVal, 0x8000, GRAD_KnobPixels, 8,
                                      ICA_TARGET, ICTARGET_IDCMP, GA_Previous, (ULONG)&glist, TAG_END);
    wheel = (struct Gadget *)NewObject(NULL, (ClassID)"colorwheel.gadget",
                                       GA_Left, 20, GA_Top, 20, GA_Width, 80, GA_Height, 80,
                                       GA_ID, 1, GA_RelVerify, TRUE, GA_Immediate, TRUE,
                                       WHEEL_Screen, (ULONG)scr, WHEEL_BevelBox, TRUE,
                                       WHEEL_Hue, 0, WHEEL_Saturation, 0,
                                       ICA_TARGET, ICTARGET_IDCMP, GA_Previous, (ULONG)grad, TAG_END);
    tape = (struct Gadget *)NewObject(NULL, (ClassID)"tapedeck.gadget",
                                      GA_Left, 120, GA_Top, 40, GA_Width, 201, GA_Height, 15,
                                      GA_ID, 3, GA_RelVerify, TRUE, TDECK_Tape, TRUE,
                                      ICA_TARGET, ICTARGET_IDCMP, GA_Previous, (ULONG)wheel, TAG_END);
    if (!grad || !wheel || !tape) {
        Printf("NewObject failed %lx %lx %lx\n", (LONG)grad, (LONG)wheel, (LONG)tape);
        return 20;
    }

    win = OpenWindowTags(NULL, WA_Left, 10, WA_Top, 20, WA_Width, 340, WA_Height, 140,
                         WA_Title, (ULONG)"GadClass", WA_Gadgets, (ULONG)glist,
                         WA_DragBar, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
                         WA_IDCMP, IDCMP_GADGETDOWN | IDCMP_GADGETUP | IDCMP_IDCMPUPDATE |
                                   IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS,
                         TAG_END);
    if (!win)
        return 20;
    Delay(10);
    Printf("Ready\n");
    Flush(Output());

    while (!done && idle < 30000) {
        struct IntuiMessage *im;
        while ((im = (struct IntuiMessage *)GetMsg(win->UserPort))) {
            ULONG cls = im->Class;
            UWORD code = im->Code;
            struct Gadget *g = (struct Gadget *)im->IAddress;
            if (cls == IDCMP_IDCMPUPDATE) {
                struct TagItem *tstate = (struct TagItem *)im->IAddress, *t;
                UWORD id = (UWORD)GetTagData(GA_ID, 0, tstate);
                /* only the last value of a drag is compared: the number of
                 * interim updates depends on the input timing */
                while ((t = NextTagItem(&tstate)))
                    if (t->ti_Tag != GA_ID && tagname(id, t->ti_Tag)) {
                        if (g_nmsg && g_msgs[g_nmsg - 1].cls == cls &&
                            g_msgs[g_nmsg - 1].id == id && g_msgs[g_nmsg - 1].tag == t->ti_Tag)
                            g_msgs[g_nmsg - 1].data = t->ti_Data;
                        else
                            addmsg(cls, id, 0, t->ti_Tag, t->ti_Data);
                    }
            }
            else if (cls == IDCMP_GADGETDOWN || cls == IDCMP_GADGETUP) {
                addmsg(cls, g->GadgetID, code, 0, 0);
                /* the state when the gadget lets go */
                if (cls == IDCMP_GADGETUP && g->GadgetID == 1) {
                    ULONG h = 0, sat = 0;
                    GetAttr(WHEEL_Hue, (Object *)wheel, &h);
                    GetAttr(WHEEL_Saturation, (Object *)wheel, &sat);
                    addmsg(0, 1, 0, h, sat);
                }
                else if (cls == IDCMP_GADGETUP && g->GadgetID == 3) {
                    ULONG mode = 0, paused = 0;
                    GetAttr(TDECK_Mode, (Object *)tape, &mode);
                    GetAttr(TDECK_Paused, (Object *)tape, &paused);
                    addmsg(0, 3, code, mode, paused);
                }
            }
            else if (cls == IDCMP_MOUSEBUTTONS)
                addmsg(cls, 0, code, 0, 0);
            else if (cls == IDCMP_CLOSEWINDOW)
                done = TRUE;
            ReplyMsg((struct Message *)im);
        }
        if (!done)
            Delay(1);
        idle++;
    }
    Printf("IDCMP:\n");
    {
        int i;
        for (i = 0; i < g_nmsg; i++) {
            struct msgent *m = &g_msgs[i];
            if (m->cls == 0 && m->id == 1)
                Printf("    hue=%lx sat=%lx\n", m->tag, m->data);
            else if (m->cls == 0)
                Printf("    code=%ld mode=%ld paused=%ld\n", (LONG)m->code, m->tag, m->data);
            else if (m->cls == IDCMP_IDCMPUPDATE)
                Printf("  IDCMPUPDATE id=%ld %s=%lx\n", (LONG)m->id, (LONG)tagname(m->id, m->tag), m->data);
            else if (m->cls == IDCMP_MOUSEBUTTONS)
                Printf("  MOUSEBUTTONS code=%lx\n", (LONG)m->code);
            else
                Printf("  %s id=%ld\n", (LONG)(m->cls == IDCMP_GADGETDOWN ? "GADGETDOWN" : "GADGETUP"),
                       (LONG)m->id);
        }
    }
    {
        ULONG hue = 0, sat = 0, bri = 0, cur = 0, mode = 0;
        GetAttr(WHEEL_Hue, (Object *)wheel, &hue);
        GetAttr(WHEEL_Saturation, (Object *)wheel, &sat);
        GetAttr(WHEEL_Brightness, (Object *)wheel, &bri);
        GetAttr(GRAD_CurVal, (Object *)grad, &cur);
        GetAttr(TDECK_Mode, (Object *)tape, &mode);
        Printf("final hue=%lx sat=%lx bri=%lx grad=%lx mode=%ld\n", hue, sat, bri, cur, mode);
        Printf("flags wheel=%lx grad=%lx tape=%lx\n", (LONG)wheel->Flags, (LONG)grad->Flags,
               (LONG)tape->Flags);
    }
    CloseWindow(win);
    DisposeObject((Object *)tape);
    DisposeObject((Object *)wheel);
    DisposeObject((Object *)grad);
    UnlockPubScreen(NULL, scr);
    CloseLibrary(TapeBase);
    CloseLibrary(GradBase);
    CloseLibrary(WheelBase);
    Printf("Done\n");
    Flush(Output());
    return 0;
}
