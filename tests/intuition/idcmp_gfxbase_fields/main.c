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

int main(void)
{
    int errors = 0;

    print("Testing GfxBase field initialization...\n");

    /* Core display fields (Phase 108 and earlier) */
    print("Test 1: NormalDisplayRows...\n");
    if (GfxBase->NormalDisplayRows != 256)
    {
        print("  FAIL: NormalDisplayRows != 256\n");
        print_num("  Got: ", (LONG)GfxBase->NormalDisplayRows);
        errors++;
    }
    else
    {
        print("  OK: NormalDisplayRows = 256\n");
    }

    print("Test 2: NormalDisplayColumns...\n");
    if (GfxBase->NormalDisplayColumns != 640)
    {
        print("  FAIL: NormalDisplayColumns != 640\n");
        print_num("  Got: ", (LONG)GfxBase->NormalDisplayColumns);
        errors++;
    }
    else
    {
        print("  OK: NormalDisplayColumns = 640\n");
    }

    print("Test 3: MaxDisplayRow...\n");
    if (GfxBase->MaxDisplayRow != 312)
    {
        print("  FAIL: MaxDisplayRow != 312\n");
        print_num("  Got: ", (LONG)GfxBase->MaxDisplayRow);
        errors++;
    }
    else
    {
        print("  OK: MaxDisplayRow = 312\n");
    }

    print("Test 4: MaxDisplayColumn...\n");
    if (GfxBase->MaxDisplayColumn != 640)
    {
        print("  FAIL: MaxDisplayColumn != 640\n");
        print_num("  Got: ", (LONG)GfxBase->MaxDisplayColumn);
        errors++;
    }
    else
    {
        print("  OK: MaxDisplayColumn = 640\n");
    }

    print("Test 5: DisplayFlags (PAL|REALLY_PAL)...\n");
    if (!(GfxBase->DisplayFlags & PAL))
    {
        print("  FAIL: PAL flag not set\n");
        print_num("  DisplayFlags: ", (LONG)GfxBase->DisplayFlags);
        errors++;
    }
    else if (!(GfxBase->DisplayFlags & REALLY_PAL))
    {
        print("  FAIL: REALLY_PAL flag not set\n");
        print_num("  DisplayFlags: ", (LONG)GfxBase->DisplayFlags);
        errors++;
    }
    else
    {
        print("  OK: DisplayFlags has PAL|REALLY_PAL\n");
    }

    print("Test 6: VBlank...\n");
    if (GfxBase->VBlank != 50)
    {
        print("  FAIL: VBlank != 50\n");
        print_num("  Got: ", (LONG)GfxBase->VBlank);
        errors++;
    }
    else
    {
        print("  OK: VBlank = 50\n");
    }

    print("Test 7: ChipRevBits0 (ECS)...\n");
    if (GfxBase->ChipRevBits0 != SETCHIPREV_ECS)
    {
        print("  FAIL: ChipRevBits0 != SETCHIPREV_ECS\n");
        print_num("  Got: ", (LONG)GfxBase->ChipRevBits0);
        errors++;
    }
    else
    {
        print("  OK: ChipRevBits0 = SETCHIPREV_ECS\n");
    }

    /* Phase 109 audit fields */
    print("Test 8: NormalDPMX...\n");
    if (GfxBase->NormalDPMX != 22)
    {
        print("  FAIL: NormalDPMX != 22\n");
        print_num("  Got: ", (LONG)GfxBase->NormalDPMX);
        errors++;
    }
    else
    {
        print("  OK: NormalDPMX = 22\n");
    }

    print("Test 9: NormalDPMY...\n");
    if (GfxBase->NormalDPMY != 22)
    {
        print("  FAIL: NormalDPMY != 22\n");
        print_num("  Got: ", (LONG)GfxBase->NormalDPMY);
        errors++;
    }
    else
    {
        print("  OK: NormalDPMY = 22\n");
    }

    print("Test 10: MicrosPerLine...\n");
    if (GfxBase->MicrosPerLine != 64)
    {
        print("  FAIL: MicrosPerLine != 64\n");
        print_num("  Got: ", (LONG)GfxBase->MicrosPerLine);
        errors++;
    }
    else
    {
        print("  OK: MicrosPerLine = 64\n");
    }

    print("Test 11: MinDisplayColumn...\n");
    if (GfxBase->MinDisplayColumn != 0x71)
    {
        print("  FAIL: MinDisplayColumn != 0x71\n");
        print_num("  Got: ", (LONG)GfxBase->MinDisplayColumn);
        errors++;
    }
    else
    {
        print("  OK: MinDisplayColumn = 0x71\n");
    }

    print("Test 12: monitor_id...\n");
    if (GfxBase->monitor_id != 0)
    {
        print("  FAIL: monitor_id != 0\n");
        print_num("  Got: ", (LONG)GfxBase->monitor_id);
        errors++;
    }
    else
    {
        print("  OK: monitor_id = 0\n");
    }

    print("Test 13: TopLine...\n");
    if (GfxBase->TopLine != 0)
    {
        print("  FAIL: TopLine != 0\n");
        print_num("  Got: ", (LONG)GfxBase->TopLine);
        errors++;
    }
    else
    {
        print("  OK: TopLine = 0\n");
    }

    if (errors == 0)
    {
        print("PASS: gfxbase_fields all tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: gfxbase_fields had errors\n");
        return 20;
    }
}
