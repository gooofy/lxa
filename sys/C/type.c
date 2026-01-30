/*
 * TYPE command - Display file contents
 * Phase 7.5 implementation for lxa
 * 
 * Template: FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S
 * 
 * Features:
 *   - Multiple file support
 *   - Output redirection with TO keyword
 *   - HEX dump with ASCII column
 *   - Line numbering with NUMBER switch
 *   - Ctrl+C break handling
 *   - Single-file header suppression
 */

#include <exec/types.h>
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
#define TEMPLATE "FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S"

/* Argument array indices */
#define ARG_FROM    0
#define ARG_TO      1
#define ARG_OPT     2
#define ARG_HEX     3
#define ARG_NUMBER  4
#define ARG_COUNT   5

/* Buffer size for reading - use moderate size to avoid stack issues */
#define BUFFER_SIZE 512

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

/* Helper: output a string to a file handle */
static void out_str(BPTR fh, const char *str)
{
    Write(fh, (STRPTR)str, strlen(str));
}

/* Helper: output newline */
static void out_nl(BPTR fh)
{
    Write(fh, (STRPTR)"\n", 1);
}

/* Helper: output a number with padding */
static void out_num_padded(BPTR fh, ULONG num, int width, char pad)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    
    do {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    
    int len = buf + sizeof(buf) - 1 - p;
    while (len < width) {
        char c = pad;
        Write(fh, &c, 1);
        len++;
    }
    out_str(fh, p);
}

/* Helper: output hex byte */
static void out_hex_byte(BPTR fh, UBYTE b)
{
    const char *hex = "0123456789abcdef";
    char buf[3];
    buf[0] = hex[(b >> 4) & 0xF];
    buf[1] = hex[b & 0xF];
    buf[2] = '\0';
    out_str(fh, buf);
}

/* Helper: output 8-digit hex number */
static void out_hex_long(BPTR fh, ULONG num)
{
    out_hex_byte(fh, (UBYTE)((num >> 24) & 0xFF));
    out_hex_byte(fh, (UBYTE)((num >> 16) & 0xFF));
    out_hex_byte(fh, (UBYTE)((num >> 8) & 0xFF));
    out_hex_byte(fh, (UBYTE)(num & 0xFF));
}

/* Display a hex dump of a buffer */
static BOOL display_hex_dump(BPTR output, const UBYTE *buffer, LONG bytes_read, LONG offset)
{
    for (int i = 0; i < bytes_read; i += 16) {
        if (check_break()) return FALSE;
        
        /* Offset */
        out_hex_long(output, (ULONG)(offset + i));
        out_str(output, "  ");
        
        /* Hex bytes */
        for (int j = 0; j < 16; j++) {
            if (i + j < bytes_read) {
                out_hex_byte(output, buffer[i + j]);
                out_str(output, " ");
            } else {
                out_str(output, "   ");
            }
            if (j == 7) out_str(output, " ");
        }
        
        out_str(output, " |");
        
        /* ASCII representation */
        for (int j = 0; j < 16; j++) {
            if (i + j < bytes_read) {
                UBYTE c = buffer[i + j];
                if (c >= 32 && c < 127) {
                    Write(output, &c, 1);
                } else {
                    out_str(output, ".");
                }
            } else {
                out_str(output, " ");
            }
        }
        
        out_str(output, "|\n");
    }
    return TRUE;
}

/* Display a single file */
static int type_single_file(CONST_STRPTR path, BPTR output, BOOL show_hex, 
                            BOOL show_line_numbers, BOOL show_header)
{
    BPTR fh;
    UBYTE buffer[BUFFER_SIZE];
    LONG bytes_read;
    LONG total_bytes = 0;
    LONG line_number = 1;
    BOOL line_start = TRUE;
    
    /* Check for break */
    if (check_break()) {
        out_str(output, "***BREAK\n");
        return 1;
    }
    
    /* Open the file */
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh) {
        out_str(output, "TYPE: Can't open '");
        out_str(output, (char *)path);
        out_str(output, "' - ");
        out_num_padded(output, IoErr(), 1, ' ');
        out_nl(output);
        return 1;
    }
    
    /* Print filename header only for multiple files */
    if (show_header && !show_hex) {
        out_str(output, "---------- ");
        out_str(output, (char *)path);
        out_str(output, " ----------\n");
    }
    
    /* Read and display file contents */
    while ((bytes_read = Read(fh, buffer, BUFFER_SIZE)) > 0) {
        if (check_break()) {
            out_str(output, "\n***BREAK\n");
            Close(fh);
            return 1;
        }
        
        if (show_hex) {
            /* Hex dump */
            if (!display_hex_dump(output, buffer, bytes_read, total_bytes)) {
                Close(fh);
                return 1;
            }
        } else {
            /* Text output */
            for (int i = 0; i < bytes_read; i++) {
                if (show_line_numbers) {
                    if (line_start) {
                        out_num_padded(output, line_number, 6, ' ');
                        out_str(output, "  ");
                        line_start = FALSE;
                    }
                    
                    Write(output, &buffer[i], 1);
                    
                    if (buffer[i] == '\n') {
                        line_number++;
                        line_start = TRUE;
                    }
                } else {
                    Write(output, &buffer[i], 1);
                }
            }
        }
        total_bytes += bytes_read;
    }
    
    if (bytes_read < 0) {
        out_str(output, "\nTYPE: Error reading '");
        out_str(output, (char *)path);
        out_str(output, "' - ");
        out_num_padded(output, IoErr(), 1, ' ');
        out_nl(output);
        Close(fh);
        return 1;
    }
    
    Close(fh);
    
    /* Add newline if file doesn't end with one */
    if (!show_hex && !line_start) {
        out_nl(output);
    }
    
    if (show_hex) {
        out_str(output, "\nTotal: ");
        out_num_padded(output, total_bytes, 1, ' ');
        out_str(output, " bytes\n");
    }
    
    return 0;
}

/* Count files in array */
static int count_files(STRPTR *file_array)
{
    int count = 0;
    if (file_array) {
        while (file_array[count] != NULL) count++;
    }
    return count;
}

int main(int argc, char **argv)
{
    LONG args[ARG_COUNT] = {0};
    struct RDArgs *rda;
    int result = 0;
    BPTR output = Output();
    BPTR outfile = 0;
    
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
            out_str(output, "TYPE: FROM argument is required\n");
        } else {
            out_str(output, "TYPE: Error parsing arguments - ");
            out_num_padded(output, err, 1, ' ');
            out_nl(output);
        }
        out_str(output, "Usage: TYPE FROM/A/M [TO filename] [HEX] [NUMBER]\n");
        out_str(output, "Template: ");
        out_str(output, TEMPLATE);
        out_nl(output);
        return 1;
    }
    
    /* Extract arguments */
    STRPTR *from_array = (STRPTR *)args[ARG_FROM];
    
    CONST_STRPTR to_path = (CONST_STRPTR)args[ARG_TO];
    CONST_STRPTR opt_str = (CONST_STRPTR)args[ARG_OPT];
    BOOL hex_flag = args[ARG_HEX] ? TRUE : FALSE;
    BOOL number_flag = args[ARG_NUMBER] ? TRUE : FALSE;
    
    /* Handle OPT keyword for legacy compatibility */
    if (opt_str) {
        const char *s = (const char *)opt_str;
        while (*s) {
            if (*s == 'H' || *s == 'h') hex_flag = TRUE;
            if (*s == 'N' || *s == 'n') number_flag = TRUE;
            s++;
        }
    }
    
    /* Open output file if TO keyword specified */
    if (to_path) {
        outfile = Open((STRPTR)to_path, MODE_NEWFILE);
        if (!outfile) {
            out_str(output, "TYPE: Can't create output file '");
            out_str(output, (char *)to_path);
            out_str(output, "'\n");
            FreeArgs(rda);
            return 1;
        }
        output = outfile;
    }
    
    /* Count number of files */
    int file_count = count_files(from_array);
    BOOL show_header = (file_count > 1);  /* Only show header for multiple files */
    
    /* Process each input file */
    if (from_array) {
        for (int i = 0; from_array[i] != NULL; i++) {
            if (check_break()) {
                out_str(output, "***BREAK\n");
                result = 1;
                break;
            }
            
            /* Add blank line between files (except first) */
            if (i > 0 && !hex_flag) {
                out_nl(output);
            }
            
            if (type_single_file(from_array[i], output, hex_flag, number_flag, show_header) != 0) {
                result = 1;
            }
        }
    }
    
    /* Ensure output is flushed */
    Flush(output);
    
    /* Clean up */
    if (outfile) {
        Close(outfile);
    }
    
    FreeArgs(rda);
    return result;
}
