/*
 * Probe (Phase 237): the ConUnit of a console.device unit opened on a
 * window - its font and character cell fields.  Fish MicroEmacs sizes its
 * terminal from cu_Window and cu_Font.
 */
#include <exec/types.h>
#include <exec/io.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/alib_protos.h>
#include "idump.h"

struct Library *IntuitionBase;

int main(void)
{
    struct Window *w;
    struct MsgPort *port;
    struct IOStdReq *io;
    struct ConUnit *cu;

    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
        return 20;
    w = OpenWindowTags(NULL, WA_Left, 20, WA_Top, 20, WA_Width, 320, WA_Height, 120,
                       WA_Title, (ULONG)"ConUnit", WA_DragBar, TRUE, TAG_END);
    if (!w)
        return 20;
    port = CreateMsgPort();
    io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    io->io_Data = (APTR)w;
    io->io_Length = sizeof(struct Window);
    P_SECTION("console unit on a window");
    if (OpenDevice((STRPTR)"console.device", CONU_STANDARD, (struct IORequest *)io, 0) == 0) {
        cu = (struct ConUnit *)io->io_Unit;
        P_BOOL("cu_Window == window", cu->cu_Window == w);
        P_NULL("cu_Font", cu->cu_Font);
        P_BOOL("cu_Font == window font", cu->cu_Font == w->RPort->Font);
        P_LONG("cu_TxHeight", cu->cu_TxHeight);
        P_LONG("cu_TxWidth", cu->cu_TxWidth);
        P_LONG("cu_TxBaseline", cu->cu_TxBaseline);
        P_LONG("cu_TxSpacing", cu->cu_TxSpacing);
        P_LONG("cu_XRSize", cu->cu_XRSize);
        P_LONG("cu_YRSize", cu->cu_YRSize);
        P_LONG("cu_XMax", cu->cu_XMax);
        P_LONG("cu_YMax", cu->cu_YMax);
        CloseDevice((struct IORequest *)io);
    } else
        probe_s("OpenDevice failed\n");
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
    CloseWindow(w);
    CloseLibrary(IntuitionBase);
    return 0;
}
