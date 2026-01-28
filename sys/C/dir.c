/*
 * DIR command - List directory contents
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

/* Default pattern to match all files */
#define DEFAULT_PATTERN "#?"

/* Buffer for pattern matching */
#define PATTERN_BUFFER_SIZE 256

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

/* List directory contents */
static int list_directory(CONST_STRPTR path, BOOL detailed, BOOL show_all)
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
        list_entry(fib, detailed);
        count++;
        total_size += fib->fib_Size;
    } else {
        /* It's a directory - list contents */
        if (detailed) {
            printf("Directory '%s':\n\n", path);
        }
        
        while (ExNext(lock, fib)) {
            /* Skip hidden files unless -a flag */
            if (!show_all && fib->fib_FileName[0] == '.') {
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
    CONST_STRPTR path = "";
    BOOL detailed = FALSE;
    BOOL show_all = FALSE;
    int i;
    
    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            /* Option */
            switch (argv[i][1]) {
                case 'l':
                    detailed = TRUE;
                    break;
                case 'a':
                    show_all = TRUE;
                    break;
                case '?':
                case 'h':
                    printf("DIR - List directory contents (v%s)\n", VERSION);
                    printf("Usage: DIR [options] [path]\n");
                    printf("Options:\n");
                    printf("  -l    Long/detailed listing\n");
                    printf("  -a    Show all files (including hidden)\n");
                    return 0;
                default:
                    printf("DIR: Unknown option '%s'\n", argv[i]);
                    return 1;
            }
        } else {
            /* Path argument */
            path = (CONST_STRPTR)argv[i];
        }
    }
    
    /* Use current directory if no path specified */
    if (path[0] == '\0') {
        path = "";  /* Empty string means current dir in AmigaDOS */
    }
    
    return list_directory(path, detailed, show_all);
}
