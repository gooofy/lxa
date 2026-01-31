/*
 * VERSION command - Display version information
 * Step 9.5 implementation for lxa
 * 
 * Template: NAME,VERSION/N,REVISION/N,FILE/S,FULL/S,RES/S
 * 
 * Usage:
 *   VERSION                - Display lxa Kickstart version
 *   VERSION name           - Display version of named library/device/file
 *   VERSION FULL           - Display full version info
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

/* lxa version info */
#define LXA_VERSION  1
#define LXA_REVISION 0

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template */
#define TEMPLATE "NAME,VERSION/N,REVISION/N,FILE/S,FULL/S,RES/S"

/* Argument array indices */
#define ARG_NAME      0
#define ARG_VERSION   1
#define ARG_REVISION  2
#define ARG_FILE      3
#define ARG_FULL      4
#define ARG_RES       5
#define ARG_COUNT     6

/* Helper: output a string */
static void out_str(const char *str)
{
    Write(Output(), (STRPTR)str, strlen(str));
}

/* Helper: output a number */
static void out_num(ULONG num)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    
    do {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    
    out_str(p);
}

/* Display system version */
static void show_system_version(BOOL full)
{
    out_str("lxa Kickstart ");
    out_num(LXA_VERSION);
    out_str(".");
    out_num(LXA_REVISION);
    out_str("\n");
    
    if (full) {
        /* Show exec version */
        out_str("Exec ");
        out_num(SysBase->LibNode.lib_Version);
        out_str(".");
        out_num(SysBase->LibNode.lib_Revision);
        out_str("\n");
        
        /* Show DOS version */
        out_str("DOS ");
        out_num(DOSBase->dl_lib.lib_Version);
        out_str(".");
        out_num(DOSBase->dl_lib.lib_Revision);
        out_str("\n");
    }
}

/* Search for $VER: string in file */
static BOOL find_version_in_file(const char *filename)
{
    BPTR fh = Open((STRPTR)filename, MODE_OLDFILE);
    if (!fh) {
        return FALSE;
    }
    
    /* Read file in chunks and search for $VER: */
    char buffer[512];
    char verstring[256];
    LONG bytesRead;
    int state = 0;  /* State machine for finding "$VER:" */
    int verpos = 0;
    BOOL found = FALSE;
    
    while ((bytesRead = Read(fh, (STRPTR)buffer, sizeof(buffer))) > 0) {
        for (int i = 0; i < bytesRead && !found; i++) {
            char c = buffer[i];
            
            /* State machine to find "$VER:" */
            switch (state) {
                case 0: state = (c == '$') ? 1 : 0; break;
                case 1: state = (c == 'V') ? 2 : (c == '$') ? 1 : 0; break;
                case 2: state = (c == 'E') ? 3 : (c == '$') ? 1 : 0; break;
                case 3: state = (c == 'R') ? 4 : (c == '$') ? 1 : 0; break;
                case 4: state = (c == ':') ? 5 : (c == '$') ? 1 : 0; break;
                case 5:
                    /* Skip leading space */
                    if (c == ' ' && verpos == 0) break;
                    /* Collect version string until newline or null */
                    if (c == '\n' || c == '\r' || c == '\0' || verpos >= (int)sizeof(verstring) - 1) {
                        verstring[verpos] = '\0';
                        found = TRUE;
                    } else {
                        verstring[verpos++] = c;
                    }
                    break;
            }
        }
        if (found) break;
    }
    
    Close(fh);
    
    if (found && verpos > 0) {
        out_str(verstring);
        out_str("\n");
        return TRUE;
    }
    
    return FALSE;
}

/* Find and display library version */
static BOOL show_library_version(const char *name)
{
    /* Try to find as library */
    struct Library *lib = (struct Library *)FindName(&SysBase->LibList, (STRPTR)name);
    if (lib) {
        out_str((char *)name);
        out_str(" ");
        out_num(lib->lib_Version);
        out_str(".");
        out_num(lib->lib_Revision);
        out_str("\n");
        return TRUE;
    }
    
    /* Try to find as device */
    struct Device *dev = (struct Device *)FindName(&SysBase->DeviceList, (STRPTR)name);
    if (dev) {
        out_str((char *)name);
        out_str(" ");
        out_num(dev->dd_Library.lib_Version);
        out_str(".");
        out_num(dev->dd_Library.lib_Revision);
        out_str("\n");
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
        PrintFault(IoErr(), (STRPTR)"VERSION");
        return RETURN_FAIL;
    }
    
    STRPTR name = (STRPTR)args[ARG_NAME];
    BOOL full = args[ARG_FULL] != 0;
    BOOL file = args[ARG_FILE] != 0;
    
    if (!name) {
        /* No name - show system version */
        show_system_version(full);
    } else {
        BOOL found = FALSE;
        
        /* If FILE switch or name contains path separator, treat as file */
        if (file || strchr((char *)name, '/') || strchr((char *)name, ':')) {
            found = find_version_in_file((char *)name);
        }
        
        if (!found) {
            /* Try as library/device name */
            found = show_library_version((char *)name);
        }
        
        if (!found) {
            /* Try adding common suffixes */
            char fullname[256];
            
            /* Try .library */
            strcpy(fullname, (char *)name);
            if (!strchr((char *)name, '.')) {
                strcat(fullname, ".library");
                found = show_library_version(fullname);
            }
            
            if (!found) {
                /* Try .device */
                strcpy(fullname, (char *)name);
                if (!strchr((char *)name, '.')) {
                    strcat(fullname, ".device");
                    found = show_library_version(fullname);
                }
            }
            
            if (!found) {
                /* Try as file path */
                found = find_version_in_file((char *)name);
            }
        }
        
        if (!found) {
            out_str("Could not find version for ");
            out_str((char *)name);
            out_str("\n");
            FreeArgs(rdargs);
            return RETURN_WARN;
        }
    }
    
    FreeArgs(rdargs);
    return RETURN_OK;
}
