/*
 * MAKELINK command - Create hard or soft links
 * Step 9.5 implementation for lxa
 * 
 * Template: FROM/A,TO/A,HARD/S,SOFT/S,FORCE/S
 * 
 * Usage:
 *   MAKELINK link target          - Create hard link (default)
 *   MAKELINK link target SOFT     - Create soft link
 *   MAKELINK link target HARD     - Create hard link (explicit)
 *   MAKELINK link target FORCE    - Overwrite existing link
 */

#include <exec/types.h>
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
#define TEMPLATE "FROM/A,TO/A,HARD/S,SOFT/S,FORCE/S"

/* Argument array indices */
#define ARG_FROM   0
#define ARG_TO     1
#define ARG_HARD   2
#define ARG_SOFT   3
#define ARG_FORCE  4
#define ARG_COUNT  5

/* Helper: output a string */
static void out_str(const char *str)
{
    Write(Output(), (STRPTR)str, strlen(str));
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    int rc = RETURN_OK;
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"MAKELINK");
        return RETURN_FAIL;
    }
    
    STRPTR from = (STRPTR)args[ARG_FROM];   /* Link name to create */
    STRPTR to = (STRPTR)args[ARG_TO];       /* Target to link to */
    BOOL hard = args[ARG_HARD] != 0;
    BOOL soft = args[ARG_SOFT] != 0;
    BOOL force = args[ARG_FORCE] != 0;
    
    /* Default to soft link if neither specified (more compatible) */
    /* AmigaOS default is hard link, but soft links are more common in practice */
    BOOL makeSoft = soft || (!hard && !soft);
    
    /* Check if link already exists */
    BPTR existLock = Lock(from, SHARED_LOCK);
    if (existLock) {
        UnLock(existLock);
        if (!force) {
            out_str("MAKELINK: ");
            out_str((char *)from);
            out_str(" already exists. Use FORCE to overwrite.\n");
            FreeArgs(rdargs);
            return RETURN_ERROR;
        }
        /* Delete existing link */
        if (!DeleteFile(from)) {
            out_str("MAKELINK: Cannot remove existing ");
            out_str((char *)from);
            out_str("\n");
            FreeArgs(rdargs);
            return RETURN_ERROR;
        }
    }
    
    /* Verify target exists for hard links */
    if (!makeSoft) {
        BPTR targetLock = Lock(to, SHARED_LOCK);
        if (!targetLock) {
            out_str("MAKELINK: Target ");
            out_str((char *)to);
            out_str(" does not exist\n");
            FreeArgs(rdargs);
            return RETURN_ERROR;
        }
        UnLock(targetLock);
    }
    
    /* Create the link */
    /* MakeLink(name, dest, soft) - soft=1 for soft link, soft=0 for hard link */
    /* For soft links, dest is the path string (as LONG cast)
     * For hard links, dest is a lock to the target */
    
    if (makeSoft) {
        /* Soft link - dest is the path string */
        if (!MakeLink(from, (LONG)to, LINK_SOFT)) {
            PrintFault(IoErr(), (STRPTR)"MAKELINK");
            rc = RETURN_ERROR;
        }
    } else {
        /* Hard link - need to pass a lock to the target */
        BPTR targetLock = Lock(to, SHARED_LOCK);
        if (!targetLock) {
            PrintFault(IoErr(), (STRPTR)"MAKELINK");
            FreeArgs(rdargs);
            return RETURN_ERROR;
        }
        
        if (!MakeLink(from, (LONG)targetLock, LINK_HARD)) {
            PrintFault(IoErr(), (STRPTR)"MAKELINK");
            rc = RETURN_ERROR;
        }
        
        UnLock(targetLock);
    }
    
    FreeArgs(rdargs);
    return rc;
}
