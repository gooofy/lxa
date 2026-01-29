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
#include <stdio.h>

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

/* Format size for human-readable output */
static void format_size(LONG size, char *buf, int buf_len)
{
    if (size < 1024) {
        snprintf(buf, buf_len, "%d", size);
    } else if (size < 1024 * 1024) {
        snprintf(buf, buf_len, "%dK", size / 1024);
    } else {
        snprintf(buf, buf_len, "%dM", size / (1024 * 1024));
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
    
    printf("%s", bits);
}

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

/* List a single directory entry */
static void list_entry(struct FileInfoBlock *fib, BOOL detailed)
{
    if (detailed) {
        /* Detailed listing: protection, size, comment, name */
        print_protection(fib->fib_Protection);
        
        if (fib->fib_DirEntryType > 0) {
            /* Directory */
            printf(" %8s ", "<DIR>");
        } else {
            /* File */
            char size_str[16];
            format_size(fib->fib_Size, size_str, sizeof(size_str));
            printf(" %8s ", size_str);
        }
        
        printf("%-20s", fib->fib_FileName);
        
        if (fib->fib_Comment[0]) {
            printf(" ; %s", fib->fib_Comment);
        }
        
        printf("\n");
    } else {
        /* Simple listing: just names */
        printf("%s", fib->fib_FileName);
        if (fib->fib_DirEntryType > 0) {
            printf("/");
        }
        printf("\n");
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
        printf("DIR: Failed to allocate FileInfoBlock\n");
        return 1;
    }
    
    /* Lock the directory */
    lock = Lock(path, SHARED_LOCK);
    if (!lock) {
        printf("DIR: Can't examine '%s' - %ld\n", path, IoErr());
        FreeDosObject(DOS_FIB, fib);
        return 1;
    }
    
    /* Check if it's a directory */
    if (!Examine(lock, fib)) {
        printf("DIR: Can't examine '%s' - %ld\n", path, IoErr());
        UnLock(lock);
        FreeDosObject(DOS_FIB, fib);
        return 1;
    }
    
    if (fib->fib_DirEntryType <= 0) {
        /* It's a file, not a directory */
        if (match_filename(fib->fib_FileName, pattern)) {
            list_entry(fib, detailed);
        }
        count++;
        total_size += fib->fib_Size;
    } else {
        /* It's a directory - list contents */
        if (detailed) {
            printf("Directory '%s':\n\n", path);
        }
        
        while (ExNext(lock, fib)) {
            /* Skip hidden files unless ALL switch */
            if (!show_all && fib->fib_FileName[0] == '.') {
                continue;
            }
            
            /* Check pattern match */
            if (!match_filename(fib->fib_FileName, pattern)) {
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
            printf("DIR: Error reading directory - %ld\n", IoErr());
        }
        
        if (detailed) {
            printf("\n%5d file(s)  %ld bytes\n", count - dir_count, total_size);
            printf("%5d dir(s)\n", dir_count);
        }
    }
    
    UnLock(lock);
    FreeDosObject(DOS_FIB, fib);
    
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
    rda = ReadArgs(TEMPLATE, args, NULL);
    if (!rda) {
        LONG err = IoErr();
        if (err == ERROR_REQUIRED_ARG_MISSING) {
            printf("DIR: Required argument missing\n");
        } else {
            printf("DIR: Error parsing arguments - %ld\n", err);
        }
        printf("Usage: DIR [pattern] [OPT keywords] [ALL] [DIRS] [FILES] [INTER]\n");
        printf("Template: %s\n", TEMPLATE);
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
        pattern = DEFAULT_PATTERN;
    }
    
    /* Handle OPT keyword for traditional options */
    BOOL detailed = FALSE;
    if (opt_str) {
        /* Parse OPT string for legacy compatibility */
        if (strstr(opt_str, "L") || strstr(opt_str, "l")) {
            detailed = TRUE;
        }
    }
    
    /* Interactive mode not yet implemented */
    if (inter_flag) {
        printf("DIR: Interactive mode not yet implemented\n");
    }
    
    /* List the directory */
    result = list_directory("", pattern, detailed, all_flag, dirs_flag, files_flag);
    
    FreeArgs(rda);
    return result;
}
