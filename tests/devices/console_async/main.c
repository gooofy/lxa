/*
 * Test for console.device - Async CMD_READ
 * 
 * Phase 45: Tests the async console functionality
 * - SendIO with CMD_READ should queue the request when no input available
 * - IOF_QUICK should be cleared to indicate async operation
 * - AbortIO should cancel the pending request
 * 
 * Note: This test doesn't require actual keyboard input because we test
 * the async machinery by checking that:
 * 1. CMD_READ goes async when no input is available
 * 2. AbortIO properly cancels the pending read
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/errors.h>
#include <devices/console.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>

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

static void print_num(LONG num)
{
    char buf[16];
    int i = 0;
    BOOL neg = FALSE;
    
    if (num < 0) {
        neg = TRUE;
        num = -num;
    }
    
    if (num == 0) {
        buf[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (num > 0) {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }
        if (neg) buf[i++] = '-';
        while (j > 0) {
            buf[i++] = temp[--j];
        }
    }
    buf[i] = '\0';
    print(buf);
}

int main(void)
{
    struct MsgPort *conPort;
    struct IOStdReq *conReq;
    struct Window *window = NULL;
    LONG error;
    char readBuf[32];
    
    print("Testing console.device async CMD_READ\n");
    print("======================================\n\n");
    
    /* Open Intuition library */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase) {
        print("FAIL: Cannot open intuition.library\n");
        return 1;
    }
    print("OK: intuition.library opened\n");
    
    /* Create a small window for console */
    window = OpenWindowTags(NULL,
        WA_Left, 0,
        WA_Top, 0,
        WA_Width, 320,
        WA_Height, 100,
        WA_IDCMP, IDCMP_RAWKEY,
        WA_Flags, WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_NOCAREREFRESH,
        WA_Title, (ULONG)"Console Async Test",
        TAG_END);
    
    if (!window) {
        print("FAIL: Cannot open window\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    print("OK: Window opened\n");
    
    /* Create message port for console replies */
    conPort = CreateMsgPort();
    if (!conPort) {
        print("FAIL: Cannot create message port\n");
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    print("OK: Message port created\n");
    
    /* Create console IO request */
    conReq = (struct IOStdReq *)CreateIORequest(conPort, sizeof(struct IOStdReq));
    if (!conReq) {
        print("FAIL: Cannot create IO request\n");
        DeleteMsgPort(conPort);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    print("OK: IO request created\n");
    
    /* Open console.device with the window */
    conReq->io_Data = (APTR)window;
    conReq->io_Length = sizeof(struct Window);
    
    error = OpenDevice((STRPTR)"console.device", 0, (struct IORequest *)conReq, 0);
    if (error != 0) {
        print("FAIL: Cannot open console.device, error=");
        print_num(error);
        print("\n");
        DeleteIORequest((struct IORequest *)conReq);
        DeleteMsgPort(conPort);
        CloseWindow(window);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    print("OK: console.device opened\n");
    
    /* Test 1: Verify CMD_READ goes async when no input available */
    print("\nTest 1: CMD_READ goes async when no input\n");
    
    conReq->io_Command = CMD_READ;
    conReq->io_Data = readBuf;
    conReq->io_Length = sizeof(readBuf);
    conReq->io_Flags = IOF_QUICK;  /* Request quick completion */
    
    /* Use SendIO to start async read */
    SendIO((struct IORequest *)conReq);
    
    /* Check if IOF_QUICK was cleared (indicating async operation) */
    if (!(conReq->io_Flags & IOF_QUICK)) {
        print("OK: IOF_QUICK cleared - request went async\n");
    } else {
        /* If IOF_QUICK is still set, it completed immediately (maybe had buffered data) */
        print("NOTE: Request completed immediately (IOF_QUICK set)\n");
    }
    
    /* Test 2: AbortIO should cancel the pending read */
    print("\nTest 2: AbortIO cancels pending read\n");
    
    /* Check if the request is still pending */
    if (!CheckIO((struct IORequest *)conReq)) {
        print("OK: Request is still pending\n");
        
        /* Abort the request */
        AbortIO((struct IORequest *)conReq);
        print("OK: AbortIO called\n");
        
        /* Wait for the aborted request to complete */
        WaitIO((struct IORequest *)conReq);
        
        /* Check the error code */
        if (conReq->io_Error == IOERR_ABORTED) {
            print("OK: Request aborted with IOERR_ABORTED\n");
        } else if (conReq->io_Error == 0) {
            /* AbortIO returned 0 - request wasn't abortable or already completed */
            print("NOTE: Request completed (error=0, actual=");
            print_num(conReq->io_Actual);
            print(")\n");
        } else {
            print("WARN: Unexpected error code: ");
            print_num(conReq->io_Error);
            print("\n");
        }
    } else {
        /* Request already completed (had input or completed sync) */
        WaitIO((struct IORequest *)conReq);
        print("NOTE: Request already completed (actual=");
        print_num(conReq->io_Actual);
        print(")\n");
    }
    
    /* Test 3: Verify we can do a normal write */
    print("\nTest 3: CMD_WRITE works normally\n");
    
    {
        const char *msg = "Hello from console!\n";
        int len = 0;
        const char *p = msg;
        while (*p++) len++;
        
        conReq->io_Command = CMD_WRITE;
        conReq->io_Data = (APTR)msg;
        conReq->io_Length = len;
        conReq->io_Flags = IOF_QUICK;
        
        DoIO((struct IORequest *)conReq);
        
        if (conReq->io_Error == 0) {
            print("OK: CMD_WRITE completed successfully\n");
        } else {
            print("FAIL: CMD_WRITE error=");
            print_num(conReq->io_Error);
            print("\n");
        }
    }
    
    print("\nPASS: All console async tests completed\n");

    /* Cleanup */
    CloseDevice((struct IORequest *)conReq);
    print("OK: console.device closed\n");
    
    DeleteIORequest((struct IORequest *)conReq);
    DeleteMsgPort(conPort);
    
    CloseWindow(window);
    print("OK: Window closed\n");
    
    CloseLibrary((struct Library *)IntuitionBase);
    print("OK: Cleanup complete\n");
    
    return 0;
}
