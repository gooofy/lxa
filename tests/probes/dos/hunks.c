/*
 * Probe (Phase 237c): which hunk types LoadSeg() accepts in a load file
 * and what they do - HUNK_NAME (Fred Fish FollowMouse, Cass, CrossDOS
 * Install carry it), HUNK_UNIT, HUNK_RELOC32SHORT, HUNK_DREL32 used as a
 * short relocation, HUNK_RELRELOC32, HUNK_RELOC16/8.  Each case writes a
 * small two-hunk file to T: and prints whether LoadSeg() succeeds and the
 * relocated longword.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/doshunks.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static ULONG buf[64];

static void try(const char *label, const ULONG *extra, int n, int kind)
{
    /* hunk 0: code, 2 longs; hunk 1: data, 1 long */
    int k = 0, i;
    BPTR fh, seg;

    buf[k++] = HUNK_HEADER; buf[k++] = 0; buf[k++] = 2; buf[k++] = 0; buf[k++] = 1;
    buf[k++] = 2; buf[k++] = 1;
    if (kind == 1)
        for (i = 0; i < n; i++)          /* before the first hunk */
            buf[k++] = extra[i];
    buf[k++] = HUNK_CODE; buf[k++] = 2; buf[k++] = 0x10; buf[k++] = 0x4e754e75;
    if (kind == 0)
        for (i = 0; i < n; i++)          /* after the code */
            buf[k++] = extra[i];
    buf[k++] = HUNK_END;
    buf[k++] = HUNK_DATA; buf[k++] = 1; buf[k++] = 0x12345678;
    buf[k++] = HUNK_END;

    fh = Open((STRPTR)"T:probe-hunks", MODE_NEWFILE);
    if (!fh)
        return;
    Write(fh, buf, k * 4);
    Close(fh);
    seg = LoadSeg((STRPTR)"T:probe-hunks");
    probe_s(label);
    if (!seg) {
        probe_s(": fails, IoErr ");
        probe_dec(IoErr());
        probe_ch('\n');
    } else {
        ULONG *h0 = (ULONG *)BADDR(seg);
        ULONG *h1 = (ULONG *)BADDR(h0[0]);
        LONG d = (LONG)h0[1] - (LONG)(h1 + 1);
        probe_s(": loads, code[0] ");
        if (d == 16)
            probe_s("relocated to the data hunk");
        else if (h0[1] == 0x10)
            probe_s("unchanged");
        else if ((LONG)h0[1] == (LONG)(h1 + 1) - (LONG)(h0 + 1) + 16)
            probe_s("relocated PC-relative");
        else
            probe_s("changed otherwise");
        probe_ch('\n');
        UnLoadSeg(seg);
    }
}

int main(void)
{
    static const ULONG name[] = { HUNK_NAME, 1, 0x6e616d65 };
    static const ULONG unit[] = { HUNK_UNIT, 1, 0x756e6974 };
    static const ULONG r32[] = { HUNK_RELOC32, 1, 1, 0, 0 };
    /* short: count.w, hunk.w, offset.w ..., 0.w, padded to a longword */
    static const ULONG r32s[] = { HUNK_RELOC32SHORT, 0x00010001, 0x00000000 };
    static const ULONG drel32[] = { HUNK_DREL32, 0x00010001, 0x00000000 };
    static const ULONG rrel32[] = { HUNK_RELRELOC32, 0x00010001, 0x00000000 };

    try("plain", NULL, 0, 0);
    try("HUNK_NAME before the hunk", name, 3, 1);
    try("HUNK_NAME after the code", name, 3, 0);
    try("HUNK_UNIT before the hunk", unit, 3, 1);
    try("HUNK_RELOC32", r32, 5, 0);
    try("HUNK_RELOC32SHORT", r32s, 3, 0);
    try("HUNK_DREL32", drel32, 3, 0);
    try("HUNK_RELRELOC32", rrel32, 3, 0);
    DeleteFile((STRPTR)"T:probe-hunks");
    return 0;
}
