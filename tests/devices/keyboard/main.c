/*
 * Test for keyboard.device event reads, matrix snapshots, aborts, and reset handlers.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/interrupts.h>
#include <exec/errors.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <devices/inputevent.h>
#include <devices/keyboard.h>
#include <libraries/keymap.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include <inline/dos.h>

#define RAWKEY_A        0x20
#define RAWKEY_NUMPAD1  0x1d

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

static volatile UWORD g_reset_calls = 0;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void test_ok(const char *msg)
{
    print("OK: ");
    print(msg);
    print("\n");
}

static void test_fail(const char *msg)
{
    print("FAIL: ");
    print(msg);
    print("\n");
}

static VOID reset_handler(register APTR data __asm("a1"))
{
    volatile UWORD *counter = (volatile UWORD *)data;

    if (counter)
    {
        (*counter)++;
    }
}

static BOOL wait_key_event(struct Window *window, UWORD *code, UWORD *qualifier, ULONG max_messages)
{
    struct IntuiMessage *msg;
    ULONG count = 0;

    while (count < max_messages)
    {
        Wait(1L << window->UserPort->mp_SigBit);
        while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
        {
            if (msg->Class == IDCMP_RAWKEY)
            {
                if (code)
                {
                    *code = (UWORD)msg->Code;
                }
                if (qualifier)
                {
                    *qualifier = (UWORD)msg->Qualifier;
                }
                ReplyMsg((struct Message *)msg);
                return TRUE;
            }
            ReplyMsg((struct Message *)msg);
            count++;
        }
    }

    return FALSE;
}

int main(void)
{
    struct MsgPort *kbd_port = NULL;
    struct IOStdReq *kbd_req = NULL;
    struct IOStdReq *kbd_req2 = NULL;
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen = NULL;
    struct Window *window = NULL;
    struct InputEvent events[2];
    UBYTE matrix[KBD_READEVENT + 23];
    struct Interrupt handler;
    LONG error;
    UWORD code;
    UWORD qualifier;
    int rc = 1;

    print("Testing keyboard.device\n");

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    if (!IntuitionBase || !GfxBase)
    {
        test_fail("required libraries failed to open");
        return 1;
    }

    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = 320;
    ns.Height = 200;
    ns.Depth = 2;
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.ViewModes = 0;
    ns.Type = CUSTOMSCREEN;
    ns.Font = NULL;
    ns.DefaultTitle = (UBYTE *)"Keyboard Device Test";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (!screen)
    {
        test_fail("OpenScreen failed");
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    nw.LeftEdge = 10;
    nw.TopEdge = 12;
    nw.Width = 280;
    nw.Height = 120;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_RAWKEY | IDCMP_CLOSEWINDOW;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_ACTIVATE;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"Keyboard Device Window";
    nw.Screen = screen;
    nw.BitMap = NULL;
    nw.MinWidth = 0;
    nw.MinHeight = 0;
    nw.MaxWidth = 0;
    nw.MaxHeight = 0;
    nw.Type = CUSTOMSCREEN;

    window = OpenWindow(&nw);
    if (!window)
    {
        test_fail("OpenWindow failed");
        CloseScreen(screen);
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    test_ok("window opened for keyboard input");

    kbd_port = CreateMsgPort();
    if (!kbd_port)
    {
        test_fail("CreateMsgPort failed");
        CloseWindow(window);
        CloseScreen(screen);
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    kbd_req = (struct IOStdReq *)CreateIORequest(kbd_port, sizeof(struct IOStdReq));
    kbd_req2 = (struct IOStdReq *)CreateIORequest(kbd_port, sizeof(struct IOStdReq));
    if (!kbd_req || !kbd_req2)
    {
        test_fail("CreateIORequest failed");
        if (kbd_req) DeleteIORequest((struct IORequest *)kbd_req);
        if (kbd_req2) DeleteIORequest((struct IORequest *)kbd_req2);
        DeleteMsgPort(kbd_port);
        CloseWindow(window);
        CloseScreen(screen);
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    error = OpenDevice((STRPTR)"keyboard.device", 0, (struct IORequest *)kbd_req, 0);
    if (error != 0)
    {
        test_fail("OpenDevice(keyboard.device) failed");
        goto cleanup;
    }
    test_ok("keyboard.device opened");

    error = OpenDevice((STRPTR)"keyboard.device", 0, (struct IORequest *)kbd_req2, 0);
    if (error != 0)
    {
        test_fail("second OpenDevice(keyboard.device) failed");
        goto cleanup;
    }
    test_ok("keyboard.device opened twice");

    kbd_req->io_Command = KBD_READEVENT;
    kbd_req->io_Data = events;
    kbd_req->io_Length = sizeof(events);
    kbd_req->io_Flags = IOF_QUICK;
    SendIO((struct IORequest *)kbd_req);
    if (CheckIO((struct IORequest *)kbd_req) == NULL)
    {
        test_ok("KBD_READEVENT stays pending without events");
    }
    else
    {
        test_fail("KBD_READEVENT should pend without events");
        goto cleanup;
    }

    print("Waiting for SHIFT key...\n");
    if (!wait_key_event(window, &code, &qualifier, 32))
    {
        test_fail("did not receive SHIFT rawkey");
        goto cleanup;
    }

    if (WaitIO((struct IORequest *)kbd_req) == 0 &&
        kbd_req->io_Error == 0 &&
        kbd_req->io_Actual == sizeof(struct InputEvent) &&
        events[0].ie_Class == IECLASS_RAWKEY &&
        events[0].ie_Code == RAWKEY_LSHIFT &&
        events[0].ie_Qualifier == IEQUALIFIER_LSHIFT &&
        events[0].ie_NextEvent == NULL)
    {
        test_ok("KBD_READEVENT returns first rawkey with held qualifier bits");
    }
    else
    {
        test_fail("KBD_READEVENT first event mismatch");
        goto cleanup;
    }

    kbd_req->io_Command = KBD_READMATRIX;
    kbd_req->io_Data = matrix;
    kbd_req->io_Length = sizeof(matrix);
    kbd_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)kbd_req);
    if (kbd_req->io_Error == 0 &&
        kbd_req->io_Actual == 32 &&
        (matrix[RAWKEY_LSHIFT >> 3] & (1 << (RAWKEY_LSHIFT & 7))) != 0)
    {
        test_ok("KBD_READMATRIX reports pressed key state");
    }
    else
    {
        test_fail("KBD_READMATRIX pressed-key snapshot mismatch");
        goto cleanup;
    }

    print("Waiting for keypad 1 and A...\n");
    if (!wait_key_event(window, &code, &qualifier, 64))
    {
        test_fail("did not receive keypad rawkey");
        goto cleanup;
    }
    if (!wait_key_event(window, &code, &qualifier, 64))
    {
        test_fail("did not receive A rawkey");
        goto cleanup;
    }

    kbd_req->io_Command = KBD_READEVENT;
    kbd_req->io_Data = events;
    kbd_req->io_Length = sizeof(events);
    kbd_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)kbd_req);
    if (kbd_req->io_Error == 0 &&
        kbd_req->io_Actual == sizeof(events) &&
        events[0].ie_Code == RAWKEY_NUMPAD1 &&
        (events[0].ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_NUMERICPAD)) ==
            (IEQUALIFIER_LSHIFT | IEQUALIFIER_NUMERICPAD) &&
        events[1].ie_Code == RAWKEY_A &&
        (events[1].ie_Qualifier & IEQUALIFIER_LSHIFT) == IEQUALIFIER_LSHIFT &&
        (events[1].ie_Qualifier & IEQUALIFIER_NUMERICPAD) == 0 &&
        events[0].ie_NextEvent == &events[1] &&
        events[1].ie_NextEvent == NULL)
    {
        test_ok("KBD_READEVENT supports partial-buffer completion and qualifiers");
    }
    else
    {
        test_fail("KBD_READEVENT multi-event read mismatch");
        goto cleanup;
    }

    kbd_req->io_Command = KBD_READEVENT;
    kbd_req->io_Data = events;
    kbd_req->io_Length = sizeof(events);
    kbd_req->io_Flags = IOF_QUICK;
    SendIO((struct IORequest *)kbd_req);
    if (CheckIO((struct IORequest *)kbd_req) == NULL)
    {
        test_ok("second KBD_READEVENT can pend");
    }
    else
    {
        test_fail("second KBD_READEVENT should pend");
        goto cleanup;
    }

    AbortIO((struct IORequest *)kbd_req);
    WaitIO((struct IORequest *)kbd_req);
    if (kbd_req->io_Error == IOERR_ABORTED)
    {
        test_ok("AbortIO aborts pending KBD_READEVENT");
    }
    else
    {
        test_fail("AbortIO did not mark request aborted");
        goto cleanup;
    }

    kbd_req->io_Command = CMD_CLEAR;
    kbd_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)kbd_req);
    if (kbd_req->io_Error == 0)
    {
        test_ok("CMD_CLEAR succeeds");
    }
    else
    {
        test_fail("CMD_CLEAR failed");
        goto cleanup;
    }

    handler.is_Node.ln_Type = NT_INTERRUPT;
    handler.is_Node.ln_Pri = 0;
    handler.is_Node.ln_Name = (STRPTR)"kbd-reset";
    handler.is_Data = (APTR)&g_reset_calls;
    handler.is_Code = (VOID (*)())reset_handler;

    kbd_req->io_Command = KBD_ADDRESETHANDLER;
    kbd_req->io_Data = &handler;
    kbd_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)kbd_req);
    if (kbd_req->io_Error == 0)
    {
        test_ok("KBD_ADDRESETHANDLER succeeds");
    }
    else
    {
        test_fail("KBD_ADDRESETHANDLER failed");
        goto cleanup;
    }

    print("Waiting for reset-warning rawkey...\n");
    if (!wait_key_event(window, &code, &qualifier, 64))
    {
        test_fail("did not receive reset-warning rawkey");
        goto cleanup;
    }
    if (g_reset_calls == 1)
    {
        test_ok("reset handler invoked by reset-warning rawkey");
    }
    else
    {
        test_fail("reset handler invocation mismatch");
        goto cleanup;
    }

    kbd_req->io_Command = KBD_RESETHANDLERDONE;
    kbd_req->io_Data = &handler;
    kbd_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)kbd_req);
    if (kbd_req->io_Error == 0)
    {
        test_ok("KBD_RESETHANDLERDONE succeeds during reset phase");
    }
    else
    {
        test_fail("KBD_RESETHANDLERDONE failed during reset phase");
        goto cleanup;
    }

    DoIO((struct IORequest *)kbd_req);
    if (kbd_req->io_Error == IOERR_NOCMD)
    {
        test_ok("KBD_RESETHANDLERDONE rejects extra acknowledgements");
    }
    else
    {
        test_fail("KBD_RESETHANDLERDONE extra-ack mismatch");
        goto cleanup;
    }

    kbd_req->io_Command = KBD_REMRESETHANDLER;
    kbd_req->io_Data = &handler;
    kbd_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)kbd_req);
    if (kbd_req->io_Error == 0)
    {
        test_ok("KBD_REMRESETHANDLER succeeds");
    }
    else
    {
        test_fail("KBD_REMRESETHANDLER failed");
        goto cleanup;
    }

    print("PASS: keyboard.device tests passed\n");
    rc = 0;

cleanup:
    if (kbd_req2 && kbd_req2->io_Device)
    {
        CloseDevice((struct IORequest *)kbd_req2);
    }
    if (kbd_req && kbd_req->io_Device)
    {
        CloseDevice((struct IORequest *)kbd_req);
    }
    if (kbd_req2)
    {
        DeleteIORequest((struct IORequest *)kbd_req2);
    }
    if (kbd_req)
    {
        DeleteIORequest((struct IORequest *)kbd_req);
    }
    if (kbd_port)
    {
        DeleteMsgPort(kbd_port);
    }
    if (window)
    {
        CloseWindow(window);
    }
    if (screen)
    {
        CloseScreen(screen);
    }
    if (GfxBase)
    {
        CloseLibrary((struct Library *)GfxBase);
    }
    if (IntuitionBase)
    {
        CloseLibrary((struct Library *)IntuitionBase);
    }

    return rc;
}
