/*
 * SORT command - Sort lines of text
 * Step 9.3 implementation for lxa
 * 
 * Template: FROM/A,TO/A,COLSTART/K/N,CASE/S,NUMERIC/S
 * 
 * Features:
 *   - Sort lines alphabetically (default)
 *   - COLSTART: Start sorting from specified column
 *   - CASE: Case-sensitive sorting (default is case-insensitive)
 *   - NUMERIC: Sort numerically instead of alphabetically
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
#define TEMPLATE "FROM/A,TO/A,COLSTART/K/N,CASE/S,NUMERIC/S"

/* Argument array indices */
#define ARG_FROM     0
#define ARG_TO       1
#define ARG_COLSTART 2
#define ARG_CASE     3
#define ARG_NUMERIC  4
#define ARG_COUNT    5

/* Configuration */
#define MAX_LINE_LENGTH 512
#define MAX_LINES       4096
#define LINE_POOL_SIZE  (MAX_LINES * MAX_LINE_LENGTH / 4)  /* Average line ~128 chars */

/* Global state for sorting options */
static LONG g_colstart = 0;
static BOOL g_case_sensitive = FALSE;
static BOOL g_numeric = FALSE;

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

/* Helper: output a string to stderr */
static void err_str(const char *str)
{
    BPTR err = Output();  /* Use Output() since lxa may not have separate stderr */
    Write(err, (STRPTR)str, strlen(str));
}

/* Convert char to lowercase */
static char to_lower(char c)
{
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

/* Parse a number from string */
static LONG parse_num(const char *s)
{
    LONG n = 0;
    BOOL neg = FALSE;
    
    /* Skip whitespace */
    while (*s == ' ' || *s == '\t') s++;
    
    /* Check for sign */
    if (*s == '-') {
        neg = TRUE;
        s++;
    } else if (*s == '+') {
        s++;
    }
    
    /* Parse digits */
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    
    return neg ? -n : n;
}

/* Compare function for sorting */
static int compare_lines(const char *a, const char *b)
{
    /* Apply column start offset */
    LONG alen = strlen(a);
    LONG blen = strlen(b);
    
    if (g_colstart > 0) {
        if (g_colstart <= alen) {
            a += g_colstart - 1;
        } else {
            a = "";
        }
        if (g_colstart <= blen) {
            b += g_colstart - 1;
        } else {
            b = "";
        }
    }
    
    if (g_numeric) {
        /* Numeric comparison */
        LONG na = parse_num(a);
        LONG nb = parse_num(b);
        if (na < nb) return -1;
        if (na > nb) return 1;
        return 0;
    }
    
    /* String comparison */
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        
        if (!g_case_sensitive) {
            ca = to_lower(ca);
            cb = to_lower(cb);
        }
        
        if (ca < cb) return -1;
        if (ca > cb) return 1;
        
        a++;
        b++;
    }
    
    /* If we get here, one string is a prefix of the other */
    if (*a) return 1;   /* a is longer */
    if (*b) return -1;  /* b is longer */
    return 0;           /* Equal */
}

/* Simple quicksort implementation */
static void quicksort(char **lines, LONG left, LONG right)
{
    if (left >= right || check_break()) return;
    
    /* Choose pivot (middle element) */
    LONG mid = (left + right) / 2;
    char *pivot = lines[mid];
    
    /* Partition */
    LONG i = left;
    LONG j = right;
    
    while (i <= j) {
        while (compare_lines(lines[i], pivot) < 0) i++;
        while (compare_lines(lines[j], pivot) > 0) j--;
        
        if (i <= j) {
            /* Swap */
            char *tmp = lines[i];
            lines[i] = lines[j];
            lines[j] = tmp;
            i++;
            j--;
        }
    }
    
    /* Recursively sort partitions */
    if (left < j) quicksort(lines, left, j);
    if (i < right) quicksort(lines, i, right);
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

/* Duplicate a string using AllocVec */
static char *str_dup(const char *s, char **pool, LONG *pool_used, LONG pool_size)
{
    LONG len = strlen(s) + 1;
    
    if (*pool_used + len > pool_size) {
        return NULL;  /* Pool exhausted */
    }
    
    char *result = *pool + *pool_used;
    strcpy(result, s);
    *pool_used += len;
    
    return result;
}

/* Main entry point */
int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    STRPTR from_file;
    STRPTR to_file;
    BPTR fh_in = 0;
    BPTR fh_out = 0;
    char **lines = NULL;
    char *line_pool = NULL;
    LONG pool_used = 0;
    char line_buf[MAX_LINE_LENGTH];
    LONG num_lines = 0;
    int rc = RETURN_OK;
    LONG i;
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"SORT");
        return RETURN_FAIL;
    }
    
    from_file = (STRPTR)args[ARG_FROM];
    to_file = (STRPTR)args[ARG_TO];
    
    if (args[ARG_COLSTART]) {
        g_colstart = *(LONG *)args[ARG_COLSTART];
    }
    g_case_sensitive = args[ARG_CASE] != 0;
    g_numeric = args[ARG_NUMERIC] != 0;
    
    /* Allocate line pointer array */
    lines = AllocVec(MAX_LINES * sizeof(char *), MEMF_CLEAR);
    if (!lines) {
        err_str("SORT: Out of memory\n");
        rc = RETURN_FAIL;
        goto cleanup;
    }
    
    /* Allocate line pool */
    line_pool = AllocVec(LINE_POOL_SIZE, MEMF_CLEAR);
    if (!line_pool) {
        err_str("SORT: Out of memory\n");
        rc = RETURN_FAIL;
        goto cleanup;
    }
    
    /* Open input file */
    fh_in = Open(from_file, MODE_OLDFILE);
    if (!fh_in) {
        PrintFault(IoErr(), from_file);
        rc = RETURN_FAIL;
        goto cleanup;
    }
    
    /* Read all lines */
    while (!check_break()) {
        LONG len = read_line(fh_in, line_buf, sizeof(line_buf));
        
        /* Check for end of file */
        if (len == 0) {
            LONG pos = Seek(fh_in, 0, OFFSET_CURRENT);
            LONG end = Seek(fh_in, 0, OFFSET_END);
            Seek(fh_in, pos, OFFSET_BEGINNING);
            if (pos >= end) break;
        }
        
        if (num_lines >= MAX_LINES) {
            err_str("SORT: Too many lines (max ");
            /* Print MAX_LINES - simple number output */
            char nbuf[16];
            char *p = nbuf + 15;
            LONG n = MAX_LINES;
            *p = '\0';
            do { *--p = '0' + (n % 10); n /= 10; } while (n);
            err_str(p);
            err_str(")\n");
            rc = RETURN_FAIL;
            goto cleanup;
        }
        
        /* Duplicate line */
        lines[num_lines] = str_dup(line_buf, &line_pool, &pool_used, LINE_POOL_SIZE);
        if (!lines[num_lines]) {
            err_str("SORT: Line pool exhausted\n");
            rc = RETURN_FAIL;
            goto cleanup;
        }
        num_lines++;
    }
    
    Close(fh_in);
    fh_in = 0;
    
    if (check_break()) {
        err_str("***Break\n");
        rc = RETURN_WARN;
        goto cleanup;
    }
    
    /* Sort the lines */
    if (num_lines > 1) {
        quicksort(lines, 0, num_lines - 1);
    }
    
    if (check_break()) {
        err_str("***Break\n");
        rc = RETURN_WARN;
        goto cleanup;
    }
    
    /* Open output file */
    fh_out = Open(to_file, MODE_NEWFILE);
    if (!fh_out) {
        PrintFault(IoErr(), to_file);
        rc = RETURN_FAIL;
        goto cleanup;
    }
    
    /* Write sorted lines */
    for (i = 0; i < num_lines && !check_break(); i++) {
        Write(fh_out, (STRPTR)lines[i], strlen(lines[i]));
        Write(fh_out, (STRPTR)"\n", 1);
    }
    
    if (check_break()) {
        err_str("***Break\n");
        rc = RETURN_WARN;
    }
    
cleanup:
    if (fh_in) Close(fh_in);
    if (fh_out) Close(fh_out);
    if (lines) FreeVec(lines);
    if (line_pool) FreeVec(line_pool);
    FreeArgs(rdargs);
    
    return rc;
}
