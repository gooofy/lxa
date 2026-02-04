/* BuildList.c - Adapted from RKM sample
 * Demonstrates Exec list management operations
 * Creates a list, adds nodes, iterates, removes nodes
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <libraries/dos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>
#include <string.h>

/* Define a custom node structure */
struct MyNode {
    struct Node mn_Node;
    ULONG mn_ID;
    char mn_Name[32];
};

void PrintList(struct List *list);

int main(int argc, char **argv)
{
    struct List *mylist;
    struct MyNode *node;
    ULONG i;

    printf("BuildList: Demonstrating Exec list management\n\n");

    /* Allocate a list header */
    mylist = (struct List *)AllocMem(sizeof(struct List), MEMF_PUBLIC | MEMF_CLEAR);
    if (!mylist) {
        printf("BuildList: Can't allocate list\n");
        return RETURN_FAIL;
    }

    /* Initialize the list */
    NewList(mylist);
    printf("BuildList: Created and initialized list at 0x%lx\n\n", (ULONG)mylist);

    /* Add 5 nodes to the list */
    printf("BuildList: Adding 5 nodes to the list...\n");
    for (i = 1; i <= 5; i++) {
        node = (struct MyNode *)AllocMem(sizeof(struct MyNode), MEMF_PUBLIC | MEMF_CLEAR);
        if (!node) {
            printf("BuildList: Can't allocate node %lu\n", i);
            break;
        }
        
        node->mn_Node.ln_Type = NT_USER;
        node->mn_Node.ln_Pri = i;  /* Priority for sorted insertion */
        node->mn_ID = i * 100;
        sprintf(node->mn_Name, "Node_%lu", i);
        node->mn_Node.ln_Name = node->mn_Name;  /* Set pointer for FindName */
        
        /* Add to tail of list */
        AddTail(mylist, (struct Node *)node);
        printf("  Added: %s (ID=%lu, Pri=%d)\n", 
               node->mn_Name, node->mn_ID, node->mn_Node.ln_Pri);
    }

    printf("\nBuildList: List contents after AddTail:\n");
    PrintList(mylist);

    /* Add a high-priority node at the head */
    printf("\nBuildList: Adding a high-priority node at the head...\n");
    node = (struct MyNode *)AllocMem(sizeof(struct MyNode), MEMF_PUBLIC | MEMF_CLEAR);
    if (node) {
        node->mn_Node.ln_Type = NT_USER;
        node->mn_Node.ln_Pri = 100;
        node->mn_ID = 999;
        sprintf(node->mn_Name, "HighPriority");
        node->mn_Node.ln_Name = node->mn_Name;  /* Set pointer for FindName */
        
        AddHead(mylist, (struct Node *)node);
        printf("  Added: %s (ID=%lu, Pri=%d)\n", 
               node->mn_Name, node->mn_ID, node->mn_Node.ln_Pri);
    }

    printf("\nBuildList: List contents after AddHead:\n");
    PrintList(mylist);

    /* Remove the head node */
    printf("\nBuildList: Removing head node...\n");
    node = (struct MyNode *)RemHead(mylist);
    if (node) {
        printf("  Removed: %s (ID=%lu)\n", node->mn_Name, node->mn_ID);
        FreeMem(node, sizeof(struct MyNode));
    }

    printf("\nBuildList: List contents after RemHead:\n");
    PrintList(mylist);

    /* Find a specific node by name */
    printf("\nBuildList: Finding node 'Node_3'...\n");
    node = (struct MyNode *)FindName(mylist, "Node_3");
    if (node) {
        printf("  Found: %s (ID=%lu) at 0x%lx\n", 
               node->mn_Name, node->mn_ID, (ULONG)node);
        
        /* Remove this specific node */
        printf("  Removing 'Node_3'...\n");
        Remove((struct Node *)node);
        FreeMem(node, sizeof(struct MyNode));
    }

    printf("\nBuildList: List contents after removing Node_3:\n");
    PrintList(mylist);

    /* Clean up - remove and free all remaining nodes */
    printf("\nBuildList: Cleaning up list...\n");
    while (!IsListEmpty(mylist)) {
        node = (struct MyNode *)RemHead(mylist);
        if (node) {
            printf("  Freeing: %s\n", node->mn_Name);
            FreeMem(node, sizeof(struct MyNode));
        }
    }

    /* Free the list header */
    FreeMem(mylist, sizeof(struct List));
    
    printf("\nBuildList: Done.\n");
    return RETURN_OK;
}

void PrintList(struct List *list)
{
    struct MyNode *node;
    ULONG count = 0;

    if (!list || IsListEmpty(list)) {
        printf("  (empty list)\n");
        return;
    }

    for (node = (struct MyNode *)list->lh_Head;
         node->mn_Node.ln_Succ;
         node = (struct MyNode *)node->mn_Node.ln_Succ) {
        count++;
        printf("  [%lu] %s - ID=%lu, Pri=%d, Addr=0x%lx\n",
               count, node->mn_Name, node->mn_ID, 
               node->mn_Node.ln_Pri, (ULONG)node);
    }
}
