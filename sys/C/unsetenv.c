/*
 * UNSETENV command - Remove global environment variables
 * Step 9.4 implementation for lxa
 * 
 * Template: NAME/A,SAVE/S
 * 
 * Removes an environment variable from ENV:.
 * If SAVE is specified, also removes from ENVARC:.
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
#define TEMPLATE "NAME/A,SAVE/S"

/* Argument array indices */
#define ARG_NAME    0
#define ARG_SAVE    1
#define ARG_COUNT   2

/* Main entry point */
int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    STRPTR name;
    BOOL save;
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs)
    {
        PrintFault(IoErr(), (STRPTR)"UNSETENV");
        return RETURN_FAIL;
    }
    
    name = (STRPTR)args[ARG_NAME];
    save = args[ARG_SAVE] != 0;
    
    /* Build flags for deletion */
    LONG flags = GVF_GLOBAL_ONLY | LV_VAR;
    if (save)
        flags |= GVF_SAVE_VAR;
    
    /* Delete the environment variable */
    BOOL result = DeleteVar(name, flags);
    
    FreeArgs(rdargs);
    
    /* DeleteVar returns TRUE on success, but it's not an error
     * if the variable didn't exist - AmigaOS behavior */
    return RETURN_OK;
    
    /* Suppress unused variable warning */
    (void)result;
}
