/*
 * Integration Test: MakeLink(LINK_SOFT) and ReadLink()
 *
 * lxa-only (tests/ref_suite.yaml): soft links are a feature of the Fast File
 * System.  The AmigaOS 3.1 reference machine has no FFS volume: its RAM: is
 * the ram-handler (MakeLink(LINK_SOFT) fails with ERROR_NOT_IMPLEMENTED) and
 * SYS: is the emulator's directory filesystem (no soft links either).
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static int tests_failed = 0;

static void print(const char *s)
{
    Write(Output(), (CONST APTR)s, strlen(s));
}

static void test_pass(const char *name)
{
    print("  PASS: ");
    print(name);
    print("\n");
}

static void test_fail(const char *name, const char *reason)
{
    print("  FAIL: ");
    print(name);
    print(" - ");
    print(reason);
    print("\n");
    tests_failed++;
}

int main(void)
{
    const char *target = "SYS:Tests/Dos/softlink_target.txt";
    const char *soft_link = "SYS:Tests/Dos/soft_link_test.lnk";
    char link_target[128];
    char data[16];
    struct DevProc *dp;
    BPTR fh;
    LONG result;

    print("Soft Link Test\n");
    print("==============\n\n");

    fh = Open((CONST_STRPTR)target, MODE_NEWFILE);
    if (!fh) {
        test_fail("Create target", "Open failed");
        return 20;
    }
    Write(fh, (CONST APTR)"SoftLink", 8);
    Close(fh);
    DeleteFile((CONST_STRPTR)soft_link);

    print("Test 1: MakeLink creates a soft link\n");
    if (MakeLink((CONST_STRPTR)soft_link, (LONG)target, LINK_SOFT))
        test_pass("MakeLink creates soft link");
    else
        test_fail("MakeLink creates soft link", "MakeLink failed");

    print("\nTest 2: ReadLink returns the link target\n");
    dp = GetDeviceProc((CONST_STRPTR)soft_link, NULL);
    if (!dp) {
        test_fail("ReadLink", "GetDeviceProc failed");
    } else {
        link_target[0] = '\0';
        result = ReadLink(dp->dvp_Port, dp->dvp_Lock, (CONST_STRPTR)soft_link,
                          (STRPTR)link_target, sizeof(link_target));
        if (result && strcmp(link_target, target) == 0)
            test_pass("ReadLink returns soft-link target");
        else
            test_fail("ReadLink returns soft-link target", "Unexpected target");

        result = ReadLink(dp->dvp_Port, dp->dvp_Lock, (CONST_STRPTR)soft_link,
                          (STRPTR)link_target, 5);
        if (!result || result == -2)
            test_pass("ReadLink with a short buffer fails");
        else
            test_fail("ReadLink with a short buffer fails", "Unexpected success");
        FreeDeviceProc(dp);
    }

    print("\nTest 3: Opening the soft link reads the target\n");
    fh = Open((CONST_STRPTR)soft_link, MODE_OLDFILE);
    if (!fh) {
        test_fail("Open soft link", "Open failed");
    } else {
        result = Read(fh, data, sizeof(data));
        Close(fh);
        if (result == 8 && strncmp(data, "SoftLink", 8) == 0)
            test_pass("Soft link resolves to target data");
        else
            test_fail("Soft link resolves to target data", "Unexpected data");
    }

    DeleteFile((CONST_STRPTR)soft_link);
    DeleteFile((CONST_STRPTR)target);

    print("\nFailed: ");
    print(tests_failed ? "some\n" : "0\n");
    return tests_failed ? 20 : 0;
}
