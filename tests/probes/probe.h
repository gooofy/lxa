/*
 * probe.h - API conformance probes (roadmap Phase 222, "WINE tests").
 *
 * A probe calls system functions with normal and edge-case inputs and
 * prints every observable result, one "label = value" line each.  It
 * asserts nothing: the expected output is captured from real AmigaOS 3.1
 * (`python3 -m rdd suite-ref --capture-ref --filter Probes/...`) into
 * tests/probes/<lib>/<name>.ref.out, and lxa must print the same
 * (CTest ref_expected_outputs).
 *
 * Rules:
 *  - never print pointers, timing or memory sizes; print what a program
 *    can rely on (return values, IoErr(), struct fields, bitmap hashes);
 *  - never pass input that crashes real AmigaOS (NULL where the autodoc
 *    does not allow it) - the probe must run unattended on the reference;
 *  - formatting is done here, not with RawDoFmt/Printf, so a bug in the
 *    library under test cannot corrupt the probe output.
 */
#ifndef LXA_PROBE_H
#define LXA_PROBE_H

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;

static char probe_buf[256];
static int probe_len;

static void probe_flush(void)
{
    if (probe_len)
        Write(Output(), probe_buf, probe_len);
    probe_len = 0;
}

static void probe_ch(char c)
{
    if (probe_len == (int)sizeof(probe_buf))
        probe_flush();
    probe_buf[probe_len++] = c;
    if (c == '\n')
        probe_flush();
}

static void probe_s(const char *s)
{
    if (!s) {
        probe_s("(null)");
        return;
    }
    while (*s)
        probe_ch(*s++);
}

static void probe_dec(LONG v)
{
    char t[12];
    int n = 0;
    ULONG u = v < 0 ? (ULONG)(-(v + 1)) + 1 : (ULONG)v;
    if (v < 0)
        probe_ch('-');
    do {
        t[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (n)
        probe_ch(t[--n]);
}

static void probe_hex(ULONG v, int digits)
{
    static const char hx[] = "0123456789abcdef";
    int i;
    probe_s("0x");
    for (i = digits - 1; i >= 0; i--)
        probe_ch(hx[(v >> (i * 4)) & 15]);
}

/* one result line each */
static void P_LONG(const char *label, LONG v)  { probe_s(label); probe_s(" = "); probe_dec(v); probe_ch('\n'); }
static void P_HEX(const char *label, ULONG v)  { probe_s(label); probe_s(" = "); probe_hex(v, 8); probe_ch('\n'); }
static void P_BOOL(const char *label, LONG v)  { probe_s(label); probe_s(v ? " = TRUE\n" : " = FALSE\n"); }
static void P_NULL(const char *label, const void *p) { probe_s(label); probe_s(p ? " = non-NULL\n" : " = NULL\n"); }

static void P_STR(const char *label, const char *s)
{
    probe_s(label);
    probe_s(" = ");
    if (s) {
        probe_ch('"');
        probe_s(s);
        probe_ch('"');
    } else
        probe_s("NULL");
    probe_ch('\n');
}

/* bytes as hex (for structs / small buffers) */
static void P_BYTES(const char *label, const void *p, LONG len)
{
    static const char hx[] = "0123456789abcdef";
    const UBYTE *b = (const UBYTE *)p;
    LONG i;
    probe_s(label);
    probe_s(" =");
    for (i = 0; i < len; i++) {
        probe_ch(' ');
        probe_ch(hx[b[i] >> 4]);
        probe_ch(hx[b[i] & 15]);
    }
    probe_ch('\n');
}

/* FNV-1a hash (for bitmaps and large buffers) */
static ULONG probe_hash(const void *p, LONG len)
{
    const UBYTE *b = (const UBYTE *)p;
    ULONG h = 2166136261UL;
    while (len-- > 0) {
        h ^= *b++;
        h *= 16777619UL;
    }
    return h;
}

static void P_SECTION(const char *name) { probe_s("-- "); probe_s(name); probe_ch('\n'); }

#endif
