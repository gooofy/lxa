/*
 * Exec Task Management Tests
 *
 * Tests:
 *   - FindTask(NULL) - get current task
 *   - FindTask(name) - find task by name
 *   - SetTaskPri() - dynamic priority changes
 *   - Task state validation
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
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

int main(void)
{
    struct Task *thisTask;
    struct Task *foundTask;
    BYTE oldPri, newPri;
    
    out = Output();
    
    print("Exec Task Management Tests\n");
    print("==========================\n\n");
    
    /* Test 1: FindTask(NULL) - get current task */
    print("Test 1: FindTask(NULL)\n");
    thisTask = FindTask(NULL);
    if (thisTask != NULL) {
        test_ok("FindTask(NULL) returned non-NULL");
        
        /* Verify task name exists */
        if (thisTask->tc_Node.ln_Name != NULL) {
            print("    Task name: ");
            print(thisTask->tc_Node.ln_Name);
            print("\n");
            test_ok("Task has valid name");
        } else {
            test_fail_msg("Task name is NULL");
        }
        
        /* Verify task is running */
        if (thisTask->tc_State == TS_RUN) {
            test_ok("Task state is TS_RUN");
        } else {
            print("    Task state: ");
            print_num(thisTask->tc_State);
            print(" (expected TS_RUN=2)\n");
            test_fail_msg("Task state is not TS_RUN");
        }
        
        /* Verify task priority is reasonable */
        if (thisTask->tc_Node.ln_Pri >= -128 && thisTask->tc_Node.ln_Pri <= 127) {
            print("    Task priority: ");
            print_num(thisTask->tc_Node.ln_Pri);
            print("\n");
            test_ok("Task priority in valid range");
        } else {
            test_fail_msg("Task priority out of range");
        }
        
        /* Verify stack pointers are set */
        if (thisTask->tc_SPLower != NULL && thisTask->tc_SPUpper != NULL) {
            test_ok("Stack pointers are set");
        } else {
            test_fail_msg("Stack pointers not properly set");
        }
        
    } else {
        test_fail_msg("FindTask(NULL) returned NULL");
    }
    
    /* Test 2: FindTask(name) - find task by name */
    print("\nTest 2: FindTask(name)\n");
    if (thisTask != NULL && thisTask->tc_Node.ln_Name != NULL) {
        foundTask = FindTask((CONST_STRPTR)thisTask->tc_Node.ln_Name);
        if (foundTask == thisTask) {
            test_ok("FindTask(name) found correct task");
        } else if (foundTask != NULL) {
            test_fail_msg("FindTask(name) found wrong task");
        } else {
            test_fail_msg("FindTask(name) returned NULL");
        }
    } else {
        print("  SKIP: Cannot test - no current task name\n");
    }
    
    /* Test 3: FindTask with non-existent name */
    print("\nTest 3: FindTask(non-existent)\n");
    foundTask = FindTask((CONST_STRPTR)"NonExistentTask12345XYZ");
    if (foundTask == NULL) {
        test_ok("FindTask returns NULL for non-existent task");
    } else {
        test_fail_msg("FindTask returned non-NULL for non-existent task");
    }
    
    /* Test 4: SetTaskPri - change priority */
    print("\nTest 4: SetTaskPri()\n");
    if (thisTask != NULL) {
        oldPri = thisTask->tc_Node.ln_Pri;
        print("    Original priority: ");
        print_num(oldPri);
        print("\n");
        
        /* Set a new priority */
        newPri = 5;
        oldPri = SetTaskPri(thisTask, newPri);
        
        /* Verify the new priority is set */
        if (thisTask->tc_Node.ln_Pri == newPri) {
            test_ok("SetTaskPri set new priority correctly");
        } else {
            print("    Expected: ");
            print_num(newPri);
            print(", Got: ");
            print_num(thisTask->tc_Node.ln_Pri);
            print("\n");
            test_fail_msg("SetTaskPri did not set priority");
        }
        
        /* Restore original priority */
        SetTaskPri(thisTask, oldPri);
        if (thisTask->tc_Node.ln_Pri == oldPri) {
            test_ok("Priority restored correctly");
        } else {
            test_fail_msg("Priority restore failed");
        }
    } else {
        print("  SKIP: Cannot test - no current task\n");
    }
    
    /* Test 5: SetTaskPri with NULL task */
    print("\nTest 5: SetTaskPri(NULL)\n");
    oldPri = SetTaskPri(NULL, 10);
    /* Should return 0 and not crash */
    test_ok("SetTaskPri(NULL) did not crash");
    
    /* Test 6: SetTaskPri with extreme priorities */
    print("\nTest 6: SetTaskPri extreme values\n");
    if (thisTask != NULL) {
        oldPri = thisTask->tc_Node.ln_Pri;
        
        /* Test maximum priority */
        SetTaskPri(thisTask, 127);
        if (thisTask->tc_Node.ln_Pri == 127) {
            test_ok("Max priority (127) accepted");
        } else {
            test_fail_msg("Max priority not set correctly");
        }
        
        /* Test minimum priority */
        SetTaskPri(thisTask, -128);
        if (thisTask->tc_Node.ln_Pri == -128) {
            test_ok("Min priority (-128) accepted");
        } else {
            test_fail_msg("Min priority not set correctly");
        }
        
        /* Restore */
        SetTaskPri(thisTask, oldPri);
    }
    
    /* Test 7: Verify SysBase->ThisTask matches FindTask(NULL) */
    print("\nTest 7: SysBase->ThisTask consistency\n");
    if (SysBase->ThisTask == FindTask(NULL)) {
        test_ok("SysBase->ThisTask == FindTask(NULL)");
    } else {
        test_fail_msg("SysBase->ThisTask != FindTask(NULL)");
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
        print("\nPASS: All task tests passed!\n");
        return 0;
    } else {
        print("\nFAIL: Some tests failed\n");
        return 20;
    }
}
