/*
 * STATUS command - Display list of tasks and processes
 * Phase 7.2 implementation for lxa
 * 
 * Template: FULL/S,TCB/S,CLI=ALL/S,COM=COMMAND/K
 * 
 * Usage:
 *   STATUS           - List CLI processes
 *   STATUS FULL      - Show detailed information
 *   STATUS ALL       - Show all tasks including non-CLI
 *   STATUS TCB       - Show task control block addresses
 *   STATUS COMMAND name - Show processes running specified command
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
#define TEMPLATE "FULL/S,TCB/S,CLI=ALL/S,COM=COMMAND/K"

/* Argument array indices */
#define ARG_FULL    0
#define ARG_TCB     1
#define ARG_ALL     2
#define ARG_COMMAND 3
#define ARG_COUNT   4

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

/* Helper: output a number (right-aligned in field width) */
static void out_num(LONG num, int width)
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
    
    /* Pad to width */
    int len = (buf + sizeof(buf) - 1) - p;
    while (len < width) {
        out_str(" ");
        len++;
    }
    out_str(p);
}

/* Helper: output hex number */
static void out_hex(ULONG num, int width)
{
    char buf[12];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    
    static const char hex[] = "0123456789abcdef";
    
    do {
        *--p = hex[num & 0xf];
        num >>= 4;
    } while (num);
    
    /* Pad to width */
    int len = (buf + sizeof(buf) - 1) - p;
    while (len < width) {
        *--p = '0';
        len++;
    }
    out_str(p);
}

/* Get task state string */
static const char *task_state_str(UBYTE state)
{
    switch (state) {
        case TS_RUN:      return "Running";
        case TS_READY:    return "Ready";
        case TS_WAIT:     return "Waiting";
        case TS_EXCEPT:   return "Exception";
        case TS_REMOVED:  return "Removed";
        default:          return "Unknown";
    }
}

/* Get process CLI number from TaskArray if available */
static LONG get_cli_number(struct Task *task)
{
    /* Walk the RootNode's TaskArray to find this task */
    struct RootNode *root = DOSBase->dl_Root;
    if (!root) return 0;
    
    /* TaskArray is a BPTR to array of pointers:
     * [0] = max slots
     * [1..n] = pointer to MsgPort of CLI process (or 0 if slot free)
     */
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

/* Check if task name matches filter (case insensitive partial match) */
static BOOL match_command(const char *name, const char *filter)
{
    if (!filter || !filter[0]) return TRUE;
    if (!name) return FALSE;
    
    /* Case-insensitive substring search */
    const char *p = name;
    while (*p) {
        const char *s1 = p;
        const char *s2 = filter;
        while (*s1 && *s2) {
            char c1 = *s1;
            char c2 = *s2;
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2) break;
            s1++;
            s2++;
        }
        if (!*s2) return TRUE;  /* Match found */
        p++;
    }
    return FALSE;
}

/* Print information about a task */
static void print_task(struct Task *task, BOOL full, BOOL tcb, const char *filter)
{
    if (!task) return;
    
    const char *name = task->tc_Node.ln_Name ? task->tc_Node.ln_Name : "(unnamed)";
    
    /* Check command filter */
    if (!match_command(name, filter)) return;
    
    LONG cliNum = 0;
    if (task->tc_Node.ln_Type == NT_PROCESS) {
        cliNum = get_cli_number(task);
    }
    
    /* Print CLI number or * for non-CLI */
    if (cliNum > 0) {
        out_num(cliNum, 3);
    } else {
        out_str("  *");
    }
    out_str(". ");
    
    if (tcb) {
        out_str("0x");
        out_hex((ULONG)task, 8);
        out_str(" ");
    }
    
    if (full) {
        /* Priority */
        out_str("Pri ");
        out_num(task->tc_Node.ln_Pri, 3);
        out_str(" ");
        
        /* State */
        out_str(task_state_str(task->tc_State));
        out_str(" ");
        
        /* Stack usage (approximate) */
        if (task->tc_SPLower && task->tc_SPUpper) {
            ULONG stackSize = (ULONG)task->tc_SPUpper - (ULONG)task->tc_SPLower;
            out_str("Stack ");
            out_num(stackSize, 5);
            out_str(" ");
        }
    }
    
    /* Task name */
    out_str(name);
    out_nl();
}

/* Walk a task list and print each task */
static void walk_task_list(struct List *list, BOOL full, BOOL tcb, BOOL showAll, const char *filter)
{
    struct Task *task;
    
    for (task = (struct Task *)list->lh_Head;
         task->tc_Node.ln_Succ != NULL;
         task = (struct Task *)task->tc_Node.ln_Succ)
    {
        /* Skip non-process tasks unless ALL is specified */
        if (!showAll && task->tc_Node.ln_Type != NT_PROCESS) {
            continue;
        }
        
        print_task(task, full, tcb, filter);
    }
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0, 0, 0, 0};
    int rc = RETURN_OK;
    
    /* Parse arguments using AmigaDOS template */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"STATUS");
        return RETURN_ERROR;
    }
    
    BOOL full = (BOOL)args[ARG_FULL];
    BOOL tcb = (BOOL)args[ARG_TCB];
    BOOL showAll = (BOOL)args[ARG_ALL];
    const char *filter = (const char *)args[ARG_COMMAND];
    
    /* Print header */
    out_str("Process List:\n");
    if (full) {
        out_str("CLI ");
        if (tcb) out_str("   TCB Addr  ");
        out_str("Pri      State        Stack Name\n");
        out_str("--- ");
        if (tcb) out_str("------------ ");
        out_str("--- ---------- ----- ----\n");
    }
    
    /* Disable task switching while we enumerate */
    Forbid();
    
    /* Print current task first */
    struct Task *thisTask = SysBase->ThisTask;
    if (thisTask) {
        if (showAll || thisTask->tc_Node.ln_Type == NT_PROCESS) {
            print_task(thisTask, full, tcb, filter);
        }
    }
    
    /* Walk TaskReady list */
    walk_task_list(&SysBase->TaskReady, full, tcb, showAll, filter);
    
    /* Walk TaskWait list */
    walk_task_list(&SysBase->TaskWait, full, tcb, showAll, filter);
    
    Permit();
    
    FreeArgs(rdargs);
    return rc;
}
