/*
 * RENAME command - Rename or move files
 * Phase 9.1 implementation for lxa
 * 
 * Template: FROM/A,TO/A,QUIET/S
 * 
 * Features:
 *   - Rename file or directory
 *   - Move file to different directory (same volume)
 *   - QUIET mode to suppress output
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
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
#define TEMPLATE "FROM/A,TO/A,QUIET/S"

/* Argument array indices */
#define ARG_FROM    0
#define ARG_TO      1
#define ARG_QUIET   2
#define ARG_COUNT   3

/* Buffer sizes */
#define MAX_PATH_LEN 512

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

/* Helper: output a number */
static void out_num(LONG num)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    unsigned long n = (num < 0) ? -num : num;
    
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n);
    
    if (num < 0) *--p = '-';
    out_str(p);
}

/* Check if path is a directory */
static BOOL is_directory(CONST_STRPTR path)
{
    BPTR lock = Lock((STRPTR)path, SHARED_LOCK);
    if (!lock) return FALSE;
    
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return FALSE;
    }
    
    BOOL is_dir = FALSE;
    if (Examine(lock, fib)) {
        is_dir = (fib->fib_DirEntryType > 0);
    }
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return is_dir;
}

/* Get filename from path */
static const char *get_filename(const char *path)
{
    const char *p = path;
    const char *last = path;
    
    while (*p) {
        if (*p == '/' || *p == ':') {
            last = p + 1;
        }
        p++;
    }
    return last;
}

/* Build destination path when dest is a directory */
static void build_dest_path(char *dest, int dest_size, const char *dest_dir, const char *filename)
{
    strncpy(dest, dest_dir, dest_size - 1);
    dest[dest_size - 1] = '\0';
    
    int len = strlen(dest);
    if (len > 0 && dest[len - 1] != ':' && dest[len - 1] != '/') {
        if (len < dest_size - 1) {
            dest[len] = '/';
            dest[len + 1] = '\0';
            len++;
        }
    }
    
    strncat(dest, filename, dest_size - len - 1);
}

int main(int argc, char **argv)
{
    LONG args[ARG_COUNT] = {0};
    struct RDArgs *rda;
    BOOL quiet;
    LONG result;
    
    (void)argc;
    (void)argv;
    
    /* Parse arguments using AmigaDOS template */
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        LONG err = IoErr();
        if (err == ERROR_REQUIRED_ARG_MISSING) {
            out_str("RENAME: FROM and TO arguments are required\n");
        } else {
            out_str("RENAME: Error parsing arguments - ");
            out_num(err);
            out_nl();
        }
        out_str("Usage: RENAME FROM/A TO/A [QUIET]\n");
        out_str("Template: ");
        out_str(TEMPLATE);
        out_nl();
        return 1;
    }
    
    /* Extract arguments */
    CONST_STRPTR from_path = (CONST_STRPTR)args[ARG_FROM];
    CONST_STRPTR to_path = (CONST_STRPTR)args[ARG_TO];
    quiet = args[ARG_QUIET] ? TRUE : FALSE;
    
    /* Check if source exists */
    BPTR check_lock = Lock((STRPTR)from_path, SHARED_LOCK);
    if (!check_lock) {
        if (!quiet) {
            out_str("RENAME: '");
            out_str((char *)from_path);
            out_str("' not found\n");
        }
        FreeArgs(rda);
        return 1;
    }
    UnLock(check_lock);
    
    /* Determine final destination path */
    char final_dest[MAX_PATH_LEN];
    
    if (is_directory(to_path)) {
        /* Destination is an existing directory - move file into it */
        build_dest_path(final_dest, MAX_PATH_LEN, (char *)to_path, get_filename((char *)from_path));
    } else {
        /* Destination is a new name or path */
        strncpy(final_dest, (char *)to_path, MAX_PATH_LEN - 1);
        final_dest[MAX_PATH_LEN - 1] = '\0';
    }
    
    /* Check if destination already exists */
    check_lock = Lock((STRPTR)final_dest, SHARED_LOCK);
    if (check_lock) {
        UnLock(check_lock);
        if (!quiet) {
            out_str("RENAME: '");
            out_str(final_dest);
            out_str("' already exists\n");
        }
        FreeArgs(rda);
        return 1;
    }
    
    /* Perform the rename */
    result = Rename((STRPTR)from_path, (STRPTR)final_dest);
    
    if (result == DOSFALSE) {
        LONG err = IoErr();
        if (!quiet) {
            out_str("RENAME: Failed to rename '");
            out_str((char *)from_path);
            out_str("' to '");
            out_str(final_dest);
            out_str("' - ");
            
            /* Provide human-readable error */
            switch (err) {
                case ERROR_OBJECT_NOT_FOUND:
                    out_str("Object not found");
                    break;
                case ERROR_OBJECT_EXISTS:
                    out_str("Object already exists");
                    break;
                case ERROR_RENAME_ACROSS_DEVICES:
                    out_str("Cannot rename across devices");
                    break;
                case ERROR_DIRECTORY_NOT_EMPTY:
                    out_str("Directory not empty");
                    break;
                case ERROR_WRITE_PROTECTED:
                    out_str("Write protected");
                    break;
                case ERROR_DELETE_PROTECTED:
                    out_str("Delete protected");
                    break;
                default:
                    out_str("Error ");
                    out_num(err);
                    break;
            }
            out_nl();
        }
        FreeArgs(rda);
        return 1;
    }
    
    if (!quiet) {
        out_str("Renamed '");
        out_str((char *)from_path);
        out_str("' to '");
        out_str(final_dest);
        out_str("'\n");
    }
    
    /* Ensure output is flushed */
    Flush(Output());
    
    FreeArgs(rda);
    return 0;
}
