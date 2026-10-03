/*
 * Test: intuition/idcmp_gfxbase_fields
 * Tests that GfxBase fields required by apps are properly initialized.
 * Many Amiga applications read GfxBase fields directly at startup
 * and fail silently (exit with rv=26, missing UI) if they find zeros.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/monitor.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(const char *label, LONG val)
{
    char buf[64];
    char *p = buf;
    LONG v = val;
    int neg = 0;
    int i = 0;
    char tmp[16];

    /* copy label */
    while (*label) *p++ = *label++;

    if (v < 0)
    {
        neg = 1;
        v = -v;
    }
    if (v == 0)
    {
        tmp[i++] = '0';
    }
    else
    {
        while (v > 0)
        {
            tmp[i++] = '0' + (v % 10);
            v /= 10;
        }
    }
    if (neg) *p++ = '-';
    while (i > 0)
    {
        *p++ = tmp[--i];
    }
    *p++ = '\n';
    *p = 0;

    print(buf);
}

/*
 * Phase 220: validated against AmigaOS 3.1 (A4000, PAL).  Only values that
 * do not depend on the machine configuration (chipset, display hardware)
 * are compared exactly; the others must merely be initialized.
 */
static int check(int ok, const char *what)
{
    print(ok ? "  OK: " : "  FAIL: ");
    print(what);
    print("\n");
    return ok ? 0 : 1;
}

int main(void)
{
    int errors = 0;

    print("Testing GfxBase field initialization...\n");

    print("Test 1: NormalDisplayRows...\n");
    errors += check(GfxBase->NormalDisplayRows == 256, "NormalDisplayRows = 256 (PAL)");

    print("Test 2: NormalDisplayColumns...\n");
    errors += check(GfxBase->NormalDisplayColumns == 640, "NormalDisplayColumns = 640");

    print("Test 3: MaxDisplayRow...\n");
    errors += check(GfxBase->MaxDisplayRow == 311, "MaxDisplayRow = 311 (PAL)");

    print("Test 4: MaxDisplayColumn...\n");
    errors += check(GfxBase->MaxDisplayColumn == 455, "MaxDisplayColumn = 455");

    print("Test 5: DisplayFlags (PAL|REALLY_PAL)...\n");
    errors += check((GfxBase->DisplayFlags & (PAL | REALLY_PAL)) == (PAL | REALLY_PAL),
                    "DisplayFlags has PAL|REALLY_PAL");

    print("Test 6: VBlankFrequency...\n");
    errors += check(SysBase->VBlankFrequency == 50, "ExecBase VBlankFrequency = 50 (PAL)");

    print("Test 7: ChipRevBits0...\n");
    errors += check((GfxBase->ChipRevBits0 & SETCHIPREV_ECS) == SETCHIPREV_ECS,
                    "ChipRevBits0 has the ECS bits");

    print("Test 8: NormalDPMX...\n");
    errors += check(GfxBase->NormalDPMX != 0, "NormalDPMX initialized");

    print("Test 9: NormalDPMY...\n");
    errors += check(GfxBase->NormalDPMY != 0, "NormalDPMY initialized");

    print("Test 10: MicrosPerLine...\n");
    /* in 1/256 microseconds: a PAL line takes ~64 us */
    errors += check(GfxBase->MicrosPerLine > 16000 && GfxBase->MicrosPerLine < 16500,
                    "MicrosPerLine ~ 64 us (in 1/256 us)");

    print("Test 11: MinDisplayColumn...\n");
    errors += check(GfxBase->MinDisplayColumn != 0, "MinDisplayColumn initialized");

    print("Test 12: monitor_id...\n");
    errors += check(GfxBase->monitor_id == (PAL_MONITOR_ID >> 16), "monitor_id = PAL monitor");

    if (errors == 0)
    {
        print("PASS: gfxbase_fields all tests passed\n");
        return 0;
    }
    print("FAIL: gfxbase_fields had errors\n");
    return 20;
}
