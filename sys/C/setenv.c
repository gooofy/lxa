/*
 * SETENV command - Set or display global environment variables
 * Step 9.4 implementation for lxa
 * 
 * Template: NAME,STRING/F,SAVE/S
 * 
 * If no arguments: list all environment variables (from ENV:)
 * If NAME only: display value of NAME
 * If NAME STRING: set NAME to STRING
 * SAVE: Also save to ENVARC: for persistence
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
#define TEMPLATE "NAME,STRING/F,SAVE/S"

/* Argument array indices */
#define ARG_NAME    0
#define ARG_STRING  1
#define ARG_SAVE    2
#define ARG_COUNT   3

/* Helper: output a string */
static void out_str(BPTR fh, const char *str)
{
    Write(fh, (STRPTR)str, strlen(str));
}

/* Helper: output newline */
static void out_nl(BPTR fh)
{
    Write(fh, (STRPTR)"\n", 1);
}

/* List all environment variables by reading ENV: directory */
static void list_all_env_vars(void)
{
    BPTR out = Output();
    BPTR lock = Lock((STRPTR)"ENV:", SHARED_LOCK);
    
    if (!lock)
    {
        out_str(out, "SETENV: Cannot access ENV:\n");
        return;
    }
    
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib)
    {
        UnLock(lock);
        out_str(out, "SETENV: Out of memory\n");
        return;
    }
    
    if (Examine(lock, fib))
    {
        while (ExNext(lock, fib))
        {
            /* Skip directories */
            if (fib->fib_DirEntryType > 0)
                continue;
            
            /* Display variable name and value */
            out_str(out, (const char *)fib->fib_FileName);
            out_str(out, "  ");
            
            /* Read the value */
            char buffer[256];
            LONG len = GetVar((CONST_STRPTR)fib->fib_FileName, (STRPTR)buffer, sizeof(buffer), 
                              GVF_GLOBAL_ONLY | LV_VAR);
            if (len > 0)
            {
                Write(out, (STRPTR)buffer, len);
            }
            out_nl(out);
        }
    }
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
}

/* Display a single environment variable */
static BOOL display_env_var(CONST_STRPTR name)
{
    BPTR out = Output();
    char buffer[256];
    
    LONG len = GetVar(name, (STRPTR)buffer, sizeof(buffer), GVF_GLOBAL_ONLY | LV_VAR);
    if (len < 0)
    {
        return FALSE;
    }
    
    out_str(out, (const char *)name);
    out_str(out, "  ");
    if (len > 0)
    {
        Write(out, (STRPTR)buffer, len);
    }
    out_nl(out);
    return TRUE;
}

/* Set an environment variable */
static BOOL set_env_var(CONST_STRPTR name, CONST_STRPTR value, BOOL save)
{
    LONG flags = GVF_GLOBAL_ONLY | LV_VAR;
    if (save)
        flags |= GVF_SAVE_VAR;
    
    LONG len = -1;  /* -1 means null-terminated string */
    return SetVar(name, value, len, flags);
}

/* Main entry point */
int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    STRPTR name;
    STRPTR value;
    BOOL save;
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs)
    {
        PrintFault(IoErr(), (STRPTR)"SETENV");
        return RETURN_FAIL;
    }
    
    name = (STRPTR)args[ARG_NAME];
    value = (STRPTR)args[ARG_STRING];
    save = args[ARG_SAVE] != 0;
    
    if (!name)
    {
        /* No arguments - list all environment variables */
        list_all_env_vars();
    }
    else if (!value)
    {
        /* Name only - display that variable */
        if (!display_env_var(name))
        {
            /* Variable not found - not an error, just no output */
        }
    }
    else
    {
        /* Set variable */
        if (!set_env_var(name, value, save))
        {
            PrintFault(IoErr(), (STRPTR)"SETENV");
            FreeArgs(rdargs);
            return RETURN_FAIL;
        }
    }
    
    FreeArgs(rdargs);
    return RETURN_OK;
}
