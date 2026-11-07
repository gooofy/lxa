/*
 * Test: devices/input_swallow (Phase 251)
 *
 * An input handler in front of Intuition (priority 60) removes the first
 * click (SELECTDOWN + SELECTUP) from the event stream.  Intuition must not
 * see it: the window gets IDCMP_MOUSEBUTTONS only for the second click.
 * The scenario tests/scenarios/interactive/input_swallow.yaml clicks twice;
 * stdout is compared with AmigaOS 3.1.
 */
#include <exec/types.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase = NULL;

static volatile LONG g_swallowed;

struct InputEvent *handler_c(struct InputEvent *events);
__attribute__((used)) struct InputEvent *handler_c(struct InputEvent *events)
{
    struct InputEvent *ie, *prev = NULL, *next;

    for (ie = events; ie; ie = next) {
        next = ie->ie_NextEvent;
        if (g_swallowed < 2 && ie->ie_Class == IECLASS_RAWMOUSE &&
            (ie->ie_Code == SELECTDOWN || ie->ie_Code == SELECTUP)) {
            g_swallowed++;
            if (prev)
                prev->ie_NextEvent = next;
            else
                events = next;
            continue;
        }
        prev = ie;
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

int main(void)
{
    struct MsgPort *ioport;
    struct IOStdReq *io;
    static struct Interrupt is;
    struct Window *win;
    struct IntuiMessage *msg;
    LONG downs = 0, ups = 0;
    BOOL done = FALSE;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    ioport = CreateMsgPort();
    io = (struct IOStdReq *)CreateIORequest(ioport, sizeof(struct IOStdReq));
    if (!IntuitionBase || !io ||
        OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)io, 0))
        return 20;

    win = OpenWindowTags(NULL,
                         WA_Left, 40, WA_Top, 40, WA_Width, 240, WA_Height, 80,
                         WA_Title, (ULONG)"Input Swallow",
                         WA_IDCMP, IDCMP_MOUSEBUTTONS,
                         WA_Flags, WFLG_ACTIVATE | WFLG_RMBTRAP,
                         TAG_DONE);
    if (!win)
        return 20;

    is.is_Code = (VOID (*)())handler;
    is.is_Node.ln_Pri = 60;
    is.is_Node.ln_Name = (char *)"swallow handler";
    io->io_Command = IND_ADDHANDLER;
    io->io_Data = &is;
    DoIO((struct IORequest *)io);

    Printf((STRPTR)"Ready\n");
    Flush(Output());

    while (!done) {
        WaitPort(win->UserPort);
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL) {
            if (msg->Class == IDCMP_MOUSEBUTTONS) {
                if (msg->Code == SELECTDOWN) {
                    Printf((STRPTR)"SELECTDOWN, swallowed so far %ld\n", g_swallowed);
                    Flush(Output());
                    downs++;
                } else if (msg->Code == SELECTUP) {
                    Printf((STRPTR)"SELECTUP, swallowed so far %ld\n", g_swallowed);
                    Flush(Output());
                    ups++;
                    done = TRUE;
                }
            }
            ReplyMsg((struct Message *)msg);
        }
    }

    io->io_Command = IND_REMHANDLER;
    io->io_Data = &is;
    DoIO((struct IORequest *)io);

    Printf((STRPTR)"buttons seen: %ld down, %ld up; swallowed %ld\n", downs, ups, g_swallowed);

    CloseWindow(win);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(ioport);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
