/*
 * SET command - Set or display local shell variables
 * Step 9.4 implementation for lxa
 * 
 * Template: NAME,STRING/F
 * 
 * If no arguments: list all local variables
 * If NAME only: display value of NAME
 * If NAME STRING: set NAME to STRING
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
#define TEMPLATE "NAME,STRING/F"

/* Argument array indices */
#define ARG_NAME    0
#define ARG_STRING  1
#define ARG_COUNT   2

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

/* List all local variables */
static void list_all_vars(void)
{
    BPTR out = Output();
    struct Process *proc = (struct Process *)FindTask(NULL);
    
    if (proc->pr_Task.tc_Node.ln_Type != NT_PROCESS)
    {
        out_str(out, "SET: Not running as a process\n");
        return;
    }
    
    struct MinList *list = &proc->pr_LocalVars;
    struct LocalVar *lv;
    
    for (lv = (struct LocalVar *)list->mlh_Head;
         lv->lv_Node.ln_Succ != NULL;
         lv = (struct LocalVar *)lv->lv_Node.ln_Succ)
    {
        /* Only show LV_VAR type, not aliases */
        if ((lv->lv_Node.ln_Type & 0x7F) != LV_VAR)
            continue;
        
        /* Skip ignored variables */
        if (lv->lv_Node.ln_Type & LVF_IGNORE)
            continue;
        
        if (lv->lv_Node.ln_Name)
        {
            out_str(out, lv->lv_Node.ln_Name);
            out_str(out, "  ");
            
            if (lv->lv_Value)
            {
                /* Output value, handling potential non-null-terminated data */
                LONG len = lv->lv_Len;
                if (len > 0 && lv->lv_Value[len - 1] == '\0')
                    len--;  /* Don't print the null terminator */
                if (len > 0)
                    Write(out, (STRPTR)lv->lv_Value, len);
            }
            out_nl(out);
        }
    }
}

/* Display a single variable */
static BOOL display_var(CONST_STRPTR name)
{
    BPTR out = Output();
    char buffer[256];
    
    LONG len = GetVar(name, (STRPTR)buffer, sizeof(buffer), GVF_LOCAL_ONLY | LV_VAR);
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

/* Set a variable */
static BOOL set_var(CONST_STRPTR name, CONST_STRPTR value)
{
    LONG len = -1;  /* -1 means null-terminated string */
    return SetVar(name, value, len, GVF_LOCAL_ONLY | LV_VAR);
}

/* Main entry point */
int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    STRPTR name;
    STRPTR value;
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs)
    {
        PrintFault(IoErr(), (STRPTR)"SET");
        return RETURN_FAIL;
    }
    
    name = (STRPTR)args[ARG_NAME];
    value = (STRPTR)args[ARG_STRING];
    
    if (!name)
    {
        /* No arguments - list all variables */
        list_all_vars();
    }
    else if (!value)
    {
        /* Name only - display that variable */
        if (!display_var(name))
        {
            /* Variable not found - not an error, just no output */
        }
    }
    else
    {
        /* Set variable */
        if (!set_var(name, value))
        {
            PrintFault(IoErr(), (STRPTR)"SET");
            FreeArgs(rdargs);
            return RETURN_FAIL;
        }
    }
    
    FreeArgs(rdargs);
    return RETURN_OK;
}
