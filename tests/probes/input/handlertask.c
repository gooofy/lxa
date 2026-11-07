/*
 * Probe (Phase 251): where input handlers and commodities' custom objects
 * run.  On AmigaOS 3.1 input.device's task calls the handler chain - for
 * events written with IND_WRITEEVENT and for its own IECLASS_TIMER events
 * - so a handler may allocate memory and call libraries.  The probe
 * records the task, the nesting counts and the DoIO() result.
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <libraries/commodities.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <clib/commodities_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/commodities.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *CxBase = NULL;

static struct Task *g_input_task;

struct seen {
    volatile LONG calls;
    struct Task *volatile task;
    volatile LONG idnest, tdnest;
    volatile LONG alloc_ok;
};

static struct seen g_null, g_timer, g_custom;

static void note(struct seen *s)
{
    APTR mem;

    if (s->calls++)
        return;
    s->task = FindTask(NULL);
    s->idnest = SysBase->IDNestCnt;
    s->tdnest = SysBase->TDNestCnt;
    /* a handler may allocate memory (commodities' custom objects do) */
    mem = AllocMem(64, MEMF_PUBLIC | MEMF_CLEAR);
    s->alloc_ok = mem != NULL;
    if (mem)
        FreeMem(mem, 64);
}

/* a0 = event list, a1 = is_Data */
struct InputEvent *handler_c(struct InputEvent *events);
__attribute__((used)) struct InputEvent *handler_c(struct InputEvent *events)
{
    struct InputEvent *ie;

    for (ie = events; ie; ie = ie->ie_NextEvent) {
        if (ie->ie_Class == IECLASS_NULL && ie->ie_Code == 0x4711)
            note(&g_null);
        else if (ie->ie_Class == IECLASS_TIMER)
            note(&g_timer);
    }
    return events;
}

struct InputEvent *handler(void);
__asm__(
    "\t.text\n\t.even\n"
    "_handler:\n\t"
    "movem.l d2-d7/a2-a6,-(sp)\n\t"
    "move.l a0,-(sp)\n\t"
    "jsr _handler_c\n\t"
    "addq.l #4,sp\n\t"
    "movem.l (sp)+,d2-d7/a2-a6\n\t"
    "rts\n"
);

static void custom_fn(CxMsg *msg, CxObj *co)
{
    (void)msg;
    (void)co;
    note(&g_custom);
}

static void show(const char *what, struct seen *s)
{
    P_SECTION(what);
    P_BOOL("called", s->calls > 0);
    if (!s->calls)
        return;
    P_BOOL("in the input.device task", s->task == g_input_task);
    P_BOOL("in the probe's task", s->task == FindTask(NULL));
    P_BOOL("interrupts enabled", s->idnest < 0);
    P_BOOL("multitasking enabled", s->tdnest < 0);
    P_BOOL("AllocMem works", s->alloc_ok);
}

int main(void)
{
    struct MsgPort *ioport = CreateMsgPort(), *cxport = CreateMsgPort();
    struct IOStdReq *io = (struct IOStdReq *)CreateIORequest(ioport, sizeof(struct IOStdReq));
    static struct Interrupt is;
    static struct InputEvent ev;
    struct NewBroker nb;
    CxObj *broker = NULL, *filter;
    LONG err, i, called_before_return;
    BYTE ioerr;

    if (!io || !cxport || OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)io, 0))
        return 20;
    Forbid();
    g_input_task = FindTask((STRPTR)"input.device");
    Permit();
    P_SECTION("setup");
    P_NULL("input.device task", g_input_task);

    is.is_Code = (VOID (*)())handler;
    is.is_Node.ln_Pri = 100;
    is.is_Node.ln_Name = (char *)"probe handler";
    io->io_Command = IND_ADDHANDLER;
    io->io_Data = &is;
    DoIO((struct IORequest *)io);

    /* IND_WRITEEVENT: the handler has run when DoIO() returns */
    ev.ie_Class = IECLASS_NULL;
    ev.ie_Code = 0x4711;
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = &ev;
    io->io_Length = sizeof(ev);
    ioerr = DoIO((struct IORequest *)io);
    called_before_return = g_null.calls > 0;
    P_SECTION("IND_WRITEEVENT");
    P_LONG("DoIO", ioerr);
    P_BOOL("handler ran before DoIO returned", called_before_return);
    show("handler, written event", &g_null);

    /* IECLASS_TIMER: input.device's own events (ten a second) */
    for (i = 0; i < 25 && !g_timer.calls; i++)
        Delay(2);
    show("handler, timer event", &g_timer);

    /* a commodity's custom object sees a written event */
    CxBase = OpenLibrary((STRPTR)"commodities.library", 37);
    if (CxBase) {
        nb.nb_Version = NB_VERSION;
        nb.nb_Name = (STRPTR)"ProbeHandlerTask";
        nb.nb_Title = (STRPTR)"Probe handler task";
        nb.nb_Descr = (STRPTR)"where custom objects run";
        nb.nb_Unique = NBU_UNIQUE;
        nb.nb_Flags = 0;
        nb.nb_Pri = 0;
        nb.nb_Port = cxport;
        nb.nb_ReservedChannel = 0;
        broker = CxBroker(&nb, &err);
        if (broker) {
            filter = CxFilter((STRPTR)"rawkey ctrl lalt f6");
            AttachCxObj(filter, CxCustom(custom_fn, 1));
            AttachCxObj(broker, filter);
            ActivateCxObj(broker, TRUE);
            ev.ie_Class = IECLASS_RAWKEY;
            ev.ie_Code = 0x55;      /* F6 */
            ev.ie_Qualifier = IEQUALIFIER_CONTROL | IEQUALIFIER_LALT;
            io->io_Command = IND_WRITEEVENT;
            io->io_Data = &ev;
            io->io_Length = sizeof(ev);
            DoIO((struct IORequest *)io);
            Delay(5);
            show("commodities custom object", &g_custom);
            DeleteCxObjAll(broker);
        }
        CloseLibrary(CxBase);
    }

    io->io_Command = IND_REMHANDLER;
    io->io_Data = &is;
    DoIO((struct IORequest *)io);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(ioport);
    DeleteMsgPort(cxport);
    return 0;
}
