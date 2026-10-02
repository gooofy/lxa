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

/* VERTB server: A1 = is_Data (a counter); returns 0 (Z set) so the
 * rest of the chain still runs. */
ULONG vblank_server(void);
asm(
".globl _vblank_server          \n"
"_vblank_server:                \n"
"       addq.l  #1,(a1)         \n"
"       moveq   #0,d0           \n"
"       rts                     \n"
);

static void SoftHandler(void)
{
    g_soft_count++;
}

static void DummyHandler(void)
{
}

/* position of a node in a list, -1 if absent */
static LONG node_index(struct List *list, struct Node *node)
{
    struct Node *n;
    LONG i = 0;

    for (n = list->lh_Head; n->ln_Succ; n = n->ln_Succ, i++)
        if (n == node)
            return i;
    return -1;
}

int main(void)
{
    struct Interrupt intA;
    struct Interrupt intB;
    struct Interrupt softInt;
    struct List *vertb_list;
    volatile ULONG countA = 0, countB = 0;

    out = Output();

    print("Exec Interrupt Tests\n");
    print("====================\n\n");

    /* Test 1: AddIntServer priority insertion */
    print("Test 1: AddIntServer priority insertion\n");
    vertb_list = (struct List *)SysBase->IntVects[INTB_VERTB].iv_Data;
    if (vertb_list != NULL) {
        test_ok("VERTB vector has a server list");
    } else {
        test_fail_msg("VERTB vector missing server list");
        return 20;
    }

    intA.is_Node.ln_Type = NT_INTERRUPT;
    intA.is_Node.ln_Pri = -60;
    intA.is_Node.ln_Name = (char *)"ServerA";
    intA.is_Data = (APTR)&countA;
    intA.is_Code = (void (*)())vblank_server;

    intB.is_Node.ln_Type = NT_INTERRUPT;
    intB.is_Node.ln_Pri = -50;
    intB.is_Node.ln_Name = (char *)"ServerB";
    intB.is_Data = (APTR)&countB;
    intB.is_Code = (void (*)())vblank_server;

    AddIntServer(INTB_VERTB, &intA);
    AddIntServer(INTB_VERTB, &intB);

    {
        LONG ia, ib;

        Disable();
        ia = node_index(vertb_list, &intA.is_Node);
        ib = node_index(vertb_list, &intB.is_Node);
        Enable();
        if (ia >= 0 && ib >= 0) {
            test_ok("AddIntServer links both servers");
        } else {
            test_fail_msg("AddIntServer did not link the servers");
        }
        if (ib >= 0 && ib < ia) {
            test_ok("AddIntServer inserts by descending priority");
        } else {
            test_fail_msg("AddIntServer priority order incorrect");
        }
    }

    /* Test 2: the servers run every vertical blank */
    print("\nTest 2: VERTB servers run\n");
    Delay(10);
    if (countA > 0 && countB > 0) {
        test_ok("VERTB servers were called");
    } else {
        test_fail_msg("VERTB servers were not called");
    }

    /* Test 3: RemIntServer removes handlers */
    print("\nTest 3: RemIntServer removes handlers\n");
    RemIntServer(INTB_VERTB, &intB);
    RemIntServer(INTB_VERTB, &intA);
    {
        LONG ia, ib;
        ULONG a0, b0;

        Disable();
        ia = node_index(vertb_list, &intA.is_Node);
        ib = node_index(vertb_list, &intB.is_Node);
        Enable();
        if (ia < 0 && ib < 0) {
            test_ok("RemIntServer unlinked both servers");
        } else {
            test_fail_msg("RemIntServer left a server linked");
        }
        a0 = countA;
        b0 = countB;
        Delay(5);
        if (countA == a0 && countB == b0) {
            test_ok("Removed servers are no longer called");
        } else {
            test_fail_msg("Removed servers are still called");
        }
    }

    /* Test 4: Cause executes a software interrupt */
    print("\nTest 4: Cause executes software interrupt\n");
    softInt.is_Node.ln_Type = NT_INTERRUPT;
    softInt.is_Node.ln_Pri = 0;
    softInt.is_Node.ln_Name = (char *)"Soft";
    softInt.is_Data = NULL;
    softInt.is_Code = (void (*)())SoftHandler;
    g_soft_count = 0;
    Cause(&softInt);
    Delay(1);
    if (g_soft_count == 1) {
        test_ok("Cause invoked soft interrupt handler once");
    } else {
        test_fail_msg("Cause did not invoke soft interrupt handler once");
    }
    if (softInt.is_Node.ln_Type == NT_INTERRUPT) {
        test_ok("Cause restores node type after dispatch");
    } else {
        test_fail_msg("Cause left node type in queued state");
    }

    /* Test 5: Disable/Enable nesting */
    print("\nTest 5: Disable/Enable nesting\n");
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
        }
    }

    /* Test 6: Forbid/Permit nesting */
    print("\nTest 6: Forbid/Permit nesting\n");
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
        }
    }

    /* Test 7: SuperState/UserState round trip */
    print("\nTest 7: SuperState/UserState round trip\n");
    {
        APTR ssp = SuperState();
        UserState(ssp);
        test_ok("SuperState/UserState round trip returned");
    }

    /* Test 8: SetIntVector on a handler vector (disk sync: not enabled, so
     * the handler never runs); the previous handler is restored. */
    print("\nTest 8: SetIntVector handler semantics\n");
    {
        struct Interrupt handlerInt;
        struct Interrupt *oldInt;

        handlerInt.is_Node.ln_Type = NT_INTERRUPT;
        handlerInt.is_Node.ln_Pri = 0;
        handlerInt.is_Node.ln_Name = (char *)"Handler";
        handlerInt.is_Data = (APTR)&countA;
        handlerInt.is_Code = (void (*)())DummyHandler;

        Disable();
        oldInt = SetIntVector(INTB_DSKSYNC, &handlerInt);
        {
            BOOL code_ok = SysBase->IntVects[INTB_DSKSYNC].iv_Code == handlerInt.is_Code;
            BOOL data_ok = SysBase->IntVects[INTB_DSKSYNC].iv_Data == handlerInt.is_Data;
            BOOL node_ok = SysBase->IntVects[INTB_DSKSYNC].iv_Node == &handlerInt.is_Node;
            struct Interrupt *back = SetIntVector(INTB_DSKSYNC, oldInt);
            Enable();

            if (code_ok) test_ok("SetIntVector updates iv_Code");
            else test_fail_msg("SetIntVector did not update iv_Code");
            if (data_ok) test_ok("SetIntVector updates iv_Data");
            else test_fail_msg("SetIntVector did not update iv_Data");
            if (node_ok) test_ok("SetIntVector stores the Interrupt in iv_Node");
            else test_fail_msg("SetIntVector did not store iv_Node");
            if (back == &handlerInt) test_ok("SetIntVector returns the previous handler");
            else test_fail_msg("SetIntVector did not return the previous handler");
        }
    }

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
