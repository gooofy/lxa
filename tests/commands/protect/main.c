/*
 * Integration Test: PROTECT command / SetProtection DOS function
 *
 * Tests:
 *   - SetProtection DOS function
 *   - Protection bits reading via Examine
 *   - Basic R/W/E permissions (only these map to Linux filesystem)
 *   - Error handling for non-existent files
 *
 * Note: Extended Amiga protection bits (Archive, Script, Pure, etc.)
 * cannot be fully tested because Linux filesystems don't support them.
 * Only basic Unix permissions (read/write/execute) are mapped.
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
    char buf[16];
    char *p = buf + 15;
    *p = '\0';
    BOOL neg = n < 0;
    unsigned long num = neg ? -n : n;
    do {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    if (neg) *--p = '-';
    print(p);
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

static void test_fail(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    tests_failed++;
}

/* Create a test file */
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

/* Get file protection bits */
static LONG get_protection(const char *name)
{
    BPTR lock = Lock((CONST_STRPTR)name, SHARED_LOCK);
    if (!lock) return -1;
    
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return -1;
    }
    
    LONG prot = -1;
    if (Examine(lock, fib)) {
        prot = fib->fib_Protection;
    }
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return prot;
}

int main(void)
{
    LONG prot;
    BOOL success;
    
    print("PROTECT DOS Function Test\n");
    print("=========================\n\n");
    
    /* Cleanup any leftover test files */
    DeleteFile((CONST_STRPTR)"test_prot.txt");
    
    /* Create test file */
    if (!create_test_file("test_prot.txt", "Test content")) {
        print("ERROR: Could not create test file\n");
        return 20;
    }
    
    /* Test 1: Get initial protection */
    print("Test 1: Read initial protection\n");
    prot = get_protection("test_prot.txt");
    if (prot >= 0) {
        test_pass("Read protection bits");
    } else {
        test_fail("Read protection failed");
    }
    
    /* Test 2: Set protection to all permissions (clear RWED bits) */
    print("\nTest 2: Set all permissions (prot=0)\n");
    success = SetProtection((CONST_STRPTR)"test_prot.txt", 0);
    if (success) {
        prot = get_protection("test_prot.txt");
        /* With prot=0, all permissions granted, so Unix should have rwx */
        if ((prot & (FIBF_READ | FIBF_WRITE | FIBF_EXECUTE)) == 0) {
            test_pass("All RWE permissions granted");
        } else {
            test_fail("Protection not set correctly");
        }
    } else {
        test_fail("SetProtection returned FALSE");
    }
    
    /* Test 3: Remove write permission */
    print("\nTest 3: Remove write permission\n");
    success = SetProtection((CONST_STRPTR)"test_prot.txt", FIBF_WRITE);
    if (success) {
        prot = get_protection("test_prot.txt");
        /* FIBF_WRITE bit set means write denied */
        if ((prot & FIBF_WRITE) != 0) {
            test_pass("Write permission removed");
        } else {
            test_fail("Write bit not set");
        }
    } else {
        test_fail("SetProtection returned FALSE");
    }
    
    /* Test 4: Remove execute permission */
    print("\nTest 4: Remove execute permission\n");
    success = SetProtection((CONST_STRPTR)"test_prot.txt", FIBF_EXECUTE);
    if (success) {
        prot = get_protection("test_prot.txt");
        /* FIBF_EXECUTE bit set means execute denied */
        if ((prot & FIBF_EXECUTE) != 0) {
            test_pass("Execute permission removed");
        } else {
            test_fail("Execute bit not set");
        }
    } else {
        test_fail("SetProtection returned FALSE");
    }
    
    /* Test 5: Remove read permission */
    print("\nTest 5: Remove read permission\n");
    success = SetProtection((CONST_STRPTR)"test_prot.txt", FIBF_READ);
    if (success) {
        prot = get_protection("test_prot.txt");
        /* FIBF_READ bit set means read denied */
        if ((prot & FIBF_READ) != 0) {
            test_pass("Read permission removed");
        } else {
            test_fail("Read bit not set");
        }
    } else {
        test_fail("SetProtection returned FALSE");
    }
    
    /* Test 6: Error - non-existent file */
    print("\nTest 6: Non-existent file (should fail)\n");
    success = SetProtection((CONST_STRPTR)"nonexistent_xyz.txt", 0);
    if (!success) {
        LONG err = IoErr();
        if (err == ERROR_OBJECT_NOT_FOUND || err == 205) {
            test_pass("Non-existent file correctly rejected");
        } else {
            print("  Unexpected IoErr: ");
            print_num(err);
            print("\n");
            test_fail("Wrong error code");
        }
    } else {
        test_fail("Should have failed");
    }
    
    /* Cleanup */
    print("\nCleanup: Removing test files\n");
    /* First restore write permission so we can delete */
    SetProtection((CONST_STRPTR)"test_prot.txt", 0);
    DeleteFile((CONST_STRPTR)"test_prot.txt");
    print("  Done\n");
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
