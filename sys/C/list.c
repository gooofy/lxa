/*
 * LIST command - Detailed directory listing
 * Phase 9.2 implementation for lxa
 * 
 * Template: DIR/M,P=PAT/K,KEYS/S,DATES/S,NODATES/S,TO/K,SUB/S,SINCE/K,UPTO/K,QUICK/S,BLOCK/S,NOHEAD/S,FILES/S,DIRS/S,LFORMAT/K,ALL/S
 * 
 * Features:
 *   - Detailed file information (protection, size, date)
 *   - Pattern filtering (P=PAT)
 *   - Key (inode) display (KEYS)
 *   - Date filtering (SINCE, UPTO)
 *   - Output redirection (TO)
 *   - Recursive listing (ALL, SUB)
 *   - Quick mode (less info)
 *   - Block size display (BLOCK)
 *   - Custom format (LFORMAT)
 *   - Filter by type (FILES, DIRS)
 *   - Ctrl+C break handling
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/datetime.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template - simplified for initial implementation */
#define TEMPLATE "DIR/M,P=PAT/K,KEYS/S,DATES/S,NODATES/S,TO/K,QUICK/S,BLOCK/S,NOHEAD/S,FILES/S,DIRS/S,ALL/S"

/* Argument array indices */
#define ARG_DIR     0
#define ARG_PAT     1
#define ARG_KEYS    2
#define ARG_DATES   3
#define ARG_NODATES 4
#define ARG_TO      5
#define ARG_QUICK   6
#define ARG_BLOCK   7
#define ARG_NOHEAD  8
#define ARG_FILES   9
#define ARG_DIRS    10
#define ARG_ALL     11
#define ARG_COUNT   12

/* Buffer sizes */
#define MAX_PATH_LEN 512
#define MAX_ENTRIES 512
#define PATTERN_BUFFER_SIZE 256

/* Protection bit definitions - use system headers */
/* FIBF_* defined in dos/dos.h */

/* Options structure */
typedef struct {
    BOOL keys;
    BOOL dates;
    BOOL nodates;
    BOOL quick;
    BOOL block;
    BOOL nohead;
    BOOL files_only;
    BOOL dirs_only;
    BOOL all;
    const char *pattern;
    BPTR output;
} ListOptions;

/* Directory entry for sorting */
typedef struct {
    char name[108];
    LONG size;
    LONG protection;
    LONG is_dir;
    LONG blocks;
    LONG key;
    struct DateStamp date;
    char comment[80];
} DirEntry;

/* Global state */
static BOOL g_user_break = FALSE;
static LONG g_total_files = 0;
static LONG g_total_dirs = 0;
static LONG g_total_blocks = 0;
static LONG g_total_bytes = 0;

/* Helper: check for Ctrl+C break */
static BOOL check_break(void)
{
    if (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
        g_user_break = TRUE;
        return TRUE;
    }
    return g_user_break;
}

/* Helper: output a string to handle */
static void out_fh(BPTR fh, const char *str)
{
    Write(fh, (STRPTR)str, strlen(str));
}

/* Helper: output newline */
static void out_nl_fh(BPTR fh)
{
    Write(fh, (STRPTR)"\n", 1);
}

/* Helper: output number */
static void out_num_fh(BPTR fh, LONG num)
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
    out_fh(fh, p);
}

/* Helper: output number with padding */
static void out_num_pad(BPTR fh, LONG num, int width)
{
    char buf[16];
    char *p = buf + 15;
    *p = '\0';
    BOOL neg = (num < 0);
    unsigned long n = neg ? -num : num;
    int digits = 0;
    do {
        *--p = '0' + (n % 10);
        n /= 10;
        digits++;
    } while (n);
    if (neg) { *--p = '-'; digits++; }
    
    while (digits < width) {
        out_fh(fh, " ");
        digits++;
    }
    out_fh(fh, p);
}

/* Format protection bits as string */
static void format_protection(LONG protection, char *buf)
{
    buf[0] = (protection & FIBF_HOLD)    ? 'h' : '-';
    buf[1] = (protection & FIBF_SCRIPT)  ? 's' : '-';
    buf[2] = (protection & FIBF_PURE)    ? 'p' : '-';
    buf[3] = (protection & FIBF_ARCHIVE) ? 'a' : '-';
    buf[4] = !(protection & FIBF_READ)    ? 'r' : '-';
    buf[5] = !(protection & FIBF_WRITE)   ? 'w' : '-';
    buf[6] = !(protection & FIBF_EXECUTE) ? 'e' : '-';
    buf[7] = !(protection & FIBF_DELETE)  ? 'd' : '-';
    buf[8] = '\0';
}

/* Format date as string */
static void format_date(struct DateStamp *ds, char *buf, int buf_size)
{
    /* Simple date formatting */
    /* DateStamp: ds_Days from Jan 1, 1978, ds_Minute, ds_Tick */
    
    /* Convert days to date */
    LONG days = ds->ds_Days;
    LONG year = 1978;
    LONG month = 1;
    LONG day = 1;
    
    /* Days per month (non-leap year) */
    static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    while (days > 0) {
        int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 1 : 0;
        int year_days = 365 + leap;
        
        if (days >= year_days) {
            days -= year_days;
            year++;
        } else {
            break;
        }
    }
    
    /* Now find month and day */
    for (month = 1; month <= 12 && days > 0; month++) {
        int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 1 : 0;
        int mdays = days_in_month[month - 1];
        if (month == 2) mdays += leap;
        
        if (days >= mdays) {
            days -= mdays;
        } else {
            break;
        }
    }
    day = days + 1;
    
    /* Format time */
    LONG hour = ds->ds_Minute / 60;
    LONG minute = ds->ds_Minute % 60;
    LONG second = ds->ds_Tick / 50;  /* 50 ticks per second */
    
    /* Format: DD-MMM-YY HH:MM:SS */
    static const char *month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    char *p = buf;
    *p++ = '0' + (day / 10);
    *p++ = '0' + (day % 10);
    *p++ = '-';
    const char *mn = month_names[(month > 0 && month <= 12) ? month - 1 : 0];
    *p++ = mn[0]; *p++ = mn[1]; *p++ = mn[2];
    *p++ = '-';
    int yr = year % 100;
    *p++ = '0' + (yr / 10);
    *p++ = '0' + (yr % 10);
    *p++ = ' ';
    *p++ = '0' + (hour / 10);
    *p++ = '0' + (hour % 10);
    *p++ = ':';
    *p++ = '0' + (minute / 10);
    *p++ = '0' + (minute % 10);
    *p++ = ':';
    *p++ = '0' + (second / 10);
    *p++ = '0' + (second % 10);
    *p = '\0';
}

/* Case-insensitive string compare */
static int stricmp_amiga(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return *a - *b;
}

/* Sort entries: directories first, then files, alphabetically */
static void sort_entries(DirEntry *entries, int count)
{
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            BOOL swap = FALSE;
            
            if (entries[j].is_dir && !entries[j+1].is_dir) {
                swap = FALSE;
            } else if (!entries[j].is_dir && entries[j+1].is_dir) {
                swap = TRUE;
            } else {
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

/* Check if filename matches pattern */
static BOOL match_filename(const char *filename, const char *pattern)
{
    if (!pattern || pattern[0] == '\0')
        return TRUE;

    char parsed_pat[PATTERN_BUFFER_SIZE];
    LONG result = ParsePattern((STRPTR)pattern, (STRPTR)parsed_pat, sizeof(parsed_pat));

    if (result == 0) {
        return (stricmp_amiga(filename, pattern) == 0);
    }
    if (result < 0) {
        return (stricmp_amiga(filename, pattern) == 0);
    }
    return MatchPattern((STRPTR)parsed_pat, (STRPTR)filename);
}

/* Print a single entry */
static void print_entry(BPTR out, DirEntry *entry, ListOptions *opts)
{
    char prot_str[16];
    char date_str[32];
    
    /* Key (inode) */
    if (opts->keys) {
        out_num_pad(out, entry->key, 6);
        out_fh(out, " ");
    }
    
    /* Name - left justified, padded */
    out_fh(out, entry->name);
    int name_len = strlen(entry->name);
    while (name_len < 24) {
        out_fh(out, " ");
        name_len++;
    }
    out_fh(out, " ");
    
    /* Size or <dir> */
    if (entry->is_dir) {
        out_fh(out, "    <Dir>");
    } else {
        if (opts->block) {
            out_num_pad(out, entry->blocks, 9);
        } else {
            out_num_pad(out, entry->size, 9);
        }
    }
    out_fh(out, " ");
    
    /* Protection bits */
    if (!opts->quick) {
        format_protection(entry->protection, prot_str);
        out_fh(out, prot_str);
        out_fh(out, " ");
    }
    
    /* Date */
    if (!opts->nodates && opts->dates) {
        format_date(&entry->date, date_str, sizeof(date_str));
        out_fh(out, date_str);
    }
    
    /* Comment */
    if (!opts->quick && entry->comment[0]) {
        out_fh(out, "\n: ");
        out_fh(out, entry->comment);
    }
    
    out_nl_fh(out);
}

/* List a single directory */
static int list_directory(const char *path, ListOptions *opts, int depth)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    DirEntry *entries;
    int entry_count = 0;
    LONG dir_files = 0, dir_dirs = 0, dir_blocks = 0, dir_bytes = 0;
    char resolved_path[MAX_PATH_LEN];
    const char *actual_path;
    BPTR out = opts->output;
    
    if (check_break()) {
        out_fh(Output(), "***BREAK\n");
        return 1;
    }
    
    /* Allocate entries array */
    entries = AllocMem(sizeof(DirEntry) * MAX_ENTRIES, MEMF_CLEAR);
    if (!entries) {
        out_fh(Output(), "LIST: Out of memory\n");
        return 1;
    }
    
    /* Allocate FileInfoBlock */
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
        out_fh(Output(), "LIST: Failed to allocate FileInfoBlock\n");
        return 1;
    }
    
    /* Lock the directory */
    if (path && path[0] != '\0') {
        lock = Lock((STRPTR)path, SHARED_LOCK);
        actual_path = path;
    } else {
        BPTR cur = CurrentDir(0);
        CurrentDir(cur);
        if (cur) {
            lock = DupLock(cur);
        } else {
            lock = Lock((STRPTR)"SYS:", SHARED_LOCK);
        }
        actual_path = "";
    }
    
    if (!lock) {
        out_fh(Output(), "LIST: Can't access '");
        out_fh(Output(), (path && path[0]) ? path : "current directory");
        out_fh(Output(), "'\n");
        FreeDosObject(DOS_FIB, fib);
        FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
        return 1;
    }
    
    if (!Examine(lock, fib)) {
        UnLock(lock);
        FreeDosObject(DOS_FIB, fib);
        FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
        return 1;
    }
    
    /* Resolve path if empty */
    if (!path || path[0] == '\0') {
        if (NameFromLock(lock, (STRPTR)resolved_path, MAX_PATH_LEN)) {
            actual_path = resolved_path;
        }
    }
    
    /* Print directory header */
    if (!opts->nohead) {
        out_fh(out, "Directory \"");
        out_fh(out, (path && path[0]) ? path : (char *)fib->fib_FileName);
        out_fh(out, "\"");
        out_nl_fh(out);
    }
    
    /* Collect entries */
    while (ExNext(lock, fib) && entry_count < MAX_ENTRIES) {
        if (check_break()) {
            out_fh(Output(), "***BREAK\n");
            break;
        }
        
        /* Check pattern match */
        if (!match_filename((char *)fib->fib_FileName, opts->pattern)) {
            continue;
        }
        
        /* Filter by type */
        BOOL is_dir = (fib->fib_DirEntryType > 0);
        if (opts->files_only && is_dir) continue;
        if (opts->dirs_only && !is_dir) continue;
        
        /* Add entry */
        strncpy(entries[entry_count].name, (char *)fib->fib_FileName, 107);
        entries[entry_count].name[107] = '\0';
        entries[entry_count].size = fib->fib_Size;
        entries[entry_count].protection = fib->fib_Protection;
        entries[entry_count].is_dir = is_dir;
        entries[entry_count].blocks = fib->fib_NumBlocks;
        entries[entry_count].key = fib->fib_DiskKey;
        entries[entry_count].date = fib->fib_Date;
        if (fib->fib_Comment[0]) {
            strncpy(entries[entry_count].comment, (char *)fib->fib_Comment, 79);
            entries[entry_count].comment[79] = '\0';
        } else {
            entries[entry_count].comment[0] = '\0';
        }
        
        if (is_dir) {
            dir_dirs++;
        } else {
            dir_files++;
            dir_bytes += fib->fib_Size;
            dir_blocks += fib->fib_NumBlocks;
        }
        
        entry_count++;
    }
    
    UnLock(lock);
    FreeDosObject(DOS_FIB, fib);
    
    /* Sort entries */
    sort_entries(entries, entry_count);
    
    /* Print entries */
    for (int i = 0; i < entry_count; i++) {
        if (check_break()) {
            out_fh(Output(), "***BREAK\n");
            break;
        }
        print_entry(out, &entries[i], opts);
    }
    
    /* Print summary */
    if (!opts->nohead && entry_count > 0) {
        out_num_fh(out, dir_files);
        out_fh(out, " file");
        if (dir_files != 1) out_fh(out, "s");
        out_fh(out, " - ");
        out_num_fh(out, dir_blocks);
        out_fh(out, " block");
        if (dir_blocks != 1) out_fh(out, "s");
        out_fh(out, " used");
        out_nl_fh(out);
    }
    
    /* Update totals */
    g_total_files += dir_files;
    g_total_dirs += dir_dirs;
    g_total_blocks += dir_blocks;
    g_total_bytes += dir_bytes;
    
    /* Recursive listing */
    if (opts->all && !check_break()) {
        for (int i = 0; i < entry_count; i++) {
            if (check_break()) break;
            
            if (entries[i].is_dir) {
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
                
                out_nl_fh(out);
                list_directory(subpath, opts, depth + 1);
            }
        }
    }
    
    FreeMem(entries, sizeof(DirEntry) * MAX_ENTRIES);
    return 0;
}

int main(int argc, char **argv)
{
    LONG args[ARG_COUNT] = {0};
    struct RDArgs *rda;
    int result = 0;
    ListOptions opts;
    BPTR output_file = 0;
    
    (void)argc;
    (void)argv;
    
    /* Clear state */
    g_user_break = FALSE;
    g_total_files = 0;
    g_total_dirs = 0;
    g_total_blocks = 0;
    g_total_bytes = 0;
    SetSignal(0L, SIGBREAKF_CTRL_C);
    
    /* Parse arguments */
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        Write(Output(), (STRPTR)"LIST: Bad arguments\n", 20);
        Write(Output(), (STRPTR)"Usage: LIST [dir...] [P=pattern] [ALL] [KEYS] [DATES] [FILES] [DIRS]\n", 70);
        return 20;
    }
    
    /* Setup options */
    opts.keys = args[ARG_KEYS] ? TRUE : FALSE;
    opts.dates = args[ARG_DATES] ? TRUE : FALSE;
    opts.nodates = args[ARG_NODATES] ? TRUE : FALSE;
    opts.quick = args[ARG_QUICK] ? TRUE : FALSE;
    opts.block = args[ARG_BLOCK] ? TRUE : FALSE;
    opts.nohead = args[ARG_NOHEAD] ? TRUE : FALSE;
    opts.files_only = args[ARG_FILES] ? TRUE : FALSE;
    opts.dirs_only = args[ARG_DIRS] ? TRUE : FALSE;
    opts.all = args[ARG_ALL] ? TRUE : FALSE;
    opts.pattern = (const char *)args[ARG_PAT];
    
    /* Default: show dates unless NODATES */
    if (!opts.nodates && !opts.dates) {
        opts.dates = TRUE;
    }
    
    /* Open output file if specified */
    if (args[ARG_TO]) {
        output_file = Open((STRPTR)args[ARG_TO], MODE_NEWFILE);
        if (!output_file) {
            Write(Output(), (STRPTR)"LIST: Can't open output file\n", 29);
            FreeArgs(rda);
            return 20;
        }
        opts.output = output_file;
    } else {
        opts.output = Output();
    }
    
    /* Get directory list */
    STRPTR *dirs = (STRPTR *)args[ARG_DIR];
    
    if (dirs && dirs[0]) {
        /* Process each specified directory */
        while (*dirs) {
            if (check_break()) {
                Write(Output(), (STRPTR)"***BREAK\n", 9);
                break;
            }
            
            list_directory((const char *)*dirs, &opts, 0);
            dirs++;
            
            /* Newline between directories */
            if (*dirs) {
                out_nl_fh(opts.output);
            }
        }
    } else {
        /* No directory specified - list current directory */
        list_directory("", &opts, 0);
    }
    
    /* Print grand total if multiple directories or ALL mode */
    if (opts.all && !opts.nohead && (g_total_files + g_total_dirs) > 0) {
        out_nl_fh(opts.output);
        out_fh(opts.output, "TOTAL: ");
        out_num_fh(opts.output, g_total_files);
        out_fh(opts.output, " file");
        if (g_total_files != 1) out_fh(opts.output, "s");
        out_fh(opts.output, " - ");
        out_num_fh(opts.output, g_total_blocks);
        out_fh(opts.output, " block");
        if (g_total_blocks != 1) out_fh(opts.output, "s");
        out_fh(opts.output, " used in ");
        out_num_fh(opts.output, g_total_dirs);
        out_fh(opts.output, " director");
        if (g_total_dirs != 1) out_fh(opts.output, "ies"); else out_fh(opts.output, "y");
        out_nl_fh(opts.output);
    }
    
    /* Close output file if opened */
    if (output_file) {
        Close(output_file);
    }
    
    FreeArgs(rda);
    return result;
}
