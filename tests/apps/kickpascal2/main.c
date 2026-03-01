/*
 * Test: apps/kickpascal2
 *
 * Automated test for KickPascal 2 IDE compatibility.
 *
 * This test verifies:
 * 1. KickPascal 2 binary can be loaded via LoadSeg
 * 2. KP2 can be launched as a background process
 * 3. KP2 opens its editor window
 *
 * The interactive KP2 tests (keyboard, mouse, text entry) are
 * handled by the dedicated kickpascal_gtest driver.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct IntuitionBase *IntuitionBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(const char *prefix, LONG val, const char *suffix)
{
    char buf[64];
    char *p = buf;
    const char *src = prefix;
    while (*src) *p++ = *src++;

    if (val < 0) {
        *p++ = '-';
        val = -val;
    }

    char digits[12];
    int i = 0;
    if (val == 0) {
        digits[i++] = '0';
    } else {
        while (val > 0) {
            digits[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    while (i > 0) *p++ = digits[--i];
    
    src = suffix;
    while (*src) *p++ = *src++;
    *p = '\0';
    print(buf);
}

/*
 * Count total windows across all screens.
 */
static int count_windows(void)
{
    struct Screen *scr;
    struct Window *win;
    int count = 0;

    scr = IntuitionBase->FirstScreen;
    while (scr) {
        win = scr->FirstWindow;
        while (win) {
            count++;
            win = win->NextWindow;
        }
        scr = scr->NextScreen;
    }
    return count;
}

int main(void)
{
    int errors = 0;
    BPTR seg = 0;
    struct Process *proc = NULL;
    int initial_windows;
    int windows_after;

    print("=== KickPascal 2 Automated Compatibility Test ===\n\n");

    /* Open required libraries */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase) {
        print("FAIL: Cannot open intuition.library\n");
        return 20;
    }

    /* ========== Test 1: Load KP2 binary ========== */
    print("--- Test 1: Load KickPascal 2 binary ---\n");

    seg = LoadSeg((STRPTR)"APPS:KP2/KP");
    if (!seg) {
        print("FAIL: Cannot load KP2 from APPS:KP2/KP\n");
        errors++;
        goto cleanup;
    }
    print("OK: KP2 binary loaded successfully\n");

    /* ========== Test 2: Launch KP2 as background process ========== */
    print("\n--- Test 2: Launch KP2 as background process ---\n");

    initial_windows = count_windows();
    print_num("  Initial window count: ", initial_windows, "\n");

    {
        BPTR nilIn = Open((STRPTR)"NIL:", MODE_OLDFILE);
        BPTR nilOut = Open((STRPTR)"NIL:", MODE_NEWFILE);

        if (!nilIn || !nilOut) {
            print("FAIL: Cannot open NIL: device\n");
            if (nilIn) Close(nilIn);
            if (nilOut) Close(nilOut);
            UnLoadSeg(seg);
            errors++;
            goto cleanup;
        }

        struct TagItem procTags[] = {
            { NP_Seglist, (ULONG)seg },
            { NP_Name, (ULONG)"KickPascal" },
            { NP_StackSize, 32768 },
            { NP_Cli, TRUE },
            { NP_Input, (ULONG)nilIn },
            { NP_Output, (ULONG)nilOut },
            { NP_Arguments, (ULONG)"\n" },
            { NP_FreeSeglist, TRUE },
            { TAG_DONE, 0 }
        };

        proc = CreateNewProc(procTags);
        if (!proc) {
            print("FAIL: CreateNewProc failed\n");
            UnLoadSeg(seg);
            Close(nilIn);
            Close(nilOut);
            errors++;
            goto cleanup;
        }
    }

    seg = 0;
    print("OK: KP2 process created\n");

    /* ========== Test 3: Wait for KP2 window to open ========== */
    print("\n--- Test 3: Wait for KP2 window ---\n");

    print("  Waiting for window to open...\n");
    {
        int wait_ticks = 0;
        int max_wait = 100;  /* 2 seconds */

        while (wait_ticks < max_wait) {
            Delay(10);
            wait_ticks += 10;

            windows_after = count_windows();
            if (windows_after > initial_windows) {
                break;
            }
        }
    }

    windows_after = count_windows();
    print_num("  Window count after launch: ", windows_after, "\n");

    if (windows_after > initial_windows) {
        print("OK: KP2 window opened\n");
    } else {
        print("FAIL: No KP2 window opened\n");
        errors++;
    }

cleanup:
    CloseLibrary((struct Library *)IntuitionBase);

    print("\n=== Test Results ===\n");
    if (errors == 0) {
        print("PASS: KickPascal 2 launch test passed\n");
        return 0;
    } else {
        print_num("FAIL: ", errors, " errors occurred\n");
        return 20;
    }
}
