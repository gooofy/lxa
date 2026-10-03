/*
 * NEWSHELL / NEWCLI - start a new shell in its own console window
 *
 * Template: WINDOW,FROM
 *
 *   WINDOW  console specification (default CON:0/50//130/AmigaShell/CLOSE)
 *   FROM    script the new shell runs first (default S:Shell-Startup)
 *
 * The new shell runs asynchronously; NewShell returns at once.  Programs
 * such as vim re-launch themselves this way ("newcli <nil: >nil:
 * con:0/0/640/200/ from t:script") when their own console is not
 * interactive.
 */

#include <exec/types.h>
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

#define TEMPLATE "WINDOW,FROM"
#define DEFAULT_WINDOW "CON:0/50//130/AmigaShell/CLOSE"
#define DEFAULT_FROM "S:Shell-Startup"

static const char *shell_path(void)
{
    static const char *const candidates[] = { "SYS:System/Shell", "System:Shell", NULL };
    int i;
    for (i = 0; candidates[i]; i++) {
        BPTR lock = Lock((STRPTR)candidates[i], SHARED_LOCK);
        if (lock) {
            UnLock(lock);
            return candidates[i];
        }
    }
    return NULL;
}

int main(void)
{
    LONG args[2] = { 0, 0 };
    struct RDArgs *rda;
    const char *window, *from, *path;
    char shell_args[300];
    struct CommandLineInterface *cli = Cli();
    BPTR con, seg, dir = 0, lock;
    struct Process *proc;
    LONG stack = cli ? cli->cli_DefaultStack * 4 : 4096;

    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), (STRPTR)"NewShell");
        return RETURN_FAIL;
    }
    window = args[0] ? (const char *)args[0] : DEFAULT_WINDOW;
    from = args[1] ? (const char *)args[1] : DEFAULT_FROM;

    shell_args[0] = '\0';
    lock = Lock((STRPTR)from, SHARED_LOCK);
    if (lock) {
        UnLock(lock);
        if (strlen(from) < sizeof(shell_args) - 8) {
            strcpy(shell_args, "FROM \"");
            strcat(shell_args, from);
            strcat(shell_args, "\"\n");
        }
    }

    path = shell_path();
    seg = path ? LoadSeg((STRPTR)path) : 0;
    if (!seg) {
        PrintFault(ERROR_OBJECT_NOT_FOUND, (STRPTR)"NewShell");
        FreeArgs(rda);
        return RETURN_FAIL;
    }
    con = Open((STRPTR)window, MODE_NEWFILE);
    if (!con) {
        PrintFault(IoErr(), (STRPTR)"NewShell");
        UnLoadSeg(seg);
        FreeArgs(rda);
        return RETURN_FAIL;
    }
    if (((struct Process *)FindTask(NULL))->pr_CurrentDir)
        dir = DupLock(((struct Process *)FindTask(NULL))->pr_CurrentDir);

    proc = CreateNewProcTags(NP_Seglist, seg,
                             NP_FreeSeglist, TRUE,
                             NP_Name, (ULONG)"Shell Process",
                             NP_Cli, TRUE,
                             NP_Input, con,
                             NP_Output, con,
                             NP_CloseInput, TRUE,
                             NP_CloseOutput, FALSE,
                             NP_CurrentDir, dir,
                             NP_StackSize, stack < 16384 ? 16384 : stack,
                             NP_Arguments, (ULONG)shell_args,
                             TAG_END);
    if (!proc) {
        PrintFault(IoErr(), (STRPTR)"NewShell");
        Close(con);
        if (dir)
            UnLock(dir);
        UnLoadSeg(seg);
        FreeArgs(rda);
        return RETURN_FAIL;
    }
    FreeArgs(rda);
    return RETURN_OK;
}
