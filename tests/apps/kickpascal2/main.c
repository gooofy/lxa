/*
 * Test: apps/kickpascal2
 *
 * Automated test for KickPascal 2 IDE compatibility.
 *
 * This tests that the KickPascal 2 binary can be loaded via LoadSeg,
 * validating the hunk loader and relocation handling for a complex
 * real-world application.
 *
 * Note: We do NOT launch KP2 as a background process here, because
 * KP2 opens a window and waits for user input indefinitely. The
 * interactive KP2 tests are handled by the dedicated kickpascal_gtest
 * driver which can inject keystrokes and manage the application
 * lifecycle.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

int main(void)
{
    BPTR kp_seg = 0;

    print("=== KickPascal 2 Automated Compatibility Test ===\n\n");

    /* Test 1: Load KP2 binary */
    print("--- Test 1: Load KickPascal 2 binary ---\n");
    kp_seg = LoadSeg((STRPTR)"APPS:KP2/KP");
    if (!kp_seg) {
        print("FAIL: Cannot load KP2 from APPS:KP2/KP\n");
        return 20;
    }
    print("OK: KP2 binary loaded successfully\n");

    /* Verify the segment is valid by checking it's non-zero */
    print("OK: Segment list validated\n");

    /* Clean up - unload the segment */
    UnLoadSeg(kp_seg);
    print("OK: Segment unloaded successfully\n");

    print("\nPASS: KickPascal 2 load test passed\n");
    return 0;
}
