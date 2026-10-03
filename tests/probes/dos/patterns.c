/*
 * Probe (Phase 222a): dos.library pattern matching - ParsePattern,
 * ParsePatternNoCase (return codes, buffer overflow), MatchPattern,
 * MatchPatternNoCase over a pattern x name matrix, MatchFirst/MatchNext/
 * MatchEnd over a directory tree in T:.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dosasl.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static const char *const pats[] = {
    "abc", "ABC", "#?", "?", "a?c", "a#?", "#?c", "#a", "a#bc", "(a|b)c", "(a|b|)c", "~(abc)", "~a#?",
    "[a-c]bc", "[~a]bc", "[abc]#?", "a%b", "a'?c", "*", "a*", "#(ab)", "(a#?|b#?)c", "#?.info", "~(#?.info)",
    "", "a|b", "(abc", "[a-", "a)b", "%", "a''b", "?#?", "#?#?c", "(ab)#(c)",
};

static const char *const names[] = {
    "abc", "ABC", "ac", "abbc", "bc", "c", "", "a?c", "a*c", "aaa", "ab", "abcd", "x.info", "ababc",
    "a'b", "ad", "dbc",
};

int main(void)
{
    UBYTE tok[128], tokn[128];
    int i, j;

    P_SECTION("ParsePattern return codes");
    for (i = 0; i < (int)(sizeof(pats) / sizeof(pats[0])); i++) {
        LONG r1, r2, e1, e2;
        SetIoErr(0);
        r1 = ParsePattern((STRPTR)pats[i], (STRPTR)tok, sizeof(tok));
        e1 = IoErr();
        SetIoErr(0);
        r2 = ParsePatternNoCase((STRPTR)pats[i], (STRPTR)tokn, sizeof(tokn));
        e2 = IoErr();
        probe_s("\"");
        probe_s(pats[i]);
        probe_s("\": ParsePattern ");
        probe_dec(r1);
        if (r1 < 0) { probe_s(" IoErr "); probe_dec(e1); }
        probe_s(", NoCase ");
        probe_dec(r2);
        if (r2 < 0) { probe_s(" IoErr "); probe_dec(e2); }
        probe_ch('\n');
    }
    {
        static const ULONG sizes[] = {0, 1, 2, 3, 4, 5};
        for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++) {
            LONG r;
            SetIoErr(0);
            r = ParsePattern((STRPTR)"#?.info", (STRPTR)tok, sizes[i]);
            probe_s("ParsePattern(#?.info, size ");
            probe_dec((LONG)sizes[i]);
            probe_s(") = ");
            probe_dec(r);
            probe_s(" IoErr ");
            probe_dec(IoErr());
            probe_ch('\n');
        }
    }

    P_SECTION("MatchPattern matrix (case, nocase)");
    for (i = 0; i < (int)(sizeof(pats) / sizeof(pats[0])); i++) {
        LONG r1 = ParsePattern((STRPTR)pats[i], (STRPTR)tok, sizeof(tok));
        LONG r2 = ParsePatternNoCase((STRPTR)pats[i], (STRPTR)tokn, sizeof(tokn));
        probe_s("\"");
        probe_s(pats[i]);
        probe_s("\":");
        for (j = 0; j < (int)(sizeof(names) / sizeof(names[0])); j++) {
            probe_ch(' ');
            probe_ch(r1 < 0 ? '-' : MatchPattern((STRPTR)tok, (STRPTR)names[j]) ? '1' : '0');
            probe_ch(r2 < 0 ? '-' : MatchPatternNoCase((STRPTR)tokn, (STRPTR)names[j]) ? '1' : '0');
        }
        probe_ch('\n');
    }

    P_SECTION("MatchFirst/MatchNext");
    {
        static const char *const files[] = {
            "T:lxaprobe_pat/a.c", "T:lxaprobe_pat/b.c", "T:lxaprobe_pat/c.h", "T:lxaprobe_pat/d/e.c",
            "T:lxaprobe_pat/d/f.txt",
        };
        static const char *const mpats[] = {
            "T:lxaprobe_pat/#?.c", "T:lxaprobe_pat/#?", "T:lxaprobe_pat/~(#?.c)", "T:lxaprobe_pat/#?/#?.c",
            "T:lxaprobe_pat/a.c", "T:lxaprobe_pat/x.c", "T:lxaprobe_pat/#?.x", "T:lxaprobe_pat/D",
            "T:lxaprobe_pat/(a|c).?",
        };
        BPTR l;
        DeleteFile((STRPTR)"T:lxaprobe_pat/d/e.c");
        DeleteFile((STRPTR)"T:lxaprobe_pat/d/f.txt");
        DeleteFile((STRPTR)"T:lxaprobe_pat/d");
        for (i = 0; i < 3; i++)
            DeleteFile((STRPTR)files[i]);
        DeleteFile((STRPTR)"T:lxaprobe_pat");
        l = CreateDir((STRPTR)"T:lxaprobe_pat");
        if (l)
            UnLock(l);
        l = CreateDir((STRPTR)"T:lxaprobe_pat/d");
        if (l)
            UnLock(l);
        for (i = 0; i < 5; i++) {
            BPTR fh = Open((STRPTR)files[i], MODE_NEWFILE);
            if (fh)
                Close(fh);
        }
        for (i = 0; i < (int)(sizeof(mpats) / sizeof(mpats[0])); i++) {
            struct AnchorPath *ap = AllocVec(sizeof(struct AnchorPath) + 256, MEMF_CLEAR);
            char found[10][64];
            int n = 0, a, b;
            LONG r;
            ap->ap_Strlen = 256;
            r = MatchFirst((STRPTR)mpats[i], ap);
            while (!r && n < 10) {
                const char *s = (const char *)ap->ap_Buf;
                const char *t = s;
                int k = 0;
                while (*s) {             /* path below T:lxaprobe_pat/ */
                    if (*s == '/' && t == (const char *)ap->ap_Buf)
                        t = s + 1;
                    s++;
                }
                for (s = t; *s && k < 63; s++)
                    found[n][k++] = *s;
                if (ap->ap_Info.fib_DirEntryType > 0 && k < 62)
                    found[n][k++] = '/';
                found[n][k] = 0;
                n++;
                r = MatchNext(ap);
            }
            MatchEnd(ap);
            for (a = 0; a < n; a++)
                for (b = a + 1; b < n; b++) {
                    int c = 0;
                    while (found[a][c] && found[a][c] == found[b][c])
                        c++;
                    if ((UBYTE)found[a][c] > (UBYTE)found[b][c]) {
                        char t[64];
                        for (c = 0; c < 64; c++) { t[c] = found[a][c]; found[a][c] = found[b][c]; found[b][c] = t[c]; }
                    }
                }
            probe_s(mpats[i] + 15);
            probe_s(": end ");
            probe_dec(r);
            probe_s(" found");
            for (a = 0; a < n; a++) {
                probe_ch(' ');
                probe_s(found[a]);
            }
            probe_ch('\n');
            FreeVec(ap);
        }
        DeleteFile((STRPTR)"T:lxaprobe_pat/d/e.c");
        DeleteFile((STRPTR)"T:lxaprobe_pat/d/f.txt");
        DeleteFile((STRPTR)"T:lxaprobe_pat/d");
        for (i = 0; i < 3; i++)
            DeleteFile((STRPTR)files[i]);
        DeleteFile((STRPTR)"T:lxaprobe_pat");
    }
    return 0;
}
