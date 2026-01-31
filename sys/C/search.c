/*
 * SEARCH command - Search for text in files
 * Step 9.3 implementation for lxa
 * 
 * Template: FROM/M/A,SEARCH/A,ALL/S,NONUM/S,QUIET/S,QUICK/S,FILE/S,PATTERN/S
 * 
 * Features:
 *   - Search for text string in one or more files
 *   - ALL: Recursive directory search
 *   - NONUM: Don't show line numbers
 *   - QUIET: Only show filenames with matches
 *   - QUICK: Stop after first match in each file
 *   - FILE: Show only filenames containing matches
 *   - PATTERN: Use AmigaDOS pattern matching for search string
 *   - Ctrl+C break handling
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
#define TEMPLATE "FROM/M/A,SEARCH/A,ALL/S,NONUM/S,QUIET/S,QUICK/S,FILE/S,PATTERN/S"

/* Argument array indices */
#define ARG_FROM    0
#define ARG_SEARCH  1
#define ARG_ALL     2
#define ARG_NONUM   3
#define ARG_QUIET   4
#define ARG_QUICK   5
#define ARG_FILE    6
#define ARG_PATTERN 7
#define ARG_COUNT   8

/* Buffer sizes */
#define LINE_BUFFER_SIZE 512
#define PATH_BUFFER_SIZE 256

/* Global state */
static BOOL g_user_break = FALSE;
static LONG g_total_matches = 0;
static LONG g_files_with_matches = 0;

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
    BOOL neg = num < 0;
    
    *p = '\0';
    if (neg) num = -num;
    
    do {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    
    if (neg) *--p = '-';
    out_str(p);
}

/* Case-insensitive string search */
static const char *stristr(const char *haystack, const char *needle)
{
    if (!*needle) return haystack;
    
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        
        while (*h && *n) {
            char hc = *h;
            char nc = *n;
            
            /* Convert to lowercase */
            if (hc >= 'A' && hc <= 'Z') hc += 32;
            if (nc >= 'A' && nc <= 'Z') nc += 32;
            
            if (hc != nc) break;
            h++;
            n++;
        }
        
        if (!*n) return haystack;
    }
    
    return NULL;
}

/* Read a line from file (up to maxlen-1 chars, null terminated) */
static LONG read_line(BPTR fh, char *buf, LONG maxlen)
{
    LONG i = 0;
    char c;
    
    while (i < maxlen - 1) {
        if (Read(fh, &c, 1) != 1) {
            break;
        }
        if (c == '\n') {
            break;
        }
        buf[i++] = c;
    }
    
    buf[i] = '\0';
    return i;
}

/* Search a single file */
static BOOL search_file(const char *filename, const char *search_str,
                        BOOL show_nums, BOOL quiet, BOOL quick, BOOL file_only,
                        BOOL use_pattern, STRPTR pattern_buf)
{
    BPTR fh;
    char line[LINE_BUFFER_SIZE];
    LONG line_num = 0;
    BOOL found_any = FALSE;
    BOOL first_match = TRUE;
    
    fh = Open((STRPTR)filename, MODE_OLDFILE);
    if (!fh) {
        if (!quiet) {
            out_str("Can't open ");
            out_str(filename);
            out_nl();
        }
        return FALSE;
    }
    
    while (!check_break()) {
        LONG len = read_line(fh, line, sizeof(line));
        if (len == 0 && Seek(fh, 0, OFFSET_CURRENT) == Seek(fh, 0, OFFSET_END)) {
            /* End of file */
            break;
        }
        
        line_num++;
        
        /* Check for match */
        BOOL match = FALSE;
        
        if (use_pattern && pattern_buf) {
            /* Use AmigaDOS pattern matching */
            match = MatchPatternNoCase(pattern_buf, (STRPTR)line);
        } else {
            /* Simple case-insensitive substring search */
            match = (stristr(line, search_str) != NULL);
        }
        
        if (match) {
            found_any = TRUE;
            g_total_matches++;
            
            if (file_only) {
                /* FILE mode: just print filename once and stop */
                out_str(filename);
                out_nl();
                break;
            }
            
            if (quiet) {
                /* QUIET mode: just count, don't print */
            } else {
                /* Normal output */
                if (first_match) {
                    /* Print filename header for first match */
                    out_str(filename);
                    out_nl();
                    first_match = FALSE;
                }
                
                if (show_nums) {
                    out_str("    ");
                    out_num(line_num);
                    out_str(": ");
                }
                out_str(line);
                out_nl();
            }
            
            if (quick) {
                /* QUICK mode: stop after first match */
                break;
            }
        }
    }
    
    Close(fh);
    
    if (found_any) {
        g_files_with_matches++;
    }
    
    return found_any;
}

/* Process a directory recursively */
static void search_directory(const char *dirname, const char *search_str,
                             BOOL recursive, BOOL show_nums, BOOL quiet,
                             BOOL quick, BOOL file_only, BOOL use_pattern,
                             STRPTR pattern_buf)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char path[PATH_BUFFER_SIZE];
    
    lock = Lock((STRPTR)dirname, SHARED_LOCK);
    if (!lock) {
        if (!quiet) {
            out_str("Can't lock ");
            out_str(dirname);
            out_nl();
        }
        return;
    }
    
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return;
    }
    
    if (Examine(lock, fib)) {
        if (fib->fib_DirEntryType > 0) {
            /* It's a directory - enumerate contents */
            while (ExNext(lock, fib) && !check_break()) {
                /* Build full path */
                LONG dlen = strlen(dirname);
                LONG nlen = strlen(fib->fib_FileName);
                
                if (dlen + nlen + 2 < PATH_BUFFER_SIZE) {
                    strcpy(path, dirname);
                    if (dlen > 0 && dirname[dlen-1] != ':' && dirname[dlen-1] != '/') {
                        strcat(path, "/");
                    }
                    strcat(path, fib->fib_FileName);
                    
                    if (fib->fib_DirEntryType > 0) {
                        /* Subdirectory */
                        if (recursive) {
                            search_directory(path, search_str, recursive,
                                           show_nums, quiet, quick, file_only,
                                           use_pattern, pattern_buf);
                        }
                    } else {
                        /* Regular file */
                        search_file(path, search_str, show_nums, quiet, quick,
                                  file_only, use_pattern, pattern_buf);
                    }
                }
            }
        } else {
            /* It's a file - search it directly */
            search_file(dirname, search_str, show_nums, quiet, quick,
                       file_only, use_pattern, pattern_buf);
        }
    }
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
}

/* Main entry point */
int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    STRPTR *from_files;
    STRPTR search_str;
    BOOL recursive, show_nums, quiet, quick, file_only, use_pattern;
    STRPTR pattern_buf = NULL;
    int rc = RETURN_OK;
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"SEARCH");
        return RETURN_FAIL;
    }
    
    from_files = (STRPTR *)args[ARG_FROM];
    search_str = (STRPTR)args[ARG_SEARCH];
    recursive = args[ARG_ALL] != 0;
    show_nums = args[ARG_NONUM] == 0;  /* Show numbers unless NONUM is set */
    quiet = args[ARG_QUIET] != 0;
    quick = args[ARG_QUICK] != 0;
    file_only = args[ARG_FILE] != 0;
    use_pattern = args[ARG_PATTERN] != 0;
    
    /* If PATTERN mode, parse the search pattern */
    if (use_pattern) {
        LONG pat_len = strlen((char *)search_str) * 2 + 2;
        pattern_buf = AllocVec(pat_len, MEMF_CLEAR);
        if (pattern_buf) {
            if (ParsePatternNoCase(search_str, pattern_buf, pat_len) < 0) {
                out_str("Invalid search pattern\n");
                FreeVec(pattern_buf);
                FreeArgs(rdargs);
                return RETURN_ERROR;
            }
        } else {
            out_str("Out of memory\n");
            FreeArgs(rdargs);
            return RETURN_FAIL;
        }
    }
    
    /* Process each file/directory argument */
    while (*from_files && !check_break()) {
        STRPTR path = *from_files++;
        BPTR lock;
        struct FileInfoBlock *fib;
        
        /* Check if it's a file or directory */
        lock = Lock(path, SHARED_LOCK);
        if (lock) {
            fib = AllocDosObject(DOS_FIB, NULL);
            if (fib) {
                if (Examine(lock, fib)) {
                    if (fib->fib_DirEntryType > 0) {
                        /* Directory */
                        search_directory((char *)path, (char *)search_str,
                                       recursive, show_nums, quiet, quick,
                                       file_only, use_pattern, pattern_buf);
                    } else {
                        /* Single file */
                        search_file((char *)path, (char *)search_str,
                                  show_nums, quiet, quick, file_only,
                                  use_pattern, pattern_buf);
                    }
                }
                FreeDosObject(DOS_FIB, fib);
            }
            UnLock(lock);
        } else {
            if (!quiet) {
                out_str("Can't find ");
                out_str((char *)path);
                out_nl();
            }
            rc = RETURN_WARN;
        }
    }
    
    /* Show summary if not quiet and not FILE mode */
    if (!quiet && !file_only && g_total_matches > 0) {
        out_nl();
        out_num(g_total_matches);
        out_str(" match");
        if (g_total_matches != 1) out_str("es");
        out_str(" found in ");
        out_num(g_files_with_matches);
        out_str(" file");
        if (g_files_with_matches != 1) out_str("s");
        out_str(".");
        out_nl();
    }
    
    if (check_break()) {
        out_str("***Break\n");
        rc = RETURN_WARN;
    }
    
    /* Cleanup */
    if (pattern_buf) {
        FreeVec(pattern_buf);
    }
    FreeArgs(rdargs);
    
    return rc;
}
