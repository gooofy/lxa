/*
 * Test: console/input_console
 *
 * Tests console device input with injected keys.
 * This test mimics the KickPascal 2 workspace input scenario:
 * 1. Open a window with console attached
 * 2. Output a prompt via console
 * 3. Inject keyboard input
 * 4. Read the input via console CMD_READ
 *
 * This tests the full console input path that KP2 uses.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <devices/console.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include <inline/dos.h>

#include "../../common/test_inject.h"

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct IntuitionBase *IntuitionBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG val)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    BOOL neg = FALSE;
    
    if (val < 0) {
        neg = TRUE;
        val = -val;
    }
    if (val == 0) {
        *--p = '0';
    } else {
        while (val > 0) {
            *--p = '0' + (val % 10);
            val /= 10;
        }
    }
    if (neg) *--p = '-';
    print(p);
}

static void print_hex(ULONG val)
{
    char buf[16];
    int i = 0;
    buf[i++] = '0';
    buf[i++] = 'x';
    for (int j = 7; j >= 0; j--) {
        int nibble = (val >> (j * 4)) & 0xF;
        buf[i++] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
    buf[i++] = '\0';
    print(buf);
}

/* Console IO helpers */
static struct IOStdReq *create_console_io(struct Window *window, struct MsgPort *port)
{
    struct IOStdReq *io;
    
    io = (struct IOStdReq *)CreateExtIO(port, sizeof(struct IOStdReq));
    if (!io) return NULL;
    
    /* Open console.device - pass window pointer in io_Data for attachment */
    io->io_Data = (APTR)window;
    io->io_Length = sizeof(struct Window);
    
    if (OpenDevice((STRPTR)"console.device", 0, (struct IORequest *)io, 0) != 0) {
        DeleteExtIO((struct IORequest *)io);
        return NULL;
    }
    
    return io;
}

static void delete_console_io(struct IOStdReq *io)
{
    if (io) {
        CloseDevice((struct IORequest *)io);
        DeleteExtIO((struct IORequest *)io);
    }
}

static LONG console_write(struct IOStdReq *io, const char *str, LONG len)
{
    if (len < 0) {
        const char *p = str;
        len = 0;
        while (*p++) len++;
    }
    
    io->io_Command = CMD_WRITE;
    io->io_Data = (APTR)str;
    io->io_Length = len;
    DoIO((struct IORequest *)io);
    return io->io_Actual;
}

static LONG console_read(struct IOStdReq *io, char *buf, LONG len)
{
    io->io_Command = CMD_READ;
    io->io_Data = (APTR)buf;
    io->io_Length = len;
    DoIO((struct IORequest *)io);
    return io->io_Actual;
}

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct MsgPort *con_port;
    struct IOStdReq *con_io;
    char buf[256];
    LONG actual;
    int errors = 0;

    print("Testing console device input with injection...\n\n");

    /* ========== Setup: Open screen, window, console ========== */
    print("--- Setup: Opening screen, window, and console ---\n");

    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = 640;
    ns.Height = 256;
    ns.Depth = 2;
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.ViewModes = 0x8000;  /* HIRES */
    ns.Type = CUSTOMSCREEN;
    ns.Font = NULL;
    ns.DefaultTitle = (UBYTE *)"Console Input Test";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (screen == NULL) {
        print("FAIL: Could not open screen\n");
        return 20;
    }
    print("OK: Screen opened\n");

    /* Open window */
    nw.LeftEdge = 0;
    nw.TopEdge = 11;  /* Below title bar */
    nw.Width = 640;
    nw.Height = 200;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_RAWKEY;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"Console Test Window";
    nw.Screen = screen;
    nw.BitMap = NULL;
    nw.MinWidth = 0;
    nw.MinHeight = 0;
    nw.MaxWidth = 0;
    nw.MaxHeight = 0;
    nw.Type = CUSTOMSCREEN;

    window = OpenWindow(&nw);
    if (window == NULL) {
        print("FAIL: Could not open window\n");
        CloseScreen(screen);
        return 20;
    }
    print("OK: Window opened\n");

    /* Create console message port */
    con_port = CreateMsgPort();
    if (con_port == NULL) {
        print("FAIL: Could not create message port\n");
        CloseWindow(window);
        CloseScreen(screen);
        return 20;
    }
    print("OK: Message port created\n");

    /* Open console.device attached to window */
    con_io = create_console_io(window, con_port);
    if (con_io == NULL) {
        print("FAIL: Could not open console.device\n");
        DeleteMsgPort(con_port);
        CloseWindow(window);
        CloseScreen(screen);
        return 20;
    }
    print("OK: Console device opened\n\n");

    /* ========== Test 1: Write prompt then inject and read input ========== */
    print("--- Test 1: Prompt with injected input ---\n");

    /* Write prompt */
    console_write(con_io, "Enter value: ", -1);
    print("OK: Wrote prompt 'Enter value: '\n");

    /* Give console time to render */
    WaitTOF();
    WaitTOF();

    /* Inject input: "100" followed by Return */
    print("Injecting '100' + Return...\n");
    test_inject_string("100");
    test_inject_return();

    /* Give events time to be processed */
    WaitTOF();
    WaitTOF();
    WaitTOF();
    WaitTOF();
    WaitTOF();

    /* Read input */
    print("Reading console input...\n");
    actual = console_read(con_io, buf, sizeof(buf) - 1);
    buf[actual] = '\0';

    print("Read ");
    print_num(actual);
    print(" bytes: '");
    
    /* Print buffer, escaping control chars */
    for (int i = 0; i < actual; i++) {
        if (buf[i] == '\r' || buf[i] == '\n') {
            print("\\n");
        } else if (buf[i] >= 32 && buf[i] < 127) {
            char c[2] = {buf[i], 0};
            print(c);
        } else {
            print("\\x");
            print_hex(buf[i] & 0xFF);
        }
    }
    print("'\n");

    /* Check if we got the expected input */
    /* Note: Console may return "100\r" or "100\n" depending on mode */
    if (actual >= 3 && buf[0] == '1' && buf[1] == '0' && buf[2] == '0') {
        print("OK: Received expected numeric input '100'\n");
    } else {
        print("FAIL: Did not receive expected input\n");
        errors++;
    }

    /* ========== Cleanup ========== */
    print("\n--- Cleanup ---\n");

    delete_console_io(con_io);
    print("OK: Console closed\n");

    DeleteMsgPort(con_port);
    print("OK: Message port deleted\n");

    CloseWindow(window);
    print("OK: Window closed\n");

    CloseScreen(screen);
    print("OK: Screen closed\n");

    /* ========== Final result ========== */
    print("\n");
    if (errors == 0) {
        print("PASS: input_console all tests passed\n");
        return 0;
    } else {
        print("FAIL: input_console had ");
        print_num(errors);
        print(" errors\n");
        return 20;
    }
}
