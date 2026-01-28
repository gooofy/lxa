/*
 * DELETE command - Delete files or directories
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

/* Delete a file or directory */
static int delete_object(CONST_STRPTR path, BOOL recursive, BOOL force)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    LONG result;
    
    /* First try to delete it as a file */
    result = DeleteFile((STRPTR)path);
    if (result != DOSFALSE) {
        return 0;  /* Successfully deleted as file */
    }
    
    /* Failed to delete as file - check if it's a directory */
    lock = Lock(path, SHARED_LOCK);
    if (!lock) {
        if (!force) {
            printf("DELETE: '%s' not found\n", path);
        }
        return 1;
    }
    
    /* Allocate FileInfoBlock */
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        printf("DELETE: Failed to allocate FileInfoBlock\n");
        UnLock(lock);
        return 1;
    }
    
    /* Check if it's a directory */
    if (!Examine(lock, fib)) {
        printf("DELETE: Can't examine '%s' - %ld\n", path, IoErr());
        UnLock(lock);
        FreeDosObject(DOS_FIB, fib);
        return 1;
    }
    
    UnLock(lock);
    
    if (fib->fib_DirEntryType <= 0) {
        /* It's a file, not a directory - deletion should have worked */
        if (!force) {
            printf("DELETE: Failed to delete '%s' - %ld\n", path, IoErr());
        }
        FreeDosObject(DOS_FIB, fib);
        return 1;
    }
    
    /* It's a directory */
    if (!recursive) {
        printf("DELETE: '%s' is a directory (use -r for recursive delete)\n", path);
        FreeDosObject(DOS_FIB, fib);
        return 1;
    }
    
    /* For recursive delete, we'd need to enumerate and delete contents first
     * For now, just try to delete the empty directory */
    result = DeleteFile((STRPTR)path);
    if (result == DOSFALSE) {
        printf("DELETE: Failed to delete directory '%s' - %ld\n", path, IoErr());
        printf("DELETE: Directory may not be empty\n");
        FreeDosObject(DOS_FIB, fib);
        return 1;
    }
    
    FreeDosObject(DOS_FIB, fib);
    return 0;
}

int main(int argc, char **argv)
{
    CONST_STRPTR path = NULL;
    BOOL recursive = FALSE;
    BOOL force = FALSE;
    int i;
    int errors = 0;
    
    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            /* Option */
            switch (argv[i][1]) {
                case 'r':
                case 'R':
                    recursive = TRUE;
                    break;
                case 'f':
                    force = TRUE;
                    break;
                case '?':
                case 'h':
                    printf("DELETE - Delete files or directories (v%s)\n", VERSION);
                    printf("Usage: DELETE [options] <file|directory>...\n");
                    printf("Options:\n");
                    printf("  -r,-R   Recursive (delete directories and contents)\n");
                    printf("  -f      Force (don't complain about errors)\n");
                    return 0;
                default:
                    printf("DELETE: Unknown option '%s'\n", argv[i]);
                    return 1;
            }
        } else {
            /* File/directory argument - process it */
            path = (CONST_STRPTR)argv[i];
            if (delete_object(path, recursive, force) != 0) {
                errors++;
            }
        }
    }
    
    /* Check for required argument */
    if (!path) {
        printf("DELETE: Missing filename\n");
        printf("Usage: DELETE [options] <file|directory>...\n");
        return 1;
    }
    
    return errors > 0 ? 1 : 0;
}
