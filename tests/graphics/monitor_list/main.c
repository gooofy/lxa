/*
 * Test: graphics/monitor_list
 *
 * Verifies GfxBase->MonitorList and OpenMonitor()/CloseMonitor() against
 * the behaviour of AmigaOS 3.1 (reference machine).  Which monitors are in
 * the list depends on the machine configuration (DEVS:Monitors), so only
 * architecturally fixed facts are asserted: the native monitor of the
 * machine (pal.monitor on a PAL system, ntsc.monitor on NTSC) is always
 * listed and is the default monitor.
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

static int check(int ok, const char *okmsg, const char *failmsg)
{
    print(ok ? okmsg : failmsg);
    return ok ? 0 : 1;
}

int main(void)
{
    int errors = 0;
    struct Node *n;
    int count;
    BOOL saw_native = FALSE;
    BOOL is_pal;
    const char *native_name;
    ULONG native_id;
    struct MonitorSpec *ms;
    struct MonitorSpec *def;

    print("Testing GfxBase->MonitorList + OpenMonitor()/CloseMonitor()...\n");

    if (GfxBase == NULL)
    {
        print("FAIL: GfxBase is NULL\n");
        return 20;
    }

    is_pal = (GfxBase->DisplayFlags & PAL) ? TRUE : FALSE;
    native_name = is_pal ? PAL_MONITOR_NAME : NTSC_MONITOR_NAME;
    native_id = is_pal ? PAL_MONITOR_ID : NTSC_MONITOR_ID;

    errors += check(GfxBase->MonitorList.lh_Head != NULL && GfxBase->MonitorList.lh_TailPred != NULL,
                    "OK: MonitorList is initialised\n", "FAIL: MonitorList is not initialised\n");

    /* Walk the list: the native monitor must be present */
    count = 0;
    for (n = GfxBase->MonitorList.lh_Head; n && n->ln_Succ; n = n->ln_Succ)
    {
        count++;
        if (n->ln_Name && xstrcmp(n->ln_Name, native_name) == 0)
            saw_native = TRUE;
        if (count > 100)
        {
            print("FAIL: MonitorList walk exceeded 100 entries (corrupt list?)\n");
            errors++;
            break;
        }
    }
    errors += check(count >= 1, "OK: MonitorList walk completed\n", "FAIL: MonitorList is empty\n");
    errors += check(saw_native, "OK: native monitor present\n", "FAIL: native monitor missing from MonitorList\n");

    /* The default monitor is the native monitor */
    def = (struct MonitorSpec *)GfxBase->default_monitor;
    errors += check(def != NULL && def->ms_Node.xln_Name && xstrcmp(def->ms_Node.xln_Name, native_name) == 0,
                    "OK: GfxBase->default_monitor is the native monitor\n",
                    "FAIL: GfxBase->default_monitor is not the native monitor\n");

    /* OpenMonitor(NULL, 0) returns the default monitor */
    ms = OpenMonitor(NULL, 0);
    errors += check(ms != NULL && ms == def, "OK: OpenMonitor(NULL, 0) returned the default monitor\n",
                    "FAIL: OpenMonitor(NULL, 0) did not return the default monitor\n");
    if (ms)
        errors += check(CloseMonitor(ms) == FALSE, "OK: CloseMonitor() returned FALSE (no error)\n",
                        "FAIL: CloseMonitor() reported an error\n");

    /* Name lookups */
    ms = OpenMonitor((STRPTR)DEFAULT_MONITOR_NAME, 0);
    errors += check(ms != NULL && ms == def, "OK: OpenMonitor(\"default.monitor\", 0) returned the default monitor\n",
                    "FAIL: OpenMonitor(\"default.monitor\", 0) lookup failed\n");
    if (ms)
        CloseMonitor(ms);

    ms = OpenMonitor((STRPTR)native_name, 0);
    errors += check(ms != NULL && ms->ms_Node.xln_Name && xstrcmp(ms->ms_Node.xln_Name, native_name) == 0,
                    "OK: OpenMonitor(native name, 0) returned the native monitor\n",
                    "FAIL: OpenMonitor(native name, 0) lookup failed\n");
    if (ms)
        CloseMonitor(ms);

    ms = OpenMonitor((STRPTR)"bogus.monitor", 0);
    errors += check(ms == NULL, "OK: OpenMonitor(\"bogus.monitor\", 0) returned NULL\n",
                    "FAIL: OpenMonitor(\"bogus.monitor\", 0) returned a monitor\n");
    if (ms)
        CloseMonitor(ms);

    /* Display ID lookups */
    ms = OpenMonitor(NULL, HIRES_KEY);
    errors += check(ms != NULL && ms == def, "OK: OpenMonitor(NULL, HIRES_KEY) returned the default monitor\n",
                    "FAIL: OpenMonitor(NULL, HIRES_KEY) did not return the default monitor\n");
    if (ms)
        CloseMonitor(ms);

    ms = OpenMonitor(NULL, native_id | HIRES_KEY);
    errors += check(ms != NULL && ms->ms_Node.xln_Name && xstrcmp(ms->ms_Node.xln_Name, native_name) == 0,
                    "OK: OpenMonitor(NULL, native ID|HIRES_KEY) selected the native monitor\n",
                    "FAIL: OpenMonitor(NULL, native ID|HIRES_KEY) did not select the native monitor\n");
    if (ms)
        CloseMonitor(ms);

    ms = OpenMonitor(NULL, native_id | 0x00000001);
    errors += check(ms == NULL, "OK: OpenMonitor(NULL, invalid mode) returned NULL\n",
                    "FAIL: OpenMonitor(NULL, invalid mode) returned a monitor\n");
    if (ms)
        CloseMonitor(ms);

    ms = OpenMonitor(NULL, INVALID_ID);
    errors += check(ms == NULL, "OK: OpenMonitor(NULL, INVALID_ID) returned NULL\n",
                    "FAIL: OpenMonitor(NULL, INVALID_ID) returned a monitor\n");
    if (ms)
        CloseMonitor(ms);

    errors += check(CloseMonitor(NULL) == TRUE, "OK: CloseMonitor(NULL) returned TRUE\n",
                    "FAIL: CloseMonitor(NULL) did not return TRUE\n");

    /* Native MonitorSpec fields */
    ms = OpenMonitor((STRPTR)native_name, 0);
    if (ms != NULL)
    {
        errors += check(ms->total_rows == (is_pal ? STANDARD_PAL_ROWS : STANDARD_NTSC_ROWS),
                        "OK: native monitor total_rows is standard\n",
                        "FAIL: native monitor total_rows is not standard\n");
        errors += check(ms->ms_Node.xln_Type == NT_GRAPHICS && ms->ms_Node.xln_Subsystem == SS_GRAPHICS &&
                        ms->ms_Node.xln_Subtype == MONITOR_SPEC_TYPE,
                        "OK: native monitor is an NT_GRAPHICS MONITOR_SPEC_TYPE node\n",
                        "FAIL: native monitor node type is wrong\n");
        errors += check(ms->ratioh == RATIO_UNITY && ms->ratiov == RATIO_UNITY,
                        "OK: native monitor ratios are RATIO_UNITY\n",
                        "FAIL: native monitor ratios are not RATIO_UNITY\n");
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
