/*
 * JOIN command - Concatenate files
 * Phase 9.1 implementation for lxa
 * 
 * Template: FROM/A/M,AS=TO/A
 * 
 * Features:
 *   - Concatenate multiple files into one output file
 *   - Binary-safe (copies all bytes)
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

#define VERSION "1.0"

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template */
#define TEMPLATE "FROM/A/M,AS=TO/A"

/* Argument array indices */
#define ARG_FROM    0
#define ARG_TO      1
#define ARG_COUNT   2

/* Buffer size for copying */
#define COPY_BUFFER_SIZE 4096

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

/* Copy contents of source file to destination handle */
static BOOL copy_file_contents(CONST_STRPTR src, BPTR dest_fh, UBYTE *buffer, LONG *total_bytes)
{
    BPTR src_fh;
    LONG bytes_read;
    
    src_fh = Open((STRPTR)src, MODE_OLDFILE);
    if (!src_fh) {
        out_str("JOIN: Can't open '");
        out_str((char *)src);
        out_str("' - ");
        out_num(IoErr());
        out_nl();
        return FALSE;
    }
    
    while ((bytes_read = Read(src_fh, buffer, COPY_BUFFER_SIZE)) > 0) {
        if (check_break()) {
            out_str("***BREAK\n");
            Close(src_fh);
            return FALSE;
        }
        
        LONG bytes_written = Write(dest_fh, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            out_str("JOIN: Write error for '");
            out_str((char *)src);
            out_str("'\n");
            Close(src_fh);
            return FALSE;
        }
        
        *total_bytes += bytes_read;
    }
    
    if (bytes_read < 0) {
        out_str("JOIN: Read error from '");
        out_str((char *)src);
        out_str("'\n");
        Close(src_fh);
        return FALSE;
    }
    
    Close(src_fh);
    return TRUE;
}

int main(int argc, char **argv)
{
    LONG args[ARG_COUNT] = {0};
    struct RDArgs *rda;
    BPTR dest_fh = 0;
    UBYTE *buffer = NULL;
    int result = 0;
    int files_joined = 0;
    LONG total_bytes = 0;
    
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
            out_str("JOIN: FROM and AS (TO) arguments are required\n");
        } else {
            out_str("JOIN: Error parsing arguments - ");
            out_num(err);
            out_nl();
        }
        out_str("Usage: JOIN FROM/A/M AS=TO/A\n");
        out_str("Template: ");
        out_str(TEMPLATE);
        out_nl();
        return 1;
    }
    
    /* Extract arguments */
    STRPTR *from_array = (STRPTR *)args[ARG_FROM];
    CONST_STRPTR to_path = (CONST_STRPTR)args[ARG_TO];
    
    /* Count source files */
    int source_count = 0;
    if (from_array) {
        while (from_array[source_count]) source_count++;
    }
    
    if (source_count == 0) {
        out_str("JOIN: No source files specified\n");
        FreeArgs(rda);
        return 1;
    }
    
    /* Allocate copy buffer */
    buffer = AllocMem(COPY_BUFFER_SIZE, MEMF_ANY);
    if (!buffer) {
        out_str("JOIN: Out of memory\n");
        FreeArgs(rda);
        return 1;
    }
    
    /* Create destination file */
    dest_fh = Open((STRPTR)to_path, MODE_NEWFILE);
    if (!dest_fh) {
        out_str("JOIN: Can't create '");
        out_str((char *)to_path);
        out_str("' - ");
        out_num(IoErr());
        out_nl();
        FreeMem(buffer, COPY_BUFFER_SIZE);
        FreeArgs(rda);
        return 1;
    }
    
    /* Process each source file */
    out_str("Joining ");
    out_num(source_count);
    out_str(" file(s) to '");
    out_str((char *)to_path);
    out_str("'\n");
    
    for (int i = 0; from_array[i] != NULL; i++) {
        if (check_break()) {
            out_str("***BREAK\n");
            result = 1;
            break;
        }
        
        out_str("  ");
        out_str((char *)from_array[i]);
        
        LONG file_bytes = 0;
        LONG bytes_before = total_bytes;
        
        if (copy_file_contents(from_array[i], dest_fh, buffer, &total_bytes)) {
            file_bytes = total_bytes - bytes_before;
            out_str(" (");
            out_num(file_bytes);
            out_str(" bytes)\n");
            files_joined++;
        } else {
            out_nl();
            result = 1;
            /* Continue with remaining files */
        }
    }
    
    /* Close destination file */
    Close(dest_fh);
    
    /* Clean up buffer */
    FreeMem(buffer, COPY_BUFFER_SIZE);
    
    /* Print summary */
    if (result == 0 || files_joined > 0) {
        out_str("Joined ");
        out_num(files_joined);
        out_str(" file(s), ");
        out_num(total_bytes);
        out_str(" bytes total\n");
    }
    
    /* If we had errors, delete the partial output file */
    if (result != 0 && files_joined == 0) {
        DeleteFile((STRPTR)to_path);
    }
    
    /* Ensure output is flushed */
    Flush(Output());
    
    FreeArgs(rda);
    return result;
}
