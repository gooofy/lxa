/*
 * FILENOTE command - Set file comment
 * Phase 9.2 implementation for lxa
 * 
 * Template: FILE/A,COMMENT,ALL/S,QUIET/S
 * 
 * Features:
 *   - Set file comment
 *   - Clear file comment (empty comment)
 *   - Recursive directory processing (ALL)
 *   - Quiet mode (QUIET)
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

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template */
#define TEMPLATE "FILE/A,COMMENT,ALL/S,QUIET/S"

/* Argument array indices */
#define ARG_FILE    0
#define ARG_COMMENT 1
#define ARG_ALL     2
#define ARG_QUIET   3
#define ARG_COUNT   4

/* Buffer sizes */
#define MAX_PATH_LEN 512
#define MAX_COMMENT_LEN 80
#define PATTERN_BUFFER_SIZE 256

/* Global state */
static BOOL g_user_break = FALSE;
static BOOL g_quiet = FALSE;
static int g_files_processed = 0;
static int g_errors = 0;

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

/* Helper: output number */
static void out_num(LONG num)
{
    char buf[16];
    char *p = buf + 15;
    *p = '\0';
    BOOL neg = (num < 0);
    unsigned long n = neg ? -num : num;
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n);
    if (neg) *--p = '-';
    out_str(p);
}

/* stricmp is provided by string.h */

/* Process a single file */
static int process_file(const char *path, const char *comment)
{
    if (check_break()) {
        return 1;
    }
    
    /* Set the comment */
    if (!SetComment((STRPTR)path, (STRPTR)(comment ? comment : ""))) {
        if (!g_quiet) {
            out_str("FILENOTE: Can't set comment on '");
            out_str(path);
            out_str("' - error ");
            out_num(IoErr());
            out_nl();
        }
        g_errors++;
        return 1;
    }
    
    /* Display result if not quiet */
    if (!g_quiet) {
        out_str("  ");
        out_str(path);
        if (comment && comment[0]) {
            out_str(": ");
            out_str(comment);
        } else {
            out_str(": (comment cleared)");
        }
        out_nl();
    }
    
    g_files_processed++;
    return 0;
}

/* Process directory recursively */
static int process_directory(const char *path, const char *pattern, const char *comment)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char subpath[MAX_PATH_LEN];
    char parsed_pattern[PATTERN_BUFFER_SIZE];
    LONG pattern_type;
    
    if (check_break()) {
        return 1;
    }
    
    /* Parse the pattern */
    pattern_type = ParsePattern((STRPTR)pattern, (STRPTR)parsed_pattern, PATTERN_BUFFER_SIZE);
    
    lock = Lock((STRPTR)path, SHARED_LOCK);
    if (!lock) {
        if (!g_quiet) {
            out_str("FILENOTE: Can't access directory '");
            out_str(path);
            out_str("'\n");
        }
        g_errors++;
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
    
    while (ExNext(lock, fib)) {
        if (check_break()) {
            out_str("***BREAK\n");
            break;
        }
        
        /* Build full path */
        strncpy(subpath, path, MAX_PATH_LEN - 2);
        int len = strlen(subpath);
        if (len > 0 && subpath[len-1] != ':' && subpath[len-1] != '/') {
            strcat(subpath, "/");
        }
        strncat(subpath, (char *)fib->fib_FileName, MAX_PATH_LEN - strlen(subpath) - 1);
        subpath[MAX_PATH_LEN - 1] = '\0';
        
        if (fib->fib_DirEntryType > 0) {
            /* Directory - recurse */
            process_directory(subpath, pattern, comment);
        } else {
            /* File - check pattern match */
            BOOL matches = FALSE;
            if (pattern_type == 0) {
                /* Literal pattern */
                matches = (stricmp((char *)fib->fib_FileName, pattern) == 0);
            } else if (pattern_type > 0) {
                /* Wildcard pattern */
                matches = MatchPattern((STRPTR)parsed_pattern, fib->fib_FileName);
            } else {
                /* Pattern parse error - match all */
                matches = TRUE;
            }
            
            if (matches) {
                process_file(subpath, comment);
            }
        }
    }
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    
    return 0;
}

/* Show current comment for a file */
static void show_comment(const char *path)
{
    BPTR lock = Lock((STRPTR)path, SHARED_LOCK);
    if (!lock) {
        out_str("FILENOTE: Can't access '");
        out_str(path);
        out_str("'\n");
        return;
    }
    
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    if (fib) {
        if (Examine(lock, fib)) {
            out_str("  ");
            out_str(path);
            out_str(": ");
            if (fib->fib_Comment[0]) {
                out_str((char *)fib->fib_Comment);
            } else {
                out_str("(no comment)");
            }
            out_nl();
        }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
}

int main(int argc, char **argv)
{
    LONG args[ARG_COUNT] = {0};
    struct RDArgs *rda;
    int result = 0;
    
    (void)argc;
    (void)argv;
    
    /* Clear state */
    g_user_break = FALSE;
    g_quiet = FALSE;
    g_files_processed = 0;
    g_errors = 0;
    SetSignal(0L, SIGBREAKF_CTRL_C);
    
    /* Parse arguments */
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        out_str("FILENOTE: Bad arguments\n");
        out_str("Usage: FILENOTE <file|pattern> [COMMENT] [ALL] [QUIET]\n");
        return 20;
    }
    
    const char *file_arg = (const char *)args[ARG_FILE];
    const char *comment = (const char *)args[ARG_COMMENT];
    BOOL all_mode = args[ARG_ALL] ? TRUE : FALSE;
    g_quiet = args[ARG_QUIET] ? TRUE : FALSE;
    
    /* If no comment specified, show current comment */
    if (!comment) {
        show_comment(file_arg);
        FreeArgs(rda);
        return 0;
    }
    
    /* Validate comment length */
    if (strlen(comment) > MAX_COMMENT_LEN - 1) {
        out_str("FILENOTE: Comment too long (max ");
        out_num(MAX_COMMENT_LEN - 1);
        out_str(" chars)\n");
        FreeArgs(rda);
        return 20;
    }
    
    /* Check if file_arg is a directory or contains wildcards */
    BPTR test_lock = Lock((STRPTR)file_arg, SHARED_LOCK);
    if (test_lock) {
        struct FileInfoBlock *test_fib = AllocDosObject(DOS_FIB, NULL);
        if (test_fib && Examine(test_lock, test_fib)) {
            if (test_fib->fib_DirEntryType > 0) {
                /* It's a directory */
                if (all_mode) {
                    /* Process all files in directory recursively */
                    FreeDosObject(DOS_FIB, test_fib);
                    UnLock(test_lock);
                    result = process_directory(file_arg, "#?", comment);
                } else {
                    /* Just process the directory itself */
                    FreeDosObject(DOS_FIB, test_fib);
                    UnLock(test_lock);
                    result = process_file(file_arg, comment);
                }
            } else {
                /* It's a file */
                FreeDosObject(DOS_FIB, test_fib);
                UnLock(test_lock);
                result = process_file(file_arg, comment);
            }
        } else {
            if (test_fib) FreeDosObject(DOS_FIB, test_fib);
            UnLock(test_lock);
            result = 10;
        }
    } else {
        /* Doesn't exist as-is - might be a pattern */
        const char *p = file_arg;
        const char *last_sep = NULL;
        while (*p) {
            if (*p == '/' || *p == ':') last_sep = p;
            p++;
        }
        
        char dir_path[MAX_PATH_LEN];
        const char *pattern;
        
        if (last_sep) {
            int dir_len = last_sep - file_arg + 1;
            strncpy(dir_path, file_arg, dir_len);
            dir_path[dir_len] = '\0';
            pattern = last_sep + 1;
        } else {
            strcpy(dir_path, "");
            pattern = file_arg;
        }
        
        /* Check if pattern contains wildcards */
        char parsed_pattern[PATTERN_BUFFER_SIZE];
        LONG pattern_type = ParsePattern((STRPTR)pattern, (STRPTR)parsed_pattern, PATTERN_BUFFER_SIZE);
        
        if (pattern_type > 0) {
            /* Has wildcards - process matching files */
            BPTR dir_lock;
            if (dir_path[0]) {
                dir_lock = Lock((STRPTR)dir_path, SHARED_LOCK);
            } else {
                BPTR cur = CurrentDir(0);
                CurrentDir(cur);
                dir_lock = cur ? DupLock(cur) : Lock((STRPTR)"", SHARED_LOCK);
            }
            
            if (dir_lock) {
                struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
                if (fib && Examine(dir_lock, fib)) {
                    while (ExNext(dir_lock, fib)) {
                        if (check_break()) {
                            out_str("***BREAK\n");
                            break;
                        }
                        
                        if (MatchPattern((STRPTR)parsed_pattern, fib->fib_FileName)) {
                            char full_path[MAX_PATH_LEN];
                            if (dir_path[0]) {
                                strncpy(full_path, dir_path, MAX_PATH_LEN - 2);
                                strncat(full_path, (char *)fib->fib_FileName, 
                                        MAX_PATH_LEN - strlen(full_path) - 1);
                            } else {
                                strncpy(full_path, (char *)fib->fib_FileName, MAX_PATH_LEN - 1);
                            }
                            full_path[MAX_PATH_LEN - 1] = '\0';
                            
                            if (fib->fib_DirEntryType > 0 && all_mode) {
                                process_directory(full_path, "#?", comment);
                            } else {
                                process_file(full_path, comment);
                            }
                        }
                    }
                }
                if (fib) FreeDosObject(DOS_FIB, fib);
                UnLock(dir_lock);
            } else {
                out_str("FILENOTE: Can't access directory\n");
                result = 20;
            }
        } else {
            out_str("FILENOTE: Can't find '");
            out_str(file_arg);
            out_str("'\n");
            result = 20;
        }
    }
    
    /* Summary if not quiet */
    if (!g_quiet && g_files_processed > 0) {
        out_str("FILENOTE: ");
        out_num(g_files_processed);
        out_str(" file(s) processed");
        if (g_errors > 0) {
            out_str(", ");
            out_num(g_errors);
            out_str(" error(s)");
        }
        out_nl();
    }
    
    FreeArgs(rda);
    return result;
}
