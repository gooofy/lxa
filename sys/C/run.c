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
    Write(Output(), (STRPTR)str, strlen(str));
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

/* Load and run a command without waiting */
static LONG run_background(const char *command)
{
    char bin_name[256];
    char *args = NULL;
    int i = 0;
    
    /* Skip leading spaces */
    while (*command == ' ') command++;
    
    /* Parse command name */
    while (*command && *command != ' ' && *command != '\n' && i < 255) {
        bin_name[i++] = *command++;
    }
    bin_name[i] = '\0';
    
    /* Rest is arguments */
    if (*command) args = (char *)command;
    
    /* If no args, provide at least a newline (Amiga startup convention) */
    if (!args || !*args) {
        args = "\n";
    }
    
    /* Try to load the program */
    BPTR seglist = LoadSeg((STRPTR)bin_name);
    if (!seglist) {
        /* Try C: prefix if no path component */
        int has_colon = 0;
        int has_slash = 0;
        for (int j = 0; bin_name[j]; j++) {
            if (bin_name[j] == ':') has_colon = 1;
            if (bin_name[j] == '/') has_slash = 1;
        }
        
        if (!has_colon && !has_slash) {
            char tmp[280];
            char *s = "SYS:C/";
            char *d = tmp;
            while (*s) *d++ = *s++;
            char *n = bin_name;
            while (*n) *d++ = *n++;
            *d = '\0';
            
            seglist = LoadSeg((STRPTR)tmp);
        }
    }
    
    if (!seglist) {
        out_str("RUN: Can't find '");
        out_str(bin_name);
        out_str("'\n");
        return RETURN_ERROR;
    }
    
    /* 
     * Open NIL: for input/output - background process shouldn't
     * interfere with the shell's I/O.
     * On real AmigaOS, you'd typically use NEWCLI for a new console.
     */
    BPTR nilIn = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR nilOut = Open((STRPTR)"NIL:", MODE_NEWFILE);
    
    if (!nilIn || !nilOut) {
        /* Fall back to using parent's I/O if NIL: isn't available */
        struct Process *me = (struct Process *)FindTask(NULL);
        if (!nilIn) nilIn = me->pr_CIS;
        if (!nilOut) nilOut = me->pr_COS;
    }
    
    /* Create the background process */
    struct TagItem procTags[] = {
        { NP_Seglist, (ULONG)seglist },
        { NP_Name, (ULONG)bin_name },
        { NP_StackSize, 8192 },          /* Generous stack for background */
        { NP_Cli, TRUE },
        { NP_Input, (ULONG)nilIn },
        { NP_Output, (ULONG)nilOut },
        { NP_Arguments, (ULONG)args },
        { NP_FreeSeglist, TRUE },        /* Free seglist on exit */
        { TAG_DONE, 0 }
    };
    
    struct Process *proc = CreateNewProc(procTags);
    if (!proc) {
        out_str("RUN: Failed to create process\n");
        UnLoadSeg(seglist);
        if (nilIn) Close(nilIn);
        if (nilOut) Close(nilOut);
        return RETURN_ERROR;
    }
    
    /* Report the CLI number of the new process */
    out_str("[CLI ");
    out_num(proc->pr_TaskNum);
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
