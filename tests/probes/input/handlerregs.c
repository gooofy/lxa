/*
 * Probe (Phase 237): an input handler that changes d4, as AMOS's does (Fish
 * WhereK).  On AmigaOS 3.1 handlers run in input.device's task, so the
 * program that writes an event with IND_WRITEEVENT keeps its registers.
 */
#include <exec/types.h>
#include <exec/io.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static volatile LONG g_calls;
static struct Task *volatile g_task;
static ULONG g_regs[10];

/* a0 = event list, a1 = is_Data; returns the list and changes d4 like
 * AMOS (changing all of d2-d7 hangs AmigaOS 3.1) */
struct InputEvent *handler_c(void);
__attribute__((used)) struct InputEvent *handler_c(void)
{
    g_calls++;
    g_task = FindTask(NULL);
    return NULL;
}

struct InputEvent *handler(void);
__asm__(
    "\t.text\n\t.even\n"
    "_handler:\n\t"
    "move.l a0,-(sp)\n\t"
    "jsr _handler_c\n\t"
    "move.l (sp)+,d0\n\t"
    "move.l #0x8000ffff,d4\n\t"
    "rts\n"
);

/* DoIO(io) with sentinels in d2-d7/a2-a5; stores them afterwards */
static void doio_regs(struct IORequest *io)
{
    register struct IORequest *r_a1 __asm("a1") = io;
    register ULONG *r_a0 __asm("a0") = g_regs;
    __asm__ __volatile__ (
        "movem.l d2-d7/a2-a6,-(sp)\n\t"
        "move.l a0,-(sp)\n\t"
        "move.l #0x5a5a5a5a,d2\n\tmove.l d2,d3\n\tmove.l d2,d4\n\tmove.l d2,d5\n\t"
        "move.l d2,d6\n\tmove.l d2,d7\n\tmove.l d2,a2\n\tmove.l d2,a3\n\tmove.l d2,a4\n\tmove.l d2,a5\n\t"
        "move.l 4.w,a6\n\t"
        "jsr -456(a6)\n\t"
        "move.l (sp)+,a0\n\t"
        "movem.l d2-d7/a2-a5,(a0)\n\t"
        "movem.l (sp)+,d2-d7/a2-a6"
        : "+r" (r_a1), "+r" (r_a0) : : "d0", "d1", "cc", "memory");
}

int main(void)
{
    struct MsgPort *port = CreateMsgPort();
    struct IOStdReq *io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    static struct Interrupt is;
    static struct InputEvent ev;
    int i, kept = 0;

    if (!io || OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)io, 0))
        return 20;
    is.is_Code = (VOID (*)())handler;
    is.is_Node.ln_Pri = 100;
    is.is_Node.ln_Name = (char *)"probe handler";
    io->io_Command = IND_ADDHANDLER;
    io->io_Data = &is;
    DoIO((struct IORequest *)io);
    Delay(5);
    g_calls = 0;

    ev.ie_Class = IECLASS_NULL;
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = &ev;
    io->io_Length = sizeof(ev);
    doio_regs((struct IORequest *)io);
    Delay(5);

    for (i = 0; i < 10; i++)
        if (g_regs[i] == 0x5a5a5a5a)
            kept++;
    P_SECTION("IND_WRITEEVENT through a handler that changes d4");
    P_BOOL("handler called", g_calls > 0);
    P_LONG("of d2-d7/a2-a5 kept", kept);

    io->io_Command = IND_REMHANDLER;
    io->io_Data = &is;
    DoIO((struct IORequest *)io);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
    return 0;
}
