/*
 * RUN command - Run a program in the background
 * Phase 7.2 implementation for lxa
 * 
 * Template: COMMAND/F
 * 
 * Usage:
 *   RUN command args   - Run command in background
 *   RUN >NIL: command  - Run silently (redirect output)
 *
 * The RUN command starts a new CLI process to execute the specified
 * command. Unlike direct execution, the parent does not wait for the
 * child to finish.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dostags.h>
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
#define TEMPLATE "COMMAND/F"

/* Argument array indices */
#define ARG_COMMAND 0
#define ARG_COUNT   1

/* Helper: output a string */
static void out_str(const char *str)
{
    FWrite(Output(), (STRPTR)str, 1, strlen(str));
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

/*
 * Run a command line without waiting: as on AmigaOS 3.1 a new background
 * CLI process is started for it (an asynchronous System()), and that CLI
 * loads and runs the command - so the command is loaded in the new
 * process, not in the shell that ran RUN (Phase 237d: Fish Wicon deletes
 * its port while its copy is still being loaded).
 */
static LONG run_background(const char *command)
{
    BPTR nilIn = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR nilOut = Open((STRPTR)"NIL:", MODE_NEWFILE);
    struct TagItem tags[5];
    LONG rc;

    while (*command == ' ')
        command++;
    tags[0].ti_Tag = SYS_Input;    tags[0].ti_Data = (ULONG)nilIn;
    tags[1].ti_Tag = SYS_Output;   tags[1].ti_Data = (ULONG)nilOut;
    tags[2].ti_Tag = SYS_Asynch;   tags[2].ti_Data = TRUE;
    tags[3].ti_Tag = NP_StackSize; tags[3].ti_Data = 8192;
    tags[4].ti_Tag = TAG_DONE;     tags[4].ti_Data = 0;
    rc = SystemTagList((STRPTR)command, tags);
    if (rc == -1) {
        out_str("RUN: Failed to create process\n");
        if (nilIn)
            Close(nilIn);
        if (nilOut)
            Close(nilOut);
        return RETURN_ERROR;
    }
    /* Report the CLI number of the new process */
    out_str("[CLI ");
    out_num(MaxCli());
    out_str("]\n");
    return RETURN_OK;
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    int rc = RETURN_OK;
    
    /* Parse arguments using AmigaDOS template */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"RUN");
        return RETURN_ERROR;
    }
    
    if (!args[ARG_COMMAND]) {
        out_str("RUN: No command specified\n");
        out_str("Usage: RUN <command> [arguments]\n");
        rc = RETURN_ERROR;
    } else {
        rc = run_background((const char *)args[ARG_COMMAND]);
    }
    
    FreeArgs(rdargs);
    return rc;
}
