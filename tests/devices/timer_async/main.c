/*
 * Test for timer.device - Async TR_ADDREQUEST
 * 
 * Phase 45: Tests the async timer functionality
 * - SendIO with TR_ADDREQUEST should queue the request
 * - WaitIO should block until the timer expires
 * - The task should be properly signaled when the timer fires
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <devices/timer.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

#define NEWLIST(l) ((l)->lh_Head = (struct Node *)&(l)->lh_Tail, \
                    (l)->lh_Tail = NULL, \
                    (l)->lh_TailPred = (struct Node *)&(l)->lh_Head)

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(ULONG num)
{
    char buf[16];
    int i = 0;
    
    if (num == 0) {
        buf[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (num > 0) {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }
        while (j > 0) {
            buf[i++] = temp[--j];
        }
    }
    buf[i] = '\0';
    print(buf);
}

int main(void)
{
    struct MsgPort *timerPort;
    struct timerequest *timerReq;
    LONG error;
    ULONG startTime, endTime;
    struct timerequest timeReq;  /* Second request for getting time */
    
    print("Testing timer.device async TR_ADDREQUEST\n");
    
    /* Create message port for timer replies */
    timerPort = CreateMsgPort();
    if (!timerPort) {
        print("FAIL: Cannot create message port\n");
        return 1;
    }
    print("OK: Message port created\n");
    
    /* Create timer request */
    timerReq = (struct timerequest *)CreateIORequest(timerPort, sizeof(struct timerequest));
    if (!timerReq) {
        print("FAIL: Cannot create IO request\n");
        DeleteMsgPort(timerPort);
        return 1;
    }
    print("OK: IO request created\n");
    
    /* Open timer.device with UNIT_MICROHZ for accurate timing */
    error = OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)timerReq, 0);
    if (error != 0) {
        print("FAIL: Cannot open timer.device, error=");
        print_num((ULONG)error);
        print("\n");
        DeleteIORequest((struct IORequest *)timerReq);
        DeleteMsgPort(timerPort);
        return 1;
    }
    print("OK: timer.device opened (UNIT_MICROHZ)\n");
    
    /* Copy the request for time queries */
    CopyMem(timerReq, &timeReq, sizeof(struct timerequest));
    
    /* Get start time */
    timeReq.tr_node.io_Command = TR_GETSYSTIME;
    timeReq.tr_node.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)&timeReq);
    startTime = timeReq.tr_time.tv_secs;
    
    /* Don't print variable timestamps for deterministic test output */
    
    /* Test 1: Short delay (100ms) using SendIO/WaitIO */
    print("\nTest 1: 100ms delay using SendIO/WaitIO\n");
    
    timerReq->tr_node.io_Command = TR_ADDREQUEST;
    timerReq->tr_time.tv_secs = 0;
    timerReq->tr_time.tv_micro = 100000;  /* 100ms */
    
    SendIO((struct IORequest *)timerReq);
    print("OK: SendIO called\n");
    
    WaitIO((struct IORequest *)timerReq);
    
    if (timerReq->tr_node.io_Error != 0) {
        print("FAIL: TR_ADDREQUEST returned error=");
        print_num((ULONG)timerReq->tr_node.io_Error);
        print("\n");
        goto cleanup;
    }
    print("OK: WaitIO returned\n");
    
    /* Test 2: Longer delay (500ms) */
    print("\nTest 2: 500ms delay\n");
    
    timerReq->tr_node.io_Command = TR_ADDREQUEST;
    timerReq->tr_time.tv_secs = 0;
    timerReq->tr_time.tv_micro = 500000;  /* 500ms */
    
    SendIO((struct IORequest *)timerReq);
    print("OK: SendIO called for 500ms delay\n");
    
    WaitIO((struct IORequest *)timerReq);
    
    if (timerReq->tr_node.io_Error != 0) {
        print("FAIL: TR_ADDREQUEST returned error=");
        print_num((ULONG)timerReq->tr_node.io_Error);
        print("\n");
        goto cleanup;
    }
    print("OK: 500ms delay completed\n");
    
    /* Test 3: 1 second delay */
    print("\nTest 3: 1 second delay\n");
    
    timerReq->tr_node.io_Command = TR_ADDREQUEST;
    timerReq->tr_time.tv_secs = 1;
    timerReq->tr_time.tv_micro = 0;
    
    SendIO((struct IORequest *)timerReq);
    print("OK: SendIO called for 1 second delay\n");
    
    WaitIO((struct IORequest *)timerReq);
    
    if (timerReq->tr_node.io_Error != 0) {
        print("FAIL: TR_ADDREQUEST returned error\n");
        goto cleanup;
    }
    print("OK: 1 second delay completed\n");
    
    /* Get end time to verify timing */
    timeReq.tr_node.io_Command = TR_GETSYSTIME;
    timeReq.tr_node.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)&timeReq);
    endTime = timeReq.tr_time.tv_secs;
    
    /* Don't print variable timestamps for deterministic test output */
    
    /* Verify reasonable timing (should be at least 1 second) */
    if (endTime - startTime >= 1) {
        print("OK: Timing is reasonable\n");
    } else {
        print("WARNING: Elapsed time seems too short\n");
    }
    
    print("\nPASS: All async timer tests completed successfully\n");

cleanup:
    /* Close timer.device */
    CloseDevice((struct IORequest *)timerReq);
    print("OK: timer.device closed\n");
    
    /* Cleanup */
    DeleteIORequest((struct IORequest *)timerReq);
    DeleteMsgPort(timerPort);
    print("OK: Cleanup complete\n");
    
    return 0;
}
