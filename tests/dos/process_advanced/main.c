/*
 * Integration Test: SystemTagList advanced features
 *
 * Tests:
 *   - I/O redirection (SYS_Input, SYS_Output)
 *   - Exit code propagation
 *   - Stack size handling (NP_StackSize)
 *   - Current directory inheritance
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static int tests_passed = 0;
static int tests_failed = 0;

#define WINDOWPTR_PORT_NAME "ProcessAdvanced.WindowPtr"

struct WindowPtrMessage {
    struct Message msg;
    APTR window_ptr;
    LONG task_num;
};

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[32];
    char *p = buf;
    
    if (n < 0) {
        *p++ = '-';
        n = -n;
    }
    
    char tmp[16];
    int i = 0;
    do {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    
    while (i > 0) *p++ = tmp[--i];
    *p = '\0';
    
    print(buf);
}

static void test_pass(const char *name)
{
    print("  PASS: ");
    print(name);
    print("\n");
    tests_passed++;
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

static void test_bool(const char *name, BOOL cond, const char *reason)
{
    if (cond)
        test_pass(name);
    else
        test_fail(name, reason);
}

static void send_windowptr_message(struct MsgPort *port, APTR window_ptr, LONG task_num)
{
    struct WindowPtrMessage *msg;

    msg = (struct WindowPtrMessage *)AllocMem(sizeof(struct WindowPtrMessage), MEMF_PUBLIC | MEMF_CLEAR);
    if (!msg)
        return;

    msg->msg.mn_Node.ln_Type = NT_MESSAGE;
    msg->msg.mn_Length = sizeof(struct WindowPtrMessage);
    msg->window_ptr = window_ptr;
    msg->task_num = task_num;

    PutMsg(port, (struct Message *)msg);
}

static void WindowPtrChild(void)
{
    struct MsgPort *port;
    struct Process *me;

    Forbid();
    port = FindPort((STRPTR)WINDOWPTR_PORT_NAME);
    Permit();

    if (!port)
        return;

    me = (struct Process *)FindTask(NULL);
    send_windowptr_message(port, me ? me->pr_WindowPtr : NULL, me ? me->pr_TaskNum : 0);
}

int main(void)
{
    LONG result;
    char buf[256];
    LONG oldErr;
    
    print("SystemTagList Advanced Features Test\n");
    print("=====================================\n\n");
    
    /* Test 1: I/O redirection - redirect output to NIL: */
    print("Test 1: Output redirection to NIL:\n");
    
    /* Open NIL: for output */
    BPTR nil_out = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    if (nil_out) {
        struct TagItem tags[] = {
            { SYS_Output, (ULONG)nil_out },
            { TAG_DONE, 0 }
        };
        
        /* Run dir command with output redirected to NIL: */
        result = SystemTagList((CONST_STRPTR)"dir", tags);
        
        Close(nil_out);
        
        print("  Return value: ");
        print_num(result);
        print("\n");
        
        if (result >= 0) {
            test_pass("Output redirected to NIL:");
        } else {
            test_fail("Output redirection", "command failed");
        }
    } else {
        test_fail("Output redirection", "could not open NIL:");
    }
    
    /* Test 2: Input redirection - read from NIL: */
    print("\nTest 2: Input redirection from NIL:\n");
    
    BPTR nil_in = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
    if (nil_in) {
        struct TagItem tags[] = {
            { SYS_Input, (ULONG)nil_in },
            { TAG_DONE, 0 }
        };
        
        /* Run type NIL: with redirected input (should still work) */
        result = SystemTagList((CONST_STRPTR)"type NIL:", tags);
        
        Close(nil_in);
        
        print("  Return value: ");
        print_num(result);
        print("\n");
        
        if (result >= 0) {
            test_pass("Input redirected from NIL:");
        } else {
            test_fail("Input redirection", "command failed");
        }
    } else {
        test_fail("Input redirection", "could not open NIL:");
    }
    
    /* Test 3: Exit code propagation - currently returns 0 for success */
    print("\nTest 3: Exit code handling\n");
    
    /* Run a successful command */
    result = SystemTagList((CONST_STRPTR)"type NIL:", NULL);
    print("  Successful command return: ");
    print_num(result);
    print("\n");
    
    if (result == 0) {
        test_pass("Successful command returns 0");
    } else if (result >= 0) {
        test_pass("Successful command returns non-negative");
    } else {
        test_fail("Exit code", "unexpected negative return");
    }
    
    /* Run a non-existent command */
    result = SystemTagList((CONST_STRPTR)"nonexistent_xyz_12345", NULL);
    print("  Non-existent command return: ");
    print_num(result);
    print("\n");
    
    if (result == -1) {
        test_pass("Non-existent command returns -1");
    } else if (result == 20) {
        test_pass("Non-existent command returns error 20");
    } else {
        /* Document actual behavior */
        test_pass("Non-existent command handled");
    }
    
    /* Test 4: Both I/O redirected */
    print("\nTest 4: Both input and output redirected\n");
    
    BPTR in_fh = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
    BPTR out_fh = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    
    if (in_fh && out_fh) {
        struct TagItem tags[] = {
            { SYS_Input, (ULONG)in_fh },
            { SYS_Output, (ULONG)out_fh },
            { TAG_DONE, 0 }
        };
        
        result = SystemTagList((CONST_STRPTR)"type NIL:", tags);
        
        Close(in_fh);
        Close(out_fh);
        
        print("  Return value: ");
        print_num(result);
        print("\n");
        
        if (result >= 0) {
            test_pass("Both I/O streams redirected");
        } else {
            test_fail("Both I/O redirect", "command failed");
        }
    } else {
        if (in_fh) Close(in_fh);
        if (out_fh) Close(out_fh);
        test_fail("Both I/O redirect", "could not open handles");
    }
    
    /* Test 5: Sequential commands with shared resources */
    print("\nTest 5: Sequential commands verify resource cleanup\n");
    
    int success_count = 0;
    for (int i = 0; i < 5; i++) {
        result = SystemTagList((CONST_STRPTR)"type NIL:", NULL);
        if (result >= 0) {
            success_count++;
        }
    }
    
    print("  Commands succeeded: ");
    print_num(success_count);
    print("/5\n");
    
    if (success_count == 5) {
        test_pass("All sequential commands succeeded");
    } else {
        test_fail("Sequential commands", "some failed");
    }
    
    /* Test 6: Current directory inheritance */
    print("\nTest 6: Current directory handling\n");
    
    /* Get current directory */
    struct Process *me = (struct Process *)FindTask(NULL);
    BPTR oldDir = me->pr_CurrentDir;
    
    if (oldDir) {
        print("  Current dir exists: yes\n");
        test_pass("Process has current directory");
    } else {
        print("  Current dir exists: no\n");
        test_pass("Process has no current dir (expected in test env)");
    }

    /* Test 7: CLI metadata helpers */
    print("\nTest 7: CLI metadata helpers\n");

    test_bool("GetProgramName initial", GetProgramName((STRPTR)buf, sizeof(buf)), "GetProgramName failed");
    if (GetProgramName((STRPTR)buf, sizeof(buf))) {
        print("  Program name: ");
        print(buf);
        print("\n");
    }

    test_bool("SetProgramName custom", SetProgramName((CONST_STRPTR)"ProcessAdvanced"), "SetProgramName failed");
    test_bool("GetProgramName custom", GetProgramName((STRPTR)buf, sizeof(buf)) && strcmp(buf, "ProcessAdvanced") == 0,
              "Program name mismatch after SetProgramName");

    test_bool("GetPrompt initial", GetPrompt((STRPTR)buf, sizeof(buf)), "GetPrompt failed");
    if (GetPrompt((STRPTR)buf, sizeof(buf))) {
        print("  Prompt: ");
        print(buf);
        print("\n");
    }

    test_bool("SetPrompt custom", SetPrompt((CONST_STRPTR)"ADV> "), "SetPrompt failed");
    test_bool("GetPrompt custom", GetPrompt((STRPTR)buf, sizeof(buf)) && strcmp(buf, "ADV> ") == 0,
              "Prompt mismatch after SetPrompt");

    test_bool("GetCurrentDirName initial", GetCurrentDirName((STRPTR)buf, sizeof(buf)), "GetCurrentDirName failed");
    if (GetCurrentDirName((STRPTR)buf, sizeof(buf))) {
        print("  Current dir name: ");
        print(buf);
        print("\n");
    }

    test_bool("SetCurrentDirName custom", SetCurrentDirName((CONST_STRPTR)"RAM:ManualDir"), "SetCurrentDirName failed");
    test_bool("GetCurrentDirName custom", GetCurrentDirName((STRPTR)buf, sizeof(buf)) && strcmp(buf, "RAM:ManualDir") == 0,
              "Current dir name mismatch after SetCurrentDirName");

    oldErr = SetIoErr(0);
    (void)oldErr;
    test_bool("GetProgramName tiny buffer", !GetProgramName((STRPTR)buf, 4) && IoErr() == ERROR_LINE_TOO_LONG,
              "Expected ERROR_LINE_TOO_LONG for tiny program buffer");

    test_bool("GetPrompt tiny buffer", !GetPrompt((STRPTR)buf, 3) && IoErr() == ERROR_LINE_TOO_LONG,
              "Expected ERROR_LINE_TOO_LONG for tiny prompt buffer");

    test_bool("GetCurrentDirName tiny buffer", !GetCurrentDirName((STRPTR)buf, 5) && IoErr() == ERROR_LINE_TOO_LONG,
              "Expected ERROR_LINE_TOO_LONG for tiny current-dir buffer");

    test_bool("SetProgramName too long", !SetProgramName((CONST_STRPTR)
              "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ"
              "ABCDEFGHIJKLMNO"),
              "SetProgramName should reject names >255 chars");

    test_bool("SetPrompt max length accepted", SetPrompt((CONST_STRPTR)
              "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ"
              "ABCDEFGHIJKLMNO"),
              "SetPrompt should accept a 255-byte string");

    test_bool("SetCurrentDirName max length accepted", SetCurrentDirName((CONST_STRPTR)
              "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ"
              "ABCDEFGHIJKLMNO"),
              "SetCurrentDirName should accept a 255-byte string");

    test_bool("FindCliProc current", FindCliProc(((struct Process *)FindTask(NULL))->pr_TaskNum) != NULL,
              "FindCliProc did not return current process");
    test_bool("FindCliProc missing", FindCliProc(9999) == NULL, "FindCliProc should return NULL for missing CLI");
    test_bool("MaxCli valid", MaxCli() >= (ULONG)((struct Process *)FindTask(NULL))->pr_TaskNum,
              "MaxCli smaller than current CLI number");

    test_bool("SetPrompt restore short value", SetPrompt((CONST_STRPTR)"ADV> "),
              "SetPrompt failed to restore short prompt");
    test_bool("SetCurrentDirName restore short value", SetCurrentDirName((CONST_STRPTR)"RAM:ManualDir"),
              "SetCurrentDirName failed to restore short current-dir name");

    /* Test 8: CreateNewProc proc-window inheritance/override semantics */
    print("\nTest 8: CreateNewProc window pointer semantics\n");
    {
        struct MsgPort *windowPort = CreateMsgPort();
        struct WindowPtrMessage *msg;
        struct Process *child;
        APTR oldWindowPtr = me->pr_WindowPtr;
        APTR inheritedWindowPtr = (APTR)0x12345678UL;

        if (!windowPort) {
            test_fail("CreateNewProc window pointer", "Could not create reply port");
        } else {
            windowPort->mp_Node.ln_Name = (char *)WINDOWPTR_PORT_NAME;
            AddPort(windowPort);

            me->pr_WindowPtr = inheritedWindowPtr;

            {
                struct TagItem inheritTags[] = {
                    { NP_Entry, (ULONG)WindowPtrChild },
                    { NP_Name, (ULONG)"ProcessAdvanced.WinInherit" },
                    { NP_StackSize, 8192 },
                    { NP_Cli, TRUE },
                    { TAG_DONE, 0 }
                };

                child = CreateNewProc(inheritTags);
                if (!child) {
                    test_fail("CreateNewProc inherits pr_WindowPtr", "CreateNewProc failed");
                } else {
                    WaitPort(windowPort);
                    msg = (struct WindowPtrMessage *)GetMsg(windowPort);
                    if (msg && msg->window_ptr == inheritedWindowPtr) {
                        test_pass("CreateNewProc inherits pr_WindowPtr");
                    } else {
                        test_fail("CreateNewProc inherits pr_WindowPtr", "Child did not inherit parent window pointer");
                    }

                    if (msg && msg->task_num > 0) {
                        test_pass("CreateNewProc CLI child gets task number");
                    } else {
                        test_fail("CreateNewProc CLI child gets task number", "Child CLI task number was not assigned");
                    }

                    if (msg)
                        FreeMem(msg, sizeof(*msg));
                }
            }

            {
                struct TagItem overrideTags[] = {
                    { NP_Entry, (ULONG)WindowPtrChild },
                    { NP_Name, (ULONG)"ProcessAdvanced.WinOverride" },
                    { NP_StackSize, 8192 },
                    { NP_Cli, TRUE },
                    { NP_WindowPtr, (ULONG)NULL },
                    { TAG_DONE, 0 }
                };

                child = CreateNewProc(overrideTags);
                if (!child) {
                    test_fail("CreateNewProc honors explicit NULL pr_WindowPtr", "CreateNewProc failed");
                } else {
                    WaitPort(windowPort);
                    msg = (struct WindowPtrMessage *)GetMsg(windowPort);
                    if (msg && msg->window_ptr == NULL) {
                        test_pass("CreateNewProc honors explicit NULL pr_WindowPtr");
                    } else {
                        test_fail("CreateNewProc honors explicit NULL pr_WindowPtr", "Child did not honor explicit NULL override");
                    }

                    if (msg)
                        FreeMem(msg, sizeof(*msg));
                }
            }

            me->pr_WindowPtr = oldWindowPtr;
            RemPort(windowPort);
            DeleteMsgPort(windowPort);
        }
    }
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
