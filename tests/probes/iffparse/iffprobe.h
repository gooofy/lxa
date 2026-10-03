/*
 * iffprobe.h - helpers for the iffparse.library conformance probes
 * (roadmap Phase 222f).
 *
 * A memory stream with a client stream hook (InitIFF) logs every hook call
 * the library makes ("I" init, "C" cleanup, "R<n>" read, "W<n>" write,
 * "S<n>" seek, "=<rc>" when the hook returned an error), so the probes
 * show the exact I/O pattern of the parser and writer, not only results.
 * A failure can be injected on the n-th call of a given command.
 */
#ifndef LXA_IFFPROBE_H
#define LXA_IFFPROBE_H

#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/iffparse.h>
#include <utility/hooks.h>
#include <clib/exec_protos.h>
#include <clib/iffparse_protos.h>
#include <inline/exec.h>
#include <inline/iffparse.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IFFParseBase;

#define ID_ILBM MAKE_ID('I','L','B','M')
#define ID_BMHD MAKE_ID('B','M','H','D')
#define ID_CMAP MAKE_ID('C','M','A','P')
#define ID_BODY MAKE_ID('B','O','D','Y')
#define ID_CRNG MAKE_ID('C','R','N','G')
#define ID_TEST MAKE_ID('T','E','S','T')
#define ID_NAME MAKE_ID('N','A','M','E')
#define ID_DATA MAKE_ID('D','A','T','A')
#define ID_FTXT MAKE_ID('F','T','X','T')
#define ID_CHRS MAKE_ID('C','H','R','S')
#define ID_ANNO MAKE_ID('A','N','N','O')
#define ID_8SVX MAKE_ID('8','S','V','X')

/* Hook entry: a0 = hook, a2 = object, a1 = message -> C function(hook, obj, msg) */
__asm__(
    "    .text\n"
    "    .globl _iffprobe_hookentry\n"
    "_iffprobe_hookentry:\n"
    "    move.l a1,-(sp)\n"
    "    move.l a2,-(sp)\n"
    "    move.l a0,-(sp)\n"
    "    move.l 12(a0),a0\n"
    "    jsr (a0)\n"
    "    lea 12(sp),sp\n"
    "    rts\n");
extern ULONG iffprobe_hookentry(void);

static void ip_inithook(struct Hook *h, ULONG (*fn)(struct Hook *, APTR, APTR), APTR data)
{
    h->h_Entry = (ULONG (*)())iffprobe_hookentry;
    h->h_SubEntry = (ULONG (*)())fn;
    h->h_Data = data;
}

/* ---- call log ---------------------------------------------------------- */

static char ip_log[1024];
static int ip_loglen;

static void ip_logc(char c)
{
    if (ip_loglen < (int)sizeof(ip_log) - 1)
        ip_log[ip_loglen++] = c;
}

static void ip_logs(const char *s)
{
    while (*s)
        ip_logc(*s++);
}

static void ip_logd(LONG v)
{
    char t[12];
    int n = 0;
    ULONG u = v < 0 ? (ULONG)(-(v + 1)) + 1 : (ULONG)v;
    if (v < 0)
        ip_logc('-');
    do {
        t[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (n)
        ip_logc(t[--n]);
}

/* print and clear the log as "  io: ..." (nothing if empty) */
static void ip_flushlog(void)
{
    int i;
    if (!ip_loglen)
        return;
    probe_s("  io:");
    for (i = 0; i < ip_loglen; i++)
        probe_ch(ip_log[i]);
    probe_ch('\n');
    ip_loglen = 0;
}

/* ---- memory stream ----------------------------------------------------- */

#define MS_CAP 1024

struct MemStream {
    UBYTE data[MS_CAP];
    LONG len;           /* valid bytes */
    LONG pos;
    LONG fail_cmd;      /* IFFCMD_* to fail, or -1 */
    LONG fail_nth;      /* fail the n-th such call (1-based) */
    LONG fail_rc;       /* value returned on failure */
    LONG count[5];
    BOOL quiet;
};

static struct Hook ip_streamhook;

static ULONG ip_streamfunc(struct Hook *h, APTR obj, APTR msg)
{
    struct IFFHandle *iff = (struct IFFHandle *)obj;
    struct IFFStreamCmd *cmd = (struct IFFStreamCmd *)msg;
    struct MemStream *ms = (struct MemStream *)iff->iff_Stream;
    LONG c = cmd->sc_Command;
    LONG n = cmd->sc_NBytes;
    LONG rc = 0;
    (void)h;

    if (!ms->quiet) {
        ip_logc(' ');
        switch (c) {
        case IFFCMD_INIT:    ip_logc('I'); break;
        case IFFCMD_CLEANUP: ip_logc('C'); break;
        case IFFCMD_READ:    ip_logc('R'); ip_logd(n); break;
        case IFFCMD_WRITE:   ip_logc('W'); ip_logd(n); break;
        case IFFCMD_SEEK:    ip_logc('S'); ip_logd(n); break;
        default:             ip_logc('?'); ip_logd(c); break;
        }
    }
    if (c >= 0 && c < 5) {
        ms->count[c]++;
        if (c == ms->fail_cmd && ms->count[c] == ms->fail_nth) {
            rc = ms->fail_rc;
            goto out;
        }
    }
    switch (c) {
    case IFFCMD_READ:
        if (n < 0 || ms->pos + n > ms->len) {
            LONG avail = ms->len - ms->pos;
            if (avail > 0 && n > 0)
                CopyMem(ms->data + ms->pos, cmd->sc_Buf, avail);
            if (avail > 0)
                ms->pos = ms->len;
            rc = 1;     /* short read */
        } else {
            CopyMem(ms->data + ms->pos, cmd->sc_Buf, n);
            ms->pos += n;
        }
        break;
    case IFFCMD_WRITE:
        if (n < 0 || ms->pos + n > MS_CAP)
            rc = 2;
        else {
            CopyMem(cmd->sc_Buf, ms->data + ms->pos, n);
            ms->pos += n;
            if (ms->pos > ms->len)
                ms->len = ms->pos;
        }
        break;
    case IFFCMD_SEEK:
        if (ms->pos + n < 0 || ms->pos + n > ms->len)
            rc = 3;
        else
            ms->pos += n;
        break;
    default:
        break;
    }
out:
    if (rc && !ms->quiet) {
        ip_logc('=');
        ip_logd(rc);
    }
    return (ULONG)rc;
}

static void ms_reset(struct MemStream *ms)
{
    int i;
    ms->len = 0;
    ms->pos = 0;
    ms->fail_cmd = -1;
    ms->fail_nth = 0;
    ms->fail_rc = 0;
    ms->quiet = FALSE;
    for (i = 0; i < 5; i++)
        ms->count[i] = 0;
}

static void ms_rewind(struct MemStream *ms)
{
    int i;
    ms->pos = 0;
    ms->fail_cmd = -1;
    for (i = 0; i < 5; i++)
        ms->count[i] = 0;
}

static void ms_fail(struct MemStream *ms, LONG cmd, LONG nth, LONG rc)
{
    ms->fail_cmd = cmd;
    ms->fail_nth = nth;
    ms->fail_rc = rc;
}

/* builder for hand-made (and malformed) files */
static void ms_id(struct MemStream *ms, ULONG v)
{
    ms->data[ms->len++] = (UBYTE)(v >> 24);
    ms->data[ms->len++] = (UBYTE)(v >> 16);
    ms->data[ms->len++] = (UBYTE)(v >> 8);
    ms->data[ms->len++] = (UBYTE)v;
}

static void ms_str(struct MemStream *ms, const char *s, LONG n)
{
    while (n-- > 0)
        ms->data[ms->len++] = (UBYTE)*s++;
}

static void ms_pad(struct MemStream *ms)
{
    if (ms->len & 1)
        ms->data[ms->len++] = 0;
}

/* ---- printing ---------------------------------------------------------- */

static void ip_id(ULONG id)
{
    int i;
    for (i = 3; i >= 0; i--) {
        UBYTE c = (UBYTE)(id >> (i * 8));
        if (c >= 0x20 && c < 0x7f)
            probe_ch((char)c);
        else
            probe_ch('.');
    }
}

static void ip_err(LONG rc)
{
    static const char *const names[] = {
        "EOF", "EOC", "NOSCOPE", "NOMEM", "READ", "WRITE", "SEEK",
        "MANGLED", "SYNTAX", "NOTIFF", "NOHOOK", "RETURN2CLIENT"
    };
    probe_dec(rc);
    if (rc <= -1 && rc >= -12) {
        probe_ch('(');
        probe_s(names[-rc - 1]);
        probe_ch(')');
    }
}

/* print one context node: ID/TYPE size scan */
static void ip_cn(struct ContextNode *cn)
{
    if (!cn) {
        probe_s("NULL");
        return;
    }
    ip_id(cn->cn_ID);
    probe_ch('/');
    ip_id(cn->cn_Type);
    probe_s(" sz=");
    probe_dec(cn->cn_Size);
    probe_s(" sc=");
    probe_dec(cn->cn_Scan);
}

/* "label: rc=<rc> d=<depth> top=<cn> < parent < ..." then the io log */
static void ip_state(const char *label, struct IFFHandle *iff, LONG rc)
{
    struct ContextNode *cn;
    int n = 0;
    probe_s(label);
    probe_s(": rc=");
    ip_err(rc);
    probe_s(" d=");
    probe_dec(iff->iff_Depth);
    probe_s(" top=");
    cn = CurrentChunk(iff);
    ip_cn(cn);
    if (cn) {
        while ((cn = ParentChunk(cn)) != NULL && n++ < 12) {
            probe_s(" < ");
            ip_cn(cn);
        }
    }
    probe_ch('\n');
    ip_flushlog();
}

static void ip_bytes(const char *label, const UBYTE *p, LONG len)
{
    LONG i;
    probe_s(label);
    probe_s(" (");
    probe_dec(len);
    probe_s(") =");
    for (i = 0; i < len; i++) {
        static const char hx[] = "0123456789abcdef";
        if (i && (i % 32) == 0)
            probe_s("\n   ");
        probe_ch(' ');
        probe_ch(hx[p[i] >> 4]);
        probe_ch(hx[p[i] & 15]);
    }
    probe_ch('\n');
}

static int ip_open(void)
{
    IFFParseBase = OpenLibrary((CONST_STRPTR)"iffparse.library", 39);
    if (!IFFParseBase) {
        P_STR("error", "cannot open iffparse.library");
        return 0;
    }
    ip_inithook(&ip_streamhook, ip_streamfunc, NULL);
    return 1;
}

#endif
