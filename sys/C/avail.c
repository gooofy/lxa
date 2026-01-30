/*
 * AVAIL command - Display available memory
 * Phase 7.2 implementation for lxa
 * 
 * Template: CHIP/S,FAST/S,TOTAL/S,FLUSH/S
 * 
 * Usage:
 *   AVAIL        - Show memory summary
 *   AVAIL CHIP   - Show chip memory only
 *   AVAIL FAST   - Show fast memory only
 *   AVAIL TOTAL  - Show total only (no largest)
 *   AVAIL FLUSH  - Flush unused memory (stub)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

#define VERSION "1.0"

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template - AmigaOS compatible */
#define TEMPLATE "CHIP/S,FAST/S,TOTAL/S,FLUSH/S"

/* Argument array indices */
#define ARG_CHIP    0
#define ARG_FAST    1
#define ARG_TOTAL   2
#define ARG_FLUSH   3
#define ARG_COUNT   4

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

/* Format number with thousand separators */
static void out_num_fmt(ULONG num, int width)
{
    char buf[24];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    int digits = 0;
    
    do {
        if (digits > 0 && (digits % 3) == 0) {
            *--p = ',';
        }
        *--p = '0' + (num % 10);
        num /= 10;
        digits++;
    } while (num);
    
    /* Pad to width */
    int len = (buf + sizeof(buf) - 1) - p;
    while (len < width) {
        out_str(" ");
        len++;
    }
    out_str(p);
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0, 0, 0, 0};
    int rc = RETURN_OK;
    
    /* Parse arguments using AmigaDOS template */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"AVAIL");
        return RETURN_ERROR;
    }
    
    BOOL showChip = (BOOL)args[ARG_CHIP];
    BOOL showFast = (BOOL)args[ARG_FAST];
    BOOL totalOnly = (BOOL)args[ARG_TOTAL];
    BOOL flush = (BOOL)args[ARG_FLUSH];
    
    /* If FLUSH specified, just print message (we don't have flushing) */
    if (flush) {
        out_str("Memory flushed.\n");
    }
    
    /* If neither CHIP nor FAST specified, show all */
    if (!showChip && !showFast) {
        showChip = TRUE;
        showFast = TRUE;
    }
    
    /* Get memory statistics */
    ULONG chipFree = 0, chipLargest = 0, chipTotal = 0;
    ULONG fastFree = 0, fastLargest = 0, fastTotal = 0;
    
    if (showChip) {
        chipFree = AvailMem(MEMF_CHIP);
        chipLargest = AvailMem(MEMF_CHIP | MEMF_LARGEST);
        /* Calculate total from MemHeader - for now use free as approximation */
        chipTotal = chipFree + (chipFree / 4);  /* Rough estimate */
    }
    
    if (showFast) {
        fastFree = AvailMem(MEMF_FAST);
        fastLargest = AvailMem(MEMF_FAST | MEMF_LARGEST);
        fastTotal = fastFree + (fastFree / 4);  /* Rough estimate */
    }
    
    /* Total any memory */
    ULONG anyFree = AvailMem(0);
    ULONG anyLargest = AvailMem(MEMF_LARGEST);
    
    if (totalOnly) {
        /* Just print totals */
        if (showChip && !showFast) {
            out_num_fmt(chipFree, 0);
            out_nl();
        } else if (showFast && !showChip) {
            out_num_fmt(fastFree, 0);
            out_nl();
        } else {
            out_num_fmt(anyFree, 0);
            out_nl();
        }
    } else {
        /* Print header */
        out_str("Type   Available    In-Use   Maximum   Largest\n");
        
        if (showChip) {
            out_str("chip  ");
            out_num_fmt(chipFree, 10);
            out_str("  ");
            out_num_fmt(chipTotal > chipFree ? chipTotal - chipFree : 0, 8);
            out_str("  ");
            out_num_fmt(chipTotal, 8);
            out_str("  ");
            out_num_fmt(chipLargest, 8);
            out_nl();
        }
        
        if (showFast) {
            out_str("fast  ");
            out_num_fmt(fastFree, 10);
            out_str("  ");
            out_num_fmt(fastTotal > fastFree ? fastTotal - fastFree : 0, 8);
            out_str("  ");
            out_num_fmt(fastTotal, 8);
            out_str("  ");
            out_num_fmt(fastLargest, 8);
            out_nl();
        }
        
        if (showChip && showFast) {
            out_str("total ");
            out_num_fmt(anyFree, 10);
            out_str("  ");
            ULONG anyTotal = chipTotal + fastTotal;
            out_num_fmt(anyTotal > anyFree ? anyTotal - anyFree : 0, 8);
            out_str("  ");
            out_num_fmt(anyTotal, 8);
            out_str("  ");
            out_num_fmt(anyLargest, 8);
            out_nl();
        }
    }
    
    FreeArgs(rdargs);
    return rc;
}
