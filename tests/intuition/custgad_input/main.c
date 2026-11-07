/*
 * Test: intuition/custgad_input (Phase 252)
 *
 * How Intuition drives BOOPSI custom gadgets with the mouse and keyboard:
 * GM_HITTEST, GM_GOACTIVE, GM_HANDLEINPUT, GM_GOINACTIVE for a private
 * gadgetclass subclass, plus the IDCMP messages buttongclass, frbuttonclass,
 * propgclass (ICA_TARGET ICTARGET_IDCMP) and strgclass objects produce.
 * The scenario tests/scenarios/interactive/custgad_input.yaml clicks, drags
 * and types; stdout is compared with AmigaOS 3.1.
 *
 * Methods are logged into a table from the dispatcher (which runs in
 * Intuition's input context) and printed by the main loop.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <intuition/icclass.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/utility_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>
#include <inline/utility.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase = NULL;
struct Library *UtilityBase = NULL;

#define NLOG 96

struct logent {
    ULONG method;
    UWORD cls, code, qual;
    WORD x, y;
    LONG extra;
};

static struct logent g_log[NLOG];
static volatile int g_nlog;
static int g_shown;
static int g_moves;
static int g_timers;

static void logit(ULONG method, UWORD cls, UWORD code, UWORD qual, WORD x, WORD y, LONG extra)
{
    struct logent *e;

    if (g_nlog >= NLOG)
        return;
    e = &g_log[g_nlog];
    e->method = method;
    e->cls = cls;
    e->code = code;
    e->qual = qual;
    e->x = x;
    e->y = y;
    e->extra = extra;
    g_nlog++;
}

static ULONG call_super(struct IClass *cl, Object *obj, Msg msg)
{
    typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                   register Object *obj __asm("a2"),
                                   register Msg msg __asm("a1"));
    struct IClass *super = cl->cl_Super;

    return ((DispatchEntry)super->cl_Dispatcher.h_Entry)(super, obj, msg);
}

/* the logging gadget: active from the click until 'q' (VERIFY, code 42)
 * or Esc (NOREUSE); a click outside while active ends it with GMR_REUSE */
static ULONG dispatch(register struct IClass *cl __asm("a0"),
                      register Object *obj __asm("a2"),
                      register Msg msg __asm("a1"))
{
    struct Gadget *g = (struct Gadget *)obj;

    switch (msg->MethodID) {
    case GM_HITTEST: {
        struct gpHitTest *h = (struct gpHitTest *)msg;
        logit(GM_HITTEST, 0, 0, 0, h->gpht_Mouse.X, h->gpht_Mouse.Y, 0);
        return GMR_GADGETHIT;
    }
    case GM_GOACTIVE: {
        struct gpInput *gi = (struct gpInput *)msg;
        struct InputEvent *ie = gi->gpi_IEvent;
        logit(GM_GOACTIVE, ie ? ie->ie_Class : 0xffff, ie ? ie->ie_Code : 0, 0,
              gi->gpi_Mouse.X, gi->gpi_Mouse.Y, gi->gpi_Termination != NULL);
        g->Flags |= GFLG_SELECTED;
        return GMR_MEACTIVE;
    }
    case GM_HANDLEINPUT: {
        struct gpInput *gi = (struct gpInput *)msg;
        struct InputEvent *ie = gi->gpi_IEvent;
        if (ie->ie_Class == IECLASS_TIMER) {
            g_timers++;
            return GMR_MEACTIVE;
        }
        if (ie->ie_Class == IECLASS_RAWMOUSE && ie->ie_Code == IECODE_NOBUTTON) {
            g_moves++;
            return GMR_MEACTIVE;
        }
        logit(GM_HANDLEINPUT, ie->ie_Class, ie->ie_Code, ie->ie_Qualifier & 0x00ff,
              gi->gpi_Mouse.X, gi->gpi_Mouse.Y, 0);
        if (ie->ie_Class == IECLASS_RAWKEY && ie->ie_Code == 0x10) {
            *gi->gpi_Termination = 42;
            return GMR_NOREUSE | GMR_VERIFY;
        }
        if (ie->ie_Class == IECLASS_RAWKEY && ie->ie_Code == 0x45)
            return GMR_NOREUSE;
        if (ie->ie_Class == IECLASS_RAWMOUSE && ie->ie_Code == SELECTDOWN &&
            (gi->gpi_Mouse.X < 0 || gi->gpi_Mouse.Y < 0 ||
             gi->gpi_Mouse.X >= g->Width || gi->gpi_Mouse.Y >= g->Height))
            return GMR_REUSE;
        return GMR_MEACTIVE;
    }
    case GM_GOINACTIVE: {
        struct gpGoInactive *gi = (struct gpGoInactive *)msg;
        logit(GM_GOINACTIVE, 0, 0, 0, 0, 0, gi->gpgi_Abort);
        g->Flags &= ~GFLG_SELECTED;
        return 0;
    }
    case GM_RENDER: {
        struct gpRender *r = (struct gpRender *)msg;
        logit(GM_RENDER, 0, 0, 0, 0, 0, r->gpr_Redraw);
        return 0;
    }
    }
    return call_super(cl, obj, msg);
}

static const char *mname(ULONG m)
{
    switch (m) {
    case GM_HITTEST: return "HITTEST";
    case GM_GOACTIVE: return "GOACTIVE";
    case GM_HANDLEINPUT: return "HANDLEINPUT";
    case GM_GOINACTIVE: return "GOINACTIVE";
    case GM_RENDER: return "RENDER";
    }
    return "?";
}

static void flush_log(void)
{
    while (g_shown < g_nlog) {
        struct logent *e = &g_log[g_shown++];
        switch (e->method) {
        case GM_HITTEST:
            Printf("  %s mouse=%ld,%ld\n", (LONG)mname(e->method), (LONG)e->x, (LONG)e->y);
            break;
        case GM_GOACTIVE:
            Printf("  %s ie=%lx/%lx mouse=%ld,%ld term=%ld\n", (LONG)mname(e->method),
                   (LONG)e->cls, (LONG)e->code, (LONG)e->x, (LONG)e->y, e->extra);
            break;
        case GM_HANDLEINPUT:
            Printf("  %s ie=%lx/%lx qual=%lx mouse=%ld,%ld\n", (LONG)mname(e->method),
                   (LONG)e->cls, (LONG)e->code, (LONG)e->qual, (LONG)e->x, (LONG)e->y);
            break;
        case GM_GOINACTIVE:
            Printf("  %s abort=%ld\n", (LONG)mname(e->method), e->extra);
            break;
        case GM_RENDER:
            Printf("  %s redraw=%ld\n", (LONG)mname(e->method), e->extra);
            break;
        }
    }
}

#define NMSG 48
struct msgent { ULONG cls; UWORD id, code; LONG top; };
static struct msgent g_msgs[NMSG];
static int g_nmsg;

static void addmsg(ULONG cls, UWORD id, UWORD code, LONG top)
{
    if (g_nmsg < NMSG) {
        g_msgs[g_nmsg].cls = cls;
        g_msgs[g_nmsg].id = id;
        g_msgs[g_nmsg].code = code;
        g_msgs[g_nmsg].top = top;
        g_nmsg++;
    }
}

static LONG g_last_top = -1;
static LONG g_pending_top = -1;

static void flush_update(void)
{
    if (g_pending_top >= 0 && g_pending_top != g_last_top) {
        addmsg(IDCMP_IDCMPUPDATE, 0, 0, g_pending_top);
        g_last_top = g_pending_top;
    }
    g_pending_top = -1;
}

static const char *cname(ULONG c)
{
    switch (c) {
    case IDCMP_GADGETDOWN: return "GADGETDOWN";
    case IDCMP_GADGETUP: return "GADGETUP";
    case IDCMP_MOUSEBUTTONS: return "MOUSEBUTTONS";
    case IDCMP_CLOSEWINDOW: return "CLOSEWINDOW";
    case IDCMP_IDCMPUPDATE: return "IDCMPUPDATE";
    }
    return "?";
}

int main(void)
{
    struct IClass *cl;
    struct Screen *scr;
    struct DrawInfo *dri;
    struct Gadget *glist = NULL, *loggad, *button, *frbutton, *prop, *str;
    Object *frame;
    struct Window *win;
    BOOL done = FALSE;
    int idle = 0;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    UtilityBase = OpenLibrary((STRPTR)"utility.library", 37);
    if (!IntuitionBase || !UtilityBase)
        return 20;
    scr = LockPubScreen(NULL);
    dri = GetScreenDrawInfo(scr);

    cl = MakeClass(NULL, (ClassID)"gadgetclass", NULL, 0, 0);
    cl->cl_Dispatcher.h_Entry = (ULONG (*)())dispatch;

    loggad = (struct Gadget *)NewObject(cl, NULL, GA_Left, 20, GA_Top, 20, GA_Width, 60,
                                        GA_Height, 20, GA_ID, 1, GA_RelVerify, TRUE,
                                        GA_Immediate, TRUE, GA_Previous, (ULONG)&glist, TAG_END);
    button = (struct Gadget *)NewObject(NULL, (ClassID)"buttongclass", GA_Left, 100, GA_Top, 20,
                                        GA_Width, 60, GA_Height, 20, GA_ID, 2,
                                        GA_RelVerify, TRUE, GA_Immediate, TRUE,
                                        GA_Text, (ULONG)"Btn", GA_Previous, (ULONG)loggad, TAG_END);
    frame = NewObject(NULL, (ClassID)"frameiclass", IA_FrameType, FRAME_BUTTON, TAG_END);
    frbutton = (struct Gadget *)NewObject(NULL, (ClassID)"frbuttonclass", GA_Left, 100, GA_Top, 60,
                                          GA_ID, 5, GA_RelVerify, TRUE, GA_Image, (ULONG)frame,
                                          GA_Text, (ULONG)"Frame", GA_DrawInfo, (ULONG)dri,
                                          GA_Previous, (ULONG)button, TAG_END);
    prop = (struct Gadget *)NewObject(NULL, (ClassID)"propgclass", GA_Left, 200, GA_Top, 20,
                                      GA_Width, 16, GA_Height, 80, GA_ID, 3, GA_RelVerify, TRUE,
                                      PGA_Total, 100, PGA_Visible, 10, PGA_Top, 0,
                                      PGA_NewLook, TRUE, ICA_TARGET, ICTARGET_IDCMP,
                                      GA_Previous, (ULONG)frbutton, TAG_END);
    str = (struct Gadget *)NewObject(NULL, (ClassID)"strgclass", GA_Left, 20, GA_Top, 60,
                                     GA_Width, 60, GA_Height, 12, GA_ID, 4, GA_RelVerify, TRUE,
                                     STRINGA_MaxChars, 20, GA_Previous, (ULONG)prop, TAG_END);
    if (!loggad || !button || !frbutton || !prop || !str) {
        Printf("NewObject failed %lx %lx %lx %lx %lx\n", (LONG)loggad, (LONG)button,
               (LONG)frbutton, (LONG)prop, (LONG)str);
        return 20;
    }

    win = OpenWindowTags(NULL, WA_Left, 10, WA_Top, 20, WA_Width, 260, WA_Height, 130,
                         WA_Title, (ULONG)"CustGad", WA_Gadgets, (ULONG)glist,
                         WA_DragBar, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
                         WA_IDCMP, IDCMP_GADGETDOWN | IDCMP_GADGETUP | IDCMP_IDCMPUPDATE |
                                   IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS,
                         TAG_END);
    if (!win)
        return 20;
    Delay(10);
    Printf("Ready\n");
    Flush(Output());

    while (!done && idle < 3000) {
        struct IntuiMessage *im;
        while ((im = (struct IntuiMessage *)GetMsg(win->UserPort))) {
            ULONG cls = im->Class;
            UWORD code = im->Code;
            struct Gadget *g = (struct Gadget *)im->IAddress;
            if (cls == IDCMP_IDCMPUPDATE) {
                struct TagItem *t = FindTagItem(PGA_Top, (struct TagItem *)im->IAddress);
                if (t)
                    g_pending_top = t->ti_Data;
                ReplyMsg((struct Message *)im);
                continue;
            }
            ReplyMsg((struct Message *)im);
            flush_update();
            if (cls == IDCMP_GADGETDOWN || cls == IDCMP_GADGETUP)
                addmsg(cls, g->GadgetID, code, -1);
            else if (cls == IDCMP_MOUSEBUTTONS)
                addmsg(cls, 0, code, -1);
            else if (cls == IDCMP_CLOSEWINDOW)
                done = TRUE;
        }
        if (!done)
            Delay(1);
        idle++;
    }
    flush_update();
    Printf("methods of the logging gadget:\n");
    flush_log();
    Printf("IDCMP:\n");
    {
        int i;
        for (i = 0; i < g_nmsg; i++) {
            struct msgent *m = &g_msgs[i];
            if (m->cls == IDCMP_IDCMPUPDATE)
                Printf("  %s PGA_Top=%ld\n", (LONG)cname(m->cls), m->top);
            else if (m->cls == IDCMP_MOUSEBUTTONS)
                Printf("  %s code=%lx\n", (LONG)cname(m->cls), (LONG)m->code);
            else if (m->id == 1 || m->id == 4 || m->cls == IDCMP_GADGETDOWN)
                Printf("  %s id=%ld code=%ld\n", (LONG)cname(m->cls), (LONG)m->id, (LONG)m->code);
            else /* built-in classes leave the termination code undefined */
                Printf("  %s id=%ld\n", (LONG)cname(m->cls), (LONG)m->id);
        }
    }
    {
        ULONG top = 0;
        STRPTR text = NULL;
        GetAttr(PGA_Top, (Object *)prop, &top);
        GetAttr(STRINGA_TextVal, (Object *)str, (ULONG *)&text);
        Printf("final top=%ld text=\"%s\" moves=%s timers=%s\n", (LONG)top,
               (LONG)(text ? text : (STRPTR)""), (LONG)(g_moves ? "yes" : "no"),
               (LONG)(g_timers ? "yes" : "no"));
        Printf("flags log=%lx button=%lx prop=%lx str=%lx\n", (LONG)loggad->Flags,
               (LONG)button->Flags, (LONG)prop->Flags, (LONG)str->Flags);
    }
    CloseWindow(win);
    DisposeObject((Object *)str);
    DisposeObject((Object *)prop);
    DisposeObject((Object *)frbutton);
    DisposeObject(frame);
    DisposeObject((Object *)button);
    DisposeObject((Object *)loggad);
    FreeClass(cl);
    FreeScreenDrawInfo(scr, dri);
    UnlockPubScreen(NULL, scr);
    Printf("Done\n");
    Flush(Output());
    return 0;
}
