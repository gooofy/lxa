#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
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

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct DosPacket dp = { 0 };
    struct CommandLineInterface *old_cli;
    struct CommandLineInterface *cli;
    BPTR old_current_dir;
    BPTR restore_dir;
    BPTR new_dir;
    BPTR std_input;
    BPTR std_output;
    BPTR old_cis;
    BPTR old_cos;
    BPTR old_ces;
    APTR old_console_task;
    LONG result;
    char prompt[32];
    char dir_name[256];

    print("CliInitNewcli Test\n");
    print("==================\n\n");

    if (!me || me->pr_Task.tc_Node.ln_Type != NT_PROCESS)
    {
        print("FAIL: Current task is not a process\n");
        return 20;
    }

    old_cli = me->pr_CLI ? (struct CommandLineInterface *)BADDR(me->pr_CLI) : NULL;
    old_current_dir = me->pr_CurrentDir;
    old_cis = me->pr_CIS;
    old_cos = me->pr_COS;
    old_ces = me->pr_CES;
    old_console_task = me->pr_ConsoleTask;

    restore_dir = old_current_dir ? DupLock(old_current_dir) : 0;
    new_dir = Lock((CONST_STRPTR)"RAM:", SHARED_LOCK);
    std_input = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
    std_output = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);

    if (!new_dir || !std_input || !std_output)
    {
        test_fail("Setup", "Could not open test lock/handles");
        if (std_output)
            Close(std_output);
        if (std_input)
            Close(std_input);
        if (new_dir)
            UnLock(new_dir);
        if (restore_dir)
            UnLock(restore_dir);
        return 20;
    }

    print("Test 1: NULL packet fails with required-argument error\n");
    SetIoErr(0);
    result = CliInitNewcli(NULL);
    if (result == 0 && IoErr() == ERROR_REQUIRED_ARG_MISSING)
        test_pass("NULL packet rejected");
    else
        test_fail("NULL packet rejected", "Wrong result or IoErr");

    print("\nTest 2: Startup packet initializes CLI-visible state\n");
    dp.dp_Res1 = 1;
    dp.dp_Arg1 = new_dir;
    dp.dp_Arg2 = std_input;
    dp.dp_Arg3 = std_output;
    dp.dp_Arg4 = std_input;

    result = CliInitNewcli(&dp);
    cli = Cli();
    if (result == 0 && IoErr() == 0 && cli != NULL)
        test_pass("CLI allocated and call succeeded");
    else
        test_fail("CLI allocated and call succeeded", "CliInitNewcli failed");

    if (cli && cli->cli_StandardInput == std_input && cli->cli_CurrentInput == std_input &&
        cli->cli_StandardOutput == std_output && cli->cli_CurrentOutput == std_output)
        test_pass("CLI stream fields initialized");
    else
        test_fail("CLI stream fields initialized", "Wrong CLI stream fields");

    if (cli && GetPrompt((STRPTR)prompt, sizeof(prompt)) && prompt[0] == '%' && prompt[1] == 'N' &&
        prompt[2] == '>' && prompt[3] == ' ' && prompt[4] == '\0')
        test_pass("Default prompt installed");
    else
        test_fail("Default prompt installed", "Prompt was not reset to %N> ");

    if (GetCurrentDirName((STRPTR)dir_name, sizeof(dir_name)) && dir_name[0] == 'R' && dir_name[1] == 'A' &&
        dir_name[2] == 'M' && dir_name[3] == ':' && dir_name[4] == '\0')
        test_pass("Current directory propagated from packet lock");
    else
        test_fail("Current directory propagated from packet lock", "CLI set name mismatch");

    if (cli && cli->cli_Background == DOSFALSE && cli->cli_FailLevel == RETURN_ERROR &&
        cli->cli_CommandName && ((UBYTE *)BADDR(cli->cli_CommandName))[0] == 0 &&
        cli->cli_CommandFile && ((UBYTE *)BADDR(cli->cli_CommandFile))[0] == 0)
        test_pass("CLI bookkeeping reset");
    else
        test_fail("CLI bookkeeping reset", "Background/fail-level/command fields wrong");

    if (me->pr_CIS == std_input && me->pr_COS == std_output && me->pr_CES == std_output)
        test_pass("Process stream fields synchronized");
    else
        test_fail("Process stream fields synchronized", "Process stream fields wrong");

    if (cli && cli->cli_Interactive == DOSFALSE)
        test_pass("Non-interactive input keeps background shell foreground state");
    else
        test_fail("Non-interactive input keeps background shell foreground state", "cli_Interactive wrong");

    CurrentDir(restore_dir);
    if (new_dir)
        UnLock(new_dir);
    Close(std_output);
    Close(std_input);

    me->pr_CIS = old_cis;
    me->pr_COS = old_cos;
    me->pr_CES = old_ces;
    me->pr_ConsoleTask = old_console_task;

    if (!old_cli && me->pr_CLI)
    {
        FreeDosObject(DOS_CLI, (APTR)BADDR(me->pr_CLI));
        me->pr_CLI = 0;
    }

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
