/*
 * TYPE - display files
 *
 * Template: FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S
 *
 * Output of AmigaOS 3.1's Type (verified on the reference, Phase 221):
 *   NUMBER  "%5ld %s" per line
 *   HEX     "0000: 7365636F 6E642066 696C6520 77697468    second file with     "
 *           (offset, four groups of four bytes, the 16 characters with '.'
 *           for unprintable ones)
 *   OPT H / OPT N are HEX / NUMBER.
 *   TYPE can't open <file>
 *   <fault text>                                            (RC 10)
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

#define TEMPLATE "FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S"

enum { A_FROM, A_TO, A_OPT, A_HEX, A_NUMBER, A_COUNT };

static BPTR out;
static BOOL broken = FALSE;

static BOOL check_break(void)
{
    if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
        broken = TRUE;
    return broken;
}

static void type_hex(BPTR fh)
{
    UBYTE buf[16];
    LONG n, offset = 0;
    static const char hexd[] = "0123456789ABCDEF";

    while (!check_break() && (n = Read(fh, buf, 16)) > 0) {
        char line[100];
        int p = 0, i;
        LONG a[1];
        a[0] = offset;
        VFPrintf(out, (STRPTR)"%04lX: ", a);
        for (i = 0; i < 16; i++) {
            if (i && (i % 4) == 0)
                line[p++] = ' ';
            if (i < n) {
                line[p++] = hexd[buf[i] >> 4];
                line[p++] = hexd[buf[i] & 15];
            } else {
                line[p++] = ' ';
                line[p++] = ' ';
            }
        }
        line[p++] = ' ';
        line[p++] = ' ';
        line[p++] = ' ';
        line[p++] = ' ';
        for (i = 0; i < 16; i++) {
            UBYTE c = i < n ? buf[i] : ' ';
            if (i < n && (c < 0x20 || (c >= 0x7f && c < 0xa0)))
                c = '.';
            line[p++] = c;
        }
        for (i = 0; i < 5; i++)
            line[p++] = ' ';
        line[p++] = '\n';
        Write(out, line, p);
        offset += n;
    }
}

static void type_number(BPTR fh)
{
    char line[1024];
    LONG num = 1;

    while (!check_break() && FGets(fh, (STRPTR)line, sizeof(line))) {
        LONG a[1];
        a[0] = num++;
        VFPrintf(out, (STRPTR)"%5ld ", a);
        FPuts(out, (STRPTR)line);
    }
}

static void type_plain(BPTR fh)
{
    char buf[512];
    LONG n;

    while (!check_break() && (n = Read(fh, buf, sizeof(buf))) > 0)
        Write(out, buf, n);
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    STRPTR *from;
    BPTR tofh = 0;
    BOOL hex, number;
    LONG rc = 0;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    hex = args[A_HEX] != 0;
    number = args[A_NUMBER] != 0;
    if (args[A_OPT]) {
        const char *o = (char *)args[A_OPT];
        for (; *o; o++) {
            if (*o == 'h' || *o == 'H')
                hex = TRUE;
            else if (*o == 'n' || *o == 'N')
                number = TRUE;
        }
    }
    out = Output();
    if (args[A_TO]) {
        tofh = Open((STRPTR)args[A_TO], MODE_NEWFILE);
        if (!tofh) {
            LONG err = IoErr();
            Printf((STRPTR)"TYPE can't open %s\n", args[A_TO]);
            PrintFault(err, NULL);
            FreeArgs(rda);
            SetIoErr(err);
            return RETURN_ERROR;
        }
        out = tofh;
    }

    for (from = (STRPTR *)args[A_FROM]; *from && !broken; from++) {
        BPTR fh = Open(*from, MODE_OLDFILE);
        if (!fh) {
            LONG err = IoErr();
            Printf((STRPTR)"TYPE can't open %s\n", (LONG)*from);
            PrintFault(err, NULL);
            SetIoErr(err);
            rc = RETURN_ERROR;
            break;
        }
        if (hex)
            type_hex(fh);
        else if (number)
            type_number(fh);
        else
            type_plain(fh);
        Close(fh);
    }
    if (broken) {
        PutStr((STRPTR)"***Break\n");
        rc = RETURN_WARN;
    }
    if (tofh)
        Close(tofh);
    FreeArgs(rda);
    return rc;
}
