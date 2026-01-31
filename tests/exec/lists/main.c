/*
 * Exec List Manipulation Tests
 *
 * Tests:
 *   - AddHead/AddTail
 *   - Remove/RemHead
 *   - Enqueue (priority ordering)
 *   - FindName
 *   - Empty list handling
 *   - Insert
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

/* NEWLIST macro - initializes an empty list */
#define NEWLIST(list) \
    do { \
        (list)->lh_Head = (struct Node *)&(list)->lh_Tail; \
        (list)->lh_Tail = NULL; \
        (list)->lh_TailPred = (struct Node *)&(list)->lh_Head; \
    } while(0)

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;

/* Helper to print a string */
static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

/* Simple number to string for test output */
static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + 15;
    BOOL neg = FALSE;
    
    *p = '\0';
    
    if (n < 0) {
        neg = TRUE;
        n = -n;
    }
    
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    
    if (neg) *--p = '-';
    
    print(p);
}

static void test_ok(const char *name)
{
    print("  OK: ");
    print(name);
    print("\n");
    test_pass++;
}

static void test_fail_msg(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    test_fail++;
}

/* Helper: check if list is empty */
static BOOL is_list_empty(struct List *list)
{
    return (list->lh_TailPred == (struct Node *)list);
}

/* Helper: count nodes in list */
static int count_nodes(struct List *list)
{
    int count = 0;
    struct Node *node;
    
    for (node = list->lh_Head; node->ln_Succ; node = node->ln_Succ) {
        count++;
    }
    return count;
}

/* Test node structure */
struct TestNode {
    struct Node node;
    LONG value;
};

int main(void)
{
    struct List list;
    struct TestNode nodes[10];
    struct Node *node;
    int i;
    
    out = Output();
    
    print("Exec List Manipulation Tests\n");
    print("============================\n\n");
    
    /* Initialize test nodes */
    for (i = 0; i < 10; i++) {
        nodes[i].node.ln_Name = NULL;
        nodes[i].node.ln_Pri = 0;
        nodes[i].node.ln_Type = NT_UNKNOWN;
        nodes[i].value = i;
    }
    
    /* Test 1: NEWLIST macro / NewList */
    print("Test 1: Initialize empty list\n");
    NEWLIST(&list);
    
    if (is_list_empty(&list)) {
        test_ok("New list is empty");
    } else {
        test_fail_msg("New list should be empty");
    }
    
    if (list.lh_Head == (struct Node *)&list.lh_Tail) {
        test_ok("lh_Head points to lh_Tail");
    } else {
        test_fail_msg("lh_Head initialization incorrect");
    }
    
    if (list.lh_Tail == NULL) {
        test_ok("lh_Tail is NULL");
    } else {
        test_fail_msg("lh_Tail should be NULL");
    }
    
    if (list.lh_TailPred == (struct Node *)&list.lh_Head) {
        test_ok("lh_TailPred points to lh_Head");
    } else {
        test_fail_msg("lh_TailPred initialization incorrect");
    }
    
    /* Test 2: AddHead */
    print("\nTest 2: AddHead\n");
    AddHead(&list, (struct Node *)&nodes[0]);
    
    if (!is_list_empty(&list)) {
        test_ok("List not empty after AddHead");
    } else {
        test_fail_msg("List should not be empty");
    }
    
    if (list.lh_Head == (struct Node *)&nodes[0]) {
        test_ok("lh_Head points to added node");
    } else {
        test_fail_msg("lh_Head not updated correctly");
    }
    
    if (count_nodes(&list) == 1) {
        test_ok("List has exactly 1 node");
    } else {
        print("    Count: ");
        print_num(count_nodes(&list));
        print("\n");
        test_fail_msg("Node count incorrect");
    }
    
    /* Test 3: AddTail */
    print("\nTest 3: AddTail\n");
    AddTail(&list, (struct Node *)&nodes[1]);
    
    if (count_nodes(&list) == 2) {
        test_ok("List has 2 nodes after AddTail");
    } else {
        test_fail_msg("Node count incorrect");
    }
    
    if (list.lh_TailPred == (struct Node *)&nodes[1]) {
        test_ok("lh_TailPred points to new tail");
    } else {
        test_fail_msg("lh_TailPred not updated");
    }
    
    /* Test 4: Multiple AddHead */
    print("\nTest 4: Multiple AddHead\n");
    AddHead(&list, (struct Node *)&nodes[2]);
    
    if (list.lh_Head == (struct Node *)&nodes[2]) {
        test_ok("New head is node 2");
    } else {
        test_fail_msg("AddHead didn't update head");
    }
    
    if (count_nodes(&list) == 3) {
        test_ok("List has 3 nodes");
    } else {
        test_fail_msg("Node count incorrect");
    }
    
    /* Test 5: RemHead */
    print("\nTest 5: RemHead\n");
    node = RemHead(&list);
    
    if (node == (struct Node *)&nodes[2]) {
        test_ok("RemHead returned correct node");
    } else {
        test_fail_msg("RemHead returned wrong node");
    }
    
    if (count_nodes(&list) == 2) {
        test_ok("List has 2 nodes after RemHead");
    } else {
        test_fail_msg("Node count incorrect after RemHead");
    }
    
    /* Test 6: Remove specific node */
    print("\nTest 6: Remove specific node\n");
    /* List now has: nodes[0] -> nodes[1] */
    /* Add more nodes first */
    AddTail(&list, (struct Node *)&nodes[3]);
    AddTail(&list, (struct Node *)&nodes[4]);
    /* List: nodes[0] -> nodes[1] -> nodes[3] -> nodes[4] */
    
    Remove((struct Node *)&nodes[1]);  /* Remove middle node */
    
    if (count_nodes(&list) == 3) {
        test_ok("List has 3 nodes after Remove");
    } else {
        print("    Count: ");
        print_num(count_nodes(&list));
        print("\n");
        test_fail_msg("Node count incorrect after Remove");
    }
    
    /* Verify nodes[0] -> nodes[3] -> nodes[4] */
    if (nodes[0].node.ln_Succ == (struct Node *)&nodes[3]) {
        test_ok("Removed node is properly unlinked");
    } else {
        test_fail_msg("List chain broken after Remove");
    }
    
    /* Test 7: RemHead on list until empty */
    print("\nTest 7: RemHead until empty\n");
    {
        int removed = 0;
        while ((node = RemHead(&list)) != NULL) {
            removed++;
            if (removed > 10) break;  /* Safety limit */
        }
        
        if (removed == 3 && is_list_empty(&list)) {
            test_ok("Removed 3 nodes, list is empty");
        } else {
            print("    Removed: ");
            print_num(removed);
            print(", Empty: ");
            print(is_list_empty(&list) ? "yes" : "no");
            print("\n");
            test_fail_msg("RemHead sequence failed");
        }
    }
    
    /* Test 8: RemHead on empty list */
    print("\nTest 8: RemHead on empty list\n");
    NEWLIST(&list);
    node = RemHead(&list);
    
    if (node == NULL) {
        test_ok("RemHead on empty list returns NULL");
    } else {
        test_fail_msg("RemHead should return NULL for empty list");
    }
    
    /* Test 9: Enqueue (priority ordering) */
    print("\nTest 9: Enqueue (priority ordering)\n");
    NEWLIST(&list);
    
    /* Set priorities */
    nodes[0].node.ln_Pri = 0;
    nodes[1].node.ln_Pri = 10;
    nodes[2].node.ln_Pri = 5;
    nodes[3].node.ln_Pri = -5;
    nodes[4].node.ln_Pri = 10;  /* Same priority as node 1 */
    
    /* Enqueue in mixed order */
    Enqueue(&list, (struct Node *)&nodes[0]);  /* Pri 0 */
    Enqueue(&list, (struct Node *)&nodes[1]);  /* Pri 10 - should be first */
    Enqueue(&list, (struct Node *)&nodes[2]);  /* Pri 5 - between 10 and 0 */
    Enqueue(&list, (struct Node *)&nodes[3]);  /* Pri -5 - should be last */
    Enqueue(&list, (struct Node *)&nodes[4]);  /* Pri 10 - after node 1 */
    
    /* Expected order: nodes[1] (10), nodes[4] (10), nodes[2] (5), nodes[0] (0), nodes[3] (-5) */
    
    if (list.lh_Head == (struct Node *)&nodes[1]) {
        test_ok("Highest priority (10) is at head");
    } else {
        test_fail_msg("Head is not highest priority node");
    }
    
    /* Verify second node is nodes[4] (same pri as first, added later) */
    if (nodes[1].node.ln_Succ == (struct Node *)&nodes[4]) {
        test_ok("Equal priority nodes maintain FIFO order");
    } else {
        test_fail_msg("Equal priority order incorrect");
    }
    
    /* Verify complete order */
    {
        BOOL order_ok = TRUE;
        struct Node *n = list.lh_Head;
        BYTE lastPri = 127;
        
        while (n->ln_Succ) {
            if (n->ln_Pri > lastPri) {
                order_ok = FALSE;
                break;
            }
            lastPri = n->ln_Pri;
            n = n->ln_Succ;
        }
        
        if (order_ok) {
            test_ok("Priority ordering is correct");
        } else {
            test_fail_msg("Priority ordering violated");
        }
    }
    
    /* Test 10: FindName */
    print("\nTest 10: FindName\n");
    NEWLIST(&list);
    
    nodes[0].node.ln_Name = (char *)"Alpha";
    nodes[1].node.ln_Name = (char *)"Beta";
    nodes[2].node.ln_Name = (char *)"Gamma";
    nodes[3].node.ln_Name = NULL;  /* Unnamed node */
    
    AddTail(&list, (struct Node *)&nodes[0]);
    AddTail(&list, (struct Node *)&nodes[1]);
    AddTail(&list, (struct Node *)&nodes[2]);
    AddTail(&list, (struct Node *)&nodes[3]);
    
    node = FindName(&list, (CONST_STRPTR)"Beta");
    if (node == (struct Node *)&nodes[1]) {
        test_ok("FindName found 'Beta'");
    } else {
        test_fail_msg("FindName didn't find 'Beta'");
    }
    
    node = FindName(&list, (CONST_STRPTR)"Alpha");
    if (node == (struct Node *)&nodes[0]) {
        test_ok("FindName found 'Alpha'");
    } else {
        test_fail_msg("FindName didn't find 'Alpha'");
    }
    
    node = FindName(&list, (CONST_STRPTR)"NonExistent");
    if (node == NULL) {
        test_ok("FindName returns NULL for non-existent name");
    } else {
        test_fail_msg("FindName should return NULL for non-existent");
    }
    
    /* Test 11: Insert */
    print("\nTest 11: Insert\n");
    NEWLIST(&list);
    
    /* Add nodes[0] and nodes[2] */
    AddHead(&list, (struct Node *)&nodes[0]);
    AddTail(&list, (struct Node *)&nodes[2]);
    /* List: nodes[0] -> nodes[2] */
    
    /* Insert nodes[1] after nodes[0] */
    Insert(&list, (struct Node *)&nodes[1], (struct Node *)&nodes[0]);
    /* List: nodes[0] -> nodes[1] -> nodes[2] */
    
    if (nodes[0].node.ln_Succ == (struct Node *)&nodes[1] &&
        nodes[1].node.ln_Succ == (struct Node *)&nodes[2]) {
        test_ok("Insert placed node correctly");
    } else {
        test_fail_msg("Insert did not place node correctly");
    }
    
    if (count_nodes(&list) == 3) {
        test_ok("List has 3 nodes after Insert");
    } else {
        test_fail_msg("Node count incorrect after Insert");
    }
    
    /* Test 12: Insert at head (pred=NULL) */
    print("\nTest 12: Insert at head (pred=NULL)\n");
    Insert(&list, (struct Node *)&nodes[5], NULL);
    /* List: nodes[5] -> nodes[0] -> nodes[1] -> nodes[2] */
    
    if (list.lh_Head == (struct Node *)&nodes[5]) {
        test_ok("Insert with NULL pred added at head");
    } else {
        test_fail_msg("Insert with NULL pred should add at head");
    }
    
    /* Summary */
    print("\n=============================\n");
    print("Tests passed: ");
    print_num(test_pass);
    print("\n");
    print("Tests failed: ");
    print_num(test_fail);
    print("\n");
    
    if (test_fail == 0) {
        print("\nPASS: All list tests passed!\n");
        return 0;
    } else {
        print("\nFAIL: Some tests failed\n");
        return 20;
    }
}
