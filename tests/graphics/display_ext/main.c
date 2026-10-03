/*
 * Test: graphics/display_ext
 *
 * lxa-only (tests/ref_suite.yaml): lxa deliberately extends the display
 * database beyond AmigaOS 3.1 (Phase 129): DTAG_NAME returns mode names
 * (the AmigaOS 3.1 ROM database without DEVS:Monitors has no NameInfo and
 * returns 0).  IDs of unknown monitors are not virtualised any more: as on
 * 3.1 they do not exist (Phase 238, tests/probes/intuition/screenmodes).
 */

#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct GfxBase *GfxBase;

static int errors;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++)
        len++;
    Write(out, (CONST APTR)s, len);
}

static void check(int ok, const char *okmsg, const char *failmsg)
{
    print(ok ? okmsg : failmsg);
    if (!ok)
        errors++;
}

static int has(const char *s, const char *n)
{
    for (; *s; s++)
    {
        const char *a = s, *b = n;
        while (*a && *b && *a == *b) { a++; b++; }
        if (!*b)
            return 1;
    }
    return 0;
}

int main(void)
{
    struct NameInfo name;
    struct DimensionInfo dims;
    ULONG result;

    print("Testing lxa display database extensions...\n");

    check(FindDisplayInfo(0x00F00000) == NULL,
          "OK: FindDisplayInfo() of an unknown monitor's ID is NULL\n",
          "FAIL: FindDisplayInfo() found an unknown monitor's ID\n");

    result = GetDisplayInfoData(NULL, &dims, sizeof(dims), DTAG_DIMS, PAL_MONITOR_ID | 0x9024);
    check(result != 0 && dims.MaxRasterWidth > dims.MinRasterWidth,
          "OK: an unlisted PAL mode key returns DTAG_DIMS\n",
          "FAIL: an unlisted PAL mode key returned no DTAG_DIMS\n");

    result = GetDisplayInfoData(NULL, &name, sizeof(name), DTAG_NAME, HIRES_KEY);
    check(result == sizeof(name) && has((const char *)name.Name, "HIRES"),
          "OK: DTAG_NAME returns a mode name\n", "FAIL: DTAG_NAME returned no name\n");

    result = GetDisplayInfoData(FindDisplayInfo(LORES_KEY), &name, sizeof(name), DTAG_NAME, INVALID_ID);
    check(result == sizeof(name) && has((const char *)name.Name, "LORES"),
          "OK: DTAG_NAME handle lookup returns a mode name\n", "FAIL: DTAG_NAME handle lookup failed\n");

    if (errors == 0)
    {
        print("PASS: graphics/display_ext all tests passed\n");
        return 0;
    }
    print("FAIL: graphics/display_ext had errors\n");
    return 20;
}
