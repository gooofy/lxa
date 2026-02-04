/* Semaphore.c - Exec semaphore sample */
#include <exec/types.h>
#include <exec/semaphores.h>
#include <clib/exec_protos.h>
#include <stdio.h>

int main(void)
{
    struct SignalSemaphore sem;

    InitSemaphore(&sem);
    printf("Semaphore initialized.\n");

    printf("Obtaining exclusive access...\n");
    ObtainSemaphore(&sem);
    printf("Got it! Holding for a bit...\n");
    
    /* In a real app, you'd do shared data access here */
    
    printf("Releasing semaphore.\n");
    ReleaseSemaphore(&sem);

    printf("Obtaining shared access...\n");
    ObtainSemaphoreShared(&sem);
    printf("Got shared access!\n");
    ReleaseSemaphore(&sem);

    printf("Done.\n");
    return 0;
}
