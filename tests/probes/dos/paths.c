/*
 * Probe (Phase 222a): dos.library path helpers - FilePart, PathPart,
 * AddPart (including buffer overflow), SplitName.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static const char *const paths[] = {
    "", "file", "dir/file", "dir/", "vol:", "vol:file", "vol:dir/file", "vol:dir/sub/",
    "/", "//", "/file", "dir//file", ":", ":file", "a:b:c", "dir/sub/file.info", "vol:/", "x/./y",
};

static const char *const adds[][2] = {
    {"", "file"}, {"dir", "file"}, {"dir/", "file"}, {"vol:", "file"}, {"vol:dir", "file"},
    {"dir", "/file"}, {"dir", "//file"}, {"dir/sub", "/file"}, {"dir", "vol:abs"}, {"dir", ":rootrel"},
    {"dir", ""}, {"", ""}, {"dir", "sub/"}, {"vol:", "/x"}, {"dir//", "x"}, {"a", "../b"}, {"dir/sub", "//x"},
};

int main(void)
{
    char buf[64];
    int i;

    P_SECTION("FilePart/PathPart");
    for (i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); i++) {
        STRPTR f = FilePart((STRPTR)paths[i]);
        STRPTR p = PathPart((STRPTR)paths[i]);
        probe_s("\"");
        probe_s(paths[i]);
        probe_s("\": FilePart @");
        probe_dec((LONG)(f - (STRPTR)paths[i]));
        probe_s(" PathPart @");
        probe_dec((LONG)(p - (STRPTR)paths[i]));
        probe_ch('\n');
    }

    P_SECTION("AddPart");
    for (i = 0; i < (int)(sizeof(adds) / sizeof(adds[0])); i++) {
        BOOL ok;
        int k;
        for (k = 0; adds[i][0][k]; k++)
            buf[k] = adds[i][0][k];
        buf[k] = 0;
        SetIoErr(0);
        ok = AddPart((STRPTR)buf, (STRPTR)adds[i][1], sizeof(buf));
        probe_s("AddPart(\"");
        probe_s(adds[i][0]);
        probe_s("\", \"");
        probe_s(adds[i][1]);
        probe_s("\") = ");
        probe_s(ok ? "TRUE" : "FALSE");
        probe_s(" \"");
        probe_s(buf);
        probe_s("\" IoErr ");
        probe_dec(IoErr());
        probe_ch('\n');
    }
    {
        static const ULONG sizes[] = {1, 4, 8, 9, 10, 11, 12};
        for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++) {
            BOOL ok;
            buf[0] = 'd'; buf[1] = 'i'; buf[2] = 'r'; buf[3] = 0;
            for (ok = 4; ok < 20; ok++)
                buf[ok] = 'Z';
            SetIoErr(0);
            ok = AddPart((STRPTR)buf, (STRPTR)"file1", sizes[i]);
            buf[19] = 0;
            probe_s("AddPart(\"dir\", \"file1\", size ");
            probe_dec((LONG)sizes[i]);
            probe_s(") = ");
            probe_s(ok ? "TRUE" : "FALSE");
            probe_s(" \"");
            probe_s(buf);
            probe_s("\" IoErr ");
            probe_dec(IoErr());
            probe_ch('\n');
        }
    }

    P_SECTION("SplitName");
    {
        static const char *const names[] = {"vol:dir/file", "dir/file", "a/b/c", "nodir", "x/", "/y", "", "a//b"};
        int n;
        for (n = 0; n < (int)(sizeof(names) / sizeof(names[0])); n++) {
            WORD pos = 0;
            int step = 0;
            probe_s("SplitName \"");
            probe_s(names[n]);
            probe_s("\" '/':");
            do {
                pos = SplitName((STRPTR)names[n], '/', (STRPTR)buf, pos, sizeof(buf));
                probe_s(" [\"");
                probe_s(buf);
                probe_s("\" ");
                probe_dec(pos);
                probe_ch(']');
            } while (pos != -1 && ++step < 8);
            probe_ch('\n');
        }
        {
            WORD pos = SplitName((STRPTR)"abcdefgh/ij", '/', (STRPTR)buf, 0, 4);
            probe_s("SplitName small buffer: \"");
            probe_s(buf);
            probe_s("\" ");
            probe_dec(pos);
            probe_ch('\n');
            pos = SplitName((STRPTR)"vol:dir", ':', (STRPTR)buf, 0, sizeof(buf));
            probe_s("SplitName ':' : \"");
            probe_s(buf);
            probe_s("\" ");
            probe_dec(pos);
            probe_ch('\n');
            pos = SplitName((STRPTR)"a/b", '/', (STRPTR)buf, 3, sizeof(buf));
            probe_s("SplitName at end: \"");
            probe_s(buf);
            probe_s("\" ");
            probe_dec(pos);
            probe_ch('\n');
        }
    }
    return 0;
}
