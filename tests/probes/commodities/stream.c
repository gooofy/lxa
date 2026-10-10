/*
 * Probe: what leaves the commodities network.  A recorder input handler
 * at priority 51 (below commodities, above Intuition) records and
 * swallows every ctrl-lalt rawkey event and counts IECLASS_TIMER events.
 * Shows the order of a translator's events, events passed through
 * unchanged, AddIEvents() chains, a custom object changing the event, the
 * InputEvent copy a sender delivers, and the timer events the network
 * sees.  Output compared with AmigaOS 3.1.
 */
#include <exec/io.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>
#include "cxprobe.h"

#define QUAL (IEQUALIFIER_CONTROL | IEQUALIFIER_LALT)

static volatile LONG g_nrec, g_ntimer, g_custom_id;
static volatile UWORD g_rec[32];

struct InputEvent *rec_c(struct InputEvent *list);
__attribute__((used)) struct InputEvent *rec_c(struct InputEvent *list)
{
    struct InputEvent *ie, *prev = NULL, *head = list;
    for (ie = list; ie; ie = ie->ie_NextEvent) {
        if (ie->ie_Class == IECLASS_TIMER)
            g_ntimer++;
        if (ie->ie_Class == IECLASS_RAWKEY && (ie->ie_Qualifier & QUAL) == QUAL) {
            if (g_nrec < 32)
                g_rec[g_nrec] = ie->ie_Code;
            g_nrec++;
            if (prev)
                prev->ie_NextEvent = ie->ie_NextEvent;
            else
                head = ie->ie_NextEvent;
        } else
            prev = ie;
    }
    return head;
}

struct InputEvent *recorder(void);
__asm__(
    "\t.text\n\t.even\n"
    "_recorder:\n\t"
    "move.l a0,-(sp)\n\t"
    "jsr _rec_c\n\t"
    "addq.l #4,sp\n\t"
    "rts\n"
);

static void custom_change(CxMsg *msg, CxObj *co)
{
    struct InputEvent *ie = (struct InputEvent *)CxMsgData(msg);
    (void)co;
    g_custom_id = CxMsgID(msg);
    if (ie)
        ie->ie_Code = 0x22;
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

static void show_rec(const char *label)
{
    LONG i, n = g_nrec;
    probe_s(label);
    probe_s(": recorded ");
    probe_dec(n);
    probe_s(":");
    for (i = 0; i < n && i < 32; i++) {
        probe_ch(' ');
        probe_hex(g_rec[i], 2);
    }
    probe_ch('\n');
    g_nrec = 0;
}

static void ev_init(struct InputEvent *e, UWORD code, struct InputEvent *next)
{
    e->ie_NextEvent = next;
    e->ie_Class = IECLASS_RAWKEY;
    e->ie_SubClass = 0;
    e->ie_Code = code;
    e->ie_Qualifier = QUAL;
    e->ie_EventAddress = NULL;
}

int main(void)
{
    struct MsgPort *port, *ioport;
    struct NewBroker nb;
    static struct Interrupt is;
    static struct InputEvent t1, t2, t3, a1, a2, a3;
    CxObj *broker, *f, *ft, *tsend;
    struct Message *m;
    LONG err, n, ok;

    if (!cx_open())
        return 20;
    port = CreateMsgPort();
    ioport = CreateMsgPort();
    io = (struct IOStdReq *)CreateIORequest(ioport, sizeof(struct IOStdReq));
    if (!port || !io || OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)io, 0))
        return 20;

    is.is_Code = (VOID (*)())recorder;
    is.is_Node.ln_Pri = 51;
    is.is_Node.ln_Name = (char *)"probe recorder";
    io->io_Command = IND_ADDHANDLER;
    io->io_Data = &is;
    DoIO((struct IORequest *)io);

    nb.nb_Version = NB_VERSION;
    nb.nb_Name = (STRPTR)"ProbeStream";
    nb.nb_Title = (STRPTR)"Probe stream";
    nb.nb_Descr = (STRPTR)"records the output";
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

    /* F9 -> translated to the list t1 (q), t2 (w), t3 (e) */
    ev_init(&t1, 0x10, &t2);
    ev_init(&t2, 0x11, &t3);
    ev_init(&t3, 0x12, NULL);
    f = CxFilter((STRPTR)"rawkey ctrl lalt f9");
    AttachCxObj(f, CxTranslate(&t1));
    AttachCxObj(broker, f);
    /* F8 -> custom changes the code to 0x22, sender 3 gets the result */
    f = CxFilter((STRPTR)"rawkey ctrl lalt f8");
    AttachCxObj(f, CxCustom(custom_change, 5));
    AttachCxObj(f, CxSender(port, 3));
    AttachCxObj(broker, f);
    /* F7 -> sender 4 */
    f = CxFilter((STRPTR)"rawkey ctrl lalt f7");
    AttachCxObj(f, CxSender(port, 4));
    AttachCxObj(broker, f);
    /* timer events -> sender 7, inactive until needed */
    ft = CxFilter((STRPTR)"timer");
    tsend = CxSender(port, 7);
    AttachCxObj(ft, tsend);
    ActivateCxObj(ft, FALSE);
    AttachCxObj(broker, ft);
    ActivateCxObj(broker, TRUE);
    Delay(5);
    g_nrec = 0;

    P_SECTION("translator order");
    write_key(0x58);
    show_rec("F9");
    drain("F9", port);

    P_SECTION("unmatched key passes");
    write_key(0x50);
    show_rec("F1");
    drain("F1", port);

    P_SECTION("custom changes the event");
    g_custom_id = -1;
    write_key(0x57);
    show_rec("F8");
    P_LONG("custom id", g_custom_id);
    drain("F8", port);

    P_SECTION("sender copy");
    write_key(0x56);
    while ((m = GetMsg(port)) != NULL) {
        struct InputEvent *ie = (struct InputEvent *)CxMsgData((CxMsg *)m);
        P_LONG("id", CxMsgID((CxMsg *)m));
        P_BOOL("data is the written event", ie == &wev);
        if (ie) {
            P_BOOL("ie_NextEvent NULL", ie->ie_NextEvent == NULL);
            P_HEX("ie_Class", ie->ie_Class);
            P_HEX("ie_Code", ie->ie_Code);
        }
        ReplyMsg(m);
    }
    show_rec("F7");

    P_SECTION("AddIEvents chain");
    ev_init(&a1, 0x20, &a2);
    ev_init(&a2, 0x58, &a3);
    ev_init(&a3, 0x21, NULL);
    AddIEvents(&a1);
    Delay(10);
    show_rec("a F9 s");
    drain("a F9 s", port);
    AddIEvents(NULL);
    Delay(5);
    show_rec("NULL");

    P_SECTION("inactive broker");
    ActivateCxObj(broker, FALSE);
    write_key(0x58);
    show_rec("F9 inactive");
    drain("F9 inactive", port);
    ActivateCxObj(broker, TRUE);

    P_SECTION("timer events");
    ActivateCxObj(ft, TRUE);
    Delay(5);
    while ((m = GetMsg(port)) != NULL)
        ReplyMsg(m);
    g_ntimer = 0;
    Delay(50);
    ActivateCxObj(ft, FALSE);
    n = g_ntimer;
    ok = n >= 8 && n <= 12;
    P_BOOL("handler saw 8..12 timer events in a second", ok);
    n = 0;
    {
        LONG cls = -1, code = -1;
        while ((m = GetMsg(port)) != NULL) {
            struct InputEvent *ie = (struct InputEvent *)CxMsgData((CxMsg *)m);
            if (CxMsgID((CxMsg *)m) == 7) {
                n++;
                if (ie) {
                    cls = ie->ie_Class;
                    code = ie->ie_Code;
                }
            }
            ReplyMsg(m);
        }
        P_BOOL("sender got 8..14 timer messages", n >= 8 && n <= 14);
        P_HEX("timer class", cls);
        P_HEX("timer code", code);
    }

    DeleteCxObjAll(broker);
    io->io_Command = IND_REMHANDLER;
    io->io_Data = &is;
    DoIO((struct IORequest *)io);
    while ((m = GetMsg(port)) != NULL)
        ReplyMsg(m);
    P_SECTION("done");

    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(ioport);
    DeleteMsgPort(port);
    CloseLibrary(CxBase);
    return 0;
}
