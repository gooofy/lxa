/*
 * Integration Test: Case-insensitive path resolution
 *
 * Amiga paths are case-insensitive. This test verifies that
 * files and directories can be accessed regardless of case.
 *
 * Tests:
 *   - Create file, access with different case
 *   - Create directory, access with different case
 *   - Lock file with mixed case
 *   - Open file with wrong case for reading
 *   - Nested path with mixed case components
 */

#include <exec/types.h>
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

static void print_num(LONG n)
{
    char buf[32];
    char *p = buf;

    if (n < 0) {
        *p++ = '-';
        n = -n;
    }

    char tmp[16];
    int i = 0;
    do {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    while (i > 0) *p++ = tmp[--i];
    *p = '\0';

    print(buf);
}

static int tests_passed = 0;
static int tests_failed = 0;

static void test_pass(const char *name)
{
    print("  PASS: ");
    print(name);
    print("\n");
    tests_passed++;
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

/* Create a test file with some content */
static BOOL create_test_file(const char *name, const char *content)
{
    BPTR fh = Open((CONST_STRPTR)name, MODE_NEWFILE);
    if (!fh) return FALSE;

    LONG len = 0;
    const char *p = content;
    while (*p++) len++;

    Write(fh, (CONST APTR)content, len);
    Close(fh);
    return TRUE;
}

int main(void)
{
    BPTR fh, lock;
    char buf[64];
    LONG n;

    print("Case-Insensitive Path Test\n");
    print("==========================\n\n");

    /* Test 1: Create file with lowercase, open with uppercase */
    print("Test 1: Open file with different case\n");

    if (!create_test_file("T:casefile.txt", "Hello Case")) {
        print("ERROR: Could not create test file\n");
        return 1;
    }

    fh = Open((CONST_STRPTR)"T:CASEFILE.TXT", MODE_OLDFILE);
    if (fh) {
        n = Read(fh, (APTR)buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            /* Check content */
            if (buf[0] == 'H' && buf[1] == 'e') {
                test_pass("Open uppercase -> read correct data");
            } else {
                test_fail("Open uppercase", "Wrong content");
            }
        } else {
            test_fail("Open uppercase", "Read returned 0");
        }
        Close(fh);
    } else {
        test_fail("Open uppercase", "Open with uppercase failed");
    }

    /* Test 2: Open with mixed case */
    print("\nTest 2: Open file with mixed case\n");
    fh = Open((CONST_STRPTR)"T:CaseFile.Txt", MODE_OLDFILE);
    if (fh) {
        test_pass("Open mixed case succeeded");
        Close(fh);
    } else {
        test_fail("Open mixed case", "Open with mixed case failed");
    }

    /* Test 3: Lock with different case */
    print("\nTest 3: Lock file with different case\n");
    lock = Lock((CONST_STRPTR)"T:CASEFILE.TXT", SHARED_LOCK);
    if (lock) {
        test_pass("Lock uppercase succeeded");
        UnLock(lock);
    } else {
        test_fail("Lock uppercase", "Lock with uppercase failed");
    }

    /* Test 4: Create directory, access with different case */
    print("\nTest 4: Directory with different case\n");
    lock = CreateDir((CONST_STRPTR)"T:CaseDir");
    if (lock) {
        UnLock(lock);

        /* Try locking with uppercase */
        lock = Lock((CONST_STRPTR)"T:CASEDIR", SHARED_LOCK);
        if (lock) {
            test_pass("Lock dir uppercase succeeded");
            UnLock(lock);
        } else {
            test_fail("Lock dir uppercase", "Lock with uppercase failed");
        }

        /* Try locking with lowercase */
        lock = Lock((CONST_STRPTR)"T:casedir", SHARED_LOCK);
        if (lock) {
            test_pass("Lock dir lowercase succeeded");
            UnLock(lock);
        } else {
            test_fail("Lock dir lowercase", "Lock with lowercase failed");
        }
    } else {
        test_fail("CreateDir", "Could not create CaseDir");
    }

    /* Test 5: Nested path with mixed case */
    print("\nTest 5: Nested path with mixed case\n");

    /* Create nested structure: T:CaseDir/SubFile.txt */
    if (create_test_file("T:CaseDir/SubFile.txt", "Nested")) {
        /* Access with different case for each component */
        fh = Open((CONST_STRPTR)"T:CASEDIR/SUBFILE.TXT", MODE_OLDFILE);
        if (fh) {
            test_pass("Nested uppercase open succeeded");
            Close(fh);
        } else {
            test_fail("Nested uppercase", "Open with uppercase failed");
        }

        fh = Open((CONST_STRPTR)"T:casedir/subfile.txt", MODE_OLDFILE);
        if (fh) {
            test_pass("Nested lowercase open succeeded");
            Close(fh);
        } else {
            test_fail("Nested lowercase", "Open with lowercase failed");
        }
    } else {
        test_fail("Nested path", "Could not create nested test file");
    }

    /* Cleanup */
    print("\nCleanup...\n");
    DeleteFile((CONST_STRPTR)"T:CaseDir/SubFile.txt");
    DeleteFile((CONST_STRPTR)"T:CaseDir");
    DeleteFile((CONST_STRPTR)"T:casefile.txt");

    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed > 0 ? 10 : 0;
}
