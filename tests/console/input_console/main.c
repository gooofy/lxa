/*
 * Test: console/input_console
 *
 * Tests receiving input through console.device
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <intuition/intuition.h>
#include <devices/console.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

int main(void)
{
    struct Window *win;
    struct NewWindow nw = {0};
    struct IOStdReq *consoleIO;
    struct MsgPort *consolePort;
    char buf[16];
    
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    
    nw.LeftEdge = 0;
    nw.TopEdge = 0;
    nw.Width = 320;
    nw.Height = 200;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.Flags = WFLG_ACTIVATE | WFLG_CLOSEGADGET;
    nw.Title = (UBYTE *)"Console Input Test";
    nw.Type = WBENCHSCREEN;
    
    win = OpenWindow(&nw);
    if (!win) return 1;
    
    consolePort = CreateMsgPort();
    consoleIO = (struct IOStdReq *)CreateExtIO(consolePort, sizeof(struct IOStdReq));
    consoleIO->io_Data = (APTR)win;
    consoleIO->io_Length = sizeof(struct Window);
    
    if (OpenDevice((STRPTR)"console.device", 0, (struct IORequest *)consoleIO, 0) != 0) return 1;
    
    print("Waiting for input...\n");
    
    consoleIO->io_Command = CMD_READ;
    consoleIO->io_Data = (APTR)buf;
    consoleIO->io_Length = 1;
    DoIO((struct IORequest *)consoleIO);
    
    if (consoleIO->io_Actual > 0) {
        print("Got input\n");
        print("PASS: input_console all tests passed\n");
    }
    
    CloseDevice((struct IORequest *)consoleIO);
    DeleteExtIO((struct IORequest *)consoleIO);
    DeleteMsgPort(consolePort);
    CloseWindow(win);
    CloseLibrary((struct Library *)IntuitionBase);
    
    return 0;
}
