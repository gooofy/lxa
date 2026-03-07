#include <exec/types.h>
#include <exec/interrupts.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <hardware/intbits.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;
static ULONG g_soft_count = 0;
static ULONG g_order[8];
static ULONG g_order_len = 0;

static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

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

static void record_order(ULONG value)
{
    if (g_order_len < 8)
        g_order[g_order_len++] = value;
}

static void SoftHandlerA(void)
{
    g_soft_count++;
    record_order(1);
}

static void SoftHandlerB(void)
{
    g_soft_count++;
    record_order(2);
}

int main(void)
{
    struct Interrupt intA;
    struct Interrupt intB;
    struct List *soft_list;

    out = Output();

    print("Exec Interrupt Tests\n");
    print("====================\n\n");

    soft_list = (struct List *)SysBase->IntVects[INTB_SOFTINT].iv_Data;

    /* Test 1: AddIntServer priority insertion */
    print("Test 1: AddIntServer priority insertion\n");
    if (soft_list != NULL) {
        test_ok("Soft interrupt vector has a server list");
    } else {
        test_fail_msg("Soft interrupt vector missing server list");
        return 20;
    }

    intA.is_Node.ln_Type = NT_INTERRUPT;
    intA.is_Node.ln_Pri = 5;
    intA.is_Node.ln_Name = (char *)"SoftA";
    intA.is_Data = NULL;
    intA.is_Code = (void (*)())SoftHandlerA;

    intB.is_Node.ln_Type = NT_INTERRUPT;
    intB.is_Node.ln_Pri = 10;
    intB.is_Node.ln_Name = (char *)"SoftB";
    intB.is_Data = NULL;
    intB.is_Code = (void (*)())SoftHandlerB;

    AddIntServer(INTB_SOFTINT, &intA);
    AddIntServer(INTB_SOFTINT, &intB);

    if (soft_list->lh_Head == &intB.is_Node && intB.is_Node.ln_Succ == &intA.is_Node) {
        test_ok("AddIntServer inserts by descending priority");
    } else {
        test_fail_msg("AddIntServer priority order incorrect");
    }

    /* Test 2: Cause executes software interrupt */
    print("\nTest 2: Cause executes software interrupt\n");
    g_soft_count = 0;
    g_order_len = 0;
    Cause(&intA);
    if (g_soft_count == 1) {
        test_ok("Cause invoked soft interrupt handler");
    } else {
        test_fail_msg("Cause did not invoke soft interrupt handler");
    }
    if (intA.is_Node.ln_Type == NT_INTERRUPT) {
        test_ok("Cause preserves node type after dispatch");
    } else {
        test_fail_msg("Cause left node type in queued state");
    }

    /* Test 3: Cause ignores duplicate queueing */
    print("\nTest 3: Cause ignores duplicate queueing\n");
    g_soft_count = 0;
    intA.is_Node.ln_Type = NT_SOFTINT;
    Cause(&intA);
    if (g_soft_count == 0) {
        test_ok("Cause ignores already queued soft interrupt");
    } else {
        test_fail_msg("Cause should ignore already queued soft interrupt");
    }
    intA.is_Node.ln_Type = NT_INTERRUPT;

    /* Test 4: RemIntServer removes handlers */
    print("\nTest 4: RemIntServer removes handlers\n");
    RemIntServer(INTB_SOFTINT, &intB);
    if (soft_list->lh_Head == &intA.is_Node) {
        test_ok("RemIntServer removed head handler");
    } else {
        test_fail_msg("RemIntServer did not remove head handler");
    }

    RemIntServer(INTB_SOFTINT, &intA);
    if (soft_list->lh_TailPred == (struct Node *)soft_list) {
        test_ok("RemIntServer restored empty list");
    } else {
        test_fail_msg("RemIntServer did not restore empty list");
    }

    /* Test 5: Cause(NULL) safety */
    print("\nTest 5: Cause(NULL) safety\n");
    Cause(NULL);
    test_ok("Cause(NULL) did not crash");

    /* Test 6: Disable/Enable nesting */
    print("\nTest 6: Disable/Enable nesting\n");
    {
        BYTE original = SysBase->IDNestCnt;

        Disable();
        if (SysBase->IDNestCnt == original + 1) {
            test_ok("Disable increments IDNestCnt");
        } else {
            test_fail_msg("Disable did not increment IDNestCnt");
        }

        Disable();
        if (SysBase->IDNestCnt == original + 2) {
            test_ok("Disable nests IDNestCnt");
        } else {
            test_fail_msg("Disable did not nest IDNestCnt");
        }

        Enable();
        if (SysBase->IDNestCnt == original + 1) {
            test_ok("Enable decrements nested IDNestCnt");
        } else {
            test_fail_msg("Enable did not decrement nested IDNestCnt");
        }

        Enable();
        if (SysBase->IDNestCnt == original) {
            test_ok("Enable restores original IDNestCnt");
        } else {
            test_fail_msg("Enable did not restore original IDNestCnt");
            SysBase->IDNestCnt = original;
        }
    }

    /* Test 7: Forbid/Permit nesting */
    print("\nTest 7: Forbid/Permit nesting\n");
    {
        BYTE original = SysBase->TDNestCnt;

        Forbid();
        if (SysBase->TDNestCnt == original + 1) {
            test_ok("Forbid increments TDNestCnt");
        } else {
            test_fail_msg("Forbid did not increment TDNestCnt");
        }

        Forbid();
        if (SysBase->TDNestCnt == original + 2) {
            test_ok("Forbid nests TDNestCnt");
        } else {
            test_fail_msg("Forbid did not nest TDNestCnt");
        }

        Permit();
        if (SysBase->TDNestCnt == original + 1) {
            test_ok("Permit decrements nested TDNestCnt");
        } else {
            test_fail_msg("Permit did not decrement nested TDNestCnt");
        }

        Permit();
        if (SysBase->TDNestCnt == original) {
            test_ok("Permit restores original TDNestCnt");
        } else {
            test_fail_msg("Permit did not restore original TDNestCnt");
            SysBase->TDNestCnt = original;
        }
    }

    /* Test 8: SuperState/UserState hosted semantics */
    print("\nTest 8: SuperState/UserState hosted semantics\n");
    if (SuperState() == NULL) {
        test_ok("SuperState returns NULL in hosted implementation");
    } else {
        test_fail_msg("SuperState should return NULL in hosted implementation");
    }

    UserState(NULL);
    test_ok("UserState(NULL) did not crash");

    print("\n=============================\n");
    print("Tests passed: ");
    print_num(test_pass);
    print("\n");
    print("Tests failed: ");
    print_num(test_fail);
    print("\n");

    if (test_fail == 0) {
        print("\nPASS: All interrupt tests passed!\n");
        return 0;
    }

    print("\nFAIL: Some interrupt tests failed\n");
    return 20;
}
