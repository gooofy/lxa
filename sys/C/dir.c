/*
 * DIR command - List directory contents
 * Phase 4 implementation for lxa
 * 
 * Template: DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <string.h>

#define VERSION "1.0"

/* External reference to DOS library base */
extern struct DosLibrary *DOSBase;

/* Default pattern to match all files */
#define DEFAULT_PATTERN "#?"

/* Buffer for pattern matching */
#define PATTERN_BUFFER_SIZE 256

/* Command template */
#define TEMPLATE "DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S"

/* Argument array indices */
#define ARG_DIR     0
#define ARG_OPT     1
#define ARG_ALL     2
#define ARG_DIRS    3
#define ARG_FILES   4
#define ARG_INTER   5
#define ARG_COUNT   6

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
    int neg = (num < 0);
    unsigned long n = neg ? -num : num;
    
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n);
    
    if (neg) *--p = '-';
    out_str(p);
}

/* Format size for human-readable output */
static void format_size(LONG size, char *buf, int buf_len)
{
    char *p = buf + buf_len - 1;
    *p = '\0';
    
    if (size < 1024) {
        unsigned long n = size;
        do {
            *--p = '0' + (n % 10);
            n /= 10;
        } while (n && p > buf);
    } else if (size < 1024 * 1024) {
        unsigned long n = size / 1024;
        *--p = 'K';
        do {
            *--p = '0' + (n % 10);
            n /= 10;
        } while (n && p > buf);
    } else {
        unsigned long n = size / (1024 * 1024);
        *--p = 'M';
        do {
            *--p = '0' + (n % 10);
            n /= 10;
        } while (n && p > buf);
    }
    
    /* Move to start of buffer if not already there */
    if (p > buf) {
        char *d = buf;
        while (*p) *d++ = *p++;
        *d = '\0';
    }
}

/* Print protection bits */
static void print_protection(LONG protection)
{
    /* Amiga protection bits are inverted: 0 = protected, 1 = allowed */
    /* HSPARWED (Hold Script Pure Archive Read Write Execute Delete) */
    char bits[8] = "-------";
    
    if (!(protection & (1 << 0))) bits[6] = 'r';  /* Delete -> read? */
    if (!(protection & (1 << 1))) bits[5] = 'w';  /* Execute? */
    if (!(protection & (1 << 2))) bits[4] = 'e';  /* Write -> execute? */
    if (!(protection & (1 << 3))) bits[3] = 'd';  /* Read -> delete? */
    if (!(protection & (1 << 4))) bits[0] = 'h';  /* Archive -> hold? */
    if (!(protection & (1 << 5))) bits[2] = 'p';  /* Pure */
    if (!(protection & (1 << 6))) bits[1] = 's';  /* Script */
    
    out_str(bits);
}

/* Check if filename matches pattern using AmigaDOS wildcards */
static BOOL match_filename(const char *filename, const char *pattern)
{
    if (!pattern || pattern[0] == '\0')
        return TRUE;

    char parsed_pat[PATTERN_BUFFER_SIZE];
    LONG result = ParsePattern((STRPTR)pattern, (STRPTR)parsed_pat, sizeof(parsed_pat));

    if (result == 0) {
        /* Parse error - treat as literal match */
        return (strcmp(filename, pattern) == 0);
    }

    if (result < 0) {
        /* Literal string - exact match */
        return (strcmp(filename, pattern) == 0);
    }

    /* Wildcard pattern */
    return MatchPattern((STRPTR)parsed_pat, (STRPTR)filename);
}

/* List a single directory entry */
static void list_entry(struct FileInfoBlock *fib, BOOL detailed)
{
    if (detailed) {
        /* Detailed listing: protection, size, comment, name */
        print_protection(fib->fib_Protection);
        
        if (fib->fib_DirEntryType > 0) {
            /* Directory */
            out_str("    <DIR> ");
        } else {
            /* File */
            char size_str[16];
            format_size(fib->fib_Size, size_str, sizeof(size_str));
            out_str(" ");
            /* Pad to 8 chars */
            int len = strlen(size_str);
            while (len < 8) { out_str(" "); len++; }
            out_str(size_str);
            out_str(" ");
        }
        
        out_str((char *)fib->fib_FileName);
        
        if (fib->fib_Comment[0]) {
            out_str(" ; ");
            out_str((char *)fib->fib_Comment);
        }
        
        out_nl();
    } else {
        /* Simple listing: just names */
        out_str((char *)fib->fib_FileName);
        if (fib->fib_DirEntryType > 0) {
            out_str("/");
        }
        out_nl();
    }
}

/* List directory contents with pattern matching */
static int list_directory(CONST_STRPTR path, CONST_STRPTR pattern, BOOL detailed, 
                          BOOL show_all, BOOL dirs_only, BOOL files_only)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    int count = 0;
    int dir_count = 0;
    LONG total_size = 0;
    
    /* Allocate FileInfoBlock */
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        out_str("DIR: Failed to allocate FileInfoBlock\n");
        return 1;
    }
    
    /* Lock the directory - use current directory if path is empty */
    if (path && path[0] != '\0') {
        lock = Lock((STRPTR)path, SHARED_LOCK);
    } else {
        /* Get current directory lock - use a dummy lock to retrieve it */
        BPTR cur = CurrentDir(0);
        CurrentDir(cur);  /* Restore immediately */
        if (cur) {
            lock = DupLock(cur);
        } else {
            /* No current directory set - try SYS: */
            lock = Lock((STRPTR)"SYS:", SHARED_LOCK);
        }
    }
    if (!lock) {
        out_str("DIR: Can't examine '");
        out_str((path && path[0]) ? (char *)path : "current directory");
        out_str("' - ");
        out_num(IoErr());
        out_nl();
        FreeDosObject(DOS_FIB, fib);
        return 1;
    }

    /* Check if it's a directory */
    if (!Examine(lock, fib)) {
        out_str("DIR: Can't examine '");
        out_str((path && path[0]) ? (char *)path : "current directory");
        out_str("' - ");
        out_num(IoErr());
        out_nl();
        UnLock(lock);
        FreeDosObject(DOS_FIB, fib);
        return 1;
    }
    
    if (fib->fib_DirEntryType <= 0) {
        /* It's a file, not a directory */
        if (match_filename((char *)fib->fib_FileName, (char *)pattern)) {
            list_entry(fib, detailed);
        }
        count++;
        total_size += fib->fib_Size;
    } else {
        /* It's a directory - list contents */
        if (detailed) {
            out_str("Directory '");
            out_str((path && path[0]) ? (char *)path : (char *)fib->fib_FileName);
            out_str("':\n\n");
        }
        
        while (ExNext(lock, fib)) {
            /* Skip hidden files unless ALL switch */
            if (!show_all && fib->fib_FileName[0] == '.') {
                continue;
            }
            
            /* Check pattern match */
            if (!match_filename((char *)fib->fib_FileName, (char *)pattern)) {
                continue;
            }
            
            /* Filter by DIRS/FILES switches */
            if (dirs_only && fib->fib_DirEntryType <= 0) {
                continue;
            }
            if (files_only && fib->fib_DirEntryType > 0) {
                continue;
            }
            
            list_entry(fib, detailed);
            count++;
            
            if (fib->fib_DirEntryType > 0) {
                dir_count++;
            } else {
                total_size += fib->fib_Size;
            }
        }
        
        /* Check for error vs end of directory */
        if (IoErr() != ERROR_NO_MORE_ENTRIES) {
            out_str("DIR: Error reading directory - ");
            out_num(IoErr());
            out_nl();
        }

        if (detailed) {
            out_nl();
            out_num(count - dir_count);
            out_str(" file(s)  ");
            out_num(total_size);
            out_str(" bytes\n");
            out_num(dir_count);
            out_str(" dir(s)\n");
        }
    }
    
    UnLock(lock);
    FreeDosObject(DOS_FIB, fib);
    
    /* Ensure output is flushed before returning */
    Flush(Output());
    
    return 0;
}

int main(int argc, char **argv)
{
    LONG args[ARG_COUNT] = {0};
    struct RDArgs *rda;
    int result;
    
    (void)argc;  /* Unused - we use ReadArgs instead */
    (void)argv;
    
    /* Parse arguments using AmigaDOS template */
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        LONG err = IoErr();
        if (err == ERROR_REQUIRED_ARG_MISSING) {
            out_str("DIR: Required argument missing\n");
        } else {
            out_str("DIR: Error parsing arguments - ");
            out_num(err);
            out_nl();
        }
        out_str("Usage: DIR [pattern] [OPT keywords] [ALL] [DIRS] [FILES] [INTER]\n");
        out_str("Template: ");
        out_str(TEMPLATE);
        out_nl();
        return 1;
    }
    
    /* Extract arguments */
    CONST_STRPTR pattern = (CONST_STRPTR)args[ARG_DIR];
    CONST_STRPTR opt_str = (CONST_STRPTR)args[ARG_OPT];
    BOOL all_flag = args[ARG_ALL] ? TRUE : FALSE;
    BOOL dirs_flag = args[ARG_DIRS] ? TRUE : FALSE;
    BOOL files_flag = args[ARG_FILES] ? TRUE : FALSE;
    BOOL inter_flag = args[ARG_INTER] ? TRUE : FALSE;
    
    /* Use default pattern if none specified */
    if (!pattern || pattern[0] == '\0') {
        pattern = (CONST_STRPTR)DEFAULT_PATTERN;
    }

    /* Handle OPT keyword for traditional options */
    BOOL detailed = FALSE;
    if (opt_str) {
        /* Parse OPT string for legacy compatibility */
        if (strstr((char *)opt_str, "L") || strstr((char *)opt_str, "l")) {
            detailed = TRUE;
        }
    }
    
    /* Interactive mode not yet implemented */
    if (inter_flag) {
        out_str("DIR: Interactive mode not yet implemented\n");
    }
    
    /* Determine the path to list - use current directory if not specified */
    CONST_STRPTR path = pattern;
    CONST_STRPTR file_pattern = NULL;
    
    /* Check if pattern contains a path component */
    if (pattern && pattern[0] != '\0') {
        /* Check if it's a directory */
        BPTR test_lock = Lock((STRPTR)pattern, SHARED_LOCK);
        if (test_lock) {
            struct FileInfoBlock *test_fib = AllocDosObject(DOS_FIB, NULL);
            if (test_fib) {
                if (Examine(test_lock, test_fib) && test_fib->fib_DirEntryType > 0) {
                    /* It's a directory - list its contents */
                    path = pattern;
                    file_pattern = (CONST_STRPTR)DEFAULT_PATTERN;
                } else {
                    /* It's a file or pattern - use current dir */
                    path = (CONST_STRPTR)"";
                    file_pattern = pattern;
                }
                FreeDosObject(DOS_FIB, test_fib);
            }
            UnLock(test_lock);
        } else {
            /* Not found as-is - treat as pattern in current directory */
            path = (CONST_STRPTR)"";
            file_pattern = pattern;
        }
    } else {
        /* No pattern - list current directory */
        path = (CONST_STRPTR)"";
        file_pattern = (CONST_STRPTR)DEFAULT_PATTERN;
    }
    
    /* List the directory */
    result = list_directory(path, file_pattern, detailed, all_flag, dirs_flag, files_flag);
    
    FreeArgs(rda);
    return result;
}
