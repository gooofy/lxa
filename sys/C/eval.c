/*
 * EVAL - evaluate an integer expression
 *
 * Template: VALUE1/A,OP,VALUE2/M,TO/K,LFORMAT/K
 *
 * Behaviour of AmigaOS 3.1's Eval (verified on the reference, Phase 221):
 *  - the expression is evaluated strictly from left to right (no operator
 *    precedence); parentheses group;
 *  - operators: + - * / MOD (M, %) & | LSH (L) RSH (R) XOR (X) EQV (E),
 *    unary - and ~;
 *  - numbers: decimal, 0x/#x hexadecimal, 0/# octal, 'c character codes;
 *    anything else counts as 0; division by zero gives 0;
 *  - LFORMAT: %N decimal, %C character, %X hexadecimal and %O octal; %X
 *    and %O take the next character as their digit count (a non-digit
 *    counts as 1 and is used up);
 *  - "Mismatched parenthesis" is reported, the value is still printed.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "VALUE1/A,OP,VALUE2/M,TO/K,LFORMAT/K"

enum { A_VALUE1, A_OP, A_VALUE2, A_TO, A_LFORMAT, A_COUNT };

static const char *pos;
static BOOL paren_error = FALSE;

static void skip(void)
{
    while (*pos == ' ' || *pos == '\t')
        pos++;
}

static int digit(char c, int base)
{
    int v;
    if (c >= '0' && c <= '9')
        v = c - '0';
    else if (c >= 'a' && c <= 'f')
        v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        v = c - 'A' + 10;
    else
        return -1;
    return v < base ? v : -1;
}

static LONG number(void)
{
    LONG v = 0;
    int base = 10, d;

    if (*pos == '\'') {
        pos++;
        if (*pos)
            v = (UBYTE)*pos++;
        if (*pos == '\'')
            pos++;
        return v;
    }
    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X')) {
        base = 16;
        pos += 2;
    } else if (pos[0] == '#' && (pos[1] == 'x' || pos[1] == 'X')) {
        base = 16;
        pos += 2;
    } else if (pos[0] == '#') {
        base = 8;
        pos++;
    } else if (pos[0] == '0') {
        base = 8;
    }
    while ((d = digit(*pos, base)) >= 0) {
        v = v * base + d;
        pos++;
    }
    /* an unknown word counts as 0 */
    while (*pos && *pos != ' ' && *pos != '\t' && *pos != ')' && *pos != '(' &&
           !strchr("+-*/%&|~", *pos))
        pos++;
    return v;
}

static LONG expression(void);

static LONG operand(void)
{
    skip();
    if (*pos == '-') {
        pos++;
        return -operand();
    }
    if (*pos == '~') {
        pos++;
        return ~operand();
    }
    if (*pos == '(') {
        LONG v;
        pos++;
        v = expression();
        skip();
        if (*pos == ')')
            pos++;
        else
            paren_error = TRUE;
        return v;
    }
    return number();
}

/* the operator at pos (advanced past it), 0 if none */
static int op(void)
{
    static const struct { const char *word; int op; } words[] = {
        { "MOD", '%' }, { "LSH", 'L' }, { "RSH", 'R' }, { "XOR", 'X' }, { "EQV", 'E' },
        { "M", '%' }, { "L", 'L' }, { "R", 'R' }, { "X", 'X' }, { "E", 'E' },
        { NULL, 0 }
    };
    int i;

    skip();
    if (!*pos || *pos == ')')
        return 0;
    if (strchr("+-*/%&|", *pos))
        return *pos++;
    if (pos[0] == '<' && pos[1] == '<') {
        pos += 2;
        return 'L';
    }
    if (pos[0] == '>' && pos[1] == '>') {
        pos += 2;
        return 'R';
    }
    for (i = 0; words[i].word; i++) {
        int n = strlen(words[i].word);
        if (!strnicmp(pos, words[i].word, n) && (pos[n] == ' ' || pos[n] == '\t' || !pos[n] ||
                                                  pos[n] == '(' || digit(pos[n], 10) >= 0)) {
            pos += n;
            return words[i].op;
        }
    }
    /* unknown: skip the word */
    while (*pos && *pos != ' ' && *pos != '\t')
        pos++;
    return 0;
}

static LONG expression(void)
{
    LONG v = operand();
    int o;

    while ((o = op())) {
        LONG r = operand();
        switch (o) {
        case '+': v += r; break;
        case '-': v -= r; break;
        case '*': v *= r; break;
        case '/': v = r ? v / r : 0; break;
        case '%': v = r ? v % r : 0; break;
        case '&': v &= r; break;
        case '|': v |= r; break;
        case 'L': v = (LONG)((ULONG)v << r); break;
        case 'R': v = (LONG)((ULONG)v >> r); break;
        case 'X': v ^= r; break;
        case 'E': v = ~(v ^ r); break;
        }
    }
    return v;
}

static void lformat(BPTR out, const char *f, LONG v)
{
    static const char hexd[] = "0123456789ABCDEF";
    char buf[40];

    for (; *f; f++) {
        if (*f != '%' || !f[1]) {
            FPutC(out, *f);
            continue;
        }
        f++;
        switch (*f) {
        case 'n': case 'N': {
            LONG a[1];
            a[0] = v;
            VFPrintf(out, (STRPTR)"%ld", a);
            break;
        }
        case 'c': case 'C':
            FPutC(out, (UBYTE)v);
            break;
        case 'x': case 'X':
        case 'o': case 'O': {
            int shift = (*f == 'x' || *f == 'X') ? 4 : 3;
            int width = 1, i;
            if (f[1]) {
                f++;
                if (*f >= '1' && *f <= '9')
                    width = *f - '0';
            }
            for (i = 0; i < width; i++)
                buf[width - 1 - i] = hexd[((ULONG)v >> (shift * i)) & ((1 << shift) - 1)];
            buf[width] = '\0';
            FPuts(out, (STRPTR)buf);
            break;
        }
        default:
            FPutC(out, '%');
            FPutC(out, *f);
            break;
        }
    }
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    static char expr[512];
    BPTR out;
    LONG v;
    STRPTR *m;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    strcpy(expr, (char *)args[A_VALUE1]);
    if (args[A_OP]) {
        strcat(expr, " ");
        strncat(expr, (char *)args[A_OP], sizeof(expr) - strlen(expr) - 1);
    }
    for (m = (STRPTR *)args[A_VALUE2]; m && *m; m++) {
        strncat(expr, " ", sizeof(expr) - strlen(expr) - 1);
        strncat(expr, (char *)*m, sizeof(expr) - strlen(expr) - 1);
    }
    pos = expr;
    v = expression();
    skip();
    if (*pos == ')')
        paren_error = TRUE;
    if (paren_error)
        PutStr((STRPTR)"Mismatched parenthesis\n");

    out = Output();
    if (args[A_TO]) {
        out = Open((STRPTR)args[A_TO], MODE_NEWFILE);
        if (!out) {
            LONG err = IoErr();
            PrintFault(err, (STRPTR)args[A_TO]);
            FreeArgs(rda);
            return RETURN_FAIL;
        }
    }
    if (args[A_LFORMAT]) {
        lformat(out, (char *)args[A_LFORMAT], v);
    } else {
        LONG a[1];
        a[0] = v;
        VFPrintf(out, (STRPTR)"%ld\n", a);
    }
    if (args[A_TO])
        Close(out);
    FreeArgs(rda);
    return 0;
}
