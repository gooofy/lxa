/*
 * MAKEDIR command - Create directories
 * Phase 4 implementation for lxa
 * 
 * Template: NAME/M/A
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <string.h>
#include <stdio.h>

#define VERSION "1.0"

/* External reference to DOS library base */
extern struct DosLibrary *DOSBase;

/* Command template */
#define TEMPLATE "NAME/M/A"

/* Argument array indices */
#define ARG_NAME    0
#define ARG_COUNT   1

/* Create a single directory */
static BOOL make_single_directory(CONST_STRPTR path)
{
    BPTR lock;
    
    /* Attempt to create the directory */
    lock = CreateDir((STRPTR)path);
    if (!lock) {
        LONG err = IoErr();
        if (err == ERROR_OBJECT_EXISTS) {
            printf("MAKEDIR: '%s' already exists\n", path);
        } else if (err == ERROR_OBJECT_NOT_FOUND) {
            printf("MAKEDIR: Cannot create '%s' - parent directory does not exist\n", path);
        } else {
            printf("MAKEDIR: Failed to create '%s' - %ld\n", path, err);
        }
        return FALSE;
    }
    
    UnLock(lock);
    printf("MAKEDIR: Created directory '%s'\n", path);
    return TRUE;
}

int main(int argc, char **argv)
{
    LONG args[ARG_COUNT] = {0};
    struct RDArgs *rda;
    int errors = 0;
    
    (void)argc;
    (void)argv;
    
    /* Parse arguments using AmigaDOS template */
    rda = ReadArgs(TEMPLATE, args, NULL);
    if (!rda) {
        LONG err = IoErr();
        if (err == ERROR_REQUIRED_ARG_MISSING) {
            printf("MAKEDIR: NAME argument is required\n");
        } else {
            printf("MAKEDIR: Error parsing arguments - %ld\n", err);
        }
        printf("Usage: MAKEDIR NAME/M/A\n");
        printf("Template: %s\n", TEMPLATE);
        return 1;
    }
    
    /* Extract arguments */
    STRPTR *name_array = (STRPTR *)args[ARG_NAME];
    
    /* Process each directory name */
    if (name_array) {
        for (int i = 0; name_array[i] != NULL; i++) {
            if (!make_single_directory(name_array[i])) {
                errors++;
            }
        }
    } else {
        printf("MAKEDIR: No directory name specified\n");
        FreeArgs(rda);
        return 1;
    }
    
    FreeArgs(rda);
    return errors > 0 ? 1 : 0;
}
