/*
 * TYPE command - Display file contents
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

/* Buffer size for reading */
#define BUFFER_SIZE 4096

/* Display file contents */
static int type_file(CONST_STRPTR path, BOOL show_hex, BOOL show_line_numbers)
{
    BPTR fh;
    UBYTE buffer[BUFFER_SIZE];
    LONG bytes_read;
    LONG total_bytes = 0;
    LONG line_number = 1;
    BOOL first_line = TRUE;
    
    /* Open the file */
    fh = Open(path, MODE_OLDFILE);
    if (!fh) {
        printf("TYPE: Can't open '%s' - %ld\n", path, IoErr());
        return 1;
    }
    
    if (show_line_numbers) {
        printf("%6ld  ", line_number);
    }
    
    /* Read and display file contents */
    while ((bytes_read = Read(fh, buffer, BUFFER_SIZE)) > 0) {
        if (show_hex) {
            /* Hex dump */
            for (int i = 0; i < bytes_read; i++) {
                if (i % 16 == 0) {
                    if (i > 0) printf("\n");
                    printf("%08lx  ", total_bytes + i);
                }
                printf("%02x ", buffer[i]);
                if (i % 16 == 7) printf(" ");
            }
        } else {
            /* Text output */
            for (int i = 0; i < bytes_read; i++) {
                if (show_line_numbers && buffer[i] == '\n') {
                    putchar('\n');
                    line_number++;
                    printf("%6ld  ", line_number);
                } else {
                    putchar(buffer[i]);
                }
            }
        }
        total_bytes += bytes_read;
    }
    
    if (show_hex) {
        printf("\n");
    }
    
    if (bytes_read < 0) {
        printf("\nTYPE: Error reading '%s' - %ld\n", path, IoErr());
        Close(fh);
        return 1;
    }
    
    Close(fh);
    
    if (show_hex) {
        printf("\nTotal: %d bytes\n", total_bytes);
    }
    
    return 0;
}

int main(int argc, char **argv)
{
    CONST_STRPTR path = NULL;
    BOOL show_hex = FALSE;
    BOOL show_line_numbers = FALSE;
    int i;
    
    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            /* Option */
            switch (argv[i][1]) {
                case 'h':
                case 'x':
                    show_hex = TRUE;
                    break;
                case 'n':
                    show_line_numbers = TRUE;
                    break;
                case '?':
                    printf("TYPE - Display file contents (v%s)\n", VERSION);
                    printf("Usage: TYPE [options] <file>\n");
                    printf("Options:\n");
                    printf("  -h,-x    Hex dump\n");
                    printf("  -n       Show line numbers\n");
                    return 0;
                default:
                    printf("TYPE: Unknown option '%s'\n", argv[i]);
                    return 1;
            }
        } else {
            /* Path argument */
            if (path) {
                printf("TYPE: Only one file can be specified\n");
                return 1;
            }
            path = (CONST_STRPTR)argv[i];
        }
    }
    
    /* Check for required argument */
    if (!path) {
        printf("TYPE: Missing filename\n");
        printf("Usage: TYPE [options] <file>\n");
        return 1;
    }
    
    return type_file(path, show_hex, show_line_numbers);
}
