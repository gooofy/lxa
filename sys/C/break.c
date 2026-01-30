/*
 * BREAK command - Send break signal to a process
 * Phase 7.2 implementation for lxa
 * 
 * Template: PROCESS/A/N,ALL/S,C/S,D/S,E/S,F/S
 * 
 * Usage:
 *   BREAK 2       - Send Ctrl-C to CLI 2
 *   BREAK 2 D     - Send Ctrl-D to CLI 2  
 *   BREAK 2 ALL   - Send all break signals to CLI 2
 *   BREAK 2 C D E - Send Ctrl-C, Ctrl-D, Ctrl-E to CLI 2
 *
 * Break signals:
 *   C - SIGBREAKF_CTRL_C (bit 12) - User break, abort program
 *   D - SIGBREAKF_CTRL_D (bit 13) - User break, abort subtask
 *   E - SIGBREAKF_CTRL_E (bit 14) - User request (often "examine")
 *   F - SIGBREAKF_CTRL_F (bit 15) - User request (often "freeze")
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

#define VERSION "1.0"

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template - AmigaOS compatible */
#define TEMPLATE "PROCESS/A/N,ALL/S,C/S,D/S,E/S,F/S"

/* Argument array indices */
#define ARG_PROCESS 0
#define ARG_ALL     1
#define ARG_C       2
#define ARG_D       3
#define ARG_E       4
#define ARG_F       5
#define ARG_COUNT   6

/* Helper: output a string */
static void out_str(const char *str)
{
    Write(Output(), (STRPTR)str, strlen(str));
}

/* Helper: output newline */
static void out_nl(void)
{
    Write(Output(), (STRPTR)"\n", 1);
}

/* Helper: output a number */
static void out_num(LONG num)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    int neg = (num < 0);
    unsigned long n = neg ? (unsigned long)(-num) : (unsigned long)num;
    
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n);
    
    if (neg) *--p = '-';
    out_str(p);
}

/* Find task by CLI number */
static struct Task *find_cli_task(LONG cliNum)
{
    struct RootNode *root = DOSBase->dl_Root;
    if (!root) return NULL;
    
    /* TaskArray is a BPTR to array of pointers:
     * [0] = max slots
     * [1..n] = pointer to MsgPort of CLI process (or 0 if slot free)
     */
    ULONG *taskArray = (ULONG *)BADDR(root->rn_TaskArray);
    if (!taskArray) return NULL;
    
    ULONG maxCli = taskArray[0];
    
    if (cliNum < 1 || (ULONG)cliNum > maxCli) {
        return NULL;
    }
    
    /* TaskArray stores pointers to MsgPort (pr_MsgPort) */
    struct MsgPort *port = (struct MsgPort *)taskArray[cliNum];
    if (!port) return NULL;
    
    /* mp_SigTask points to the Task */
    return (struct Task *)port->mp_SigTask;
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0, 0, 0, 0, 0, 0};
    int rc = RETURN_OK;
    
    /* Parse arguments using AmigaDOS template */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"BREAK");
        return RETURN_ERROR;
    }
    
    /* lxa ReadArgs stores /N values directly, not as pointers */
    LONG cliNum = args[ARG_PROCESS];
    BOOL breakAll = (BOOL)args[ARG_ALL];
    BOOL breakC = (BOOL)args[ARG_C];
    BOOL breakD = (BOOL)args[ARG_D];
    BOOL breakE = (BOOL)args[ARG_E];
    BOOL breakF = (BOOL)args[ARG_F];
    
    /* Default to Ctrl-C if no specific signal requested */
    if (!breakAll && !breakC && !breakD && !breakE && !breakF) {
        breakC = TRUE;
    }
    
    /* Build signal mask */
    ULONG signals = 0;
    if (breakAll) {
        signals = SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | 
                  SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F;
    } else {
        if (breakC) signals |= SIGBREAKF_CTRL_C;
        if (breakD) signals |= SIGBREAKF_CTRL_D;
        if (breakE) signals |= SIGBREAKF_CTRL_E;
        if (breakF) signals |= SIGBREAKF_CTRL_F;
    }
    
    /* Find the target task */
    Forbid();
    
    struct Task *task = find_cli_task(cliNum);
    
    if (!task) {
        Permit();
        out_str("BREAK: Process ");
        out_num(cliNum);
        out_str(" not found\n");
        FreeArgs(rdargs);
        return RETURN_WARN;
    }
    
    /* Send the signal */
    Signal(task, signals);
    
    Permit();
    
    /* Report what we did */
    out_str("BREAK: Signal sent to CLI ");
    out_num(cliNum);
    if (breakAll) {
        out_str(" (ALL)");
    } else {
        if (breakC) out_str(" C");
        if (breakD) out_str(" D");
        if (breakE) out_str(" E");
        if (breakF) out_str(" F");
    }
    out_nl();
    
    FreeArgs(rdargs);
    return rc;
}
