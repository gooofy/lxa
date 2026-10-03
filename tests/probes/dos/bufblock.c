/*
 * Probe (Phase 222a): dos.library buffer blocks - how much FGetC() reads
 * ahead (a following unbuffered Read() continues behind the block), what
 * Flush()/Seek() do to a read buffer, EOF, and in which order buffered
 * output (FPuts/FPutC) and unbuffered Write() reach a file.
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

#define F "T:lxaprobe_bufblock"
#define SIZE 4096

static UBYTE data[SIZE];

/* every 4-byte group holds its own offset as 4 hex digits */
static void make_big(void)
{
    static const char hx[] = "0123456789abcdef";
    BPTR fh;
    LONG o;
    for (o = 0; o < SIZE; o += 4) {
        data[o] = hx[(o >> 12) & 15];
        data[o + 1] = hx[(o >> 8) & 15];
        data[o + 2] = hx[(o >> 4) & 15];
        data[o + 3] = hx[o & 15];
    }
    fh = Open((STRPTR)F, MODE_NEWFILE);
    Write(fh, data, SIZE);
    Close(fh);
}

static void make(const char *s)
{
    BPTR fh = Open((STRPTR)F, MODE_NEWFILE);
    LONG n = 0;
    while (s[n])
        n++;
    Write(fh, (APTR)s, n);
    Close(fh);
}

static void show_bytes(const char *label, const UBYTE *b, LONG n)
{
    LONG i;
    probe_s(label);
    probe_s(" \"");
    for (i = 0; i < n; i++) {
        if (b[i] == '\n')
            probe_s("\\n");
        else if (b[i] < 32 || b[i] > 126)
            probe_ch('?');
        else
            probe_ch(b[i]);
    }
    probe_s("\"\n");
}

static void read4(BPTR fh, const char *label)
{
    UBYTE b[4];
    LONG n = Read(fh, b, 4);
    probe_s(label);
    probe_s(" Read(4) = ");
    probe_dec(n);
    if (n > 0)
        show_bytes("", b, n);
    else
        probe_ch('\n');
}

static void dump(const char *label)
{
    BPTR fh = Open((STRPTR)F, MODE_OLDFILE);
    UBYTE buf[600];
    LONG n = fh ? Read(fh, buf, sizeof(buf)) : -1;
    if (fh)
        Close(fh);
    probe_s(label);
    probe_s(": ");
    probe_dec(n);
    if (n > 0 && n <= 64)
        show_bytes(" bytes", buf, n);
    else if (n > 0) {
        LONG i;
        probe_s(" bytes, '|' at");
        for (i = 0; i < n; i++)
            if (buf[i] == '|') {
                probe_ch(' ');
                probe_dec(i);
            }
        probe_ch('\n');
    } else
        probe_ch('\n');
}

int main(void)
{
    BPTR fh, fh2;
    LONG i;

    P_SECTION("read-ahead");
    make_big();
    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("FGetC", FGetC(fh));
    read4(fh, "after FGetC");
    P_LONG("Seek(0, CURRENT)", Seek(fh, 0, OFFSET_CURRENT));
    P_LONG("FGetC after Seek", FGetC(fh));
    Close(fh);

    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("FGetC", FGetC(fh));
    P_LONG("Flush", Flush(fh));
    read4(fh, "after Flush");
    P_LONG("FGetC", FGetC(fh));
    Close(fh);

    fh = Open((STRPTR)F, MODE_OLDFILE);
    for (i = 0; i < 300; i++)
        FGetC(fh);
    read4(fh, "after 300 FGetC");
    for (i = 0; i < 300; i++)
        FGetC(fh);
    read4(fh, "after 300 more FGetC");
    Close(fh);

    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("FGetC", FGetC(fh));
    P_LONG("UnGetC(-1)", UnGetC(fh, -1));
    read4(fh, "after UnGetC");
    P_LONG("FGetC", FGetC(fh));
    Close(fh);

    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("SetVBuf(BUF_FULL, 100)", SetVBuf(fh, NULL, BUF_FULL, 100));
    P_LONG("FGetC", FGetC(fh));
    read4(fh, "100-byte buffer");
    Close(fh);

    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("SetVBuf(BUF_FULL, 1000)", SetVBuf(fh, NULL, BUF_FULL, 1000));
    P_LONG("FGetC", FGetC(fh));
    read4(fh, "1000-byte buffer");
    Close(fh);

    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("SetVBuf(BUF_FULL, 300)", SetVBuf(fh, NULL, BUF_FULL, 300));
    P_LONG("FGetC", FGetC(fh));
    read4(fh, "300-byte buffer");
    Close(fh);

    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("FGetC", FGetC(fh));
    P_LONG("SetVBuf(BUF_FULL, 1000) while reading", SetVBuf(fh, NULL, BUF_FULL, 1000));
    read4(fh, "after SetVBuf");
    P_LONG("FGetC", FGetC(fh));
    read4(fh, "then");
    Close(fh);

    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("FGetC", FGetC(fh));
    P_LONG("FPutC", FPutC(fh, 'W'));
    read4(fh, "after FGetC, FPutC");
    Close(fh);

    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("FPutC", FPutC(fh, 'W'));
    P_LONG("FGetC after FPutC", FGetC(fh));
    read4(fh, "then");
    Close(fh);
    make_big();

    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("SetVBuf(BUF_NONE)", SetVBuf(fh, NULL, BUF_NONE, -1));
    P_LONG("FGetC", FGetC(fh));
    read4(fh, "unbuffered");
    Close(fh);

    fh = Open((STRPTR)F, MODE_OLDFILE);
    {
        UBYTE b[8];
        P_LONG("FRead(4 x 1)", FRead(fh, b, 4, 1));
        read4(fh, "after FRead");
    }
    Close(fh);

    P_SECTION("EOF");
    make("ab");
    fh = Open((STRPTR)F, MODE_OLDFILE);
    P_LONG("FGetC", FGetC(fh));
    P_LONG("FGetC", FGetC(fh));
    P_LONG("FGetC (EOF)", FGetC(fh));
    fh2 = Open((STRPTR)F, MODE_OLDFILE);
    Seek(fh2, 0, OFFSET_END);
    Write(fh2, (APTR)"cd", 2);
    Close(fh2);
    P_LONG("FGetC after the file grew", FGetC(fh));
    P_LONG("FGetC", FGetC(fh));
    P_LONG("Flush", Flush(fh));
    P_LONG("FGetC after Flush", FGetC(fh));
    read4(fh, "then");
    Close(fh);

    P_SECTION("buffered output vs Write");
    fh = Open((STRPTR)F, MODE_NEWFILE);
    FPuts(fh, (STRPTR)"A");
    Write(fh, (APTR)"B", 1);
    FPuts(fh, (STRPTR)"C\n");
    Write(fh, (APTR)"D", 1);
    FPutC(fh, 'E');
    Close(fh);
    dump("FPuts A, Write B, FPuts C\\n, Write D, FPutC E");

    fh = Open((STRPTR)F, MODE_NEWFILE);
    FPuts(fh, (STRPTR)"A");
    P_LONG("Flush", Flush(fh));
    Write(fh, (APTR)"B", 1);
    Close(fh);
    dump("FPuts A, Flush, Write B");

    fh = Open((STRPTR)F, MODE_NEWFILE);
    for (i = 0; i < 500; i++)
        FPutC(fh, 'x');
    Write(fh, (APTR)"|", 1);
    Close(fh);
    dump("500 FPutC, Write |");

    fh = Open((STRPTR)F, MODE_NEWFILE);
    SetVBuf(fh, NULL, BUF_LINE, 64);
    FPuts(fh, (STRPTR)"A\nB");
    Write(fh, (APTR)"C", 1);
    Close(fh);
    dump("BUF_LINE: FPuts A\\nB, Write C");

    fh = Open((STRPTR)F, MODE_NEWFILE);
    SetVBuf(fh, NULL, BUF_LINE, 64);
    FPutC(fh, 'A');
    FPutC(fh, '\n');
    Write(fh, (APTR)"C", 1);
    Close(fh);
    dump("BUF_LINE: FPutC A, FPutC \\n, Write C");

    fh = Open((STRPTR)F, MODE_NEWFILE);
    FPutC(fh, 'A');
    FPutC(fh, '\n');
    Write(fh, (APTR)"C", 1);
    Close(fh);
    dump("FPutC A, FPutC \\n, Write C");

    fh = Open((STRPTR)F, MODE_NEWFILE);
    SetVBuf(fh, NULL, BUF_FULL, 1000);
    for (i = 0; i < 500; i++)
        FPutC(fh, 'x');
    Write(fh, (APTR)"|", 1);
    Close(fh);
    dump("1000-byte buffer: 500 FPutC, Write |");

    fh = Open((STRPTR)F, MODE_NEWFILE);
    FPuts(fh, (STRPTR)"A");
    SetVBuf(fh, NULL, BUF_FULL, 1000);
    Write(fh, (APTR)"B", 1);
    Close(fh);
    dump("FPuts A, SetVBuf, Write B");

    fh = Open((STRPTR)F, MODE_NEWFILE);
    SetVBuf(fh, NULL, BUF_NONE, -1);
    FPuts(fh, (STRPTR)"A");
    Write(fh, (APTR)"B", 1);
    Close(fh);
    dump("BUF_NONE: FPuts A, Write B");

    fh = Open((STRPTR)F, MODE_NEWFILE);
    FPuts(fh, (STRPTR)"AB");
    P_LONG("Seek(0, BEGINNING) with buffered output", Seek(fh, 0, OFFSET_BEGINNING));
    FPuts(fh, (STRPTR)"x");
    Close(fh);
    dump("FPuts AB, Seek 0, FPuts x");

    fh = Open((STRPTR)F, MODE_NEWFILE);
    Write(fh, (APTR)"0123", 4);
    Seek(fh, 0, OFFSET_BEGINNING);
    P_LONG("FGetC", FGetC(fh));
    FPuts(fh, (STRPTR)"w");
    Close(fh);
    dump("0123, FGetC, FPuts w");

    /* Output() is not probed here: it is a file on the reference (RUN >x)
     * but the host console on lxa, which flushes at '\n' like a console
     * window (tests/scenarios/interactive/conbuf.yaml covers consoles). */

    DeleteFile((STRPTR)F);
    return 0;
}
