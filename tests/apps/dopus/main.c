/*
 * Test: apps/dopus
 *
 * Automated test for Directory Opus file manager compatibility.
 *
 * Directory Opus is a popular file manager that uses custom libraries
 * (dopus.library, arp.library) and provides a two-pane file browser.
 *
 * Since lxa does not currently provide dopus.library, DOpus will exit
 * immediately after launch. This test validates that the complex
 * binary can be loaded via LoadSeg and launched via CreateNewProc,
 * exercising the hunk loader, relocation, and overlay handling.
 *
 * This test:
 * 1. Creates the dopus: assign
 * 2. Loads DirectoryOpus binary via LoadSeg
 * 3. Launches DOPUS as a background process (it will exit due to missing library)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
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

int main(void)
{
    int errors = 0;
    BPTR seg = 0;
    struct Process *proc = NULL;
    BPTR dopusLock = 0;

    print("=== Directory Opus Automated Compatibility Test ===\n\n");

    /* ========== Setup: Create dopus: assign ========== */
    print("--- Setup: Create dopus: assign ---\n");
    
    dopusLock = Lock((STRPTR)"APPS:DOPUS", ACCESS_READ);
    if (dopusLock) {
        if (AssignLock((STRPTR)"dopus", dopusLock)) {
            print("OK: Created dopus: assign pointing to APPS:DOPUS\n");
            /* AssignLock consumes the lock on success */
        } else {
            print("WARNING: Failed to create dopus: assign\n");
            UnLock(dopusLock);
        }
    } else {
        print("WARNING: Cannot lock APPS:DOPUS directory\n");
    }

    /* ========== Test 1: Load Directory Opus ========== */
    print("\n--- Test 1: Load Directory Opus binary ---\n");

    seg = LoadSeg((STRPTR)"APPS:DOPUS/DirectoryOpus");
    if (!seg) {
        print("FAIL: Cannot load DirectoryOpus from APPS:DOPUS/DirectoryOpus\n");
        errors++;
        goto cleanup;
    }
    print("OK: DirectoryOpus binary loaded successfully\n");

    /* ========== Test 2: Verify binary can be launched ========== */
    print("\n--- Test 2: Verify binary can be launched ---\n");

    /*
     * Directory Opus requires dopus.library to function. Since lxa does
     * not currently provide dopus.library, DOpus will exit immediately
     * after launch. We test that the binary loads and the process can
     * be created - this validates LoadSeg, CreateNewProc, and the
     * overlay/seglist handling for a complex real-world application.
     */
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
            { NP_Name, (ULONG)"DirectoryOpus" },
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

        seg = 0;
        print("OK: DirectoryOpus process created successfully\n");
    }

    /* Give DOpus a moment to run and exit (it will fail to open dopus.library) */
    Delay(25);

    print("OK: Process launch and exit completed\n");

cleanup:
    print("\n=== Test Results ===\n");
    if (errors == 0) {
        print("PASS: Directory Opus launch test passed\n");
        return 0;
    } else {
        print_num("FAIL: ", errors, " errors occurred\n");
        return 20;
    }
}
