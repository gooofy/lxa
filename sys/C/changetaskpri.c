/*
 * CHANGETASKPRI command - Change task priority
 * Phase 7.2 implementation for lxa
 * 
 * Template: PRI=PRIORITY/A/N,PROCESS/K/N
 * 
 * Usage:
 *   CHANGETASKPRI 5          - Set current process priority to 5
 *   CHANGETASKPRI -2 PROCESS 3 - Set CLI 3's priority to -2
 *
 * Priority range is -128 to 127. Higher values = higher priority.
 * Default system priority is 0.
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
#define TEMPLATE "PRI=PRIORITY/A/N,PROCESS/K/N"

/* Argument array indices */
#define ARG_PRI     0
#define ARG_PROCESS 1
#define ARG_COUNT   2

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

/* Get CLI number for a task */
static LONG get_cli_number(struct Task *task)
{
    struct RootNode *root = DOSBase->dl_Root;
    if (!root) return 0;
    
    ULONG *taskArray = (ULONG *)BADDR(root->rn_TaskArray);
    if (!taskArray) return 0;
    
    ULONG maxCli = taskArray[0];
    
    for (ULONG i = 1; i <= maxCli; i++) {
        struct MsgPort *port = (struct MsgPort *)taskArray[i];
        if (port && port->mp_SigTask == task) {
            return (LONG)i;
        }
    }
    
    return 0;
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0, 0};
    int rc = RETURN_OK;
    
    /* Parse arguments using AmigaDOS template */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"CHANGETASKPRI");
        return RETURN_ERROR;
    }
    
    /* 
     * Note: The lxa ReadArgs implementation stores numeric values directly
     * in the array, not as pointers like real AmigaDOS does.
     */
    if (!args[ARG_PRI]) {
        out_str("CHANGETASKPRI: Priority required\n");
        FreeArgs(rdargs);
        return RETURN_ERROR;
    }
    
    LONG newPri = args[ARG_PRI];  /* Value is stored directly, not as pointer */
    
    /* Validate priority range */
    if (newPri < -128 || newPri > 127) {
        out_str("CHANGETASKPRI: Priority must be between -128 and 127\n");
        FreeArgs(rdargs);
        return RETURN_ERROR;
    }
    
    struct Task *task;
    LONG cliNum;
    
    if (args[ARG_PROCESS]) {
        /* Find specified CLI process - value stored directly, not as pointer */
        cliNum = args[ARG_PROCESS];
        
        Forbid();
        task = find_cli_task(cliNum);
        
        if (!task) {
            Permit();
            out_str("CHANGETASKPRI: Process ");
            out_num(cliNum);
            out_str(" not found\n");
            FreeArgs(rdargs);
            return RETURN_WARN;
        }
    } else {
        /* Use current task */
        task = FindTask(NULL);
        cliNum = get_cli_number(task);
        Forbid();
    }
    
    /* Change the priority */
    BYTE oldPri = SetTaskPri(task, newPri);
    
    Permit();
    
    /* Report the change */
    out_str("Process ");
    if (cliNum > 0) {
        out_num(cliNum);
    } else {
        out_str("(");
        out_str(task->tc_Node.ln_Name ? task->tc_Node.ln_Name : "unnamed");
        out_str(")");
    }
    out_str(" changed from priority ");
    out_num(oldPri);
    out_str(" to ");
    out_num(newPri);
    out_nl();
    
    FreeArgs(rdargs);
    return rc;
}
