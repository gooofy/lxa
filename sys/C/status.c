/*
 * STATUS - list the CLI processes
 *
 * Template: PROCESS/N,FULL/S,TCB/S,CLI=ALL/S,COM=COMMAND/K
 *
 * Output of AmigaOS 3.1 (verified on the reference, Phase 221):
 *   Process  1: Loaded as command: List
 *   Process  1: stk 4000, gv 150, pri   0 Loaded as command: List   (FULL)
 *   Process  2: No command loaded
 *   COMMAND=<name>: the process numbers running it (" %2ld"), RC 5 if none
 *   Process <n> does not exist                               (RC 20)
 */

#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>
#include <stddef.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "PROCESS/N,FULL/S,TCB/S,CLI=ALL/S,COM=COMMAND/K"

enum { A_PROCESS, A_FULL, A_TCB, A_ALL, A_COMMAND, A_COUNT };

static LONG args[A_COUNT];

static struct Process *process_of(ULONG *array, ULONG n)
{
    struct MsgPort *port = (struct MsgPort *)array[n];
    if (!port)
        return NULL;
    return (struct Process *)((UBYTE *)port - offsetof(struct Process, pr_MsgPort));
}

static void command_name(struct Process *pr, char *buf, int len)
{
    struct CommandLineInterface *cli = BADDR(pr->pr_CLI);
    UBYTE *b = cli ? BADDR(cli->cli_CommandName) : NULL;
    int n = 0;

    if (b && b[0]) {
        n = b[0];
        if (n > len - 1)
            n = len - 1;
        CopyMem(b + 1, buf, n);
    }
    buf[n] = '\0';
}

static void show(ULONG num, struct Process *pr)
{
    char cmd[256];

    command_name(pr, cmd, sizeof(cmd));
    Printf((STRPTR)"Process %2ld: ", (LONG)num);
    if (args[A_FULL] || args[A_TCB]) {
        struct CommandLineInterface *cli = BADDR(pr->pr_CLI);
        Printf((STRPTR)"stk %ld, gv %ld, pri %3ld ",
               cli ? cli->cli_DefaultStack * 4 : (LONG)pr->pr_StackSize,
               150L, (LONG)pr->pr_Task.tc_Node.ln_Pri);
    }
    if (cmd[0])
        Printf((STRPTR)"Loaded as command: %s\n", (LONG)cmd);
    else
        PutStr((STRPTR)"No command loaded\n");
}

int main(void)
{
    struct RDArgs *rda;
    struct RootNode *root = DOSBase->dl_Root;
    ULONG *array, max, i;
    LONG rc = 0;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    array = root ? (ULONG *)BADDR(root->rn_TaskArray) : NULL;
    max = array ? array[0] : 0;

    Forbid();
    if (args[A_COMMAND]) {
        BOOL found = FALSE;
        for (i = 1; i <= max; i++) {
            struct Process *pr = process_of(array, i);
            char cmd[256];
            if (!pr)
                continue;
            command_name(pr, cmd, sizeof(cmd));
            if (cmd[0] && !stricmp((char *)FilePart((STRPTR)cmd), (char *)args[A_COMMAND])) {
                Printf((STRPTR)" %ld\n", (LONG)i);
                found = TRUE;
            }
        }
        if (!found)
            rc = RETURN_WARN;
    } else if (args[A_PROCESS]) {
        ULONG n = *(LONG *)args[A_PROCESS];
        struct Process *pr = (n >= 1 && n <= max) ? process_of(array, n) : NULL;
        if (pr) {
            show(n, pr);
        } else {
            Printf((STRPTR)"Process %ld does not exist\n", (LONG)n);
            SetIoErr(0);
            rc = RETURN_FAIL;
        }
    } else {
        for (i = 1; i <= max; i++) {
            struct Process *pr = process_of(array, i);
            if (pr)
                show(i, pr);
        }
    }
    Permit();
    FreeArgs(rda);
    return rc;
}
