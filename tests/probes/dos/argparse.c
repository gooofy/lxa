/*
 * Probe (Phase 222a): dos.library argument helpers - FindArg (template
 * aliases, case), ReadItem over a CSource (quotes, escapes, '=', ';',
 * small buffers), StrToLong (whitespace, signs, partial numbers,
 * overflow).
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

int main(void)
{
    P_SECTION("FindArg");
    {
        static const char *const keys[] = {"FROM", "from", "TO", "Q", "QUIET", "F", "FILE", "ALL", "X", "", "FR"};
        int i;
        for (i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++) {
            probe_s("FindArg(\"Q=QUIET/S,FROM/A,TO,F=FILE/K,ALL/S\", \"");
            probe_s(keys[i]);
            probe_s("\") = ");
            probe_dec(FindArg((STRPTR)"Q=QUIET/S,FROM/A,TO,F=FILE/K,ALL/S", (STRPTR)keys[i]));
            probe_ch('\n');
        }
    }

    P_SECTION("ReadItem");
    {
        static const char *const srcs[] = {
            "word rest\n", "  spaced\n", "\"quoted text\" x\n", "\"esc*\"aped*n\"\n", "key=value\n",
            "=x\n", "\n", "", ";comment\n", "\"unterminated\n", "a\"b\" c\n", "tab\tx\n",
            "ab", "\"q\"", "a;b\n", "x =y\n", "\"q\"x y\n",
        };
        int i;
        for (i = 0; i < (int)(sizeof(srcs) / sizeof(srcs[0])); i++) {
            struct CSource cs;
            UBYTE buf[16];
            LONG r, len = 0, n = 0;
            while (srcs[i][len])
                len++;
            cs.CS_Buffer = (UBYTE *)srcs[i];
            cs.CS_Length = len;
            cs.CS_CurChr = 0;
            probe_s("ReadItem src ");
            probe_dec(i);
            probe_s(":");
            do {
                int k;
                for (k = 0; k < 16; k++)
                    buf[k] = 0xee;
                r = ReadItem((STRPTR)buf, sizeof(buf), &cs);
                probe_s(" [");
                probe_dec(r);
                probe_s(" \"");
                for (k = 0; k < 15 && buf[k] && buf[k] != 0xee; k++) {
                    if (buf[k] == '\n')
                        probe_s("\\n");
                    else if (buf[k] == 27)
                        probe_s("\\e");
                    else
                        probe_ch(buf[k]);
                }
                probe_s("\" @");
                probe_dec(cs.CS_CurChr);
                probe_ch(']');
            } while (r != ITEM_NOTHING && r != ITEM_ERROR && ++n < 6);
            probe_ch('\n');
        }
        {
            struct CSource cs;
            UBYTE buf[4];
            LONG r;
            cs.CS_Buffer = (UBYTE *)"toolongword\n";
            cs.CS_Length = 12;
            cs.CS_CurChr = 0;
            r = ReadItem((STRPTR)buf, sizeof(buf), &cs);
            probe_s("ReadItem small buffer: ");
            probe_dec(r);
            probe_s(" @");
            probe_dec(cs.CS_CurChr);
            probe_ch('\n');
        }
    }

    P_SECTION("StrToLong");
    {
        static const char *const nums[] = {
            "0", "123", "-123", "+5", "  42", "\t7", "12abc", "abc", "", "-", "2147483647", "2147483648",
            "-2147483648", "4294967296", "0x1f", "$1f", "007", "1 2", "- 3",
        };
        int i;
        for (i = 0; i < (int)(sizeof(nums) / sizeof(nums[0])); i++) {
            LONG v = 0x5555;
            LONG r = StrToLong((STRPTR)nums[i], &v);
            probe_s("StrToLong(\"");
            probe_s(nums[i]);
            probe_s("\") = ");
            probe_dec(r);
            probe_s(" value ");
            probe_dec(v);
            probe_ch('\n');
        }
    }
    return 0;
}
