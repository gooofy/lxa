/* SimpleTask.c - Adapted from RKM sample */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <libraries/dos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdlib.h>
#include <stdio.h>

#define STACK_SIZE 4096

struct Task *task = NULL;
char *simpletaskname = "SimpleTask";
volatile ULONG sharedvar;

void simpletask(void);

int main(int argc, char **argv)
{
    sharedvar = 0;

    task = CreateTask((CONST_STRPTR)simpletaskname, 0, simpletask, STACK_SIZE);
    if (!task) {
        printf("Can't create task\n");
        return RETURN_FAIL;
    }

    printf("Started a separate task which is incrementing a shared variable.\n");
    printf("Waiting for shared variable to reach 100000...\n");

    while (sharedvar < 100000) {
        /* Just wait a bit */
    }

    printf("The shared variable reached %u\n", (unsigned int)sharedvar);

    Forbid();
    DeleteTask(task);
    Permit();

    printf("Task deleted. Exiting.\n");
    return RETURN_OK;
}

void simpletask()
{
    while (sharedvar < 100000) {
        sharedvar++;
    }
    while (1) {
        Wait(0);
    }
}
