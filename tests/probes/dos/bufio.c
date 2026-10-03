/*
 * Probe (Phase 222a): dos.library buffered I/O - FGetC, UnGetC, FPutC,
 * FGets, FPuts, FRead, FWrite, Flush, SetVBuf, VFPrintf/FPrintf, VFWritef,
 * and how buffered and unbuffered (Read/Write/Seek) calls interleave on
 * one file handle.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/stdio.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

#define F "T:lxaprobe_bufio"

static void dump(const char *label)
{
    BPTR fh = Open((STRPTR)F, MODE_OLDFILE);
    UBYTE buf[200];
    LONG n, i;
    probe_s(label);
    probe_s(": ");
    if (!fh) {
        probe_s("(cannot open)\n");
        return;
    }
    n = Read(fh, buf, sizeof(buf));
    Close(fh);
    probe_dec(n);
    probe_s(" bytes \"");
    for (i = 0; i < n; i++) {
        if (buf[i] == '\n')
            probe_s("\\n");
        else if (buf[i] < 32 || buf[i] > 126) {
            probe_s("\\x");
            probe_ch("0123456789abcdef"[buf[i] >> 4]);
            probe_ch("0123456789abcdef"[buf[i] & 15]);
        } else
            probe_ch(buf[i]);
    }
    probe_s("\"\n");
}

static void make(const char *data)
{
    BPTR fh = Open((STRPTR)F, MODE_NEWFILE);
    LONG n = 0;
    while (data[n])
        n++;
    Write(fh, (APTR)data, n);
    Close(fh);
}

int main(void)
{
    BPTR fh;
    UBYTE buf[64];
    LONG r, i;

    P_SECTION("FGetC/UnGetC");
    make("abc\ndef\n");
    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("FGetC", FGetC(fh));
    P_LONG("FGetC", FGetC(fh));
    P_LONG("UnGetC(-1) (push back last)", UnGetC(fh, -1));
    P_LONG("FGetC after UnGetC(-1)", FGetC(fh));
    P_LONG("UnGetC('X')", UnGetC(fh, 'X'));
    P_LONG("FGetC after UnGetC('X')", FGetC(fh));
    P_LONG("FGetC", FGetC(fh));
    P_LONG("Seek(0, CURRENT) (buffered position)", Seek(fh, 0, OFFSET_CURRENT));
    P_LONG("FGetC after Seek", FGetC(fh));
    /* not probed: Read() after buffered reads (3.1 FGetC() pre-reads a
     * block, so Read() continues behind it; lxa reads through) */
    P_LONG("FGetC", FGetC(fh));
    for (i = 0; i < 4; i++)
        P_LONG("FGetC (to EOF)", FGetC(fh));
    P_LONG("UnGetC(-1) after EOF", UnGetC(fh, -1));
    P_LONG("FGetC after UnGetC at EOF", FGetC(fh));
    Close(fh);

    P_SECTION("FGets");
    make("line one\nline two is longer than the buffer\n\nlast");
    fh = Open((STRPTR)F, MODE_OLDFILE);
    for (i = 0; i < 7; i++) {
        STRPTR s;
        int k;
        for (k = 0; k < 16; k++)
            buf[k] = 0xee;
        s = FGets(fh, (STRPTR)buf, 12);
        probe_s("FGets(12): ");
        if (!s) {
            probe_s("NULL IoErr ");
            probe_dec(IoErr());
            probe_ch('\n');
            continue;
        }
        probe_s(s == (STRPTR)buf ? "buf \"" : "other \"");
        for (k = 0; k < 12 && buf[k]; k++)
            if (buf[k] == '\n')
                probe_s("\\n");
            else
                probe_ch(buf[k]);
        probe_s("\"\n");
    }
    Close(fh);

    P_SECTION("FPutC/FPuts/FWrite/Flush");
    fh = Open((STRPTR)F, MODE_NEWFILE);
    P_LONG("FPutC('A')", FPutC(fh, 'A'));
    P_LONG("FPutC(0x142) (byte)", FPutC(fh, 0x142));
    P_LONG("FPuts", FPuts(fh, (STRPTR)"xyz\n"));
    P_LONG("FPuts(\"\")", FPuts(fh, (STRPTR)""));
    P_LONG("FWrite(3 x 2)", FWrite(fh, (STRPTR)"112233", 2, 3));
    P_LONG("FWrite(0 blocks)", FWrite(fh, (STRPTR)"x", 1, 0));
    P_LONG("Seek(0, CURRENT) with buffered output", Seek(fh, 0, OFFSET_CURRENT));
    r = Write(fh, (APTR)"[W]", 3);
    P_LONG("Write after buffered output", r);
    P_LONG("FPutC('Z')", FPutC(fh, 'Z'));
    P_LONG("Flush", Flush(fh));
    P_LONG("Flush again", Flush(fh));
    Close(fh);
    dump("file");

    P_SECTION("FRead");
    make("0123456789abcdef");
    fh = Open((STRPTR)F, MODE_OLDFILE);
    r = FRead(fh, buf, 4, 2);
    P_LONG("FRead(4 x 2)", r);
    P_BYTES("  data", buf, 8);
    r = FRead(fh, buf, 3, 5);
    P_LONG("FRead(3 x 5) (8 left: partial block)", r);
    P_BYTES("  data", buf, 8);
    P_LONG("FGetC at end", FGetC(fh));
    P_LONG("FRead at EOF", FRead(fh, buf, 1, 1));
    Close(fh);

    P_SECTION("SetVBuf");
    fh = Open((STRPTR)F, MODE_NEWFILE);
    P_LONG("SetVBuf(BUF_NONE)", SetVBuf(fh, NULL, BUF_NONE, -1));
    FPutC(fh, 'n');
    FPuts(fh, (STRPTR)"one\n");
    P_LONG("SetVBuf(BUF_LINE, 16)", SetVBuf(fh, NULL, BUF_LINE, 16));
    FPuts(fh, (STRPTR)"line\nrest");
    P_LONG("SetVBuf(BUF_FULL, 32)", SetVBuf(fh, NULL, BUF_FULL, 32));
    FPuts(fh, (STRPTR)"full");
    P_LONG("SetVBuf(own buffer, 8)", SetVBuf(fh, (STRPTR)buf, BUF_FULL, 8));
    FPuts(fh, (STRPTR)"0123456789");
    P_LONG("SetVBuf(bad mode 7)", SetVBuf(fh, NULL, 7, 16));
    Close(fh);
    dump("file");

    P_SECTION("VFPrintf/VFWritef");
    fh = Open((STRPTR)F, MODE_NEWFILE);
    {
        LONG args[4];
        args[0] = 42;
        args[1] = (LONG)"str";
        args[2] = -7;
        P_LONG("VFPrintf(\"%ld-%s-%ld\\n\")", VFPrintf(fh, (STRPTR)"%ld-%s-%ld\n", args));
        P_LONG("VFPrintf(\"\")", VFPrintf(fh, (STRPTR)"", args));
        args[0] = 0x2a0000;
        P_LONG("VFPrintf(\"%d|\") word", VFPrintf(fh, (STRPTR)"%d|", args));
        P_LONG("Flush before VFWritef", Flush(fh));
    }
    Close(fh);
    dump("file");

    P_SECTION("VFWritef (BCPL formats)");
    {
        static const char *const fmts[] = {
            "N=%N|", "S=%S|", "X4=%X4|", "X8=%X8|", "XA=%XA|", "I3=%I3|", "I6=%I6|", "O4=%O4|", "U5=%U5|",
            "C=%C|", "T8=%T8|", "TZ=%TZ|", "$=%$%N|", "*N stays|", "%%|", "%Z|",
            "X2=%X2|", "I2=%I2|", "U8=%U8|", "O2=%O2|", "I0=%I0|", "T2=%T2|", "N=%N|", "X1=%X1|",
        };
        static LONG vals[][2] = {
            {12345, 0}, {(LONG)"cstr", 0}, {0xbeef, 0}, {-1, 0}, {0x1234, 0}, {-42, 0}, {-42, 0},
            {8, 0}, {-1, 0}, {'Q', 0}, {(LONG)"ab", 0}, {(LONG)"left", 0}, {111, 222}, {0, 0}, {0, 0},
            {0, 0},
            {0xbeef, 0}, {12345, 0}, {7, 0}, {64, 0}, {5, 0}, {(LONG)"long", 0}, {-77, 0}, {0, 0},
        };
        int k;
        for (k = 0; k < (int)(sizeof(fmts) / sizeof(fmts[0])); k++) {
            fh = Open((STRPTR)F, MODE_NEWFILE);
            VFWritef(fh, (STRPTR)fmts[k], vals[k]);
            Close(fh);
            probe_s("VFWritef \"");
            probe_s(fmts[k]);
            probe_s("\" -> ");
            dump("");
        }
    }

    DeleteFile((STRPTR)F);
    return 0;
}
