/*
 * Probe (Phase 243): which windows' MouseX/MouseY follow the pointer?
 * Personal Paint reads its backdrop window's MouseX/MouseY after a
 * requester window closes; on AmigaOS 3.1 they hold the pointer position
 * even though another window was active while the pointer moved.  The
 * pointer is moved through input.device (IECLASS_NEWPOINTERPOS).  C is a
 * GimmeZeroZero window (GZZMouseX/Y).
 */
#include <exec/types.h>
#include <exec/ports.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/alib_protos.h>
#include <string.h>
#include "probe.h"

struct Library *IntuitionBase;

static struct MsgPort *g_iport;
static struct IOStdReq *g_ireq;
static struct Screen *scr;

static void pointer_to(WORD x, WORD y)
{
    struct InputEvent ie;
    struct IEPointerPixel pp;

    memset(&ie, 0, sizeof(ie));
    pp.iepp_Screen = scr;
    pp.iepp_Position.X = x;
    pp.iepp_Position.Y = y;
    ie.ie_Class = IECLASS_NEWPOINTERPOS;
    ie.ie_SubClass = IESUBCLASS_PIXEL;
    ie.ie_Code = IECODE_NOBUTTON;
    ie.ie_EventAddress = (APTR)&pp;
    g_ireq->io_Command = IND_WRITEEVENT;
    g_ireq->io_Data = (APTR)&ie;
    g_ireq->io_Length = sizeof(ie);
    DoIO((struct IORequest *)g_ireq);
    Delay(5);
}

static void drain(struct Window *w)
{
    struct Message *m;
    if (w && w->UserPort)
        while ((m = GetMsg(w->UserPort)))
            ReplyMsg(m);
}

static void pos(const char *name, struct Window *w)
{
    probe_s(" ");
    probe_s(name);
    probe_s("=");
    if (!w)
    {
        probe_s("-");
        return;
    }
    probe_dec(w->MouseX);
    probe_ch(',');
    probe_dec(w->MouseY);
    probe_s(" gzz ");
    probe_dec(w->GZZMouseX);
    probe_ch(',');
    probe_dec(w->GZZMouseY);
}

static void show(const char *label, struct Window *a, struct Window *b, struct Window *c)
{
    probe_s(label);
    probe_s(":");
    pos("A", a);
    pos("B", b);
    pos("C", c);
    probe_s(" screen=");
    probe_dec(scr->MouseX);
    probe_ch(',');
    probe_dec(scr->MouseY);
    probe_ch('\n');
}

int main(void)
{
    struct Window *a, *b, *c;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 0;
    g_iport = CreateMsgPort();
    g_ireq = g_iport ? (struct IOStdReq *)CreateIORequest(g_iport, sizeof(struct IOStdReq)) : NULL;
    if (!g_ireq || OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)g_ireq, 0))
    {
        probe_s("input.device: cannot open\n");
        return 0;
    }
    scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 200, SA_Depth, 2,
                         SA_DisplayID, 0x8000, SA_Title, (ULONG)"Probe", TAG_END);
    if (!scr)
    {
        probe_s("screen: NULL\n");
        return 0;
    }
    a = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)scr, WA_Left, 0, WA_Top, 0, WA_Width, 640,
                       WA_Height, 200, WA_Borderless, TRUE, WA_Backdrop, TRUE,
                       WA_IDCMP, IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS, WA_ReportMouse, TRUE, TAG_END);
    c = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)scr, WA_Left, 400, WA_Top, 50, WA_Width, 200,
                       WA_Height, 100, WA_Title, (ULONG)"C", WA_DragBar, TRUE, WA_GimmeZeroZero, TRUE,
                       WA_IDCMP, IDCMP_MOUSEMOVE, WA_ReportMouse, TRUE, TAG_END);
    b = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)scr, WA_Left, 100, WA_Top, 50, WA_Width, 200,
                       WA_Height, 100, WA_Title, (ULONG)"B", WA_DragBar, TRUE, WA_Activate, TRUE,
                       WA_IDCMP, IDCMP_MOUSEMOVE, WA_ReportMouse, TRUE, TAG_END);
    if (!a || !b || !c)
    {
        probe_s("windows: NULL\n");
        return 0;
    }
    ScreenToFront(scr);
    Delay(10);
    pointer_to(150, 80);
    show("in B (B active)", a, b, c);
    pointer_to(450, 100);
    show("in C (B active)", a, b, c);
    pointer_to(20, 180);
    show("in A only (B active)", a, b, c);
    ReportMouse(FALSE, b);
    ModifyIDCMP(b, 0);
    pointer_to(30, 170);
    show("B without MOUSEMOVE", a, b, c);
    drain(a);
    drain(c);
    ActivateWindow(a);
    Delay(10);
    pointer_to(450, 90);
    show("in C (A active)", a, b, c);
    drain(a);
    drain(c);
    CloseWindow(b);
    b = NULL;
    Delay(10);
    pointer_to(320, 160);
    show("B closed", a, b, c);
    drain(a);
    drain(c);
    CloseWindow(c);
    CloseWindow(a);
    CloseScreen(scr);
    CloseDevice((struct IORequest *)g_ireq);
    DeleteIORequest((struct IORequest *)g_ireq);
    DeleteMsgPort(g_iport);
    CloseLibrary(IntuitionBase);
    return 0;
}
