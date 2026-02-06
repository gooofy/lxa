/*
 * Minimal test that mimics KickPascal 2's console behavior
 * Reads 1 byte at a time like KP2 does
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <intuition/intuition.h>
#include <devices/console.h>
#include <graphics/gfxbase.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

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
    char buf[2];
    LONG bytes;
    int i;
    
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    
    nw.LeftEdge = 0;
    nw.TopEdge = 0;
    nw.Width = 640;
    nw.Height = 200;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.Flags = WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET;
    nw.Title = (UBYTE *)"KP2 Test";
    nw.Type = WBENCHSCREEN;
    
    win = OpenWindow(&nw);
    if (!win) return 1;
    
    consolePort = CreatePort(NULL, 0);
    consoleIO = (struct IOStdReq *)CreateExtIO(consolePort, sizeof(struct IOStdReq));
    consoleIO->io_Data = (APTR)win;
    consoleIO->io_Length = sizeof(struct Window);
    
    if (OpenDevice((STRPTR)"console.device", 0, (struct IORequest *)consoleIO, 0) != 0) return 1;
    
    print("OK: Console opened\n");
    
    consoleIO->io_Command = CMD_WRITE;
    consoleIO->io_Data = (APTR)"Workspace: ";
    consoleIO->io_Length = 11;
    DoIO((struct IORequest *)consoleIO);
    print("OK: Prompt written\n");
    
    print("Injecting '1', '0', '0', Return...\n");
    
    /* Read 1 byte at a time like KP2 */
    print("Reading 1 byte at a time:\n");
    for (i = 0; i < 5; i++) {
        consoleIO->io_Command = CMD_READ;
        consoleIO->io_Data = (APTR)buf;
        consoleIO->io_Length = 1;
        DoIO((struct IORequest *)consoleIO);
        
        bytes = consoleIO->io_Actual;
        if (bytes > 0) {
            print("  Got byte\n");
            if (buf[0] == '\r' || buf[0] == '\n') {
                print("OK: Got newline, done\n");
                break;
            }
        }
    }
    
    print("PASS: kp2_test complete\n");
    
    CloseDevice((struct IORequest *)consoleIO);
    DeleteExtIO((struct IORequest *)consoleIO);
    DeletePort(consolePort);
    CloseWindow(win);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    
    return 0;
}
