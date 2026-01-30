/*
 * TYPE command - Display file contents
 * Phase 4 implementation for lxa
 * 
 * Template: FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S
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

/* Command template */
#define TEMPLATE "FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S"

/* Argument array indices */
#define ARG_FROM    0
#define ARG_TO      1
#define ARG_OPT     2
#define ARG_HEX     3
#define ARG_NUMBER  4
#define ARG_COUNT   5

/* Buffer size for reading */
#define BUFFER_SIZE 4096

/* Display a hex dump of a buffer */
static void display_hex_dump(const UBYTE *buffer, LONG bytes_read, LONG offset)
{
    for (int i = 0; i < bytes_read; i += 16) {
        printf("%08lx  ", offset + i);
        
        /* Hex bytes */
        for (int j = 0; j < 16; j++) {
            if (i + j < bytes_read) {
                printf("%02x ", buffer[i + j]);
            } else {
                printf("   ");
            }
            if (j == 7) printf(" ");
        }
        
        printf(" |");
        
        /* ASCII representation */
        for (int j = 0; j < 16; j++) {
            if (i + j < bytes_read) {
                UBYTE c = buffer[i + j];
                if (c >= 32 && c < 127) {
                    printf("%c", c);
                } else {
                    printf(".");
                }
            } else {
                printf(" ");
            }
        }
        
        printf("|\n");
    }
}

/* Display a single file */
static int type_single_file(CONST_STRPTR path, FILE *output, BOOL show_hex, BOOL show_line_numbers)
{
    BPTR fh;
    UBYTE buffer[BUFFER_SIZE];
    LONG bytes_read;
    LONG total_bytes = 0;
    LONG line_number = 1;
    BOOL in_line = FALSE;
    
    /* Open the file */
    fh = Open(path, MODE_OLDFILE);
    if (!fh) {
        fprintf(output, "TYPE: Can't open '%s' - %ld\n", path, IoErr());
        return 1;
    }
    
    /* Print filename header for multiple files */
    if (!show_hex) {
        fprintf(output, "---------- %s ----------\n", path);
    }
    
    if (show_line_numbers && !show_hex) {
        fprintf(output, "%6ld  ", line_number);
        in_line = TRUE;
    }
    
    /* Read and display file contents */
    while ((bytes_read = Read(fh, buffer, BUFFER_SIZE)) > 0) {
        if (show_hex) {
            /* Hex dump */
            display_hex_dump(buffer, bytes_read, total_bytes);
        } else {
            /* Text output */
            for (int i = 0; i < bytes_read; i++) {
                if (show_line_numbers) {
                    if (buffer[i] == '\n') {
                        fputc('\n', output);
                        line_number++;
                        fprintf(output, "%6ld  ", line_number);
                        in_line = TRUE;
                    } else {
                        fputc(buffer[i], output);
                        in_line = TRUE;
                    }
                } else {
                    fputc(buffer[i], output);
                }
            }
        }
        total_bytes += bytes_read;
    }
    
    /* Clean up line numbering */
    if (show_line_numbers && !show_hex && in_line) {
        fputc('\n', output);
    }
    
    if (bytes_read < 0) {
        fprintf(output, "\nTYPE: Error reading '%s' - %ld\n", path, IoErr());
        Close(fh);
        return 1;
    }
    
    Close(fh);
    
    if (show_hex) {
        fprintf(output, "\nTotal: %ld bytes\n", total_bytes);
    }
    
    return 0;
}

int main(int argc, char **argv)
{
    LONG args[ARG_COUNT] = {0};
    struct RDArgs *rda;
    int result = 0;
    
    (void)argc;
    (void)argv;
    
    /* Parse arguments using AmigaDOS template */
    rda = ReadArgs(TEMPLATE, args, NULL);
    if (!rda) {
        LONG err = IoErr();
        if (err == ERROR_REQUIRED_ARG_MISSING) {
            printf("TYPE: FROM argument is required\n");
        } else {
            printf("TYPE: Error parsing arguments - %ld\n", err);
        }
        printf("Usage: TYPE FROM/A/M [TO filename] [HEX] [NUMBER]\n");
        printf("Template: %s\n", TEMPLATE);
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
        if (strstr(opt_str, "H") || strstr(opt_str, "h")) {
            hex_flag = TRUE;
        }
        if (strstr(opt_str, "N") || strstr(opt_str, "n")) {
            number_flag = TRUE;
        }
    }
    
    /* Open output file if TO keyword specified */
    FILE *output = stdout;
    FILE *outfile = NULL;
    if (to_path) {
        outfile = fopen(to_path, "w");
        if (!outfile) {
            printf("TYPE: Can't create output file '%s'\n", to_path);
            FreeArgs(rda);
            return 1;
        }
        output = outfile;
    }
    
    /* Process each input file */
    if (from_array) {
        for (int i = 0; from_array[i] != NULL; i++) {
            if (type_single_file(from_array[i], output, hex_flag, number_flag) != 0) {
                result = 1;
            }
        }
    }
    
    /* Clean up */
    if (outfile) {
        fclose(outfile);
    }
    
    FreeArgs(rda);
    return result;
}
