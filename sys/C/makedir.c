/*
 * MAKEDIR command - Create directories
 * Phase 4 implementation for lxa
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

/* Create a directory */
static int make_directory(CONST_STRPTR path, BOOL create_parents)
{
    BPTR lock;
    
    if (create_parents) {
        /* For parent creation, we'd need to parse path and create each component
         * For simplicity, just try to create the lock directly */
        lock = CreateDir((STRPTR)path);
        if (!lock) {
            printf("MAKEDIR: Failed to create '%s' - %ld\n", path, IoErr());
            return 1;
        }
        UnLock(lock);
    } else {
        /* Simple directory creation */
        lock = CreateDir((STRPTR)path);
        if (!lock) {
            printf("MAKEDIR: Failed to create '%s' - %ld\n", path, IoErr());
            return 1;
        }
        UnLock(lock);
    }
    
    return 0;
}

int main(int argc, char **argv)
{
    CONST_STRPTR path = NULL;
    BOOL create_parents = FALSE;
    int i;
    int errors = 0;
    
    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            /* Option */
            switch (argv[i][1]) {
                case 'p':
                    create_parents = TRUE;
                    break;
                case '?':
                case 'h':
                    printf("MAKEDIR - Create directories (v%s)\n", VERSION);
                    printf("Usage: MAKEDIR [options] <directory>...\n");
                    printf("Options:\n");
                    printf("  -p      Create parent directories as needed\n");
                    return 0;
                default:
                    printf("MAKEDIR: Unknown option '%s'\n", argv[i]);
                    return 1;
            }
        } else {
            /* Directory argument - process it */
            path = (CONST_STRPTR)argv[i];
            if (make_directory(path, create_parents) != 0) {
                errors++;
            }
        }
    }
    
    /* Check for required argument */
    if (!path) {
        printf("MAKEDIR: Missing directory name\n");
        printf("Usage: MAKEDIR [options] <directory>...\n");
        return 1;
    }
    
    return errors > 0 ? 1 : 0;
}
