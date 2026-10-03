/*
 * Probe (Phase 222d): intuition.library BuildEasyRequestArgs,
 * BuildSysRequest, FreeSysRequest, SysReqHandler, InitRequester,
 * Request, EndRequest - the window, gadgets and texts of EasyRequest and
 * AutoRequest requesters (size rule, button spacing, title, IDCMP), where
 * they open relative to the pointer, and the fields of a window requester.
 */
#include <exec/memory.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/alib_protos.h>
#include <string.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;

static struct MsgPort *g_iport;
static struct IOStdReq *g_ireq;

/* move the pointer through input.device, like a mouse would */
static void pointer_to(struct Screen *s, WORD x, WORD y)
{
    struct InputEvent ie;
    struct IEPointerPixel pp;

    if (!g_ireq)
        return;
    memset(&ie, 0, sizeof(ie));
    pp.iepp_Screen = s;
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

static void dump_req_window(struct Window *w, BOOL with_pos)
{
    probe_s("size"); ps_kv("w", w->Width); ps_kv("h", w->Height);
    if (with_pos)
    {
        ps_kv("left", w->LeftEdge); ps_kv("top", w->TopEdge);
    }
    probe_ch('\n');
    probe_s("  ");
    ps_kv("bl", w->BorderLeft); ps_kv("bt", w->BorderTop);
    ps_kv("br", w->BorderRight); ps_kv("bb", w->BorderBottom);
    ps_kv("minw", w->MinWidth); ps_kv("minh", w->MinHeight);
    ps_kv("maxw", (UWORD)w->MaxWidth); ps_kv("maxh", (UWORD)w->MaxHeight);
    probe_ch('\n');
    probe_s("  ");
    ps_kx("flags", w->Flags & ~(WFLG_WINDOWACTIVE | WFLG_WINDOWTICKED), 8);
    ps_kx("idcmp", w->IDCMPFlags, 8);
    ps_str("title", (const char *)w->Title);
    probe_ch('\n');
    /* the buttons' geometry; how 3.1 builds them (BOOPSI button gadgets,
     * a frame gadget) is roadmap Phase 252 */
    {
        struct Gadget *g;
        for (g = w->FirstGadget; g; g = g->NextGadget)
            if (!(g->GadgetType & GTYP_SYSGADGET) && (g->Activation & GACT_RELVERIFY))
            {
                probe_s("button");
                ps_kv("id", g->GadgetID);
                ps_box(g->LeftEdge, g->TopEdge, g->Width, g->Height);
                probe_ch('\n');
            }
    }
}

static void easy(const char *name, struct Window *ref, struct EasyStruct *es, ULONG idcmp, APTR args)
{
    ULONG flags = idcmp;
    struct Window *w;

    P_SECTION(name);
    w = BuildEasyRequestArgs(ref, es, flags, args);
    if (!w || w == (struct Window *)1)
    {
        P_LONG("BuildEasyRequest", (LONG)w);
        return;
    }
    dump_req_window(w, ref != NULL);
    P_LONG("SysReqHandler(no wait)", SysReqHandler(w, NULL, FALSE));
    FreeSysRequest(w);
}

static struct IntuiText body2 = { 0, 1, JAM2, 0, 10, NULL, (UBYTE *)"Second line of the body", NULL };
static struct IntuiText body1 = { 0, 1, JAM2, 5, 3, NULL, (UBYTE *)"AutoRequest body", &body2 };
static struct IntuiText postxt = { 0, 1, JAM2, 5, 3, NULL, (UBYTE *)"Retry", NULL };
static struct IntuiText negtxt = { 0, 1, JAM2, 5, 3, NULL, (UBYTE *)"Cancel", NULL };
static struct IntuiText shortb = { 0, 1, JAM2, 0, 0, NULL, (UBYTE *)"x", NULL };

static void sysreq(const char *name, struct Window *ref, struct IntuiText *b, struct IntuiText *p,
                   struct IntuiText *n, ULONG flags, WORD w, WORD h)
{
    struct Window *win;

    P_SECTION(name);
    win = BuildSysRequest(ref, b, p, n, flags, w, h);
    if (!win || win == (struct Window *)1)
    {
        P_LONG("BuildSysRequest", (LONG)win);
        return;
    }
    dump_req_window(win, ref != NULL);
    FreeSysRequest(win);
}

static void pos_case(const char *name, struct Screen *s, WORD x, WORD y)
{
    struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)"Pos", (UBYTE *)"Where am I?",
                             (UBYTE *)"OK" };
    struct Window *w;

    P_SECTION(name);
    pointer_to(s, x, y);
    P_LONG("mouse x", s->MouseX);
    P_LONG("mouse y", s->MouseY);
    w = BuildEasyRequestArgs(NULL, &es, 0, NULL);
    if (w && w != (struct Window *)1)
    {
        P_LONG("left", w->LeftEdge);
        P_LONG("top", w->TopEdge);
        P_LONG("width", w->Width);
        P_LONG("height", w->Height);
        FreeSysRequest(w);
    }
    w = BuildSysRequest(NULL, &body1, &postxt, &negtxt, 0, 200, 60);
    if (w && w != (struct Window *)1)
    {
        P_LONG("sys left", w->LeftEdge);
        P_LONG("sys top", w->TopEdge);
        FreeSysRequest(w);
    }
}

int main(void)
{
    struct Screen *s;
    struct Window *win;
    struct Requester req;
    struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)"Probe Request",
                             (UBYTE *)"Body text", (UBYTE *)"OK" };
    LONG args[4];

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 20;
    s = LockPubScreen(NULL);
    if (!s)
        return 20;
    g_iport = CreateMsgPort();
    g_ireq = g_iport ? (struct IOStdReq *)CreateIORequest(g_iport, sizeof(struct IOStdReq)) : NULL;
    if (g_ireq && OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)g_ireq, 0))
    {
        DeleteIORequest((struct IORequest *)g_ireq);
        g_ireq = NULL;
    }

    pointer_to(s, 0, 0);
    win = OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 40, WA_Top, 30, WA_Width, 300, WA_Height, 120,
             WA_Title, (ULONG)"Probe parent", WA_DragBar, TRUE, WA_CloseGadget, TRUE, TAG_END);
    if (!win)
        return 20;

    /* sizes do not depend on the pointer: requesters on the parent window */
    easy("easy one button", win, &es, 0, NULL);
    es.es_GadgetFormat = (UBYTE *)"Yes|No";
    easy("easy two buttons", win, &es, 0, NULL);
    es.es_GadgetFormat = (UBYTE *)"One|Two|Three|Four";
    easy("easy four buttons", win, &es, IDCMP_DISKINSERTED, NULL);
    es.es_GadgetFormat = (UBYTE *)"A very long button text|B";
    es.es_TextFormat = (UBYTE *)"Line one\nA much longer second line of text\n\nfourth";
    easy("easy multiline long button", win, &es, 0, NULL);
    es.es_Title = NULL;
    es.es_TextFormat = (UBYTE *)"Value %ld and %s";
    es.es_GadgetFormat = (UBYTE *)"%s|Cancel";
    args[0] = 42; args[1] = (LONG)"text"; args[2] = (LONG)"Fmt";
    easy("easy no title, formatted", win, &es, 0, args);
    es.es_Title = (UBYTE *)"A title that is much wider than the body text of this requester";
    es.es_TextFormat = (UBYTE *)"x";
    es.es_GadgetFormat = (UBYTE *)"OK";
    easy("easy wide title", win, &es, 0, NULL);
    es.es_Title = (UBYTE *)"T";
    es.es_TextFormat = (UBYTE *)"";
    es.es_GadgetFormat = (UBYTE *)"Only";
    easy("easy empty body", win, &es, 0, NULL);

    sysreq("sys pos neg", win, &body1, &postxt, &negtxt, 0, 300, 70);
    sysreq("sys small size", win, &body1, &postxt, &negtxt, 0, 50, 20);
    sysreq("sys large size", win, &body1, &postxt, &negtxt, 0, 500, 150);
    sysreq("sys neg only", win, &body1, NULL, &negtxt, 0, 200, 60);
    sysreq("sys short", win, &shortb, &shortb, &shortb, IDCMP_DISKINSERTED, 100, 40);

    P_SECTION("window requester");
    InitRequester(&req);
    P_LONG("init LeftEdge", req.LeftEdge);
    P_LONG("init Flags", req.Flags);
    req.LeftEdge = 10; req.TopEdge = 15; req.Width = 120; req.Height = 40;
    req.BackFill = 1;
    P_BOOL("Request", Request(&req, win));
    P_HEX("Flags after Request", req.Flags);
    P_HEX("window Flags & INREQUEST", win->Flags & WFLG_INREQUEST);
    P_LONG("ReqCount", win->ReqCount);
    P_NULL("ReqLayer", req.ReqLayer);
    if (req.ReqLayer)
    {
        P_LONG("ReqLayer MinX - window", req.ReqLayer->bounds.MinX - win->LeftEdge);
        P_LONG("ReqLayer MinY - window", req.ReqLayer->bounds.MinY - win->TopEdge);
        P_LONG("ReqLayer width", req.ReqLayer->bounds.MaxX - req.ReqLayer->bounds.MinX + 1);
        P_LONG("ReqLayer height", req.ReqLayer->bounds.MaxY - req.ReqLayer->bounds.MinY + 1);
    }
    EndRequest(&req, win);
    P_HEX("Flags after EndRequest", req.Flags);
    P_LONG("ReqCount after", win->ReqCount);
    CloseWindow(win);

    /* where requesters without a reference window open */
    pos_case("pos 0,0", s, 0, 0);
    pos_case("pos 300,100", s, 300, 100);
    pos_case("pos 630,250", s, 630, 250);
    pos_case("pos 100,200", s, 100, 200);
    pointer_to(s, 0, 0);

    if (g_ireq)
    {
        CloseDevice((struct IORequest *)g_ireq);
        DeleteIORequest((struct IORequest *)g_ireq);
    }
    if (g_iport)
        DeleteMsgPort(g_iport);
    UnlockPubScreen(NULL, s);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
