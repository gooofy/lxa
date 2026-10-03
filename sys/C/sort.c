/*
 * SORT - sort the lines of a file
 *
 * Template: FROM/A,TO/A,COLSTART/K,CASE/S,NUMERIC/S
 *
 * Lines compare case-insensitively unless CASE is given; COLSTART n
 * compares from column n (1 = first); NUMERIC compares the leading
 * numbers.  Equal keys keep their order.  "Can't open <file>" (RC 10)
 * as AmigaOS 3.1 (verified on the reference, Phase 221).
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

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "FROM/A,TO/A,COLSTART/K,CASE/S,NUMERIC/S"

enum { A_FROM, A_TO, A_COLSTART, A_CASE, A_NUMERIC, A_COUNT };

static LONG args[A_COUNT];
static LONG colstart = 0;

static const char *key(const char *line)
{
    int n = strlen(line);
    return colstart < n ? line + colstart : line + n;
}

static LONG number(const char *s)
{
    LONG v = 0, sign = 1;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return v * sign;
}

static int compare(const char *a, const char *b)
{
    const char *ka = key(a), *kb = key(b);
    if (args[A_NUMERIC]) {
        LONG na = number(ka), nb = number(kb);
        return na < nb ? -1 : na > nb ? 1 : 0;
    }
    if (args[A_CASE])
        return strcmp(ka, kb);
    return stricmp(ka, kb);
}

int main(void)
{
    struct RDArgs *rda;
    BPTR in, out;
    char *text;
    char **lines;
    LONG size, nlines = 0, i, j;
    struct FileInfoBlock *fib;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    if (args[A_COLSTART]) {
        LONG c = 0;
        StrToLong((STRPTR)args[A_COLSTART], &c);
        colstart = c > 0 ? c - 1 : 0;
    }

    in = Open((STRPTR)args[A_FROM], MODE_OLDFILE);
    if (!in) {
        LONG err = IoErr();
        Printf((STRPTR)"Can't open %s\n", args[A_FROM]);
        FreeArgs(rda);
        SetIoErr(err);
        return RETURN_ERROR;
    }
    fib = AllocDosObject(DOS_FIB, NULL);
    size = (fib && ExamineFH(in, fib)) ? fib->fib_Size : 0;
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    text = AllocVec(size + 2, MEMF_PUBLIC);
    if (!text) {
        Close(in);
        PrintFault(ERROR_NO_FREE_STORE, NULL);
        FreeArgs(rda);
        return RETURN_FAIL;
    }
    size = Read(in, text, size);
    Close(in);
    if (size < 0)
        size = 0;
    if (size && text[size - 1] != '\n')
        text[size++] = '\n';
    text[size] = '\0';
    for (i = 0; i < size; i++)
        if (text[i] == '\n')
            nlines++;
    lines = AllocVec((nlines + 1) * sizeof(char *), MEMF_PUBLIC);
    if (!lines) {
        FreeVec(text);
        PrintFault(ERROR_NO_FREE_STORE, NULL);
        FreeArgs(rda);
        return RETURN_FAIL;
    }
    {
        char *p = text;
        for (i = 0; i < nlines; i++) {
            char *e = strchr(p, '\n');
            *e = '\0';
            lines[i] = p;
            p = e + 1;
        }
    }
    /* stable insertion sort */
    for (i = 1; i < nlines; i++) {
        char *l = lines[i];
        for (j = i - 1; j >= 0 && compare(lines[j], l) > 0; j--)
            lines[j + 1] = lines[j];
        lines[j + 1] = l;
    }

    out = Open((STRPTR)args[A_TO], MODE_NEWFILE);
    if (!out) {
        LONG err = IoErr();
        Printf((STRPTR)"Can't open %s\n", args[A_TO]);
        FreeVec(lines);
        FreeVec(text);
        FreeArgs(rda);
        SetIoErr(err);
        return RETURN_ERROR;
    }
    for (i = 0; i < nlines; i++) {
        FPuts(out, (STRPTR)lines[i]);
        FPutC(out, '\n');
    }
    Close(out);
    FreeVec(lines);
    FreeVec(text);
    FreeArgs(rda);
    return 0;
}
