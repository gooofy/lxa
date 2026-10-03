/*
 * Probe (Phase 222a): dos.library ReadArgs (CSource input) - every
 * template modifier, error codes (IoErr) and the resulting argument array.
 * FindArg/ReadItem/StrToLong: argparse.c.
 *
 * No readargs.ref.out is checked in yet: the Phase 221 ReadArgs() still
 * differs from 3.1 in ~15 cases (';' comments, input without '\n', "=x",
 * /S value DOSTRUE, /T values, "+5" for /N, quotes in /F, /M with
 * trailing /A items, empty template).  Capture it with
 * `python3 -m rdd suite-ref --filter Probes/dos/readargs --capture-ref`
 * once ReadArgs matches.
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

    return 0;
}
