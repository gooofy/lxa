/*
 * tasklist.c - Snapshot and print ExecBase task lists
 *
 * Based on RKM Exec sample, adapted for automated testing.
 * Tests: FindTask, Disable/Enable, SysBase task lists (TaskWait, TaskReady),
 *        AllocMem, FreeMem, AddTail, list traversal.
 */

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/memory.h>
#include <exec/execbase.h>

#include <clib/alib_protos.h>
#include <clib/exec_protos.h>

#include <stdio.h>
#include <string.h>

extern struct ExecBase *SysBase;

/* Extended structure to hold task information */
struct TaskNode {
    struct Node tn_Node;
    ULONG tn_TaskAddress;
    ULONG tn_SigAlloc;
    ULONG tn_SigWait;
    UBYTE tn_Name[32];
};

int main(int argc, char **argv)
{
    struct List *ourtasklist;
    struct List *exectasklist;
    struct Task *task;
    struct TaskNode *node, *tnode, *rnode = NULL;
    struct Node *execnode;
    int wait_count = 0;
    int ready_count = 0;

    printf("TaskList: ExecBase task list demonstration\n\n");

    /* Allocate memory for our list */
    if (!(ourtasklist = AllocMem(sizeof(struct List), MEMF_CLEAR))) {
        printf("ERROR: Can't allocate list\n");
        return 1;
    }
    printf("OK: Allocated list at 0x%08lx\n", (ULONG)ourtasklist);

    /* Initialize list structure (ala NewList()) */
    ourtasklist->lh_Head = (struct Node *)&ourtasklist->lh_Tail;
    ourtasklist->lh_Tail = 0;
    ourtasklist->lh_TailPred = (struct Node *)&ourtasklist->lh_Head;

    /* Make sure tasks won't switch lists or go away */
    printf("\nDisabling interrupts...\n");
    Disable();

    /* Snapshot task WAIT list */
    exectasklist = &(SysBase->TaskWait);
    for (execnode = exectasklist->lh_Head;
         execnode->ln_Succ; execnode = execnode->ln_Succ)
    {
        if ((tnode = AllocMem(sizeof(struct TaskNode), MEMF_CLEAR)))
        {
            /* Save task information we want to print */
            if (execnode->ln_Name)
                strncpy(tnode->tn_Name, execnode->ln_Name, 31);
            else
                strcpy(tnode->tn_Name, "(unnamed)");
            tnode->tn_Node.ln_Pri = execnode->ln_Pri;
            tnode->tn_TaskAddress = (ULONG)execnode;
            tnode->tn_SigAlloc = ((struct Task *)execnode)->tc_SigAlloc;
            tnode->tn_SigWait = ((struct Task *)execnode)->tc_SigWait;
            AddTail(ourtasklist, (struct Node *)tnode);
            wait_count++;
        }
        else break;
    }

    /* Snapshot task READY list */
    exectasklist = &(SysBase->TaskReady);
    for (execnode = exectasklist->lh_Head;
         execnode->ln_Succ; execnode = execnode->ln_Succ)
    {
        if ((tnode = AllocMem(sizeof(struct TaskNode), MEMF_CLEAR)))
        {
            /* Save task information we want to print */
            if (execnode->ln_Name)
                strncpy(tnode->tn_Name, execnode->ln_Name, 31);
            else
                strcpy(tnode->tn_Name, "(unnamed)");
            tnode->tn_Node.ln_Pri = execnode->ln_Pri;
            tnode->tn_TaskAddress = (ULONG)execnode;
            tnode->tn_SigAlloc = ((struct Task *)execnode)->tc_SigAlloc;
            tnode->tn_SigWait = ((struct Task *)execnode)->tc_SigWait;
            AddTail(ourtasklist, (struct Node *)tnode);
            if (!rnode) rnode = tnode;  /* first READY task */
            ready_count++;
        }
        else break;
    }

    /* Re-enable interrupts and taskswitching */
    Enable();
    printf("OK: Interrupts re-enabled\n");

    /* Summary */
    printf("\n=== Task Summary ===\n");
    printf("Waiting tasks: %d\n", wait_count);
    printf("Ready tasks: %d\n", ready_count);

    /* Print the lists - format with normalized addresses for testing */
    printf("\n=== Task Details ===\n");
    printf("Pri  Address     SigAlloc    SigWait     Name\n");

    node = (struct TaskNode *)(ourtasklist->lh_Head);
    printf("\nWAITING:\n");
    while ((tnode = (struct TaskNode *)node->tn_Node.ln_Succ))
    {
        if (tnode == rnode)
            printf("\nREADY:\n");
        
        printf("%3d  0x%08lx  0x%08lx  0x%08lx  %s\n",
               node->tn_Node.ln_Pri, node->tn_TaskAddress,
               node->tn_SigAlloc, node->tn_SigWait, node->tn_Name);

        /* Free the memory */
        FreeMem(node, sizeof(struct TaskNode));
        node = tnode;
    }
    
    FreeMem(ourtasklist, sizeof(struct List));
    printf("OK: List freed\n");

    /* Current task info */
    printf("\n=== Current Task ===\n");
    task = FindTask(NULL);
    if (task) {
        printf("Name: %s\n", task->tc_Node.ln_Name ? task->tc_Node.ln_Name : "(unnamed)");
        printf("Priority: %d\n", task->tc_Node.ln_Pri);
        printf("State: %d\n", task->tc_State);
        printf("SigAlloc: 0x%08lx\n", task->tc_SigAlloc);
        printf("SigWait: 0x%08lx\n", task->tc_SigWait);
        printf("OK: Found current task\n");
    } else {
        printf("ERROR: FindTask(NULL) returned NULL\n");
        return 1;
    }

    printf("\nPASS: TaskList tests completed successfully\n");
    return 0;
}
