/*
 * WAIT command - Wait for specified time or until file exists
 * Step 9.5 implementation for lxa
 * 
 * Template: TIME,SEC=SECS/S,MIN=MINS/S,UNTIL/K
 * 
 * Usage:
 *   WAIT 5              - Wait for 5 seconds
 *   WAIT 5 SECS         - Wait for 5 seconds
 *   WAIT 2 MINS         - Wait for 2 minutes
 *   WAIT UNTIL 14:30    - Wait until time (not implemented yet)
 */

#include <exec/types.h>
#include <exec/tasks.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template */
#define TEMPLATE "TIME/N,SEC=SECS/S,MIN=MINS/S,UNTIL/K"

/* Argument array indices */
#define ARG_TIME   0
#define ARG_SECS   1
#define ARG_MINS   2
#define ARG_UNTIL  3
#define ARG_COUNT  4

/* Helper: output a string */
static void out_str(const char *str)
{
    Write(Output(), (STRPTR)str, strlen(str));
}

/* Check for Ctrl+C break */
static BOOL check_break(void)
{
    if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
        SetSignal(0, SIGBREAKF_CTRL_C);  /* Clear the signal */
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"WAIT");
        return RETURN_FAIL;
    }
    
    LONG time_val = args[ARG_TIME];
    /* BOOL secs = args[ARG_SECS] != 0; */  /* Unused - seconds is default */
    BOOL mins = args[ARG_MINS] != 0;
    STRPTR until = (STRPTR)args[ARG_UNTIL];
    
    /* Handle UNTIL (time string like HH:MM:SS) - not fully implemented */
    if (until) {
        out_str("WAIT: UNTIL not implemented yet\n");
        FreeArgs(rdargs);
        return RETURN_WARN;
    }
    
    /* If no time specified, wait for 1 second by default */
    if (time_val == 0) {
        time_val = 1;
    }
    
    /* Convert to seconds */
    LONG seconds;
    if (mins) {
        seconds = time_val * 60;
    } else {
        /* Default is seconds */
        seconds = time_val;
    }
    
    /* Validate */
    if (seconds < 0) {
        out_str("WAIT: Invalid time value\n");
        FreeArgs(rdargs);
        return RETURN_ERROR;
    }
    
    if (seconds > 3600) {
        out_str("WAIT: Maximum wait time is 3600 seconds (1 hour)\n");
        FreeArgs(rdargs);
        return RETURN_ERROR;
    }
    
    /* Wait using Delay() - 50 ticks per second */
    /* Check for break every second */
    int rc = RETURN_OK;
    
    while (seconds > 0) {
        /* Wait for 1 second (50 ticks) */
        Delay(50);
        seconds--;
        
        /* Check for Ctrl+C */
        if (check_break()) {
            out_str("***Break\n");
            rc = RETURN_WARN;
            break;
        }
    }
    
    FreeArgs(rdargs);
    return rc;
}
