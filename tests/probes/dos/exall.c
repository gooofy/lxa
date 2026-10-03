/*
 * Probe (Phase 222a): dos.library ExAll/ExAllEnd - every ED_ level up to
 * ED_COMMENT, eac_MatchString, small buffers (several calls), ExAllEnd
 * mid-scan, errors.  Entries are sorted before printing (the order is
 * file-system specific).
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/exall.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

#define BASE "T:lxaprobe_exall"

static char lines[32][96];
static int nlines;

static void add_line(struct ExAllData *ed, LONG type)
{
    char *o = lines[nlines];
    int k = 0;
    const char *s = (const char *)ed->ed_Name;
    while (*s && k < 30)
        o[k++] = *s++;
    if (type >= ED_TYPE) {
        o[k++] = ' ';
        o[k++] = 't';
        o[k++] = ed->ed_Type < 0 ? '-' : '+';
        o[k++] = (char)('0' + (ed->ed_Type < 0 ? -ed->ed_Type : ed->ed_Type));
    }
    if (type >= ED_SIZE) {
        ULONG v = ed->ed_Type < 0 ? ed->ed_Size : 0;   /* directory sizes are fs specific */
        char t[12];
        int n = 0;
        o[k++] = ' ';
        o[k++] = 's';
        do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
        while (n)
            o[k++] = t[--n];
    }
    if (type >= ED_PROTECTION) {
        static const char hx[] = "0123456789abcdef";
        int b;
        o[k++] = ' ';
        o[k++] = 'p';
        for (b = 7; b >= 0; b--)
            o[k++] = hx[(ed->ed_Prot >> (b * 4)) & 15];
    }
    if (type >= ED_COMMENT) {
        const char *c = (const char *)ed->ed_Comment;
        o[k++] = ' ';
        o[k++] = 'c';
        o[k++] = '"';
        while (c && *c && k < 90)
            o[k++] = *c++;
        o[k++] = '"';
    }
    o[k] = 0;
    if (nlines < 31)
        nlines++;
}

static void flush_lines(void)
{
    int a, b;
    for (a = 0; a < nlines; a++)
        for (b = a + 1; b < nlines; b++) {
            int c = 0;
            while (lines[a][c] && lines[a][c] == lines[b][c])
                c++;
            if ((UBYTE)lines[a][c] > (UBYTE)lines[b][c]) {
                char t[96];
                for (c = 0; c < 96; c++) { t[c] = lines[a][c]; lines[a][c] = lines[b][c]; lines[b][c] = t[c]; }
            }
        }
    for (a = 0; a < nlines; a++) {
        probe_s("  ");
        probe_s(lines[a]);
        probe_ch('\n');
    }
    nlines = 0;
}

static void scan(const char *label, LONG type, ULONG bufsize, const char *pattern, int stop_after)
{
    struct ExAllControl *eac = AllocDosObject(DOS_EXALLCONTROL, NULL);
    UBYTE *buf = AllocVec(bufsize, MEMF_CLEAR);
    UBYTE pat[64];
    BPTR lock = Lock((STRPTR)BASE, SHARED_LOCK);
    LONG more, calls = 0, total = 0;

    if (!eac || !buf || !lock)
        return;
    eac->eac_LastKey = 0;
    eac->eac_MatchString = NULL;
    if (pattern) {
        ParsePatternNoCase((STRPTR)pattern, (STRPTR)pat, sizeof(pat));
        eac->eac_MatchString = (STRPTR)pat;
    }
    probe_s(label);
    probe_ch('\n');
    do {
        struct ExAllData *ed;
        more = ExAll(lock, (struct ExAllData *)buf, bufsize, type, eac);
        calls++;
        if (!more && IoErr() != ERROR_NO_MORE_ENTRIES) {
            probe_s("  ExAll failed IoErr ");
            probe_dec(IoErr());
            probe_ch('\n');
            break;
        }
        total += eac->eac_Entries;
        for (ed = eac->eac_Entries ? (struct ExAllData *)buf : NULL; ed; ed = ed->ed_Next)
            add_line(ed, type);
        if (more && stop_after && calls >= stop_after) {
            ExAllEnd(lock, (struct ExAllData *)buf, bufsize, type, eac);
            probe_s("  ExAllEnd after ");
            probe_dec(calls);
            probe_s(" call(s)\n");
            break;
        }
    } while (more && calls < 40);
    flush_lines();
    probe_s("  entries ");
    probe_dec(total);
    probe_s(calls > 1 ? ", several calls\n" : ", one call\n");
    UnLock(lock);
    FreeVec(buf);
    FreeDosObject(DOS_EXALLCONTROL, eac);
}

static void mk(const char *name, LONG size, ULONG prot, const char *comment)
{
    BPTR fh = Open((STRPTR)name, MODE_NEWFILE);
    static char data[100];
    if (fh) {
        Write(fh, data, size);
        Close(fh);
    }
    if (comment)
        SetComment((STRPTR)name, (STRPTR)comment);
    SetProtection((STRPTR)name, prot);
}

static void cleanup(void)
{
    static const char *const f[] = {"alpha", "beta.c", "gamma.c", "delta.info", "epsilon", "zeta", "dir/x", "dir"};
    char path[64];
    int i;
    for (i = 0; i < 8; i++) {
        int k = 0;
        const char *s = BASE "/";
        while (*s) path[k++] = *s++;
        s = f[i];
        while (*s) path[k++] = *s++;
        path[k] = 0;
        SetProtection((STRPTR)path, 0);
        DeleteFile((STRPTR)path);
    }
    DeleteFile((STRPTR)BASE);
}

int main(void)
{
    BPTR l;
    cleanup();
    l = CreateDir((STRPTR)BASE);
    if (!l)
        return 20;
    UnLock(l);
    l = CreateDir((STRPTR)BASE "/dir");
    if (l)
        UnLock(l);
    mk(BASE "/alpha", 10, 0, "first file");
    mk(BASE "/beta.c", 0, FIBF_DELETE, NULL);
    mk(BASE "/gamma.c", 33, FIBF_SCRIPT | FIBF_ARCHIVE, "");
    mk(BASE "/delta.info", 99, 0, "an icon");
    mk(BASE "/epsilon", 1, FIBF_READ | FIBF_WRITE, NULL);
    mk(BASE "/zeta", 50, 0, NULL);
    mk(BASE "/dir/x", 5, 0, NULL);

    P_SECTION("ExAll levels");
    scan("ED_NAME", ED_NAME, 4096, NULL, 0);
    scan("ED_TYPE", ED_TYPE, 4096, NULL, 0);
    scan("ED_SIZE", ED_SIZE, 4096, NULL, 0);
    scan("ED_PROTECTION", ED_PROTECTION, 4096, NULL, 0);
    scan("ED_COMMENT", ED_COMMENT, 4096, NULL, 0);

    P_SECTION("ExAll pattern / small buffer / ExAllEnd");
    scan("pattern #?.c", ED_TYPE, 4096, "#?.c", 0);
    scan("pattern ~(#?.c)", ED_NAME, 4096, "~(#?.c)", 0);
    scan("pattern none", ED_NAME, 4096, "nomatch", 0);
    scan("small buffer (64 bytes)", ED_COMMENT, 64, NULL, 0);
    scan("ExAllEnd after first call", ED_NAME, 64, NULL, 1);

    P_SECTION("ExAll errors");
    {
        struct ExAllControl *eac = AllocDosObject(DOS_EXALLCONTROL, NULL);
        UBYTE buf[512];
        LONG r;
        BPTR f = Lock((STRPTR)BASE "/alpha", SHARED_LOCK);
        eac->eac_LastKey = 0;
        r = ExAll(f, (struct ExAllData *)buf, sizeof(buf), ED_NAME, eac);
        P_LONG("ExAll(file lock)", r);
        P_LONG("  IoErr", IoErr());
        P_LONG("  eac_Entries", eac->eac_Entries);
        UnLock(f);
        f = Lock((STRPTR)BASE, SHARED_LOCK);
        eac->eac_LastKey = 0;
        r = ExAll(f, (struct ExAllData *)buf, sizeof(buf), 99, eac);
        P_LONG("ExAll(type 99)", r);
        P_LONG("  IoErr", IoErr());
        if (r)
            ExAllEnd(f, (struct ExAllData *)buf, sizeof(buf), 99, eac);
        UnLock(f);
        FreeDosObject(DOS_EXALLCONTROL, eac);
    }
    cleanup();
    return 0;
}
