/*
 * Probe (Phase 222a): exec.library RawDoFmt - every conversion, flag,
 * width and precision combination, WORD vs LONG data, odd inputs, the
 * exact byte stream sent to PutChProc (including the terminating NUL and
 * any stray bytes) and the returned data-stream pointer.
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

struct Sink {
    UBYTE *p;
    ULONG n;
};

__asm__(
    "    .text\n"
    "    .globl _probe_putch\n"
    "_probe_putch:\n"
    "    move.l (a3),a0\n"
    "    move.b d0,(a0)+\n"
    "    move.l a0,(a3)\n"
    "    addq.l #1,4(a3)\n"
    "    rts\n");
extern void probe_putch(void);

static UBYTE out[200];

static void show(const char *fmt, APTR data)
{
    struct Sink sink;
    APTR ret;
    ULONG i;
    sink.p = out;
    sink.n = 0;
    ret = RawDoFmt((STRPTR)fmt, data, (void (*)())probe_putch, &sink);
    probe_ch('[');
    probe_s(fmt);
    probe_s("] -> \"");
    for (i = 0; i < sink.n && i < sizeof(out); i++) {
        UBYTE c = out[i];
        if (c == 0)
            probe_s("\\0");
        else if (c < 32 || c > 126) {
            probe_s("\\x");
            probe_ch("0123456789abcdef"[c >> 4]);
            probe_ch("0123456789abcdef"[c & 15]);
        } else
            probe_ch((char)c);
    }
    probe_s("\" n=");
    probe_dec((LONG)sink.n);
    probe_s(" used=");
    probe_dec((LONG)((UBYTE *)ret - (UBYTE *)data));
    probe_ch('\n');
}

static UWORD w[16];
static ULONG l[16];

static APTR W1(UWORD a) { w[0] = a; return w; }
static APTR W2(UWORD a, UWORD b) { w[0] = a; w[1] = b; return w; }
static APTR L1(ULONG a) { l[0] = a; return l; }
static APTR L2(ULONG a, ULONG b) { l[0] = a; l[1] = b; return l; }

int main(void)
{
    static char bstr[] = "\005hello";
    static const char *const wfmts[] = {
        "%d", "%u", "%x", "%X", "%c", "%5d", "%-5d|", "%05d", "%05u", "%5x", "%05x", "%-05d|",
        "%1d", "%0d", "%+d", "% d", "%.3d", "%3.3d", "%hd", "%i", "%o", "%e", "%z", "%%", "100%",
        "%-d|", "%5c|", "%-3c|",
    };
    static const UWORD wvals[] = {0, 1, 0x7fff, 0x8000, 0xffff, 0xffce, 42, 0x41};
    static const char *const lfmts[] = {
        "%ld", "%lu", "%lx", "%lX", "%lc", "%8ld", "%-8ld|", "%08ld", "%08lx", "%08lu", "%2ld",
        "%ld%ld", "%lld", "%Ld", "%l", "%lo", "%12lu", "%-012ld|",
    };
    static const ULONG lvals[] = {0, 1, 0x7fffffff, 0x80000000, 0xffffffff, 0xffffff85, 123456789, 0xdeadbeef};
    int i, k;

    P_SECTION("WORD conversions");
    for (i = 0; i < (int)(sizeof(wfmts) / sizeof(wfmts[0])); i++)
        for (k = 0; k < (int)(sizeof(wvals) / sizeof(wvals[0])); k++)
            show(wfmts[i], W2(wvals[k], wvals[(k + 3) % 8]));

    P_SECTION("LONG conversions");
    for (i = 0; i < (int)(sizeof(lfmts) / sizeof(lfmts[0])); i++)
        for (k = 0; k < (int)(sizeof(lvals) / sizeof(lvals[0])); k++)
            show(lfmts[i], L2(lvals[k], lvals[(k + 5) % 8]));

    P_SECTION("negative zero padding");
    show("%05ld", L1(-12));
    show("%05ld", L1(-1234));
    show("%05ld", L1(-12345));
    show("%5ld", L1(-12));
    show("%-5ld|", L1(-12));
    show("%05d", W1((UWORD)-12));
    show("%08ld|%ld", L2(-5, 7));
    show("%03ld", L1(-1));

    P_SECTION("strings");
    show("%s", L1((ULONG)"abc"));
    show("%s|", L1((ULONG)""));
    show("%6s|", L1((ULONG)"abc"));
    show("%-6s|", L1((ULONG)"abc"));
    show("%06s|", L1((ULONG)"abc"));
    show("%.2s|", L1((ULONG)"abcdef"));
    show("%5.2s|", L1((ULONG)"abcdef"));
    show("%-5.2s|", L1((ULONG)"abcdef"));
    show("%.0s|", L1((ULONG)"abcdef"));
    show("%.10s|", L1((ULONG)"abc"));
    show("%2s|", L1((ULONG)"abcdef"));
    show("%ls|", L1((ULONG)"long"));
    show("%s %s", L2((ULONG)"one", (ULONG)"two"));
    show("%b|", L1((ULONG)bstr >> 2));
    show("%8b|", L1((ULONG)bstr >> 2));
    show("%.3b|", L1((ULONG)bstr >> 2));
    show("%lb|", L1((ULONG)bstr >> 2));
    show("%s", L1(0));
    show("%5s|", L1(0));

    P_SECTION("misc");
    show("", NULL);
    show("plain text", NULL);
    show("tab\there\nnewline", NULL);
    show("%", NULL);
    show("%5", NULL);
    show("%-", NULL);
    show("%l", NULL);
    show("end%%", NULL);
    show("%d%c%d", W2(1, 'x'));
    {
        static UWORD mix2[] = {0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005};
        show("%ld%d%ld", mix2);
    }
    {
        static UWORD mix[] = {0x1234, 0x0000, 0x5678, 0x0009, 0x0001};
        show("%d %ld %d", mix);
    }
    show("%c%c%c", W2(0x4142, 0x0043));
    show("%lc%lc", L2(0x41424344, 0x45));
    show("%x %X", W2(0xabcd, 0xabcd));
    show("%5.3x|", W1(0xab));
    show("%-+5d|", W1(3));
    show("%,ld", L1(1234567));
    show("%10.4ld|", L1(42));
    show("%2$d", W2(1, 2));
    show("%*d", W2(5, 7));

    P_SECTION("probing the rules");
    show("%06.3ld|", L1(-12345));
    show("%07.3ld|", L1(-12345));
    show("%06.4ld|", L1(12345));
    show("%06s|", L1((ULONG)"-ab"));
    show("%6s|", L1((ULONG)"-ab"));
    show("%06.2s|", L1((ULONG)"-abc"));
    show("%05c|", W1('A'));
    show("%05c|", W1('-'));
    show("%.0c|", W1('A'));
    show("%.0d|", W1(5));
    show("%.0ld|", L1(-5));
    show("%.1ld|", L1(-5));
    show("%5e|", NULL);
    show("%-5e|", NULL);
    show("%05e|", NULL);
    show("%5%|", NULL);
    show("%0-5d|", W1(3));
    show("%-0-5d|", W1(3));
    show("%00005d|", W1(3));
    show("%08b|", L1((ULONG)bstr >> 2));
    show("%-8b|", L1((ULONG)bstr >> 2));
    show("%D|%U|%X|", W2(1, 2));
    show("%S|", L1((ULONG)"up"));
    show("%C|", W1('c'));
    show("%B|", L1((ULONG)bstr >> 2));
    show("%lu %lU", L2(1, 2));
    show("%1$d|%2$d", W2(1, 2));
    show("%2$d|%1$d", W2(1, 2));
    show("%1$ld|%2$ld", L2(11, 22));
    show("%2$ld|%1$ld", L2(11, 22));
    show("%2$d", W2(1, 2));
    show("%3$d|%1$d", W2(1, 2));
    show("a%$d", W2(1, 2));
    show("%5$", W2(1, 2));
    show("%.d|", W1(77));
    show("%-.2s|", L1((ULONG)"xyz"));
    show("%5.0s|", L1((ULONG)"xyz"));
    show("%010lx", L1(0xabc));
    show("%-010lx|", L1(0xabc));
    show("%010lu|", L1(77));
    show("%5u|%-5u|", W2(7, 8));

    P_SECTION("positional arguments");
    {
        static UWORD w3[] = {1, 2, 3, 4};
        static ULONG l3[] = {11, 22, 33, 44};
        static ULONG s2[2];
        s2[0] = (ULONG)"first";
        s2[1] = (ULONG)"second";
        show("%2$d|%d", w3);
        show("%d|%1$d", w3);
        show("%d|%3$d|%d", w3);
        show("%d|%d|%1$d", w3);
        show("%1$d|%1$d", w3);
        show("%1$ld|%1$d", l3);
        show("%1$d|%1$ld", l3);
        show("%2$s %1$s", s2);
        show("%0$d|", w3);
        show("%2$ld|%d", l3);
        show("%3$ld|%1$d", l3);
        show("%2$5d|%1$-3d|", w3);
        show("%2$05ld|%1$.1ld|", l3);
        show("%2$%|%d", w3);
        show("%2$e|%d", w3);
        show("%4$d", w3);
        show("%1$c%2$c", w3);
    }
    return 0;
}
