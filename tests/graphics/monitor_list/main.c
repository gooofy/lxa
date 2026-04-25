/*
 * Test: graphics/monitor_list (Phase 151)
 *
 * Verifies that GfxBase->MonitorList is populated with the standard system
 * MonitorSpec nodes (default.monitor, pal.monitor, ntsc.monitor) at
 * coldstart, and that OpenMonitor()/CloseMonitor() return real entries
 * instead of NULL stubs.
 *
 * Apps such as DPaint V's Screen Format dialog enumerate this list to
 * populate their available-mode panels.  Without populated entries those
 * panels render empty.  Per RKRM (Libraries, Graphics, "Monitors"):
 *   - default.monitor is always the head of the list.
 *   - pal.monitor and ntsc.monitor are present on every system.
 *   - OpenMonitor(NULL, 0) returns the first MonitorSpec.
 *   - OpenMonitor(name, 0) looks up by ms_Node.ln_Name.
 *   - OpenMonitor(NULL, displayID) selects by monitor compatibility bits.
 *   - CloseMonitor() returns TRUE on success.
 */

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxnodes.h>
#include <graphics/monitor.h>
#include <graphics/modeid.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase   *SysBase;
extern struct GfxBase    *GfxBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

/* Tiny in-place strcmp — avoids depending on libc. */
static int xstrcmp(const char *a, const char *b)
{
    if (!a || !b) return -1;
    while (*a && *b && *a == *b) { a++; b++; }
    return (int)((UBYTE)*a) - (int)((UBYTE)*b);
}

int main(void)
{
    int errors = 0;
    struct Node *n;
    int count;
    BOOL saw_default = FALSE;
    BOOL saw_pal     = FALSE;
    BOOL saw_ntsc    = FALSE;
    struct MonitorSpec *ms;

    print("Testing GfxBase->MonitorList + OpenMonitor()/CloseMonitor()...\n");

    /* 1. MonitorList must be initialised (non-NULL head/tailpred). */
    if (GfxBase == NULL)
    {
        print("FAIL: GfxBase is NULL\n");
        return 20;
    }
    if (GfxBase->MonitorList.lh_Head == NULL ||
        GfxBase->MonitorList.lh_TailPred == NULL)
    {
        print("FAIL: MonitorList is not initialised\n");
        errors++;
    }
    else
    {
        print("OK: MonitorList is initialised\n");
    }

    /* 2. Walk the list: must contain at least default/pal/ntsc by name. */
    count = 0;
    for (n = GfxBase->MonitorList.lh_Head;
         n && n->ln_Succ;
         n = n->ln_Succ)
    {
        count++;
        if (n->ln_Name)
        {
            if (xstrcmp(n->ln_Name, DEFAULT_MONITOR_NAME) == 0) saw_default = TRUE;
            if (xstrcmp(n->ln_Name, PAL_MONITOR_NAME)     == 0) saw_pal     = TRUE;
            if (xstrcmp(n->ln_Name, NTSC_MONITOR_NAME)    == 0) saw_ntsc    = TRUE;
        }
        if (count > 100)
        {
            print("FAIL: MonitorList walk exceeded 100 entries (corrupt list?)\n");
            errors++;
            break;
        }
    }
    if (count < 3)
    {
        print("FAIL: MonitorList has fewer than 3 entries\n");
        errors++;
    }
    else
    {
        print("OK: MonitorList walk completed\n");
    }
    if (!saw_default) { print("FAIL: default.monitor missing from MonitorList\n"); errors++; }
    else              print("OK: default.monitor present\n");
    if (!saw_pal)     { print("FAIL: pal.monitor missing from MonitorList\n");     errors++; }
    else              print("OK: pal.monitor present\n");
    if (!saw_ntsc)    { print("FAIL: ntsc.monitor missing from MonitorList\n");    errors++; }
    else              print("OK: ntsc.monitor present\n");

    /* 3. OpenMonitor(NULL, 0) must return the head node (non-NULL). */
    ms = OpenMonitor(NULL, 0);
    if (ms == NULL)
    {
        print("FAIL: OpenMonitor(NULL, 0) returned NULL\n");
        errors++;
    }
    else
    {
        print("OK: OpenMonitor(NULL, 0) returned non-NULL\n");
        /* The returned MonitorSpec must be the head of the list. */
        if ((struct Node *)ms != GfxBase->MonitorList.lh_Head)
        {
            print("FAIL: OpenMonitor(NULL, 0) did not return list head\n");
            errors++;
        }
        else
        {
            print("OK: OpenMonitor(NULL, 0) returned list head\n");
        }
        if (CloseMonitor(ms) != TRUE)
        {
            print("FAIL: CloseMonitor() returned FALSE\n");
            errors++;
        }
        else
        {
            print("OK: CloseMonitor() returned TRUE\n");
        }
    }

    /* 4. OpenMonitor(name, 0) lookup by name. */
    ms = OpenMonitor((STRPTR)PAL_MONITOR_NAME, 0);
    if (ms == NULL || ms->ms_Node.xln_Name == NULL ||
        xstrcmp(ms->ms_Node.xln_Name, PAL_MONITOR_NAME) != 0)
    {
        print("FAIL: OpenMonitor(\"pal.monitor\", 0) lookup failed\n");
        errors++;
    }
    else
    {
        print("OK: OpenMonitor(\"pal.monitor\", 0) returned pal node\n");
        CloseMonitor(ms);
    }

    ms = OpenMonitor((STRPTR)NTSC_MONITOR_NAME, 0);
    if (ms == NULL || ms->ms_Node.xln_Name == NULL ||
        xstrcmp(ms->ms_Node.xln_Name, NTSC_MONITOR_NAME) != 0)
    {
        print("FAIL: OpenMonitor(\"ntsc.monitor\", 0) lookup failed\n");
        errors++;
    }
    else
    {
        print("OK: OpenMonitor(\"ntsc.monitor\", 0) returned ntsc node\n");
        CloseMonitor(ms);
    }

    /* 5. OpenMonitor(NULL, displayID) lookup by monitor compatibility bits.
     *    PAL_MONITOR_ID = 0x00021000, NTSC_MONITOR_ID = 0x00011000. */
    ms = OpenMonitor(NULL, PAL_MONITOR_ID | 0x00000001);
    if (ms == NULL || ms->ms_Node.xln_Name == NULL ||
        xstrcmp(ms->ms_Node.xln_Name, PAL_MONITOR_NAME) != 0)
    {
        print("FAIL: OpenMonitor(NULL, PAL_MONITOR_ID|...) did not select pal\n");
        errors++;
    }
    else
    {
        print("OK: OpenMonitor(NULL, PAL_MONITOR_ID|...) selected pal\n");
        CloseMonitor(ms);
    }

    ms = OpenMonitor(NULL, NTSC_MONITOR_ID | 0x00000001);
    if (ms == NULL || ms->ms_Node.xln_Name == NULL ||
        xstrcmp(ms->ms_Node.xln_Name, NTSC_MONITOR_NAME) != 0)
    {
        print("FAIL: OpenMonitor(NULL, NTSC_MONITOR_ID|...) did not select ntsc\n");
        errors++;
    }
    else
    {
        print("OK: OpenMonitor(NULL, NTSC_MONITOR_ID|...) selected ntsc\n");
        CloseMonitor(ms);
    }

    /* 6. Each MonitorSpec must have plausible field values (not zero). */
    ms = OpenMonitor((STRPTR)PAL_MONITOR_NAME, 0);
    if (ms != NULL)
    {
        if (ms->total_rows != STANDARD_PAL_ROWS)
        {
            print("FAIL: pal.monitor total_rows != STANDARD_PAL_ROWS\n");
            errors++;
        }
        else
        {
            print("OK: pal.monitor total_rows = STANDARD_PAL_ROWS\n");
        }
        if (ms->ms_Node.xln_Subsystem != SS_GRAPHICS)
        {
            print("FAIL: pal.monitor xln_Subsystem != SS_GRAPHICS\n");
            errors++;
        }
        else
        {
            print("OK: pal.monitor xln_Subsystem = SS_GRAPHICS\n");
        }
        if (ms->ms_Node.xln_Subtype != MONITOR_SPEC_TYPE)
        {
            print("FAIL: pal.monitor xln_Subtype != MONITOR_SPEC_TYPE\n");
            errors++;
        }
        else
        {
            print("OK: pal.monitor xln_Subtype = MONITOR_SPEC_TYPE\n");
        }
        CloseMonitor(ms);
    }

    if (errors == 0)
    {
        print("PASS: all MonitorList / OpenMonitor checks passed\n");
        return 0;
    }
    print("FAIL: MonitorList tests reported errors\n");
    return 20;
}
