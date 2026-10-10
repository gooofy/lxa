/*
 * Probe: commodities.library message routing.  A broker with filters,
 * senders, translators, a signal object and custom objects that call
 * DisposeCxMsg(), RouteCxMsg() and DivertCxMsg(); input events are
 * written with input.device IND_WRITEEVENT (and AddIEvents()), and the
 * CxMsgs arriving at the broker's port are printed (CxMsgType, CxMsgID,
 * CxMsgData).  Every event the probe writes is swallowed by the network,
 * so nothing reaches the console.  Output compared with AmigaOS 3.1.
 */
#include <exec/io.h>
#include <devices/input.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>
#include "cxprobe.h"

#define QUAL (IEQUALIFIER_CONTROL | IEQUALIFIER_LALT)

static volatile LONG g_custom_calls, g_custom_id, g_custom_type, g_custom_code;
static CxObj *g_route_to, *g_divert_head;

/* commodities call custom functions with (msg, co) on the stack */
static void custom_record(CxMsg *msg, CxObj *co)
{
    struct InputEvent *ie = (struct InputEvent *)CxMsgData(msg);
    (void)co;
    g_custom_calls++;
    g_custom_id = CxMsgID(msg);
    g_custom_type = CxMsgType(msg);
    g_custom_code = ie ? ie->ie_Code : -1;
}

static void custom_dispose(CxMsg *msg, CxObj *co)
{
    (void)co;
    g_custom_calls++;
    DisposeCxMsg(msg);
}

static void custom_route(CxMsg *msg, CxObj *co)
{
    (void)co;
    g_custom_calls++;
    RouteCxMsg(msg, g_route_to);
}

static void custom_divert(CxMsg *msg, CxObj *co)
{
    g_custom_calls++;
    DivertCxMsg(msg, g_divert_head, co);
}

static struct IOStdReq *io;
static struct InputEvent wev;

static void write_key(UWORD code)
{
    wev.ie_NextEvent = NULL;
    wev.ie_Class = IECLASS_RAWKEY;
    wev.ie_SubClass = 0;
    wev.ie_Code = code;
    wev.ie_Qualifier = QUAL;
    wev.ie_EventAddress = NULL;
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = &wev;
    io->io_Length = sizeof(wev);
    DoIO((struct IORequest *)io);
    Delay(10);
}

static CxObj *filter(const char *desc)
{
    CxObj *f = CxFilter((STRPTR)desc);
    return f;
}

int main(void)
{
    struct MsgPort *port, *ioport;
    struct NewBroker nb;
    CxObj *broker, *fA, *fB, *fC, *fD, *fE, *fF, *fG, *fH, *catchall;
    static struct InputEvent transB, transC;
    static IX ix2;
    CxObj *broker2, *f2;
    static struct InputEvent addev;
    static IX ix;
    LONG err, sigbit;

    if (!cx_open())
        return 20;
    port = CreateMsgPort();
    ioport = CreateMsgPort();
    io = (struct IOStdReq *)CreateIORequest(ioport, sizeof(struct IOStdReq));
    if (!port || !io || OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)io, 0))
        return 20;
    sigbit = AllocSignal(-1);
    SetSignal(0, 1L << sigbit);

    nb.nb_Version = NB_VERSION;
    nb.nb_Name = (STRPTR)"ProbeRouting";
    nb.nb_Title = (STRPTR)"Probe routing";
    nb.nb_Descr = (STRPTR)"routes probe events";
    nb.nb_Unique = NBU_UNIQUE;
    nb.nb_Flags = 0;
    nb.nb_Pri = 0;
    nb.nb_Port = port;
    nb.nb_ReservedChannel = 0;
    broker = CxBroker(&nb, &err);
    P_SECTION("setup");
    P_NULL("broker", broker);
    if (!broker)
        return 20;

    /* F9 -> sender 1, translated to F8 up, F7 up (key-ups: harmless
     * should they reach the console) */
    fA = filter("rawkey ctrl lalt f9");
    AttachCxObj(fA, CxSender(port, 1));
    transB.ie_NextEvent = &transC;
    transB.ie_Class = IECLASS_RAWKEY;
    transB.ie_Code = 0x57 | IECODE_UP_PREFIX;
    transB.ie_Qualifier = QUAL;
    transC.ie_NextEvent = NULL;
    transC.ie_Class = IECLASS_RAWKEY;
    transC.ie_Code = 0x56 | IECODE_UP_PREFIX;
    transC.ie_Qualifier = QUAL;
    AttachCxObj(fA, CxTranslate(&transB));
    AttachCxObj(broker, fA);

    /* F8 -> sender 2, custom 3, signal, swallowed */
    fB = filter("rawkey ctrl lalt f8");
    AttachCxObj(fB, CxSender(port, 2));
    AttachCxObj(fB, CxCustom(custom_record, 3));
    AttachCxObj(fB, CxSignal(FindTask(NULL), sigbit));
    AttachCxObj(fB, CxTranslate(NULL));
    AttachCxObj(broker, fB);

    /* F7 -> sender 4 (filter toggled), swallowed */
    fC = filter("rawkey ctrl lalt f7");
    AttachCxObj(fC, CxSender(port, 4));
    AttachCxObj(fC, CxTranslate(NULL));
    AttachCxObj(broker, fC);

    /* F6 -> custom DisposeCxMsg, sender 5 never sees it */
    fD = filter("rawkey ctrl lalt f6");
    AttachCxObj(fD, CxCustom(custom_dispose, 0));
    AttachCxObj(fD, CxSender(port, 5));
    AttachCxObj(broker, fD);

    /* F5 -> custom RouteCxMsg to sender 7 in F4's list... */
    fE = filter("rawkey ctrl lalt f5");
    AttachCxObj(fE, CxCustom(custom_route, 0));
    AttachCxObj(fE, CxSender(port, 6));
    AttachCxObj(broker, fE);

    /* ...a filter that never matches, holding sender 7 + swallow */
    fF = filter("rawkey ctrl lalt f1");
    g_route_to = CxSender(port, 7);
    AttachCxObj(fF, g_route_to);
    AttachCxObj(fF, CxTranslate(NULL));
    AttachCxObj(broker, fF);

    /* F4 -> custom DivertCxMsg into fH's list (sender 8), then back:
     * sender 9, swallowed */
    fG = filter("rawkey ctrl lalt f4");
    AttachCxObj(fG, CxCustom(custom_divert, 0));
    AttachCxObj(fG, CxSender(port, 9));
    AttachCxObj(fG, CxTranslate(NULL));
    AttachCxObj(broker, fG);
    fH = filter("rawkey ctrl lalt f2");
    AttachCxObj(fH, CxSender(port, 8));
    g_divert_head = fH;
    AttachCxObj(broker, fH);

    /* catch-all: every ctrl-lalt key the probe writes ends here */
    catchall = CxFilter(NULL);
    ix.ix_Version = IX_VERSION;
    ix.ix_Class = IECLASS_RAWKEY;
    ix.ix_Code = 0;
    ix.ix_CodeMask = 0;
    ix.ix_Qualifier = QUAL;
    ix.ix_QualMask = QUAL;
    ix.ix_QualSame = 0;
    SetFilterIX(catchall, &ix);
    AttachCxObj(catchall, CxSender(port, 99));
    AttachCxObj(catchall, CxTranslate(NULL));
    EnqueueCxObj(broker, catchall);
    SetCxObjPri(catchall, -10);
    /* a second broker after ours: catches ctrl-lalt key-ups (sender 50) */
    nb.nb_Name = (STRPTR)"ProbeRouting2";
    nb.nb_Pri = -5;
    broker2 = CxBroker(&nb, &err);
    P_NULL("broker2", broker2);
    if (!broker2)
        return 20;
    f2 = CxFilter(NULL);
    ix2.ix_Version = IX_VERSION;
    ix2.ix_Class = IECLASS_RAWKEY;
    ix2.ix_Code = IECODE_UP_PREFIX;
    ix2.ix_CodeMask = IECODE_UP_PREFIX;
    ix2.ix_Qualifier = QUAL;
    ix2.ix_QualMask = QUAL;
    ix2.ix_QualSame = 0;
    SetFilterIX(f2, &ix2);
    AttachCxObj(f2, CxSender(port, 50));
    AttachCxObj(f2, CxTranslate(NULL));
    AttachCxObj(broker2, f2);
    ActivateCxObj(broker2, TRUE);

    P_LONG("broker error", CxObjError(broker));
    P_LONG("fA error", CxObjError(fA));
    P_LONG("ActivateCxObj(broker, TRUE)", ActivateCxObj(broker, TRUE));
    Delay(5);
    drain("after activate", port);

    P_SECTION("F9: sender, translator");
    write_key(0x58);
    drain("F9", port);
    P_LONG("custom calls", g_custom_calls);
    P_LONG("custom id", g_custom_id);
    P_LONG("custom type", g_custom_type);
    P_HEX("custom code", g_custom_code);
    P_BOOL("signal", SetSignal(0, 1L << sigbit) & (1L << sigbit));

    P_SECTION("F8 directly");
    g_custom_calls = 0;
    write_key(0x57);
    drain("F8", port);
    P_LONG("custom calls", g_custom_calls);
    P_BOOL("signal", SetSignal(0, 1L << sigbit) & (1L << sigbit));

    P_SECTION("F8 upstroke");
    write_key(0x57 | IECODE_UP_PREFIX);
    drain("F8 up", port);

    P_SECTION("F7, filter active / inactive");
    write_key(0x56);
    drain("F7", port);
    P_LONG("ActivateCxObj(fC, FALSE)", ActivateCxObj(fC, FALSE));
    write_key(0x56);
    drain("F7 inactive", port);
    P_LONG("ActivateCxObj(fC, TRUE)", ActivateCxObj(fC, TRUE));

    P_SECTION("F6: custom DisposeCxMsg");
    g_custom_calls = 0;
    write_key(0x55);
    drain("F6", port);
    P_LONG("custom calls", g_custom_calls);

    P_SECTION("F5: custom RouteCxMsg");
    g_custom_calls = 0;
    write_key(0x54);
    drain("F5", port);
    P_LONG("custom calls", g_custom_calls);

    P_SECTION("F4: custom DivertCxMsg");
    g_custom_calls = 0;
    write_key(0x53);
    drain("F4", port);
    P_LONG("custom calls", g_custom_calls);

    P_SECTION("unmatched key: catch-all");
    write_key(0x20);
    drain("A", port);

    P_SECTION("AddIEvents");
    addev.ie_NextEvent = NULL;
    addev.ie_Class = IECLASS_RAWKEY;
    addev.ie_Code = 0x56;
    addev.ie_Qualifier = QUAL;
    AddIEvents(&addev);
    Delay(10);
    drain("AddIEvents F7", port);

    P_SECTION("inactive broker");
    P_LONG("ActivateCxObj(broker, FALSE)", ActivateCxObj(broker, FALSE));
    Delay(5);
    drain("inactive", port);

    P_SECTION("BrokerCommand to our broker");
    P_LONG("BrokerCommand(ProbeRouting, CXCMD_KILL)", BrokerCommand((STRPTR)"ProbeRouting", CXCMD_KILL));
    drain("command", port);

    DeleteCxObjAll(broker);
    DeleteCxObjAll(broker2);
    Delay(5);
    drain("after delete", port);
    P_SECTION("done");

    FreeSignal(sigbit);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(ioport);
    DeleteMsgPort(port);
    CloseLibrary(CxBase);
    return 0;
}
