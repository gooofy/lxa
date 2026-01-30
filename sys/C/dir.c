/*
 * DIR command - List directory contents
 * Phase 7.5 implementation for lxa
 * 
 * Template: DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S
 * 
 * Features:
 *   - Recursive directory traversal with ALL switch
 *   - Sorted output (dirs first, then files, alphabetically)
 *   - Two-column output for non-detailed listing
 *   - Interactive mode (INTER) with navigation
 *   - Ctrl+C break handling
 *   - Pattern matching support
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

/* Default pattern to match all files */
#define DEFAULT_PATTERN "#?"

/* Buffer sizes */
#define PATTERN_BUFFER_SIZE 256
#define MAX_PATH_LEN 512
#define MAX_ENTRIES 512

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

/* Directory entry for sorting */
typedef struct {
    char name[108];       /* Amiga max filename length */
    LONG size;
    LONG protection;
    LONG is_dir;
    char comment[80];
} DirEntry;

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
    char bits[9];
    
    bits[0] = (protection & (1 << 7)) ? 'h' : '-';  /* Hold */
    bits[1] = (protection & (1 << 6)) ? 's' : '-';  /* Script */
    bits[2] = (protection & (1 << 5)) ? 'p' : '-';  /* Pure */
    bits[3] = (protection & (1 << 4)) ? 'a' : '-';  /* Archive */
    /* Note: bits 0-3 are inverted in Amiga */
    bits[4] = !(protection & (1 << 3)) ? 'r' : '-'; /* Read */
    bits[5] = !(protection & (1 << 2)) ? 'w' : '-'; /* Write */
    bits[6] = !(protection & (1 << 1)) ? 'e' : '-'; /* Execute */
    bits[7] = !(protection & (1 << 0)) ? 'd' : '-'; /* Delete */
    bits[8] = '\0';
    
    out_str(bits);
}

/* Case-insensitive string compare for sorting */
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

/* Sort entries: directories first (alphabetically), then files (alphabetically) */
static void sort_entries(DirEntry *entries, int count)
{
    /* Simple bubble sort - adequate for typical directory sizes */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            BOOL swap = FALSE;
            
            /* Directories come before files */
            if (entries[j].is_dir && !entries[j+1].is_dir) {
                swap = FALSE;  /* j is dir, j+1 is file - correct order */
            } else if (!entries[j].is_dir && entries[j+1].is_dir) {
                swap = TRUE;   /* j is file, j+1 is dir - need to swap */
            } else {
                /* Both same type - alphabetical order */
                if (stricmp_amiga(entries[j].name, entries[j+1].name) > 0) {
                    swap = TRUE;
                }
            }
            
            if (swap) {
                DirEntry temp = entries[j];
                entries[j] = entries[j+1];
                entries[j+1] = temp;
            }
        }
    }
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

/* Print a single entry in detailed format */
static void print_detailed_entry(DirEntry *entry)
{
    print_protection(entry->protection);
    
    if (entry->is_dir) {
        out_str("    <DIR> ");
    } else {
        char size_str[16];
        format_size(entry->size, size_str, sizeof(size_str));
        out_str(" ");
        /* Pad to 8 chars */
        int len = strlen(size_str);
        while (len < 8) { out_str(" "); len++; }
        out_str(size_str);
        out_str(" ");
    }
    
    out_str(entry->name);
    
    if (entry->comment[0]) {
        out_str(" ; ");
        out_str(entry->comment);
    }
    
    out_nl();
}

/* Interactive mode handler */
static int interactive_mode(CONST_STRPTR path, CONST_STRPTR pattern, BOOL dirs_only, BOOL files_only);

/* List directory contents with pattern matching */
static int list_directory(CONST_STRPTR path, CONST_STRPTR pattern, BOOL detailed, 
                          BOOL recursive, BOOL dirs_only, BOOL files_only, int depth)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    DirEntry *entries;
    int entry_count = 0;
    int dir_count = 0;
    int file_count = 0;
    LONG total_size = 0;
    char indent[64];
    char resolved_path[MAX_PATH_LEN];  /* For storing resolved path when path is empty */
    const char *actual_path;           /* Points to path we'll use for recursion */
    
    /* Check for break */
    if (check_break()) {
        out_str("***BREAK\n");
        return 1;
    }
    
    /* Build indentation for recursive listing */
    int indent_len = depth * 2;
    if (indent_len > 60) indent_len = 60;
    for (int i = 0; i < indent_len; i++) indent[i] = ' ';
    indent[indent_len] = '\0';
    
    /* Allocate entries array */
    entries = AllocMem(sizeof(DirEntry) * MAX_ENTRIES, MEMF_CLEAR);
    if (!entries) {
        out_str("DIR: Out of memory\n");
        return 1;
    }
    
    /* Allocate FileInfoBlock */
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        out_str("DIR: Failed to allocate FileInfoBlock\n");
        FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
        return 1;
    }
    
    /* Lock the directory */
    if (path && path[0] != '\0') {
        lock = Lock((STRPTR)path, SHARED_LOCK);
        actual_path = (const char *)path;
    } else {
        /* Get current directory lock */
        BPTR cur = CurrentDir(0);
        CurrentDir(cur);
        if (cur) {
            lock = DupLock(cur);
        } else {
            lock = Lock((STRPTR)"SYS:", SHARED_LOCK);
        }
        actual_path = "";  /* Will be resolved after Examine */
    }
    
    if (!lock) {
        out_str(indent);
        out_str("DIR: Can't access '");
        out_str((path && path[0]) ? (char *)path : "current directory");
        out_str("' - ");
        out_num(IoErr());
        out_nl();
        FreeDosObject(DOS_FIB, fib);
        FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
        return 1;
    }

    /* Examine the directory */
    if (!Examine(lock, fib)) {
        out_str(indent);
        out_str("DIR: Can't examine '");
        out_str((path && path[0]) ? (char *)path : "current directory");
        out_str("' - ");
        out_num(IoErr());
        out_nl();
        UnLock(lock);
        FreeDosObject(DOS_FIB, fib);
        FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
        return 1;
    }
    
    /* If path was empty, get the full path from the lock for recursion */
    if (!path || path[0] == '\0') {
        if (NameFromLock(lock, (STRPTR)resolved_path, MAX_PATH_LEN)) {
            actual_path = resolved_path;
        }
    }
    
    /* Print directory header */
    if (detailed || recursive) {
        out_str(indent);
        out_str("Directory \"");
        out_str((path && path[0]) ? (char *)path : (char *)fib->fib_FileName);
        out_str("\"\n");
        if (detailed) {
            out_str(indent);
            out_nl();
        }
    }
    
    /* Collect all entries */
    while (ExNext(lock, fib) && entry_count < MAX_ENTRIES) {
        /* Check for break */
        if (check_break()) {
            out_str("***BREAK\n");
            UnLock(lock);
            FreeDosObject(DOS_FIB, fib);
            FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
            return 1;
        }
        
        /* Check pattern match */
        if (!match_filename((char *)fib->fib_FileName, (char *)pattern)) {
            continue;
        }
        
        /* Filter by DIRS/FILES switches */
        BOOL is_dir = (fib->fib_DirEntryType > 0);
        if (dirs_only && !is_dir) continue;
        if (files_only && is_dir) continue;
        
        /* Add to entries array */
        strncpy(entries[entry_count].name, (char *)fib->fib_FileName, 107);
        entries[entry_count].name[107] = '\0';
        entries[entry_count].size = fib->fib_Size;
        entries[entry_count].protection = fib->fib_Protection;
        entries[entry_count].is_dir = is_dir;
        if (fib->fib_Comment[0]) {
            strncpy(entries[entry_count].comment, (char *)fib->fib_Comment, 79);
            entries[entry_count].comment[79] = '\0';
        } else {
            entries[entry_count].comment[0] = '\0';
        }
        
        if (is_dir) {
            dir_count++;
        } else {
            file_count++;
            total_size += fib->fib_Size;
        }
        
        entry_count++;
    }
    
    /* Check for error vs end of directory */
    LONG err = IoErr();
    if (err != ERROR_NO_MORE_ENTRIES && err != 0) {
        out_str(indent);
        out_str("DIR: Error reading directory - ");
        out_num(err);
        out_nl();
    }
    
    UnLock(lock);
    FreeDosObject(DOS_FIB, fib);
    
    /* Sort entries */
    sort_entries(entries, entry_count);
    
    /* Print entries */
    if (detailed) {
        for (int i = 0; i < entry_count; i++) {
            if (check_break()) {
                out_str("***BREAK\n");
                FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
                return 1;
            }
            out_str(indent);
            print_detailed_entry(&entries[i]);
        }
    } else {
        /* Two-column output for simple listing */
        for (int i = 0; i < entry_count; i++) {
            if (check_break()) {
                out_str("***BREAK\n");
                FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
                return 1;
            }
            out_str(indent);
            out_str(entries[i].name);
            if (entries[i].is_dir) {
                out_str(" (dir)");
            }
            out_nl();
        }
    }
    
    /* Print summary for detailed listing */
    if (detailed) {
        out_str(indent);
        out_nl();
        out_str(indent);
        out_num(file_count);
        out_str(" file(s) - ");
        out_num(total_size);
        out_str(" bytes\n");
        out_str(indent);
        out_num(dir_count);
        out_str(" dir(s)\n");
    }
    
    /* Recursive listing */
    if (recursive && !check_break()) {
        for (int i = 0; i < entry_count; i++) {
            if (check_break()) {
                out_str("***BREAK\n");
                break;
            }
            
            if (entries[i].is_dir) {
                /* Build full path to subdirectory using actual_path (resolved if needed) */
                char subpath[MAX_PATH_LEN];
                if (actual_path && actual_path[0] != '\0') {
                    strncpy(subpath, actual_path, MAX_PATH_LEN - 2);
                    int len = strlen(subpath);
                    if (len > 0 && subpath[len-1] != ':' && subpath[len-1] != '/') {
                        strcat(subpath, "/");
                    }
                    strncat(subpath, entries[i].name, MAX_PATH_LEN - strlen(subpath) - 1);
                } else {
                    strncpy(subpath, entries[i].name, MAX_PATH_LEN - 1);
                }
                subpath[MAX_PATH_LEN - 1] = '\0';
                
                out_str(indent);
                out_nl();
                
                /* Recurse into subdirectory with default pattern */
                list_directory((CONST_STRPTR)subpath, (CONST_STRPTR)DEFAULT_PATTERN, 
                              detailed, TRUE, dirs_only, files_only, depth + 1);
            }
        }
    }
    
    FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
    
    /* Ensure output is flushed */
    Flush(Output());
    
    return 0;
}

/* Interactive mode - basic implementation */
static int interactive_mode(CONST_STRPTR path, CONST_STRPTR pattern, BOOL dirs_only, BOOL files_only)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    DirEntry *entries;
    int entry_count = 0;
    int current_idx = 0;
    BPTR input = Input();
    char cmd_buf[256];
    char current_path[MAX_PATH_LEN];
    
    /* Initialize current path */
    if (path && path[0]) {
        strncpy(current_path, (char *)path, MAX_PATH_LEN - 1);
    } else {
        current_path[0] = '\0';
    }
    current_path[MAX_PATH_LEN - 1] = '\0';
    
    /* Allocate entries array */
    entries = AllocMem(sizeof(DirEntry) * MAX_ENTRIES, MEMF_CLEAR);
    if (!entries) {
        out_str("DIR: Out of memory\n");
        return 1;
    }
    
    /* Allocate FileInfoBlock */
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
        out_str("DIR: Failed to allocate FileInfoBlock\n");
        return 1;
    }

reload_dir:
    entry_count = 0;
    
    /* Lock the directory */
    if (current_path[0] != '\0') {
        lock = Lock((STRPTR)current_path, SHARED_LOCK);
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
        out_str("DIR: Can't access directory\n");
        FreeDosObject(DOS_FIB, fib);
        FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
        return 1;
    }

    if (!Examine(lock, fib)) {
        UnLock(lock);
        out_str("DIR: Can't examine directory\n");
        FreeDosObject(DOS_FIB, fib);
        FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
        return 1;
    }
    
    /* Collect entries */
    while (ExNext(lock, fib) && entry_count < MAX_ENTRIES) {
        if (!match_filename((char *)fib->fib_FileName, (char *)pattern)) continue;
        
        BOOL is_dir = (fib->fib_DirEntryType > 0);
        if (dirs_only && !is_dir) continue;
        if (files_only && is_dir) continue;
        
        strncpy(entries[entry_count].name, (char *)fib->fib_FileName, 107);
        entries[entry_count].name[107] = '\0';
        entries[entry_count].size = fib->fib_Size;
        entries[entry_count].protection = fib->fib_Protection;
        entries[entry_count].is_dir = is_dir;
        entry_count++;
    }
    
    UnLock(lock);
    
    /* Sort entries */
    sort_entries(entries, entry_count);
    
    out_str("\nInteractive mode - ");
    out_num(entry_count);
    out_str(" entries\n");
    out_str("Commands: ? (help), Return (next), E (enter dir), B (back), Q (quit)\n\n");
    
    current_idx = 0;
    
    while (1) {
        if (check_break()) {
            out_str("***BREAK\n");
            break;
        }
        
        /* Show current entry */
        if (current_idx < entry_count) {
            out_str("  ");
            out_str(entries[current_idx].name);
            if (entries[current_idx].is_dir) {
                out_str(" (dir)");
            } else {
                char size_str[16];
                format_size(entries[current_idx].size, size_str, sizeof(size_str));
                out_str(" - ");
                out_str(size_str);
                out_str(" bytes");
            }
            out_str(" : ");
        } else {
            out_str("End of directory : ");
        }
        
        Flush(Output());
        
        /* Read command */
        LONG len = Read(input, cmd_buf, 255);
        if (len <= 0) break;
        cmd_buf[len] = '\0';
        
        /* Process command */
        char cmd = cmd_buf[0];
        if (cmd >= 'a' && cmd <= 'z') cmd -= 32;  /* Uppercase */
        
        switch (cmd) {
            case '?':
                out_str("\nInteractive DIR commands:\n");
                out_str("  ?         - Show this help\n");
                out_str("  Return    - Show next entry\n");
                out_str("  E         - Enter directory\n");
                out_str("  B         - Go back (parent directory)\n");
                out_str("  T         - Type file contents\n");
                out_str("  Q         - Quit interactive mode\n\n");
                break;
                
            case '\n':
            case '\r':
            case ' ':
                /* Next entry */
                current_idx++;
                if (current_idx >= entry_count) {
                    out_str("\nEnd of directory listing.\n");
                }
                break;
                
            case 'E':
                /* Enter directory */
                if (current_idx < entry_count && entries[current_idx].is_dir) {
                    int len = strlen(current_path);
                    if (len > 0 && current_path[len-1] != ':' && current_path[len-1] != '/') {
                        strcat(current_path, "/");
                    }
                    strncat(current_path, entries[current_idx].name, MAX_PATH_LEN - strlen(current_path) - 1);
                    current_idx = 0;
                    out_str("\nEntering: ");
                    out_str(current_path);
                    out_nl();
                    goto reload_dir;
                } else {
                    out_str("Not a directory.\n");
                }
                break;
                
            case 'B':
                /* Back to parent */
                {
                    int len = strlen(current_path);
                    if (len > 0) {
                        /* Find last separator */
                        char *sep = current_path + len - 1;
                        while (sep > current_path && *sep != '/' && *sep != ':') sep--;
                        if (*sep == ':') {
                            *(sep + 1) = '\0';
                        } else if (sep > current_path) {
                            *sep = '\0';
                        }
                        current_idx = 0;
                        out_str("\nGoing to: ");
                        out_str(current_path[0] ? current_path : "(root)");
                        out_nl();
                        goto reload_dir;
                    } else {
                        out_str("Already at root.\n");
                    }
                }
                break;
                
            case 'T':
                /* Type file */
                if (current_idx < entry_count && !entries[current_idx].is_dir) {
                    char full_path[MAX_PATH_LEN];
                    if (current_path[0]) {
                        strncpy(full_path, current_path, MAX_PATH_LEN - 2);
                        int plen = strlen(full_path);
                        if (plen > 0 && full_path[plen-1] != ':' && full_path[plen-1] != '/') {
                            strcat(full_path, "/");
                        }
                        strncat(full_path, entries[current_idx].name, MAX_PATH_LEN - strlen(full_path) - 1);
                    } else {
                        strncpy(full_path, entries[current_idx].name, MAX_PATH_LEN - 1);
                    }
                    full_path[MAX_PATH_LEN - 1] = '\0';
                    
                    /* Open and type file */
                    BPTR fh = Open((STRPTR)full_path, MODE_OLDFILE);
                    if (fh) {
                        char buf[512];
                        LONG bytes;
                        out_str("\n--- ");
                        out_str(entries[current_idx].name);
                        out_str(" ---\n");
                        while ((bytes = Read(fh, buf, 511)) > 0) {
                            buf[bytes] = '\0';
                            out_str(buf);
                        }
                        out_str("\n--- end ---\n");
                        Close(fh);
                    } else {
                        out_str("Can't open file.\n");
                    }
                } else {
                    out_str("Not a file.\n");
                }
                break;
                
            case 'Q':
                out_str("Quit.\n");
                FreeDosObject(DOS_FIB, fib);
                FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
                return 0;
                
            default:
                out_str("Unknown command. Type ? for help.\n");
                break;
        }
    }
    
    FreeDosObject(DOS_FIB, fib);
    FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
    return 0;
}

int main(int argc, char **argv)
{
    LONG args[ARG_COUNT] = {0};
    struct RDArgs *rda;
    int result;
    
    (void)argc;
    (void)argv;
    
    /* Clear break flag */
    g_user_break = FALSE;
    SetSignal(0L, SIGBREAKF_CTRL_C);  /* Clear any pending break */
    
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
        out_str("Usage: DIR [directory|pattern] [OPT L] [ALL] [DIRS] [FILES] [INTER]\n");
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
    
    /* Handle OPT keyword for traditional options */
    BOOL detailed = FALSE;
    if (opt_str) {
        const char *s = (const char *)opt_str;
        while (*s) {
            if (*s == 'L' || *s == 'l') detailed = TRUE;
            s++;
        }
    }
    
    /* Determine path and file pattern */
    CONST_STRPTR path = (CONST_STRPTR)"";
    CONST_STRPTR file_pattern = (CONST_STRPTR)DEFAULT_PATTERN;
    
    if (pattern && pattern[0] != '\0') {
        /* Check if it's a directory or pattern */
        BPTR test_lock = Lock((STRPTR)pattern, SHARED_LOCK);
        if (test_lock) {
            struct FileInfoBlock *test_fib = AllocDosObject(DOS_FIB, NULL);
            if (test_fib) {
                if (Examine(test_lock, test_fib) && test_fib->fib_DirEntryType > 0) {
                    /* It's a directory - list its contents */
                    path = pattern;
                    file_pattern = (CONST_STRPTR)DEFAULT_PATTERN;
                } else {
                    /* It's a file */
                    path = (CONST_STRPTR)"";
                    file_pattern = pattern;
                }
                FreeDosObject(DOS_FIB, test_fib);
            }
            UnLock(test_lock);
        } else {
            /* Doesn't exist - treat as pattern */
            /* Check if it contains a path component */
            const char *p = (const char *)pattern;
            const char *last_sep = NULL;
            while (*p) {
                if (*p == '/' || *p == ':') last_sep = p;
                p++;
            }
            if (last_sep) {
                /* Split into path and pattern */
                static char path_buf[MAX_PATH_LEN];
                int path_len = last_sep - (const char *)pattern + 1;
                if (path_len >= MAX_PATH_LEN) path_len = MAX_PATH_LEN - 1;
                strncpy(path_buf, (const char *)pattern, path_len);
                path_buf[path_len] = '\0';
                path = (CONST_STRPTR)path_buf;
                file_pattern = (CONST_STRPTR)(last_sep + 1);
            } else {
                /* No path - pattern in current directory */
                path = (CONST_STRPTR)"";
                file_pattern = pattern;
            }
        }
    }
    
    /* Interactive mode */
    if (inter_flag) {
        result = interactive_mode(path, file_pattern, dirs_flag, files_flag);
    } else {
        /* Normal listing */
        result = list_directory(path, file_pattern, detailed, all_flag, dirs_flag, files_flag, 0);
    }
    
    FreeArgs(rda);
    return result;
}
