/*
 * Probe (Phase 237c): overlaid executables (HUNK_OVERLAY).  Writes a small
 * overlaid hunk file to T:, LoadSeg()s it and checks what LoadSeg() leaves
 * in the root's first hunk for the overlay manager ($ABCD magic, open file,
 * overlay table, hunk table), then loads the overlay node the way an
 * overlay manager does - LoadSeg(NULL) with D2 = hunk table, D3 = file,
 * positioned at the node - and unloads everything.  Fred Fish's "Movie"
 * players, MRBackup and DataEasy are overlaid programs.
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

/* LoadSeg(NULL, table, fh) - the overlay manager's call */
static BPTR load_node(BPTR table, BPTR fh)
{
    register BPTR d0 __asm("d0");
    register LONG d1 __asm("d1") = 0;
    register BPTR d2 __asm("d2") = table;
    register BPTR d3 __asm("d3") = fh;
    register struct DosLibrary *a6 __asm("a6") = DOSBase;
    __asm volatile ("jsr -150(a6)" : "=r"(d0), "+r"(d1), "+r"(d2), "+r"(d3) : "r"(a6) : "a0", "a1", "cc", "memory");
    return d0;
}

static ULONG file[] = {
    HUNK_HEADER, 0, 3, 0, 1,        /* 3 hunks, root = 0..1 */
    8, 1,                           /* sizes in longs of hunks 0 and 1 */
    /* root hunk 0: overlay manager stub */
    HUNK_CODE, 8,
    0x60000010, 0x0000abcd, 0, 0, 0, 0, 0, 0x4e754e75,
    HUNK_END,
    /* root hunk 1 */
    HUNK_DATA, 1, 0x11111111,
    HUNK_END,
    /* overlay table: 2 longs (+1) */
    HUNK_OVERLAY, 2, 0xa1a1a1a1, 0xb2b2b2b2, 0xc3c3c3c3,
    HUNK_BREAK,
    /* node: hunk 2, relocated against root hunk 1 */
    HUNK_HEADER, 0, 3, 2, 2, 2,
    HUNK_DATA, 2, 0x22222222, 0,
    HUNK_RELOC32, 1, 1, 4, 0,
    HUNK_END,
    HUNK_BREAK,
};
#define NODE_OFFSET ((5 + 2 + 2 + 8 + 1 + 3 + 1 + 5 + 1) * 4)

int main(void)
{
    BPTR fh = Open((STRPTR)"T:probe-overlay", MODE_NEWFILE), seg, node;
    ULONG *h, *h1, *n;

    if (!fh)
        return 20;
    Write(fh, file, sizeof(file));
    Close(fh);

    seg = LoadSeg((STRPTR)"T:probe-overlay");
    P_BOOL("LoadSeg root", seg != 0);
    if (!seg) {
        P_LONG("IoErr", IoErr());
        DeleteFile((STRPTR)"T:probe-overlay");
        return 0;
    }
    h = (ULONG *)BADDR(seg);
    h1 = (ULONG *)BADDR(h[0]);
    P_HEX("h[2] magic", h[2]);
    P_BOOL("h[3] file set", h[3] != 0);
    P_BOOL("h[4] overlay table set", h[4] != 0);
    if (h[4]) {
        P_HEX("table[0]", ((ULONG *)h[4])[0]);
        P_HEX("table[2]", ((ULONG *)h[4])[2]);
    }
    P_BOOL("h[5] hunk table set", h[5] != 0);
    P_BOOL("h[6] global vector set", h[6] != 0);
    P_BOOL("root has two hunks", h1 != NULL && h1[0] == 0);
    if (h[5]) {
        ULONG *tab = (ULONG *)BADDR(h[5]);
        P_BOOL("hunk table [0] is the root", tab[0] == (ULONG)seg);
        P_BOOL("hunk table [2] empty", tab[2] == 0);
    }
    if (h[3] && h[5]) {
        Seek((BPTR)h[3], NODE_OFFSET, OFFSET_BEGINNING);
        node = load_node((BPTR)h[5], (BPTR)h[3]);
        P_BOOL("node loaded", node != 0);
        if (node) {
            n = (ULONG *)BADDR(node);
            P_HEX("node data", n[1]);
            P_BOOL("node relocated to root hunk 1", n[2] == (ULONG)(h1 + 1));
            P_BOOL("hunk table [2] is the node", ((ULONG *)BADDR(h[5]))[2] == (ULONG)node);
            UnLoadSeg(node);
        }
    }
    UnLoadSeg(seg);
    P_BOOL("unloaded", TRUE);
    DeleteFile((STRPTR)"T:probe-overlay");
    return 0;
}
