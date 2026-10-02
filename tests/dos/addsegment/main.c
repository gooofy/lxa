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

static void cleanup_segment(struct Segment *segment)
{
    if (!segment)
        return;

    if (segment->seg_Seg)
        UnLoadSeg(segment->seg_Seg);

    FreeVec(segment);
}

static void restore_reslist(struct DosInfo *dos_info, BPTR old_head)
{
    struct Segment *current = (struct Segment *)BADDR(dos_info->di_ResList);

    while (current && dos_info->di_ResList != old_head)
    {
        struct Segment *next = (struct Segment *)BADDR(current->seg_Next);
        cleanup_segment(current);
        current = next;
        dos_info->di_ResList = current ? MKBADDR(current) : 0;
    }

    dos_info->di_ResList = old_head;
}

static BOOL bstr_equals_cstr(const UBYTE *bstr, const char *str)
{
    ULONG len = bstr ? bstr[0] : 0;
    ULONG i;

    if (!bstr || !str)
        return FALSE;

    for (i = 0; i < len; i++)
    {
        if (bstr[i + 1] != (UBYTE)str[i])
            return FALSE;
    }

    return str[len] == '\0';
}

int main(void)
{
    struct RootNode *root;
    struct DosInfo *dos_info;
    BPTR old_head;
    BPTR seg1;
    BPTR seg2;
    BPTR seg3;
    LONG ok;
    LONG err;
    struct Segment *entry = NULL;

    print("AddSegment Test\n");
    print("===============\n\n");

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

    old_head = dos_info->di_ResList;
    seg1 = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/HelloWorld");
    seg2 = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/HelloWorld");
    seg3 = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/HelloWorld");
    if (!seg1 || !seg2 || !seg3)
    {
        print("FAIL: Could not load test seglists\n");
        if (seg1)
            UnLoadSeg(seg1);
        if (seg2)
            UnLoadSeg(seg2);
        if (seg3)
            UnLoadSeg(seg3);
        return 20;
    }

    SetIoErr(0);
    print("Test 3: Prepends a user segment and copies its name\n");
    ok = AddSegment((CONST_STRPTR)"AddSegmentUser", seg1, 0);
    err = IoErr();
    print("  PROBE ok="); print_num(ok); print(" err="); print_num(err);
    { struct Segment *h = (struct Segment *)BADDR(dos_info->di_ResList);
      print(" head1="); print_num(h == entry); print(" uc="); print_num(h ? h->seg_UC : 99);
      print(" seg="); print_num(h ? (h->seg_Seg == seg1 ? 1 : h->seg_Seg == seg2 ? 2 : h->seg_Seg == seg3 ? 3 : 0) : -1);
      print(" nextold="); print_num(h ? h->seg_Next == old_head : -1);
      print(" name="); if (h) { char nb[64]; int k; int l = h->seg_Name[0]; if (l > 60) l = 60; for (k = 0; k < l; k++) nb[k] = h->seg_Name[k+1]; nb[l] = 0; print(nb);} print("\n"); }
    entry = (struct Segment *)BADDR(dos_info->di_ResList);
    if (ok == DOSTRUE && err == 0 && entry && entry->seg_Seg == seg1 && entry->seg_UC == 0 &&
        bstr_equals_cstr(entry->seg_Name, "AddSegmentUser") && entry->seg_Next == old_head)
        test_pass("Add user segment");
    else
        test_fail("Add user segment", "Resident list head was not initialized correctly");

    print("\nTest 4: Rejects same-type duplicates case-insensitively\n");
    ok = AddSegment((CONST_STRPTR)"addsegmentuser", seg2, 0);
    err = IoErr();
    print("  PROBE ok="); print_num(ok); print(" err="); print_num(err);
    { struct Segment *h = (struct Segment *)BADDR(dos_info->di_ResList);
      print(" head1="); print_num(h == entry); print(" uc="); print_num(h ? h->seg_UC : 99);
      print(" seg="); print_num(h ? (h->seg_Seg == seg1 ? 1 : h->seg_Seg == seg2 ? 2 : h->seg_Seg == seg3 ? 3 : 0) : -1);
      print(" nextold="); print_num(h ? h->seg_Next == old_head : -1);
      print(" name="); if (h) { char nb[64]; int k; int l = h->seg_Name[0]; if (l > 60) l = 60; for (k = 0; k < l; k++) nb[k] = h->seg_Name[k+1]; nb[l] = 0; print(nb);} print("\n"); }
    if (ok == DOSFALSE && err == ERROR_OBJECT_EXISTS && (struct Segment *)BADDR(dos_info->di_ResList) == entry)
        test_pass("Reject duplicate user segment");
    else
        test_fail("Reject duplicate user segment", "Duplicate user-name handling behaved incorrectly");

    print("\nTest 5: Allows the same name in the separate system segment namespace\n");
    ok = AddSegment((CONST_STRPTR)"ADDSEGMENTUSER", seg3, CMD_SYSTEM);
    err = IoErr();
    print("  PROBE ok="); print_num(ok); print(" err="); print_num(err);
    { struct Segment *h = (struct Segment *)BADDR(dos_info->di_ResList);
      print(" head1="); print_num(h == entry); print(" uc="); print_num(h ? h->seg_UC : 99);
      print(" seg="); print_num(h ? (h->seg_Seg == seg1 ? 1 : h->seg_Seg == seg2 ? 2 : h->seg_Seg == seg3 ? 3 : 0) : -1);
      print(" nextold="); print_num(h ? h->seg_Next == old_head : -1);
      print(" name="); if (h) { char nb[64]; int k; int l = h->seg_Name[0]; if (l > 60) l = 60; for (k = 0; k < l; k++) nb[k] = h->seg_Name[k+1]; nb[l] = 0; print(nb);} print("\n"); }
    if (ok == DOSTRUE && err == 0)
    {
        struct Segment *system_entry = (struct Segment *)BADDR(dos_info->di_ResList);
        if (system_entry && system_entry->seg_Seg == seg3 && system_entry->seg_UC == CMD_SYSTEM &&
            bstr_equals_cstr(system_entry->seg_Name, "ADDSEGMENTUSER") &&
            BADDR(system_entry->seg_Next) == entry)
            test_pass("Separate user/system namespaces");
        else
            test_fail("Separate user/system namespaces", "System segment entry was not linked correctly");
    }
    else
    {
        test_fail("Separate user/system namespaces", "System segment insertion unexpectedly failed");
    }

    restore_reslist(dos_info, old_head);

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
