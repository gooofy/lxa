/*
 * EVAL command - Evaluate arithmetic expressions
 * Step 9.3 implementation for lxa
 * 
 * Template: VALUE1/A,OP,VALUE2/M,TO/K,LFORMAT/K
 * 
 * Features:
 *   - Basic arithmetic: + - * / mod
 *   - Comparison operators: EQ NE LT LE GT GE (return 1 or 0)
 *   - Bitwise operators: AND OR XOR NOT LAND LOR LNOT
 *   - TO: Output to file instead of stdout
 *   - LFORMAT: Custom output format (printf-like)
 *   - Supports negative numbers
 *   - Supports hex input (0x prefix or $ prefix)
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
#define TEMPLATE "VALUE1/A,OP,VALUE2/M,TO/K,LFORMAT/K"

/* Argument array indices */
#define ARG_VALUE1  0
#define ARG_OP      1
#define ARG_VALUE2  2
#define ARG_TO      3
#define ARG_LFORMAT 4
#define ARG_COUNT   5

/* Helper: output a string */
static void out_str(BPTR fh, const char *str)
{
    Write(fh, (STRPTR)str, strlen(str));
}

/* Helper: output newline */
static void out_nl(BPTR fh)
{
    Write(fh, (STRPTR)"\n", 1);
}

/* Helper: output a number (signed) */
static void out_num(BPTR fh, LONG num)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    BOOL neg = num < 0;
    ULONG n;
    
    *p = '\0';
    n = neg ? -num : num;
    
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n);
    
    if (neg) *--p = '-';
    out_str(fh, p);
}

/* Helper: output hex number */
static void out_hex(BPTR fh, ULONG num)
{
    const char *hex = "0123456789ABCDEF";
    char buf[12];
    char *p = buf + sizeof(buf) - 1;
    
    *p = '\0';
    
    do {
        *--p = hex[num & 0xF];
        num >>= 4;
    } while (num);
    
    out_str(fh, "0x");
    out_str(fh, p);
}

/* Case-insensitive string compare */
static BOOL str_eq_nocase(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return FALSE;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

/* Parse a number (decimal or hex) */
static BOOL parse_number(const char *s, LONG *result)
{
    BOOL neg = FALSE;
    LONG n = 0;
    
    /* Skip whitespace */
    while (*s == ' ' || *s == '\t') s++;
    
    /* Check for sign */
    if (*s == '-') {
        neg = TRUE;
        s++;
    } else if (*s == '+') {
        s++;
    }
    
    /* Check for hex prefix */
    if (*s == '$' || (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))) {
        if (*s == '$') {
            s++;
        } else {
            s += 2;
        }
        
        /* Parse hex digits */
        while (*s) {
            char c = *s;
            if (c >= '0' && c <= '9') {
                n = n * 16 + (c - '0');
            } else if (c >= 'a' && c <= 'f') {
                n = n * 16 + (c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                n = n * 16 + (c - 'A' + 10);
            } else {
                break;
            }
            s++;
        }
    } else {
        /* Parse decimal digits */
        while (*s >= '0' && *s <= '9') {
            n = n * 10 + (*s - '0');
            s++;
        }
    }
    
    *result = neg ? -n : n;
    return TRUE;
}

/* Output using format string */
static void format_output(BPTR fh, const char *fmt, LONG value)
{
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd':
                case 'D':
                    out_num(fh, value);
                    break;
                case 'x':
                case 'X':
                    out_hex(fh, (ULONG)value);
                    break;
                case 'c':
                case 'C':
                    if (value >= 32 && value < 127) {
                        char c = (char)value;
                        Write(fh, &c, 1);
                    }
                    break;
                case 'n':
                case 'N':
                    out_num(fh, value);
                    break;
                case '%':
                    Write(fh, (STRPTR)"%", 1);
                    break;
                default:
                    /* Unknown format - output literal */
                    Write(fh, (STRPTR)"%", 1);
                    if (*fmt) {
                        Write(fh, (STRPTR)fmt, 1);
                    }
                    break;
            }
            if (*fmt) fmt++;
        } else if (*fmt == '\\') {
            fmt++;
            switch (*fmt) {
                case 'n':
                    Write(fh, (STRPTR)"\n", 1);
                    break;
                case 't':
                    Write(fh, (STRPTR)"\t", 1);
                    break;
                case '\\':
                    Write(fh, (STRPTR)"\\", 1);
                    break;
                default:
                    Write(fh, (STRPTR)"\\", 1);
                    if (*fmt) {
                        Write(fh, (STRPTR)fmt, 1);
                    }
                    break;
            }
            if (*fmt) fmt++;
        } else {
            Write(fh, (STRPTR)fmt, 1);
            fmt++;
        }
    }
}

/* Main entry point */
int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    STRPTR value1_str;
    STRPTR op_str;
    STRPTR *value2_array;
    STRPTR to_file;
    STRPTR lformat;
    LONG value1, value2 = 0;
    LONG result;
    BPTR fh_out = 0;
    BOOL opened_output = FALSE;
    int rc = RETURN_OK;
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"EVAL");
        return RETURN_FAIL;
    }
    
    value1_str = (STRPTR)args[ARG_VALUE1];
    op_str = (STRPTR)args[ARG_OP];
    value2_array = (STRPTR *)args[ARG_VALUE2];
    to_file = (STRPTR)args[ARG_TO];
    lformat = (STRPTR)args[ARG_LFORMAT];
    
    /* Parse first value */
    if (!parse_number((char *)value1_str, &value1)) {
        out_str(Output(), "Bad number: ");
        out_str(Output(), (char *)value1_str);
        out_nl(Output());
        FreeArgs(rdargs);
        return RETURN_ERROR;
    }
    
    /* If no operator, just output the value */
    if (!op_str) {
        result = value1;
    } else {
        /* Parse second value (first element of VALUE2 array) */
        if (!value2_array || !value2_array[0]) {
            out_str(Output(), "Missing second value\n");
            FreeArgs(rdargs);
            return RETURN_ERROR;
        }
        
        if (!parse_number((char *)value2_array[0], &value2)) {
            out_str(Output(), "Bad number: ");
            out_str(Output(), (char *)value2_array[0]);
            out_nl(Output());
            FreeArgs(rdargs);
            return RETURN_ERROR;
        }
        
        /* Perform operation */
        if (str_eq_nocase((char *)op_str, "+") || str_eq_nocase((char *)op_str, "ADD")) {
            result = value1 + value2;
        } else if (str_eq_nocase((char *)op_str, "-") || str_eq_nocase((char *)op_str, "SUB")) {
            result = value1 - value2;
        } else if (str_eq_nocase((char *)op_str, "*") || str_eq_nocase((char *)op_str, "MUL")) {
            result = value1 * value2;
        } else if (str_eq_nocase((char *)op_str, "/") || str_eq_nocase((char *)op_str, "DIV")) {
            if (value2 == 0) {
                out_str(Output(), "Division by zero\n");
                FreeArgs(rdargs);
                return RETURN_ERROR;
            }
            result = value1 / value2;
        } else if (str_eq_nocase((char *)op_str, "MOD") || str_eq_nocase((char *)op_str, "%")) {
            if (value2 == 0) {
                out_str(Output(), "Division by zero\n");
                FreeArgs(rdargs);
                return RETURN_ERROR;
            }
            result = value1 % value2;
        } else if (str_eq_nocase((char *)op_str, "AND")) {
            result = value1 & value2;
        } else if (str_eq_nocase((char *)op_str, "OR")) {
            result = value1 | value2;
        } else if (str_eq_nocase((char *)op_str, "XOR")) {
            result = value1 ^ value2;
        } else if (str_eq_nocase((char *)op_str, "NOT")) {
            result = ~value1;  /* Unary, ignore value2 */
        } else if (str_eq_nocase((char *)op_str, "LAND")) {
            result = (value1 && value2) ? 1 : 0;
        } else if (str_eq_nocase((char *)op_str, "LOR")) {
            result = (value1 || value2) ? 1 : 0;
        } else if (str_eq_nocase((char *)op_str, "LNOT")) {
            result = (!value1) ? 1 : 0;  /* Unary, ignore value2 */
        } else if (str_eq_nocase((char *)op_str, "EQ") || str_eq_nocase((char *)op_str, "=")) {
            result = (value1 == value2) ? 1 : 0;
        } else if (str_eq_nocase((char *)op_str, "NE") || str_eq_nocase((char *)op_str, "<>")) {
            result = (value1 != value2) ? 1 : 0;
        } else if (str_eq_nocase((char *)op_str, "LT") || str_eq_nocase((char *)op_str, "<")) {
            result = (value1 < value2) ? 1 : 0;
        } else if (str_eq_nocase((char *)op_str, "LE") || str_eq_nocase((char *)op_str, "<=")) {
            result = (value1 <= value2) ? 1 : 0;
        } else if (str_eq_nocase((char *)op_str, "GT") || str_eq_nocase((char *)op_str, ">")) {
            result = (value1 > value2) ? 1 : 0;
        } else if (str_eq_nocase((char *)op_str, "GE") || str_eq_nocase((char *)op_str, ">=")) {
            result = (value1 >= value2) ? 1 : 0;
        } else if (str_eq_nocase((char *)op_str, "LSHIFT") || str_eq_nocase((char *)op_str, "<<")) {
            result = value1 << value2;
        } else if (str_eq_nocase((char *)op_str, "RSHIFT") || str_eq_nocase((char *)op_str, ">>")) {
            result = value1 >> value2;
        } else {
            out_str(Output(), "Unknown operator: ");
            out_str(Output(), (char *)op_str);
            out_nl(Output());
            FreeArgs(rdargs);
            return RETURN_ERROR;
        }
    }
    
    /* Open output file if specified */
    if (to_file) {
        fh_out = Open(to_file, MODE_NEWFILE);
        if (!fh_out) {
            PrintFault(IoErr(), to_file);
            FreeArgs(rdargs);
            return RETURN_FAIL;
        }
        opened_output = TRUE;
    } else {
        fh_out = Output();
    }
    
    /* Output result */
    if (lformat) {
        format_output(fh_out, (char *)lformat, result);
    } else {
        out_num(fh_out, result);
        out_nl(fh_out);
    }
    
    /* Cleanup */
    if (opened_output) {
        Close(fh_out);
    }
    FreeArgs(rdargs);
    
    return rc;
}
