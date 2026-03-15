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

#define FNF_VALIDFLAGS  ((LONG)0x80000000UL)
#define FNF_USERINPUT   (1 << 1)
#define FNF_RUNOUTPUT   (1 << 0)

static int tests_failed = 0;
static BPTR report_output = 0;

static void print(const char *s)
{
    BPTR out = report_output ? report_output : Output();
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
    struct CommandLineInterface *seed_cli;
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

    print("CliInitRun Test\n");
    print("===============\n\n");

    if (!me || me->pr_Task.tc_Node.ln_Type != NT_PROCESS)
    {
        print("FAIL: Current task is not a process\n");
        return 20;
    }

    report_output = Output();

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
    result = CliInitRun(NULL);
    if (result == 0 && IoErr() == ERROR_REQUIRED_ARG_MISSING)
        test_pass("NULL packet rejected");
    else
        test_fail("NULL packet rejected", "Wrong result or IoErr");

    print("\nTest 2: Missing input reports process pointer error\n");
    dp.dp_Arg1 = 0;
    dp.dp_Arg2 = 0;
    dp.dp_Arg3 = std_output;
    dp.dp_Arg4 = 0;
    dp.dp_Arg5 = new_dir;
    dp.dp_Arg6 = 0;
    SetIoErr(0);
    result = CliInitRun(&dp);
    if (result == 0 && IoErr() == (LONG)me)
        test_pass("Missing input rejected with process pointer");
    else
        test_fail("Missing input rejected with process pointer", "Wrong result or IoErr");

    print("\nTest 3: Run packet without parent CLI installs default prompt and lock name\n");
    if (!old_cli && me->pr_CLI)
    {
        seed_cli = (struct CommandLineInterface *)BADDR(me->pr_CLI);
        FreeDosObject(DOS_CLI, seed_cli);
        me->pr_CLI = 0;
    }
    else
    {
        seed_cli = NULL;
    }

    dp.dp_Arg1 = 0;
    dp.dp_Arg2 = std_input;
    dp.dp_Arg3 = std_output;
    dp.dp_Arg4 = 0;
    dp.dp_Arg5 = new_dir;
    dp.dp_Arg6 = 0;
    SetIoErr(0);
    result = CliInitRun(&dp);
    cli = Cli();
    if (result == FNF_VALIDFLAGS && IoErr() == 0 && cli != NULL)
        test_pass("Default Run init succeeded");
    else
        test_fail("Default Run init succeeded", "CliInitRun failed");

    if (cli && cli->cli_StandardInput == std_input && cli->cli_CurrentInput == std_input &&
        cli->cli_StandardOutput == std_output && cli->cli_CurrentOutput == std_output)
        test_pass("Default Run stream fields initialized");
    else
        test_fail("Default Run stream fields initialized", "Wrong CLI stream fields");

    if (cli && GetPrompt((STRPTR)prompt, sizeof(prompt)) && prompt[0] == '%' && prompt[1] == 'N' &&
        prompt[2] == '>' && prompt[3] == ' ' && prompt[4] == '\0')
        test_pass("Default Run prompt installed");
    else
        test_fail("Default Run prompt installed", "Prompt was not reset to %N> ");

    if (GetCurrentDirName((STRPTR)dir_name, sizeof(dir_name)) && dir_name[0] == 'R' && dir_name[1] == 'A' &&
        dir_name[2] == 'M' && dir_name[3] == ':' && dir_name[4] == '\0')
        test_pass("Current directory name synced from lock");
    else
        test_fail("Current directory name synced from lock", "CLI set name mismatch");

    if (cli && cli->cli_Background == DOSTRUE && cli->cli_FailLevel == RETURN_ERROR &&
        cli->cli_DefaultStack >= 1024 && cli->cli_CommandName && ((UBYTE *)BADDR(cli->cli_CommandName))[0] == 0 &&
        cli->cli_CommandFile && ((UBYTE *)BADDR(cli->cli_CommandFile))[0] == 0)
        test_pass("Default Run bookkeeping initialized");
    else
        test_fail("Default Run bookkeeping initialized", "Background/fail-level/stack/command fields wrong");

    print("\nTest 4: Parent CLI prompt and set name are inherited, output can be synthesized\n");
    seed_cli = Cli();
    if (seed_cli)
    {
        SetPrompt((CONST_STRPTR)"OLD> ");
        SetCurrentDirName((CONST_STRPTR)"RAM:Inherited");
    }

    dp.dp_Arg1 = seed_cli ? MKBADDR(seed_cli) : 0;
    dp.dp_Arg2 = std_input;
    dp.dp_Arg3 = 0;
    dp.dp_Arg4 = std_input;
    dp.dp_Arg5 = new_dir;
    dp.dp_Arg6 = 1;
    SetIoErr(0);
    result = CliInitRun(&dp);
    cli = Cli();
    if (result == (FNF_VALIDFLAGS | FNF_USERINPUT | FNF_RUNOUTPUT) && IoErr() == 0 && cli != NULL &&
        cli->cli_StandardOutput != 0)
        test_pass("Run flags and synthesized output returned");
    else
        test_fail("Run flags and synthesized output returned", "Wrong flags or missing synthesized output");

    if (cli && GetPrompt((STRPTR)prompt, sizeof(prompt)) &&
        prompt[0] == 'O' && prompt[1] == 'L' && prompt[2] == 'D' && prompt[3] == '>' && prompt[4] == ' ' && prompt[5] == '\0')
        test_pass("Parent prompt preserved");
    else
        test_fail("Parent prompt preserved", "Prompt was not inherited from old CLI");

    if (GetCurrentDirName((STRPTR)dir_name, sizeof(dir_name)) &&
        dir_name[0] == 'R' && dir_name[1] == 'A' && dir_name[2] == 'M' && dir_name[3] == ':')
        test_pass("Run shell keeps a CLI current-dir name");
    else
        test_fail("Run shell keeps a CLI current-dir name", "CLI set name was not populated");

    if (cli && cli->cli_Background == DOSTRUE && cli->cli_Interactive == DOSFALSE &&
        me->pr_CIS == std_input && me->pr_COS == cli->cli_StandardOutput && me->pr_CES == cli->cli_StandardOutput)
        test_pass("Process streams synchronized for Run shell");
    else
        test_fail("Process streams synchronized for Run shell", "Process stream fields wrong");

    CurrentDir(restore_dir);
    if (new_dir)
        UnLock(new_dir);
    if (cli && cli->cli_StandardOutput && cli->cli_StandardOutput != std_output)
        Close(cli->cli_StandardOutput);
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
    else if (old_cli)
    {
        me->pr_CLI = MKBADDR(old_cli);
    }

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
