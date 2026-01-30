/*
 * DELETE command - Delete files or directories
 * Phase 7.5 implementation for lxa
 * 
 * Template: FILE/M/A,ALL/S,QUIET/S,FORCE/S
 * 
 * Features:
 *   - Multiple file/directory deletion
 *   - Pattern matching support (#? wildcards)
 *   - Recursive deletion with ALL switch
 *   - QUIET mode to suppress output
 *   - FORCE to override protection and suppress errors
 *   - Ctrl+C break handling
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

#define VERSION "2.0"

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template */
#define TEMPLATE "FILE/M/A,ALL/S,QUIET/S,FORCE/S"

/* Argument array indices */
#define ARG_FILE    0
#define ARG_ALL     1
#define ARG_QUIET   2
#define ARG_FORCE   3
#define ARG_COUNT   4

/* Buffer sizes */
#define PATTERN_BUFFER_SIZE 256
#define MAX_PATH_LEN 512
#define MAX_MATCHES 256

/* Global state for break checking */
static BOOL g_user_break = FALSE;

/* Helper: check for Ctrl+C break */
static BOOL check_break(void)
{
    if (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
        g_user_break = TRUE;
        return TRUE;
    }
    return g_user_break;
}

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

/* Case-insensitive string compare */
static int stricmp_amiga(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return *a - *b;
}

/* Check if filename matches pattern using AmigaDOS wildcards */
static BOOL match_filename(const char *filename, const char *pattern)
{
    if (!pattern || pattern[0] == '\0')
        return TRUE;

    char parsed_pat[PATTERN_BUFFER_SIZE];
    LONG result = ParsePattern((STRPTR)pattern, (STRPTR)parsed_pat, sizeof(parsed_pat));

    if (result == 0) {
        /* No wildcards - literal match */
        return (stricmp_amiga(filename, pattern) == 0);
    }

    if (result < 0) {
        /* Parse error - treat as literal match */
        return (stricmp_amiga(filename, pattern) == 0);
    }

    /* Wildcard pattern */
    return MatchPattern((STRPTR)parsed_pat, (STRPTR)filename);
}

/* Check if pattern contains wildcards */
static BOOL has_wildcards(const char *pattern)
{
    char parsed_pat[PATTERN_BUFFER_SIZE];
    return (ParsePattern((STRPTR)pattern, (STRPTR)parsed_pat, sizeof(parsed_pat)) > 0);
}

/* Delete a single file (no directory check) */
static BOOL delete_single_file(CONST_STRPTR path, BOOL force, BOOL quiet)
{
    LONG result = DeleteFile((STRPTR)path);
    
    if (result == DOSFALSE) {
        LONG err = IoErr();
        if (!force && !quiet) {
            out_str("DELETE: Failed to delete '");
            out_str((char *)path);
            out_str("' - ");
            out_num(err);
            out_nl();
        }
        return FALSE;
    }
    
    if (!quiet) {
        out_str((char *)path);
        out_str("  Deleted\n");
    }
    
    return TRUE;
}

/* Delete a directory and its contents recursively */
static BOOL delete_directory_recursive(CONST_STRPTR path, BOOL force, BOOL quiet)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    BOOL success = TRUE;
    
    /* Check for break */
    if (check_break()) {
        out_str("***BREAK\n");
        return FALSE;
    }
    
    lock = Lock((STRPTR)path, SHARED_LOCK);
    if (!lock) {
        if (!force && !quiet) {
            out_str("DELETE: Cannot access '");
            out_str((char *)path);
            out_str("' - ");
            out_num(IoErr());
            out_nl();
        }
        return FALSE;
    }
    
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        if (!quiet) {
            out_str("DELETE: Failed to allocate memory\n");
        }
        UnLock(lock);
        return FALSE;
    }
    
    if (!Examine(lock, fib)) {
        FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        return FALSE;
    }
    
    /* Delete contents first */
    while (ExNext(lock, fib)) {
        if (check_break()) {
            out_str("***BREAK\n");
            success = FALSE;
            break;
        }
        
        char full_path[MAX_PATH_LEN];
        strncpy(full_path, (char *)path, MAX_PATH_LEN - 2);
        int len = strlen(full_path);
        if (len > 0 && full_path[len - 1] != ':' && full_path[len - 1] != '/') {
            strcat(full_path, "/");
        }
        strncat(full_path, (char *)fib->fib_FileName, MAX_PATH_LEN - strlen(full_path) - 1);
        full_path[MAX_PATH_LEN - 1] = '\0';
        
        if (fib->fib_DirEntryType > 0) {
            /* Subdirectory - recurse */
            if (!delete_directory_recursive((CONST_STRPTR)full_path, force, quiet)) {
                success = FALSE;
                if (!force) break;
            }
        } else {
            /* File */
            if (!delete_single_file((CONST_STRPTR)full_path, force, quiet)) {
                success = FALSE;
                if (!force) break;
            }
        }
    }
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    
    /* Now delete the empty directory */
    if (success || force) {
        LONG result = DeleteFile((STRPTR)path);
        if (result == DOSFALSE) {
            if (!force && !quiet) {
                out_str("DELETE: Failed to delete directory '");
                out_str((char *)path);
                out_str("' - ");
                out_num(IoErr());
                out_nl();
            }
            return FALSE;
        }
        
        if (!quiet) {
            out_str((char *)path);
            out_str(" (dir)  Deleted\n");
        }
    }
    
    return success;
}

/* Delete a file or directory (handles both) */
static BOOL delete_object(CONST_STRPTR path, BOOL recursive, BOOL force, BOOL quiet)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    BOOL is_dir;
    
    /* First try simple delete (works for files) */
    if (DeleteFile((STRPTR)path) != DOSFALSE) {
        if (!quiet) {
            out_str((char *)path);
            out_str("  Deleted\n");
        }
        return TRUE;
    }
    
    /* Failed - check what it is */
    lock = Lock((STRPTR)path, SHARED_LOCK);
    if (!lock) {
        if (!force && !quiet) {
            out_str("DELETE: '");
            out_str((char *)path);
            out_str("' not found\n");
        }
        return FALSE;
    }
    
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return FALSE;
    }
    
    if (!Examine(lock, fib)) {
        FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        if (!force && !quiet) {
            out_str("DELETE: Cannot examine '");
            out_str((char *)path);
            out_str("'\n");
        }
        return FALSE;
    }
    
    is_dir = (fib->fib_DirEntryType > 0);
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    
    if (!is_dir) {
        /* It's a file but we couldn't delete it (e.g., protected) */
        if (!force && !quiet) {
            out_str("DELETE: Failed to delete '");
            out_str((char *)path);
            out_str("' - ");
            out_num(IoErr());
            out_nl();
        }
        return FALSE;
    }
    
    /* It's a directory */
    if (!recursive) {
        if (!force && !quiet) {
            out_str("DELETE: '");
            out_str((char *)path);
            out_str("' is a directory (use ALL for recursive delete)\n");
        }
        return FALSE;
    }
    
    return delete_directory_recursive(path, force, quiet);
}

/* Expand pattern and delete matching files */
static int delete_with_pattern(CONST_STRPTR pattern, BOOL recursive, BOOL force, BOOL quiet)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char dir_path[MAX_PATH_LEN];
    char file_pattern[MAX_PATH_LEN];
    int errors = 0;
    int deleted = 0;
    
    /* Split pattern into directory and file parts */
    const char *p = (const char *)pattern;
    const char *last_sep = NULL;
    
    while (*p) {
        if (*p == '/' || *p == ':') last_sep = p;
        p++;
    }
    
    if (last_sep) {
        /* Has directory component */
        int dir_len = last_sep - (const char *)pattern + 1;
        if (dir_len >= MAX_PATH_LEN) dir_len = MAX_PATH_LEN - 1;
        strncpy(dir_path, (const char *)pattern, dir_len);
        dir_path[dir_len] = '\0';
        strncpy(file_pattern, last_sep + 1, MAX_PATH_LEN - 1);
    } else {
        /* Pattern in current directory */
        dir_path[0] = '\0';
        strncpy(file_pattern, (const char *)pattern, MAX_PATH_LEN - 1);
    }
    file_pattern[MAX_PATH_LEN - 1] = '\0';
    
    /* Lock the directory */
    if (dir_path[0]) {
        lock = Lock((STRPTR)dir_path, SHARED_LOCK);
    } else {
        BPTR cur = CurrentDir(0);
        CurrentDir(cur);
        if (cur) {
            lock = DupLock(cur);
        } else {
            lock = Lock((STRPTR)"SYS:", SHARED_LOCK);
        }
    }
    
    if (!lock) {
        if (!force && !quiet) {
            out_str("DELETE: Cannot access directory '");
            out_str(dir_path[0] ? dir_path : "current");
            out_str("'\n");
        }
        return 1;
    }
    
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return 1;
    }
    
    if (!Examine(lock, fib)) {
        FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        return 1;
    }
    
    /* Find and delete matching files */
    while (ExNext(lock, fib)) {
        if (check_break()) {
            out_str("***BREAK\n");
            break;
        }
        
        if (match_filename((char *)fib->fib_FileName, file_pattern)) {
            /* Build full path */
            char full_path[MAX_PATH_LEN];
            if (dir_path[0]) {
                strncpy(full_path, dir_path, MAX_PATH_LEN - 2);
            } else {
                full_path[0] = '\0';
            }
            int len = strlen(full_path);
            if (len > 0 && full_path[len-1] != ':' && full_path[len-1] != '/') {
                strcat(full_path, "/");
            }
            strncat(full_path, (char *)fib->fib_FileName, MAX_PATH_LEN - strlen(full_path) - 1);
            full_path[MAX_PATH_LEN - 1] = '\0';
            
            /* Delete the matched file/directory */
            if (delete_object((CONST_STRPTR)full_path, recursive, force, quiet)) {
                deleted++;
            } else {
                errors++;
            }
        }
    }
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    
    /* Report if nothing matched */
    if (deleted == 0 && errors == 0 && !g_user_break) {
        if (!quiet) {
            out_str("DELETE: No files matching '");
            out_str((char *)pattern);
            out_str("'\n");
        }
        return 1;
    }
    
    return errors > 0 ? 1 : 0;
}

int main(int argc, char **argv)
{
    LONG args[ARG_COUNT] = {0};
    struct RDArgs *rda;
    int errors = 0;
    
    (void)argc;
    (void)argv;
    
    /* Clear break flag */
    g_user_break = FALSE;
    SetSignal(0L, SIGBREAKF_CTRL_C);
    
    /* Parse arguments using AmigaDOS template */
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        LONG err = IoErr();
        if (err == ERROR_REQUIRED_ARG_MISSING) {
            out_str("DELETE: FILE argument is required\n");
        } else {
            out_str("DELETE: Error parsing arguments - ");
            out_num(err);
            out_nl();
        }
        out_str("Usage: DELETE FILE/M/A [ALL] [QUIET] [FORCE]\n");
        out_str("Template: ");
        out_str(TEMPLATE);
        out_nl();
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
            if (check_break()) {
                out_str("***BREAK\n");
                errors++;
                break;
            }
            
            /* Check if argument contains wildcards */
            if (has_wildcards((char *)file_array[i])) {
                /* Pattern - expand and delete matches */
                if (delete_with_pattern(file_array[i], all_flag, force_flag, quiet_flag) != 0) {
                    errors++;
                }
            } else {
                /* Direct file/directory - delete it */
                if (!delete_object(file_array[i], all_flag, force_flag, quiet_flag)) {
                    errors++;
                }
            }
        }
    }
    
    /* Ensure output is flushed */
    Flush(Output());
    
    FreeArgs(rda);
    return errors > 0 ? 1 : 0;
}
