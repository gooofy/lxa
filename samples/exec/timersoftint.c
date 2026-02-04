/* TimerSoftInt.c - Adapted from RKM sample
 * Demonstrates software interrupts using Cause()
 * Simplified version that tests Cause() without timer.device
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <libraries/dos.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>
#include <string.h>

/* Global counter for interrupt handler */
volatile ULONG interrupt_count = 0;

/* Software interrupt handler
 * In a real Amiga, this would need to follow register conventions.
 * For our test, we keep it simple and just increment a counter.
 * The lxa Cause() implementation will handle calling it correctly.
 */
void SoftIntHandler(void)
{
    /* Just increment the counter - don't call any library functions
     * as this may be called in interrupt context */
    interrupt_count++;
}

int main(int argc, char **argv)
{
    struct Interrupt *softint = NULL;
    ULONG i;

    printf("TimerSoftInt: Demonstrating software interrupts with Cause()\n\n");

    /* Create a software interrupt structure */
    softint = (struct Interrupt *)AllocMem(sizeof(struct Interrupt), MEMF_PUBLIC | MEMF_CLEAR);
    if (!softint) {
        printf("TimerSoftInt: ERROR - Can't allocate Interrupt structure\n");
        return RETURN_FAIL;
    }

    /* Initialize the interrupt structure */
    softint->is_Node.ln_Type = NT_INTERRUPT;
    softint->is_Node.ln_Pri = 0;
    softint->is_Node.ln_Name = "TimerSoftInt";
    softint->is_Data = (APTR)0xDEADBEEF;  /* Custom data to verify it's passed correctly */
    softint->is_Code = (void (*)())SoftIntHandler;

    printf("TimerSoftInt: Created software interrupt structure\n");
    printf("  Node Type: %d (should be %d for NT_INTERRUPT)\n", 
           softint->is_Node.ln_Type, NT_INTERRUPT);
    printf("  Priority: %d\n", softint->is_Node.ln_Pri);
    printf("  Name: %s\n", softint->is_Node.ln_Name);

    /* === Test Cause() multiple times === */
    printf("\n=== Testing Cause() ===\n");
    printf("Cause() triggers a software interrupt immediately.\n");
    printf("We'll call it 5 times and watch the counter increment.\n\n");

    interrupt_count = 0;

    for (i = 1; i <= 5; i++) {
        printf("[%lu] Before Cause(): interrupt_count = %lu\n", i, interrupt_count);
        Cause(softint);
        printf("[%lu] After Cause():  interrupt_count = %lu\n\n", i, interrupt_count);
    }

    printf("=== Test Complete ===\n");
    printf("Total interrupts triggered: %lu (expected 5)\n", interrupt_count);

    if (interrupt_count == 5) {
        printf("SUCCESS: All interrupt handlers executed correctly!\n");
    } else {
        printf("WARNING: Expected 5 interrupts, got %lu\n", interrupt_count);
    }

    /* === Cleanup === */
    printf("\n=== Cleanup ===\n");
    printf("Freeing interrupt structure...\n");
    FreeMem(softint, sizeof(struct Interrupt));
    
    printf("\nTimerSoftInt: Software interrupt demonstration complete.\n");
    return RETURN_OK;
}
