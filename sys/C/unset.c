/*
 * UNSET command - Remove local shell variables
 * Step 9.4 implementation for lxa
 * 
 * Template: NAME/A
 * 
 * Removes a local variable from the current process.
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
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs)
    {
        PrintFault(IoErr(), (STRPTR)"UNSET");
        return RETURN_FAIL;
    }
    
    name = (STRPTR)args[ARG_NAME];
    
    /* Delete local variable */
    BOOL result = DeleteVar(name, GVF_LOCAL_ONLY | LV_VAR);
    
    FreeArgs(rdargs);
    
    /* DeleteVar returns TRUE on success, but it's not an error
     * if the variable didn't exist - AmigaOS behavior */
    return RETURN_OK;
    
    /* Suppress unused variable warning */
    (void)result;
}
