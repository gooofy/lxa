/* DeviceUse.c - Adapted from RKM sample
 * Demonstrates basic device I/O using timer.device
 * Shows OpenDevice, DoIO, SendIO, WaitIO, CloseDevice
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <libraries/dos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    struct MsgPort *timerport = NULL;
    struct timerequest *timereq = NULL;
    LONG error;
    ULONG i;

    printf("DeviceUse: Demonstrating basic device I/O\n\n");

    /* Create a message port for device replies */
    timerport = CreatePort(NULL, 0);
    if (!timerport) {
        printf("DeviceUse: Can't create message port\n");
        return RETURN_FAIL;
    }
    printf("DeviceUse: Created message port at 0x%lx\n", (ULONG)timerport);

    /* Create an IORequest for the timer device */
    timereq = (struct timerequest *)CreateExtIO(timerport, sizeof(struct timerequest));
    if (!timereq) {
        printf("DeviceUse: Can't create IORequest\n");
        DeletePort(timerport);
        return RETURN_FAIL;
    }
    printf("DeviceUse: Created IORequest at 0x%lx\n", (ULONG)timereq);

    /* Open the timer device */
    printf("DeviceUse: Opening timer.device (UNIT_VBLANK)...\n");
    error = OpenDevice((CONST_STRPTR)"timer.device", UNIT_VBLANK, (struct IORequest *)timereq, 0);
    if (error != 0) {
        printf("DeviceUse: OpenDevice failed! Error = %ld\n", error);
        DeleteExtIO((struct IORequest *)timereq);
        DeletePort(timerport);
        return RETURN_FAIL;
    }
    printf("DeviceUse: timer.device opened successfully\n");
    printf("  Device: 0x%lx\n", (ULONG)timereq->tr_node.io_Device);
    printf("  Unit:   0x%lx\n", (ULONG)timereq->tr_node.io_Unit);

    /* === Synchronous I/O Example (DoIO) === */
    printf("\n=== Synchronous I/O Example (DoIO) ===\n");
    printf("Waiting 2 seconds using DoIO...\n");
    
    timereq->tr_node.io_Command = TR_ADDREQUEST;
    timereq->tr_time.tv_secs = 2;
    timereq->tr_time.tv_micro = 0;
    
    printf("  Starting DoIO (blocks until complete)...\n");
    error = DoIO((struct IORequest *)timereq);
    printf("  DoIO completed! Error = %ld\n", error);

    /* === Asynchronous I/O Example (SendIO/WaitIO) === */
    printf("\n=== Asynchronous I/O Example (SendIO/WaitIO) ===\n");
    printf("Performing 3 async timer requests...\n");
    
    for (i = 1; i <= 3; i++) {
        timereq->tr_node.io_Command = TR_ADDREQUEST;
        timereq->tr_time.tv_secs = 1;
        timereq->tr_time.tv_micro = 0;
        
        printf("  [%lu] Sending async request (SendIO)...\n", i);
        SendIO((struct IORequest *)timereq);
        
        printf("  [%lu] Request sent, doing other work...\n", i);
        /* In a real program, you'd do other work here */
        
        printf("  [%lu] Waiting for completion (WaitIO)...\n", i);
        error = WaitIO((struct IORequest *)timereq);
        printf("  [%lu] Request completed! Error = %ld\n", i, error);
    }

    /* === GetMsg/CheckIO Example === */
    printf("\n=== CheckIO Example ===\n");
    printf("Sending async request and polling with CheckIO...\n");
    
    timereq->tr_node.io_Command = TR_ADDREQUEST;
    timereq->tr_time.tv_secs = 1;
    timereq->tr_time.tv_micro = 500000;  /* 1.5 seconds */
    
    printf("  Sending async request...\n");
    SendIO((struct IORequest *)timereq);
    
    /* Poll until complete */
    ULONG count = 0;
    while (!CheckIO((struct IORequest *)timereq)) {
        count++;
        if (count % 100000 == 0) {
            printf("  Still waiting... (%lu checks)\n", count);
        }
    }
    
    /* Get the completed request */
    WaitIO((struct IORequest *)timereq);
    printf("  Request completed after %lu checks!\n", count);

    /* === AbortIO Example === */
    printf("\n=== AbortIO Example ===\n");
    printf("Sending request and immediately aborting it...\n");
    
    timereq->tr_node.io_Command = TR_ADDREQUEST;
    timereq->tr_time.tv_secs = 10;  /* Long delay */
    timereq->tr_time.tv_micro = 0;
    
    printf("  Sending 10-second request...\n");
    SendIO((struct IORequest *)timereq);
    
    printf("  Aborting request...\n");
    AbortIO((struct IORequest *)timereq);
    
    printf("  Waiting for abort to complete...\n");
    error = WaitIO((struct IORequest *)timereq);
    printf("  Request aborted! Error = %ld\n", error);
    if (error == -2) {  /* IOERR_ABORTED */
        printf("  (IOERR_ABORTED = request was successfully aborted)\n");
    }

    /* === Cleanup === */
    printf("\n=== Cleanup ===\n");
    
    printf("  Closing timer.device...\n");
    CloseDevice((struct IORequest *)timereq);
    
    printf("  Deleting IORequest...\n");
    DeleteExtIO((struct IORequest *)timereq);
    
    printf("  Deleting message port...\n");
    DeletePort(timerport);
    
    printf("\nDeviceUse: Device I/O demonstration complete.\n");
    return RETURN_OK;
}
