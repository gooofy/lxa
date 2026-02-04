/* TimerSoftInt.c - Adapted from RKM sample
 * Demonstrates software interrupts using timer.device
 * Shows how to set up an Interrupt structure for async callbacks
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <libraries/dos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>
#include <string.h>

/* Global counter for interrupt handler */
volatile ULONG interrupt_count = 0;

/* Software interrupt handler (called in interrupt context!)
 * IMPORTANT: Must be pure C, no library calls allowed!
 * In real Amiga code, this would be in assembly.
 * For demonstration, we'll just increment a counter.
 */
void SoftIntHandler(void)
{
    interrupt_count++;
}

int main(int argc, char **argv)
{
    struct MsgPort *timerport = NULL;
    struct timerequest *timereq = NULL;
    struct Interrupt *softint = NULL;
    LONG error;
    ULONG i;

    printf("TimerSoftInt: Demonstrating software interrupts\n\n");

    /* Create a message port for device replies */
    timerport = CreatePort(NULL, 0);
    if (!timerport) {
        printf("TimerSoftInt: Can't create message port\n");
        return RETURN_FAIL;
    }
    printf("TimerSoftInt: Created message port\n");

    /* Create an IORequest for the timer device */
    timereq = (struct timerequest *)CreateExtIO(timerport, sizeof(struct timerequest));
    if (!timereq) {
        printf("TimerSoftInt: Can't create IORequest\n");
        DeletePort(timerport);
        return RETURN_FAIL;
    }
    printf("TimerSoftInt: Created IORequest\n");

    /* Open the timer device */
    printf("TimerSoftInt: Opening timer.device...\n");
    error = OpenDevice((CONST_STRPTR)"timer.device", UNIT_VBLANK, (struct IORequest *)timereq, 0);
    if (error != 0) {
        printf("TimerSoftInt: OpenDevice failed! Error = %ld\n", error);
        DeleteExtIO((struct IORequest *)timereq);
        DeletePort(timerport);
        return RETURN_FAIL;
    }
    printf("TimerSoftInt: timer.device opened\n");

    /* Create a software interrupt structure */
    softint = (struct Interrupt *)AllocMem(sizeof(struct Interrupt), MEMF_PUBLIC | MEMF_CLEAR);
    if (!softint) {
        printf("TimerSoftInt: Can't allocate Interrupt structure\n");
        CloseDevice((struct IORequest *)timereq);
        DeleteExtIO((struct IORequest *)timereq);
        DeletePort(timerport);
        return RETURN_FAIL;
    }

    /* Initialize the interrupt structure */
    softint->is_Node.ln_Type = NT_INTERRUPT;
    softint->is_Node.ln_Pri = 0;
    softint->is_Node.ln_Name = "TimerSoftInt";
    softint->is_Data = NULL;  /* Could pass custom data here */
    softint->is_Code = (void (*)())SoftIntHandler;

    printf("TimerSoftInt: Created software interrupt structure\n");
    printf("  Handler at: 0x%lx\n", (ULONG)softint->is_Code);

    /* === Test software interrupts === */
    printf("\n=== Testing Software Interrupts ===\n");
    printf("Sending 5 async timer requests with interrupt callback...\n\n");

    interrupt_count = 0;

    for (i = 1; i <= 5; i++) {
        /* Set up the timer request */
        timereq->tr_node.io_Command = TR_ADDREQUEST;
        timereq->tr_time.tv_secs = 0;
        timereq->tr_time.tv_micro = 500000 * i;  /* 0.5, 1.0, 1.5, 2.0, 2.5 seconds */

        /* Install the software interrupt for this request */
        /* Note: In real code, you'd use the ECLOCK flags to trigger the interrupt */
        
        printf("[%lu] Sending async request (%lu.%lu seconds)...\n", 
               i, timereq->tr_time.tv_secs, timereq->tr_time.tv_micro);
        
        SendIO((struct IORequest *)timereq);
        
        /* Simulate waiting and checking interrupt count */
        printf("[%lu] Waiting for completion...\n", i);
        WaitIO((struct IORequest *)timereq);
        
        /* Manually trigger the software interrupt for demonstration
         * In real code, the device would trigger it automatically */
        Cause(softint);
        
        printf("[%lu] Request completed. Interrupt count = %lu\n\n", i, interrupt_count);
        
        /* Small delay between requests */
        Delay(10);
    }

    printf("=== Test Complete ===\n");
    printf("Total interrupts triggered: %lu\n", interrupt_count);

    /* === Demonstrate Cause() vs Signal() === */
    printf("\n=== Demonstrating Cause() ===\n");
    printf("Cause() triggers a software interrupt immediately...\n");
    
    ULONG prev_count = interrupt_count;
    printf("Current interrupt count: %lu\n", prev_count);
    
    printf("Calling Cause() directly...\n");
    Cause(softint);
    
    printf("New interrupt count: %lu\n", interrupt_count);
    if (interrupt_count > prev_count) {
        printf("SUCCESS: Interrupt handler was called!\n");
    } else {
        printf("INFO: Interrupt count unchanged (may be system-dependent)\n");
    }

    /* === Cleanup === */
    printf("\n=== Cleanup ===\n");
    
    printf("Closing timer.device...\n");
    CloseDevice((struct IORequest *)timereq);
    
    printf("Freeing interrupt structure...\n");
    FreeMem(softint, sizeof(struct Interrupt));
    
    printf("Deleting IORequest...\n");
    DeleteExtIO((struct IORequest *)timereq);
    
    printf("Deleting message port...\n");
    DeletePort(timerport);
    
    printf("\nTimerSoftInt: Software interrupt demonstration complete.\n");
    return RETURN_OK;
}
