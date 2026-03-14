#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static int tests_failed = 0;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[32];
    char tmp[16];
    char *p = buf;
    int i = 0;

    if (n < 0)
    {
        *p++ = '-';
        n = -n;
    }

    do
    {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    while (i > 0)
        *p++ = tmp[--i];

    *p = '\0';
    print(buf);
}

static void test_pass(const char *name)
{
    print("  PASS: ");
    print(name);
    print("\n");
}

static void test_fail(const char *name, const char *reason)
{
    print("  FAIL: ");
    print(name);
    print(" - ");
    print(reason);
    print("\n");
    tests_failed++;
}

static struct DosList *alloc_node(LONG type)
{
    struct DosList *node = (struct DosList *)AllocMem(sizeof(*node), MEMF_PUBLIC | MEMF_CLEAR);

    if (node)
        node->dol_Type = type;

    return node;
}

static void link_nodes(struct DosInfo *dos_info,
                       struct DosList *first,
                       struct DosList *second,
                       struct DosList *third)
{
    if (first)
        first->dol_Next = second ? MKBADDR(second) : 0;
    if (second)
        second->dol_Next = third ? MKBADDR(third) : 0;
    if (third)
        third->dol_Next = 0;

    dos_info->di_DevInfo = first ? MKBADDR(first) : 0;
}

int main(void)
{
    struct RootNode *root;
    struct DosInfo *dos_info;
    BPTR old_head;
    struct DosList *node_a;
    struct DosList *node_b;
    struct DosList *node_c;
    struct DosList *node_x;
    BOOL ok;

    print("RemDosEntry Test\n");
    print("================\n\n");

    if (!DOSBase || !DOSBase->dl_Root)
    {
        print("FAIL: DOS root node is not initialized\n");
        return 20;
    }

    root = DOSBase->dl_Root;
    dos_info = (struct DosInfo *)BADDR(root->rn_Info);
    if (!dos_info)
    {
        print("FAIL: DOS info is not initialized\n");
        return 20;
    }

    node_a = alloc_node(DLT_DEVICE);
    node_b = alloc_node(DLT_DIRECTORY);
    node_c = alloc_node(DLT_VOLUME);
    node_x = alloc_node(DLT_DEVICE);
    if (!node_a || !node_b || !node_c || !node_x)
    {
        test_fail("Allocate DosList nodes", "AllocMem failed");
        if (node_a)
            FreeMem(node_a, sizeof(*node_a));
        if (node_b)
            FreeMem(node_b, sizeof(*node_b));
        if (node_c)
            FreeMem(node_c, sizeof(*node_c));
        if (node_x)
            FreeMem(node_x, sizeof(*node_x));
        return 20;
    }

    old_head = dos_info->di_DevInfo;

    print("Test 1: Removes the head entry\n");
    link_nodes(dos_info, node_a, node_b, node_c);
    ok = RemDosEntry(node_a);
    if (ok && BADDR(dos_info->di_DevInfo) == node_b && BADDR(node_b->dol_Next) == node_c)
        test_pass("Remove head entry");
    else
        test_fail("Remove head entry", "Head removal left the wrong chain");

    print("\nTest 2: Removes a middle entry\n");
    link_nodes(dos_info, node_a, node_b, node_c);
    ok = RemDosEntry(node_b);
    if (ok && BADDR(dos_info->di_DevInfo) == node_a && BADDR(node_a->dol_Next) == node_c)
        test_pass("Remove middle entry");
    else
        test_fail("Remove middle entry", "Middle removal left the wrong chain");

    print("\nTest 3: Removes the tail entry\n");
    link_nodes(dos_info, node_a, node_b, node_c);
    ok = RemDosEntry(node_c);
    if (ok && BADDR(dos_info->di_DevInfo) == node_a && BADDR(node_a->dol_Next) == node_b && node_b->dol_Next == 0)
        test_pass("Remove tail entry");
    else
        test_fail("Remove tail entry", "Tail removal left the wrong chain");

    print("\nTest 4: Missing entries are rejected without mutating the list\n");
    link_nodes(dos_info, node_a, node_b, node_c);
    ok = RemDosEntry(node_x);
    if (!ok && BADDR(dos_info->di_DevInfo) == node_a && BADDR(node_a->dol_Next) == node_b && BADDR(node_b->dol_Next) == node_c)
        test_pass("Reject missing entry");
    else
        test_fail("Reject missing entry", "Missing removal mutated the list");

    print("\nTest 5: NULL entries are rejected\n");
    link_nodes(dos_info, node_a, node_b, node_c);
    ok = RemDosEntry(NULL);
    if (!ok && BADDR(dos_info->di_DevInfo) == node_a)
        test_pass("Reject NULL entry");
    else
        test_fail("Reject NULL entry", "NULL handling behaved incorrectly");

    dos_info->di_DevInfo = old_head;
    FreeMem(node_x, sizeof(*node_x));
    FreeMem(node_c, sizeof(*node_c));
    FreeMem(node_b, sizeof(*node_b));
    FreeMem(node_a, sizeof(*node_a));

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
