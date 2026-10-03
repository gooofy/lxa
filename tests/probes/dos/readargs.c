/*
 * Probe (Phase 222a): dos.library ReadArgs (CSource input) - every
 * template modifier, error codes (IoErr) and the resulting argument array.
 * FindArg/ReadItem/StrToLong: argparse.c.  The command line read from
 * Input(): cmdline.c.
 *
 * Input without a trailing '\n' is only given to templates without /M:
 * the last unquoted item is read again and again (CS_UnReadChar after the
 * end), which a /M item would collect forever.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

/* item kinds from the template: 'S' switch/toggle, 'N' number, 'M' multi,
 * 'm' numeric multi, 'A' string */
static int kinds(const char *tmpl, char *out)
{
    int n = 0;
    const char *p = tmpl;
    while (*p && n < 16) {
        int isN = 0, isM = 0, isS = 0;
        while (*p && *p != ',') {
            if (*p == '/' && p[1]) {
                char c = p[1];
                if (c >= 'a' && c <= 'z')
                    c -= 32;
                if (c == 'N') isN = 1;
                if (c == 'M') isM = 1;
                if (c == 'S' || c == 'T') isS = 1;
                p++;
            }
            p++;
        }
        out[n++] = isS ? 'S' : isM ? (isN ? 'm' : 'M') : isN ? 'N' : 'A';
        if (*p == ',')
            p++;
    }
    return n;
}

static void run(const char *tmpl, const char *input)
{
    struct RDArgs *rda = AllocDosObject(DOS_RDARGS, NULL);
    LONG array[16];
    char k[16];
    int n = kinds(tmpl, k), i;
    LONG len = 0;
    struct RDArgs *res;

    for (i = 0; i < 16; i++)
        array[i] = 0;
    while (input[len])
        len++;
    rda->RDA_Source.CS_Buffer = (UBYTE *)input;
    rda->RDA_Source.CS_Length = len;
    rda->RDA_Source.CS_CurChr = 0;
    rda->RDA_Flags = RDAF_NOPROMPT;
    SetIoErr(0);
    res = ReadArgs((STRPTR)tmpl, array, rda);
    probe_s("[");
    probe_s(tmpl);
    probe_s("] \"");
    for (i = 0; i < len; i++) {
        if (input[i] == '\n')
            probe_s("\\n");
        else
            probe_ch(input[i]);
    }
    probe_s("\" -> ");
    if (!res) {
        probe_s("FAIL IoErr ");
        probe_dec(IoErr());
    } else {
        probe_s("OK CurChr ");
        probe_dec(rda->RDA_Source.CS_CurChr);
        for (i = 0; i < n; i++) {
            probe_s(" | ");
            switch (k[i]) {
            case 'S':
                probe_s(array[i] ? "T" : "F");
                if (array[i] && array[i] != -1) {
                    probe_ch('=');
                    probe_dec(array[i]);
                }
                break;
            case 'N':
                if (array[i]) probe_dec(*(LONG *)array[i]);
                else probe_s("-");
                break;
            case 'A':
                if (array[i]) { probe_ch('"'); probe_s((char *)array[i]); probe_ch('"'); }
                else probe_s("-");
                break;
            case 'M':
            case 'm':
                if (!array[i]) {
                    probe_s("-");
                    break;
                }
                {
                    LONG *m = (LONG *)array[i];
                    int c = 0;
                    probe_ch('{');
                    while (m[c] && c < 10) {
                        if (c)
                            probe_ch(',');
                        if (k[i] == 'm')
                            probe_dec(*(LONG *)m[c]);
                        else {
                            probe_ch('"');
                            probe_s((char *)m[c]);
                            probe_ch('"');
                        }
                        c++;
                    }
                    probe_ch('}');
                }
                break;
            }
        }
        FreeArgs(res);
    }
    probe_ch('\n');
    FreeDosObject(DOS_RDARGS, rda);
}

int main(void)
{
    P_SECTION("ReadArgs basics");
    run("FROM,TO", "a b\n");
    run("FROM,TO", "a\n");
    run("FROM,TO", "\n");
    run("FROM,TO", "a b c\n");
    run("FROM,TO", "TO x y\n");
    run("FROM,TO", "to=x from=y\n");
    run("FROM,TO", "TO=x FROM y\n");
    run("FROM,TO", "\"quoted arg\" \"with \"\"escapes\"\"\"\n");
    run("FROM,TO", "\"a*\"b*n\" c\n");
    run("FROM,TO", "a ; comment\n");
    run("FROM,TO", "  a   b  \n");
    run("FROM,TO", "a b");
    run("FROM,TO", "a\tb\n");
    run("FROM,TO", "=x\n");
    run("FROM,TO", "TO\n");
    run("FROM,TO", "\"\" b\n");
    run("FROM,TO", "a");
    run("FROM,TO", "");
    run("FROM,TO", "\"a\"");
    run("FROM", "a");
    run("FROM,TO,X", "a b");
    run("FROM,TO,X,Y", "a");
    run("FROM,TO", "a;b\n");
    run("FROM,TO", "a ;b\n");
    run("FROM,TO", "\"a\";x\n");
    run("FROM,TO", ";\n");
    run("FROM,TO", "a=b\n");
    run("FROM,TO", "a= b\n");
    run("FROM,TO", "TO= x\n");
    run("FROM,TO", "TO =x\n");
    run("FROM,TO", "TO = x\n");
    run("FROM,TO", "x=\n");
    run("FROM,TO", "a\nb\n");
    run("FROM,TO", "a b\nc\n");
    run("FROM,TO", "?\n");
    run("FROM,TO", "a\"b c\n");
    run("FROM,TO", "\"a\"b c\n");
    run("FROM,TO", "\"unterminated\n");

    P_SECTION("/A /K /S /T /N /F");
    run("FILE/A", "\n");
    run("FILE/A", "x\n");
    run("FILE/A,OPT/S", "OPT\n");
    run("FILE/A,OPT/S", "x OPT\n");
    run("FILE/A,OPT/S", "OPT x\n");
    run("FILE/A,OPT/S", "x opt=1\n");
    run("NAME/K", "x\n");
    run("NAME/K", "NAME x\n");
    run("NAME/K", "NAME=x\n");
    run("NAME/K", "NAME\n");
    run("NAME/K/A", "\n");
    run("ON/T", "ON\n");
    run("ON/T", "ON=yes\n");
    run("ON/T", "ON=no\n");
    run("ON/T", "ON=OFF\n");
    run("ON/T", "ON=1\n");
    run("ON/T", "\n");
    run("NUM/N", "42\n");
    run("NUM/N", "-17\n");
    run("NUM/N", "+5\n");
    run("NUM/N", "12abc\n");
    run("NUM/N", "abc\n");
    run("NUM/N", "0x10\n");
    run("NUM/N", "\n");
    run("NUM/N", "2147483648\n");
    run("NUM/N/A", "\n");
    run("NUM/K/N", "NUM 7\n");
    run("NUM/K/N", "NUM=-3\n");
    run("REST/F", "a b c\n");
    run("REST/F", "  \"a\" b ; c\n");
    run("X,REST/F", "a b c\n");
    run("REST/F,X", "a b c\n");
    run("X/K,REST/F", "X=1 a b\n");
    run("X/K,REST/F", "a b X=1\n");
    run("FILE/A,OPT/S", "x OPT OPT\n");
    run("FILE/A,OPT/S", "x OPT=\n");
    run("NAME/K", "NAME=\"x y\"\n");
    run("NAME/K", "NAME= x\n");
    run("NAME/K", "NAME=\n");
    run("NAME/K", "NAME=x NAME=y\n");
    run("ON/T", "ON=TRUE\n");
    run("ON/T", "ON=FALSE\n");
    run("ON/T", "ON yes\n");
    run("ON/T", "ON=Yes\n");
    run("ON/T", "ON=on\n");
    run("ON/T", "ON=y\n");
    run("ON/T", "ON=\"yes\"\n");
    run("ON/T", "ON=yes ON=no\n");
    run("ON/T", "yes\n");
    run("ON/T,X", "ON=no x\n");
    run("NUM/N", "007\n");
    run("NUM/N", "-0\n");
    run("NUM/N", "-\n");
    run("NUM/N", "+\n");
    run("NUM/N", "1 2\n");
    run("NUM/N", "-2147483648\n");
    run("NUM/N", "4294967296\n");
    run("NUM/N", "\"42\"\n");
    run("NUM/N", "\"\"\n");
    run("NUM/N", " 42 \n");
    run("REST/F", "a \"b c\" d\n");
    run("REST/F", "\"a b\" c\n");
    run("REST/F", "a*\"b c\n");
    run("REST/F", "\"a*nb\" c\n");
    run("REST/F", "a ; c\n");
    run("REST/F", "\"a\"\n");
    run("REST/F", "a  b   c\n");
    run("REST/F", "a \"b\n");
    run("REST/F", "a=b c\n");
    run("REST/F", "\n");
    run("REST/F", "  \n");
    run("REST/F/A", "\n");
    run("X,REST/F", "x \"y\" ; z\n");
    run("X/K,REST/F", "X=\"1\" a\n");
    run("REST/F,X/K", "a X=1\n");
    run("REST/F,X/S", "X a\n");
    run("REST/F", "a b=c\n");
    run("REST/F", "a\tb\n");
    run("REST/F", "a=b=c\n");
    run("REST/F", "\"a\"  b\n");
    run("REST/F", "\"a\"=b\n");
    run("REST/F", "a b  \n");
    run("REST/F", "a\t b\n");
    run("REST/F", "=a b\n");
    run("REST/F", "\"\" a\n");
    run("REST/F", "REST a b\n");
    run("REST/F", "REST=a b\n");
    run("REST/F", "REST \"a\" b\n");
    run("X,REST/F", "1 \"a\" b\n");
    run("ON/T/A", "\n");
    run("ON/T/A", "ON=no\n");
    run("OPT/S/A", "OPT\n");

    P_SECTION("/M");
    run("FILES/M", "a b c\n");
    run("FILES/M", "\n");
    run("FILES/M/A", "\n");
    run("FILES/M,TO/A", "a b c\n");
    run("FILES/M,TO/A", "a\n");
    run("FILES/M,TO/K", "a b TO c\n");
    run("FILES/M,TO/K", "a TO c b\n");
    run("FILES/M,ALL/S", "a ALL b\n");
    run("FILES/M/N", "1 2 3\n");
    run("FILES/M/N", "1 x 3\n");
    run("A/A,FILES/M,B/A", "1 2 3 4\n");
    run("A/A,FILES/M,B/A", "1 2\n");
    run("FILES/M,A/A,B/A", "a b\n");
    run("FILES/M,A/A,B/A", "a b c\n");
    run("FILES/M,A/A,B/A", "a\n");
    run("FILES/M,TO/A", "a b\n");
    run("FILES/M/A,TO/A", "a\n");
    run("FILES/M/A,TO/A", "a b\n");
    /* not probed: "FILES/M,TO/A/N" "a 5" - 3.1 moves the string pointer
     * into the /N item unconverted, so *array[1] is "5\0" plus whatever
     * follows in its buffer (address dependent) */
    run("FILES/M,P/A,Q/A", "1 2 3\n");
    run("FILES/M,P/A,Q/A", "1 2 3 4\n");
    run("FILES/M,P/A,Q/A", "1 2\n");
    run("FILES/M,P/A,Q", "1 2 3\n");
    run("FILES/M,Q,P/A", "1 2 3\n");
    run("FILES/M,TO", "FILES a b\n");
    run("FILES/M,TO", "a FILES b\n");
    run("FILES/M,TO", "a TO b c\n");
    run("FILES/M,TO/A,X/K", "a b X 1\n");
    run("FILES/M/N,TO/A", "1 x\n");
    run("A/A,FILES/M,B/A", "1 2 3\n");
    run("FILES/M,TO/K,DEST/A", "a b\n");
    run("FILES/M,TO", "a b\n");
    run("X,FILES/M,TO/A", "a b\n");
    run("FILES/M", "\"a b\" c\n");
    run("FILES/M", "a;b\n");

    P_SECTION("aliases and errors");
    run("Q=QUIET/S,F=FILE", "Q x\n");
    run("Q=QUIET/S,F=FILE", "quiet f=x\n");
    run("Q=QUIET/S,F=FILE", "FILE x\n");
    run("FROM,TO", "a b c d\n");
    run("A/S,B/S", "A A\n");
    run("FILE", "FILE=\n");
    run("A/X", "a\n");
    run("A,,B", "1 2\n");
    run("", "\n");
    run("", "x\n");
    run("", "x y\n");
    run("", "=\n");
    run("", "\"x\n");
    run("", "x=y\n");
    run("", "");
    run("A=B=C", "c 1\n");
    run("A/K/S", "A\n");
    run("FROM,TO/A/S", "TO\n");

    return 0;
}
