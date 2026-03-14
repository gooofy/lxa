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

static BSTR alloc_bstr(const char *name)
{
    ULONG len = 0;
    UBYTE *buffer;
    ULONG i;

    while (name[len])
        len++;

    buffer = (UBYTE *)AllocMem(len + 2, MEMF_PUBLIC | MEMF_CLEAR);
    if (!buffer)
        return 0;

    buffer[0] = (UBYTE)len;
    for (i = 0; i < len; i++)
        buffer[i + 1] = (UBYTE)name[i];

    return MKBADDR(buffer);
}

static void free_bstr(BSTR bstr)
{
    UBYTE *buffer = (UBYTE *)BADDR(bstr);

    if (buffer)
        FreeMem(buffer, buffer[0] + 2);
}

static struct DosList *alloc_node(LONG type, const char *name)
{
    struct DosList *node = (struct DosList *)AllocMem(sizeof(*node), MEMF_PUBLIC | MEMF_CLEAR);

    if (!node)
        return NULL;

    node->dol_Type = type;
    node->dol_Name = alloc_bstr(name);
    if (!node->dol_Name)
    {
        FreeMem(node, sizeof(*node));
        return NULL;
    }

    return node;
}

static void free_node(struct DosList *node)
{
    if (!node)
        return;

    free_bstr(node->dol_Name);
    FreeMem(node, sizeof(*node));
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
    struct DosList *node_assign_dup;
    struct DosList *node_volume_dup;
    LONG ok;
    LONG err;

    print("AddDosEntry Test\n");
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

    node_a = alloc_node(DLT_DEVICE, "SYS");
    node_b = alloc_node(DLT_DEVICE, "C");
    node_assign_dup = alloc_node(DLT_DIRECTORY, "sys");
    node_volume_dup = alloc_node(DLT_VOLUME, "SYS");
    if (!node_a || !node_b || !node_assign_dup || !node_volume_dup)
    {
        test_fail("Allocate DosList nodes", "AllocMem failed");
        free_node(node_volume_dup);
        free_node(node_assign_dup);
        free_node(node_b);
        free_node(node_a);
        return 20;
    }

    old_head = dos_info->di_DevInfo;

    print("Test 1: Adds the first entry to an empty list\n");
    link_nodes(dos_info, NULL, NULL, NULL);
    ok = AddDosEntry(node_a);
    err = IoErr();
    if (ok == DOSTRUE && err == 0 && BADDR(dos_info->di_DevInfo) == node_a && node_a->dol_Next == 0)
        test_pass("Add first entry");
    else
        test_fail("Add first entry", "First entry was not inserted as the head");

    print("\nTest 2: Adds unique non-volume entries at the head\n");
    link_nodes(dos_info, node_a, NULL, NULL);
    ok = AddDosEntry(node_b);
    err = IoErr();
    if (ok == DOSTRUE && err == 0 && BADDR(dos_info->di_DevInfo) == node_b && BADDR(node_b->dol_Next) == node_a)
        test_pass("Add unique non-volume entry");
    else
        test_fail("Add unique non-volume entry", "Unique entry was not prepended correctly");

    print("\nTest 3: Rejects duplicate device or assign names\n");
    link_nodes(dos_info, node_a, node_b, NULL);
    ok = AddDosEntry(node_assign_dup);
    err = IoErr();
    if (ok == DOSFALSE && err == ERROR_OBJECT_EXISTS && BADDR(dos_info->di_DevInfo) == node_a && BADDR(node_a->dol_Next) == node_b)
        test_pass("Reject duplicate non-volume name");
    else
        test_fail("Reject duplicate non-volume name", "Duplicate non-volume entry was not rejected cleanly");

    print("\nTest 4: Allows volume names to overlap non-volume names\n");
    link_nodes(dos_info, node_a, node_b, NULL);
    ok = AddDosEntry(node_volume_dup);
    err = IoErr();
    if (ok == DOSTRUE && err == 0 && BADDR(dos_info->di_DevInfo) == node_volume_dup && BADDR(node_volume_dup->dol_Next) == node_a)
        test_pass("Allow duplicate volume name");
    else
        test_fail("Allow duplicate volume name", "Volume entry should not conflict with device or assign names");

    print("\nTest 5: NULL entries are rejected\n");
    link_nodes(dos_info, node_a, node_b, NULL);
    ok = AddDosEntry(NULL);
    err = IoErr();
    if (ok == DOSFALSE && err == ERROR_REQUIRED_ARG_MISSING && BADDR(dos_info->di_DevInfo) == node_a && BADDR(node_a->dol_Next) == node_b)
        test_pass("Reject NULL entry");
    else
        test_fail("Reject NULL entry", "NULL handling behaved incorrectly");

    dos_info->di_DevInfo = old_head;
    free_node(node_volume_dup);
    free_node(node_assign_dup);
    free_node(node_b);
    free_node(node_a);

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
