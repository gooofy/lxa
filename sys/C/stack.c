/*
 * STACK command - Display or set stack size
 * Phase 7.2 implementation for lxa
 * 
 * Template: SIZE/N
 * 
 * Usage:
 *   STACK        - Display current stack size
 *   STACK 8000   - Set default stack to 8000 bytes
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
#define TEMPLATE "SIZE/N"

/* Argument array indices */
#define ARG_SIZE    0
#define ARG_COUNT   1

/* Helper: output a string */
static void out_str(const char *str)
{
    Write(Output(), (STRPTR)str, strlen(str));
}

/* Helper: output a number */
static void out_num(ULONG num)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    
    do {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    
    out_str(p);
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    int rc = RETURN_OK;
    
    /* Parse arguments using AmigaDOS template */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"STACK");
        return RETURN_ERROR;
    }
    
    /* Get current task */
    struct Task *task = FindTask(NULL);
    
    /* Calculate current stack size */
    ULONG currentStack = 0;
    if (task->tc_SPLower && task->tc_SPUpper) {
        currentStack = (ULONG)task->tc_SPUpper - (ULONG)task->tc_SPLower;
    }
    
    if (args[ARG_SIZE]) {
        /* Set new stack size - lxa ReadArgs stores /N values directly */
        LONG newSize = args[ARG_SIZE];
        
        if (newSize < 1000) {
            out_str("STACK: Minimum stack size is 1000 bytes\n");
            rc = RETURN_WARN;
        } else if (newSize > 1000000) {
            out_str("STACK: Maximum stack size is 1000000 bytes\n");
            rc = RETURN_WARN;
        } else {
            /* 
             * Note: On real AmigaOS, STACK sets the default stack for
             * subsequently launched processes via the CLI. We store this
             * in the CLI structure's cli_DefaultStack field.
             * For lxa, we just print a message since processes inherit
             * stack size from their creator.
             */
            struct Process *proc = (struct Process *)task;
            if (proc->pr_Task.tc_Node.ln_Type == NT_PROCESS && proc->pr_CLI) {
                struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(proc->pr_CLI);
                /* DefaultStack is in longwords */
                cli->cli_DefaultStack = newSize >> 2;
            }
            
            out_str("Stack set to ");
            out_num(newSize);
            out_str(" bytes\n");
        }
    } else {
        /* Display current stack */
        out_str("Stack size: ");
        out_num(currentStack);
        out_str(" bytes\n");
        
        /* Show usage if available */
        if (task->tc_SPLower && task->tc_SPReg) {
            ULONG used = (ULONG)task->tc_SPUpper - (ULONG)task->tc_SPReg;
            out_str("Stack used: ");
            out_num(used);
            out_str(" bytes\n");
            out_str("Stack free: ");
            out_num(currentStack - used);
            out_str(" bytes\n");
        }
    }
    
    FreeArgs(rdargs);
    return rc;
}
