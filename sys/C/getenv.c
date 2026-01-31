/*
 * GETENV command - Display value of environment variable
 * Step 9.4 implementation for lxa
 * 
 * Template: NAME/A
 * 
 * Outputs the value of the environment variable to stdout.
 * Returns WARN (5) if variable doesn't exist.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

#define VERSION "1.0"

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template */
#define TEMPLATE "NAME/A"

/* Argument array indices */
#define ARG_NAME    0
#define ARG_COUNT   1

/* Main entry point */
int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    STRPTR name;
    char buffer[1024];
    BPTR out = Output();
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs)
    {
        PrintFault(IoErr(), (STRPTR)"GETENV");
        return RETURN_FAIL;
    }
    
    name = (STRPTR)args[ARG_NAME];
    
    /* Get the variable - try global first (ENV:), then local */
    LONG len = GetVar(name, (STRPTR)buffer, sizeof(buffer), LV_VAR);
    
    if (len < 0)
    {
        /* Variable not found */
        FreeArgs(rdargs);
        return RETURN_WARN;
    }
    
    /* Output the value */
    if (len > 0)
    {
        Write(out, (STRPTR)buffer, len);
    }
    Write(out, (STRPTR)"\n", 1);
    
    FreeArgs(rdargs);
    return RETURN_OK;
}
