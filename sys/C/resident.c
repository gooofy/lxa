/*
 * RESIDENT command - Manage resident commands
 * Phase 17 implementation for lxa
 * 
 * Template: NAME,FILE,REMOVE/S,ADD/S,REPLACE/S,PURE/S,SYSTEM/S,FORCE/S
 * 
 * Usage:
 *   RESIDENT           - List all resident commands
 *   RESIDENT <name>    - Show info about specific resident
 *   RESIDENT <file> ADD - Add a command to resident list
 *   RESIDENT <name> REMOVE - Remove from resident list
 *
 * Note: In lxa, resident commands are managed by the shell's internal
 * segment list. This command provides a user interface to that list.
 * Full implementation requires AddSegment/FindSegment/RemSegment in DOS.
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
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

/* Command template - AmigaOS compatible */
#define TEMPLATE "NAME,FILE,REMOVE/S,ADD/S,REPLACE/S,PURE/S,SYSTEM/S,FORCE/S"

/* Argument array indices */
#define ARG_NAME    0
#define ARG_FILE    1
#define ARG_REMOVE  2
#define ARG_ADD     3
#define ARG_REPLACE 4
#define ARG_PURE    5
#define ARG_SYSTEM  6
#define ARG_FORCE   7
#define ARG_COUNT   8

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

/* List libraries and devices (the actual populated lists in lxa) */
static void list_exec_residents(void)
{
    struct Node *node;
    
    out_str("NAME                    VERSION  TYPE\n");
    out_str("----                    -------  ----\n");
    
    /* List libraries */
    Forbid();
    
    for (node = SysBase->LibList.lh_Head; node->ln_Succ; node = node->ln_Succ)
    {
        struct Library *lib = (struct Library *)node;
        
        /* Name (padded to 24 chars) */
        if (node->ln_Name)
        {
            out_str(node->ln_Name);
            int len = strlen(node->ln_Name);
            while (len < 24)
            {
                out_str(" ");
                len++;
            }
        }
        else
        {
            out_str("(unnamed)               ");
        }
        
        /* Version */
        char verbuf[16];
        char *p = verbuf + sizeof(verbuf) - 1;
        int ver = lib->lib_Version;
        int rev = lib->lib_Revision;
        *p = '\0';
        
        /* Build "ver.rev" string */
        do
        {
            *--p = '0' + (rev % 10);
            rev /= 10;
        } while (rev);
        *--p = '.';
        do
        {
            *--p = '0' + (ver % 10);
            ver /= 10;
        } while (ver);
        
        out_str(p);
        
        int vlen = strlen(p);
        while (vlen < 9)
        {
            out_str(" ");
            vlen++;
        }
        
        out_str("library");
        out_nl();
    }
    
    /* List devices */
    for (node = SysBase->DeviceList.lh_Head; node->ln_Succ; node = node->ln_Succ)
    {
        struct Library *lib = (struct Library *)node;
        
        /* Name (padded to 24 chars) */
        if (node->ln_Name)
        {
            out_str(node->ln_Name);
            int len = strlen(node->ln_Name);
            while (len < 24)
            {
                out_str(" ");
                len++;
            }
        }
        else
        {
            out_str("(unnamed)               ");
        }
        
        /* Version */
        char verbuf[16];
        char *p = verbuf + sizeof(verbuf) - 1;
        int ver = lib->lib_Version;
        int rev = lib->lib_Revision;
        *p = '\0';
        
        /* Build "ver.rev" string */
        do
        {
            *--p = '0' + (rev % 10);
            rev /= 10;
        } while (rev);
        *--p = '.';
        do
        {
            *--p = '0' + (ver % 10);
            ver /= 10;
        } while (ver);
        
        out_str(p);
        
        int vlen = strlen(p);
        while (vlen < 9)
        {
            out_str(" ");
            vlen++;
        }
        
        out_str("device");
        out_nl();
    }
    
    Permit();
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    
    /* Parse arguments using AmigaDOS template */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs)
    {
        PrintFault(IoErr(), (STRPTR)"RESIDENT");
        return RETURN_ERROR;
    }
    
    STRPTR name = (STRPTR)args[ARG_NAME];
    STRPTR file = (STRPTR)args[ARG_FILE];
    BOOL doRemove = (BOOL)args[ARG_REMOVE];
    BOOL doAdd = (BOOL)args[ARG_ADD];
    BOOL doReplace = (BOOL)args[ARG_REPLACE];
    /* BOOL pure = (BOOL)args[ARG_PURE]; */
    /* BOOL system = (BOOL)args[ARG_SYSTEM]; */
    /* BOOL force = (BOOL)args[ARG_FORCE]; */
    
    if (!name && !file && !doRemove && !doAdd && !doReplace)
    {
        /* No arguments - list resident modules */
        list_exec_residents();
        FreeArgs(rdargs);
        return RETURN_OK;
    }
    
    if (doAdd || doReplace)
    {
        /* ADD or REPLACE - need to implement segment list management */
        out_str("RESIDENT: ADD/REPLACE not yet implemented in lxa.\n");
        out_str("Commands are executed directly from disk.\n");
        FreeArgs(rdargs);
        return RETURN_WARN;
    }
    
    if (doRemove)
    {
        out_str("RESIDENT: REMOVE not yet implemented in lxa.\n");
        FreeArgs(rdargs);
        return RETURN_WARN;
    }
    
    if (name && !doAdd && !doRemove && !doReplace)
    {
        /* Just a name - show info about that library/device */
        struct Library *lib;
        BOOL found = FALSE;
        
        /* Try to find in library list */
        Forbid();
        lib = (struct Library *)FindName(&SysBase->LibList, name);
        if (lib)
        {
            out_str("Name: ");
            out_str(lib->lib_Node.ln_Name);
            out_nl();
            
            if (lib->lib_IdString)
            {
                out_str("ID: ");
                out_str(lib->lib_IdString);
                out_nl();
            }
            
            out_str("Version: ");
            char verbuf[16];
            char *p = verbuf + sizeof(verbuf) - 1;
            int ver = lib->lib_Version;
            int rev = lib->lib_Revision;
            *p = '\0';
            do
            {
                *--p = '0' + (rev % 10);
                rev /= 10;
            } while (rev);
            *--p = '.';
            do
            {
                *--p = '0' + (ver % 10);
                ver /= 10;
            } while (ver);
            out_str(p);
            out_nl();
            
            out_str("Type: library\n");
            found = TRUE;
        }
        
        if (!found)
        {
            /* Try device list */
            lib = (struct Library *)FindName(&SysBase->DeviceList, name);
            if (lib)
            {
                out_str("Name: ");
                out_str(lib->lib_Node.ln_Name);
                out_nl();
                
                if (lib->lib_IdString)
                {
                    out_str("ID: ");
                    out_str(lib->lib_IdString);
                    out_nl();
                }
                
                out_str("Version: ");
                char verbuf[16];
                char *p = verbuf + sizeof(verbuf) - 1;
                int ver = lib->lib_Version;
                int rev = lib->lib_Revision;
                *p = '\0';
                do
                {
                    *--p = '0' + (rev % 10);
                    rev /= 10;
                } while (rev);
                *--p = '.';
                do
                {
                    *--p = '0' + (ver % 10);
                    ver /= 10;
                } while (ver);
                out_str(p);
                out_nl();
                
                out_str("Type: device\n");
                found = TRUE;
            }
        }
        Permit();
        
        if (!found)
        {
            out_str("RESIDENT: ");
            out_str((char *)name);
            out_str(" not found.\n");
            FreeArgs(rdargs);
            return RETURN_WARN;
        }
    }
    
    FreeArgs(rdargs);
    return RETURN_OK;
}
