/*
 * DELETE command - Delete files or directories
 * Phase 4 implementation for lxa
 * 
 * Template: FILE/M/A,ALL/S,QUIET/S,FORCE/S
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
#define TEMPLATE "FILE/M/A,ALL/S,QUIET/S,FORCE/S"

/* Argument array indices */
#define ARG_FILE    0
#define ARG_ALL     1
#define ARG_QUIET   2
#define ARG_FORCE   3
#define ARG_COUNT   4

/* Buffer for pattern matching */
#define PATTERN_BUFFER_SIZE 256

/* Check if filename matches pattern using AmigaDOS wildcards */
static BOOL match_filename(const char *filename, const char *pattern)
{
    if (!pattern || pattern[0] == '\0')
        return TRUE;
    
    char parsed_pat[PATTERN_BUFFER_SIZE];
    LONG result = ParsePattern(pattern, parsed_pat, sizeof(parsed_pat));
    
    if (result == 0) {
        /* Parse error - treat as literal match */
        return (strcmp(filename, pattern) == 0);
    }
    
    if (result < 0) {
        /* Literal string - exact match */
        return (strcmp(filename, pattern) == 0);
    }
    
    /* Wildcard pattern */
    return MatchPattern(parsed_pat, (STRPTR)filename);
}

/* Delete a single file */
static BOOL delete_single_file(CONST_STRPTR path, BOOL force, BOOL quiet)
{
    LONG result = DeleteFile((STRPTR)path);
    
    if (result == DOSFALSE) {
        if (!force && !quiet) {
            printf("DELETE: Failed to delete '%s' - %ld\n", path, IoErr());
        }
        return FALSE;
    }
    
    if (!quiet) {
        printf("DELETE: Deleted '%s'\n", path);
    }
    
    return TRUE;
}

/* Delete a directory and optionally its contents */
static BOOL delete_directory(CONST_STRPTR path, BOOL recursive, BOOL force, BOOL quiet)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    LONG result;
    BOOL success = TRUE;
    
    if (recursive) {
        /* Recursive delete - first delete contents */
        lock = Lock(path, SHARED_LOCK);
        if (!lock) {
            if (!force && !quiet) {
                printf("DELETE: Cannot access '%s' - %ld\n", path, IoErr());
            }
            return FALSE;
        }
        
        fib = AllocDosObject(DOS_FIB, NULL);
        if (!fib) {
            if (!quiet) {
                printf("DELETE: Failed to allocate memory\n");
            }
            UnLock(lock);
            return FALSE;
        }
        
        if (Examine(lock, fib)) {
            while (ExNext(lock, fib)) {
                char full_path[512];
                strcpy(full_path, path);
                
                /* Add separator if needed */
                int len = strlen(full_path);
                if (len > 0 && full_path[len - 1] != ':' && full_path[len - 1] != '/') {
                    strcat(full_path, "/");
                }
                strcat(full_path, fib->fib_FileName);
                
                if (fib->fib_DirEntryType > 0) {
                    /* Subdirectory - recurse */
                    if (!delete_directory(full_path, TRUE, force, quiet)) {
                        success = FALSE;
                    }
                } else {
                    /* File */
                    if (!delete_single_file(full_path, force, quiet)) {
                        success = FALSE;
                    }
                }
            }
        }
        
        FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
    }
    
    /* Now delete the empty directory */
    result = DeleteFile((STRPTR)path);
    if (result == DOSFALSE) {
        if (!force && !quiet) {
            printf("DELETE: Failed to delete directory '%s' - %ld\n", path, IoErr());
        }
        return FALSE;
    }
    
    if (!quiet) {
        printf("DELETE: Deleted directory '%s'\n", path);
    }
    
    return success;
}

/* Delete a file or directory (handles both) */
static BOOL delete_object(CONST_STRPTR path, BOOL recursive, BOOL force, BOOL quiet)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    
    /* First try to delete as a file */
    if (DeleteFile((STRPTR)path) != DOSFALSE) {
        if (!quiet) {
            printf("DELETE: Deleted '%s'\n", path);
        }
        return TRUE;
    }
    
    /* Failed - check if it's a directory */
    lock = Lock(path, SHARED_LOCK);
    if (!lock) {
        if (!force && !quiet) {
            printf("DELETE: '%s' not found\n", path);
        }
        return FALSE;
    }
    
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return FALSE;
    }
    
    if (!Examine(lock, fib)) {
        UnLock(lock);
        FreeDosObject(DOS_FIB, fib);
        if (!force && !quiet) {
            printf("DELETE: Cannot examine '%s' - %ld\n", path, IoErr());
        }
        return FALSE;
    }
    
    UnLock(lock);
    
    if (fib->fib_DirEntryType <= 0) {
        /* It's a file but we couldn't delete it */
        FreeDosObject(DOS_FIB, fib);
        if (!force && !quiet) {
            printf("DELETE: Failed to delete '%s' - %ld\n", path, IoErr());
        }
        return FALSE;
    }
    
    FreeDosObject(DOS_FIB, fib);
    
    /* It's a directory */
    if (!recursive) {
        if (!force && !quiet) {
            printf("DELETE: '%s' is a directory (use ALL for recursive delete)\n", path);
        }
        return FALSE;
    }
    
    return delete_directory(path, TRUE, force, quiet);
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
            printf("DELETE: FILE argument is required\n");
        } else {
            printf("DELETE: Error parsing arguments - %ld\n", err);
        }
        printf("Usage: DELETE FILE/M/A [ALL] [QUIET] [FORCE]\n");
        printf("Template: %s\n", TEMPLATE);
        return 1;
    }
    
    /* Extract arguments */
    STRPTR *file_array = (STRPTR *)args[ARG_FILE];
    BOOL all_flag = args[ARG_ALL] ? TRUE : FALSE;
    BOOL quiet_flag = args[ARG_QUIET] ? TRUE : FALSE;
    BOOL force_flag = args[ARG_FORCE] ? TRUE : FALSE;
    
    /* Process each file argument */
    if (file_array) {
        for (int i = 0; file_array[i] != NULL; i++) {
            if (!delete_object(file_array[i], all_flag, force_flag, quiet_flag)) {
                errors++;
            }
        }
    }
    
    FreeArgs(rda);
    return errors > 0 ? 1 : 0;
}
