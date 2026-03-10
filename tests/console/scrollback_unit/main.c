/*
 * Test: console/scrollback_unit
 *
 * Verifies console.device scrollback setup and position commands.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <graphics/rastport.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/graphics.h>

extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;
extern struct ExecBase *SysBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++) {
        len++;
    }

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[16];
    int i = 0;

    if (n < 0) {
        print("-");
        n = -n;
    }

    if (n == 0) {
        print("0");
        return;
    }

    while (n > 0 && i < (int)sizeof(buf)) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    while (i > 0) {
        char c[2] = { buf[--i], '\0' };
        print(c);
    }
}

static LONG con_write(struct IOStdReq *req, const char *data, LONG len)
{
    req->io_Command = CMD_WRITE;
    req->io_Data = (APTR)data;
    req->io_Length = len;
    DoIO((struct IORequest *)req);
    return req->io_Actual;
}

static LONG con_puts(struct IOStdReq *req, const char *str)
{
    LONG len = 0;
    const char *p = str;

    while (*p++) {
        len++;
    }

    return con_write(req, str, len);
}

int main(void)
{
    struct Window *window = NULL;
    struct MsgPort *console_port = NULL;
    struct IOStdReq *console_req = NULL;
    struct NewWindow nw = {0};
    struct ConsoleScrollback scrollback = {0};
    LONG result;

    print("scrollback_unit: testing console scrollback commands\n");

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase) {
        print("FAIL: could not open intuition.library\n");
        return 1;
    }

    nw.LeftEdge = 0;
    nw.TopEdge = 0;
    nw.Width = 160;
    nw.Height = 56;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_RAWKEY;
    nw.Flags = WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_CLOSEGADGET;
    nw.Title = (UBYTE *)"Console Scrollback Test";
    nw.Type = WBENCHSCREEN;

    window = OpenWindow(&nw);
    if (!window) {
        print("FAIL: could not open test window\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    console_port = CreateMsgPort();
    if (!console_port) {
        print("FAIL: could not create console port\n");
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    console_req = (struct IOStdReq *)CreateIORequest(console_port, sizeof(struct IOStdReq));
    if (!console_req) {
        print("FAIL: could not create console request\n");
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    console_req->io_Data = (APTR)window;
    console_req->io_Length = sizeof(struct Window);
    result = OpenDevice((STRPTR)"console.device", CONU_STANDARD, (struct IORequest *)console_req, 0);
    if (result != 0) {
        print("FAIL: could not open console.device, error=");
        print_num(result);
        print("\n");
        DeleteIORequest((struct IORequest *)console_req);
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    scrollback.cs_ScrollerGadget = NULL;
    scrollback.cs_NumLines = 4;
    console_req->io_Command = CD_SETUPSCROLLBACK;
    console_req->io_Data = (APTR)&scrollback;
    console_req->io_Length = sizeof(scrollback);
    DoIO((struct IORequest *)console_req);
    if (console_req->io_Error != 0 || console_req->io_Actual != sizeof(scrollback)) {
        print("FAIL: CD_SETUPSCROLLBACK failed\n");
        CloseDevice((struct IORequest *)console_req);
        DeleteIORequest((struct IORequest *)console_req);
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    con_puts(console_req, "A\nB\nC\nD");
    WaitTOF();
    WaitTOF();

    console_req->io_Command = CD_SETSCROLLBACKPOSITION;
    console_req->io_Offset = 1;
    DoIO((struct IORequest *)console_req);
    if (console_req->io_Error != 0) {
        print("FAIL: CD_SETSCROLLBACKPOSITION(1) failed\n");
        CloseDevice((struct IORequest *)console_req);
        DeleteIORequest((struct IORequest *)console_req);
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    print("Waiting for scrollback position 1\n");
    WaitTOF();
    WaitTOF();

    console_req->io_Command = CD_SETSCROLLBACKPOSITION;
    console_req->io_Offset = 0;
    DoIO((struct IORequest *)console_req);
    if (console_req->io_Error != 0) {
        print("FAIL: CD_SETSCROLLBACKPOSITION(0) failed\n");
        CloseDevice((struct IORequest *)console_req);
        DeleteIORequest((struct IORequest *)console_req);
        DeleteMsgPort(console_port);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    print("Waiting for scrollback position 0\n");
    WaitTOF();
    WaitTOF();

    print("PASS: scrollback_unit all tests passed\n");

    CloseDevice((struct IORequest *)console_req);
    DeleteIORequest((struct IORequest *)console_req);
    DeleteMsgPort(console_port);
    CloseWindow(window);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
