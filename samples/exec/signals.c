/* Signals.c - Exec signals sample */
#include <exec/types.h>
#include <exec/tasks.h>
#include <clib/exec_protos.h>
#include <stdio.h>

int main(void)
{
    BYTE sigBit;
    ULONG sigMask;
    ULONG received;

    sigBit = AllocSignal(-1);
    if (sigBit == -1) {
        printf("Can't allocate signal\n");
        return 1;
    }

    sigMask = 1L << sigBit;
    printf("Allocated signal\n");

    printf("Signaling ourselves...\n");
    Signal(FindTask(NULL), sigMask);

    printf("Waiting for signal...\n");
    received = Wait(sigMask);

    if (received & sigMask) {
        printf("Received expected signal!\n");
    } else {
        printf("Received unexpected signal: 0x%08x\n", (unsigned int)received);
    }

    FreeSignal(sigBit);
    return 0;
}
