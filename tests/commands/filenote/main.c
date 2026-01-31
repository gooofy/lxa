/*
 * Integration Test: FILENOTE command / SetComment DOS function
 *
 * Tests:
 *   - SetComment DOS function to set file comments
 *   - Reading comments back via Examine
 *   - Error handling for non-existent files
 *
 * Note: File comments are stored as extended attributes on Linux.
 * This test verifies the SetComment emucall works correctly.
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

/* Get file comment via Examine */
static BOOL get_comment(const char *name, char *comment_buf, LONG buf_size)
{
    BPTR lock = Lock((CONST_STRPTR)name, SHARED_LOCK);
    if (!lock) return FALSE;
    
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return FALSE;
    }
    
    BOOL ok = FALSE;
    if (Examine(lock, fib)) {
        /* Copy comment from FIB */
        char *src = fib->fib_Comment;
        char *dst = comment_buf;
        LONG i;
        for (i = 0; i < buf_size - 1 && src[i]; i++) {
            dst[i] = src[i];
        }
        dst[i] = '\0';
        ok = TRUE;
    }
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return ok;
}

/* Simple string compare */
static int str_equal(const char *a, const char *b)
{
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

int main(void)
{
    BOOL success;
    char comment[80];
    
    print("FILENOTE DOS Function Test\n");
    print("==========================\n\n");
    
    /* Cleanup any leftover test files */
    DeleteFile((CONST_STRPTR)"test_note.txt");
    
    /* Create test file */
    if (!create_test_file("test_note.txt", "Test content")) {
        print("ERROR: Could not create test file\n");
        return 20;
    }
    
    /* Test 1: Read initial comment (should be empty) */
    print("Test 1: Read initial comment\n");
    if (get_comment("test_note.txt", comment, sizeof(comment))) {
        if (comment[0] == '\0') {
            test_pass("Initial comment is empty");
        } else {
            print("  Initial comment: '");
            print(comment);
            print("'\n");
            test_pass("Got initial comment");
        }
    } else {
        test_fail("Could not read comment");
    }
    
    /* Test 2: Set a simple comment */
    print("\nTest 2: Set simple comment\n");
    success = SetComment((CONST_STRPTR)"test_note.txt", (CONST_STRPTR)"This is a test comment");
    if (success) {
        if (get_comment("test_note.txt", comment, sizeof(comment))) {
            if (str_equal(comment, "This is a test comment")) {
                test_pass("Comment set correctly");
            } else {
                print("  Got: '");
                print(comment);
                print("'\n");
                test_fail("Comment mismatch");
            }
        } else {
            test_fail("Could not read back comment");
        }
    } else {
        test_fail("SetComment returned FALSE");
    }
    
    /* Test 3: Change the comment */
    print("\nTest 3: Change comment\n");
    success = SetComment((CONST_STRPTR)"test_note.txt", (CONST_STRPTR)"New comment");
    if (success) {
        if (get_comment("test_note.txt", comment, sizeof(comment))) {
            if (str_equal(comment, "New comment")) {
                test_pass("Comment changed correctly");
            } else {
                print("  Got: '");
                print(comment);
                print("'\n");
                test_fail("Comment mismatch");
            }
        } else {
            test_fail("Could not read back comment");
        }
    } else {
        test_fail("SetComment returned FALSE");
    }
    
    /* Test 4: Clear comment (empty string) */
    print("\nTest 4: Clear comment\n");
    success = SetComment((CONST_STRPTR)"test_note.txt", (CONST_STRPTR)"");
    if (success) {
        if (get_comment("test_note.txt", comment, sizeof(comment))) {
            if (comment[0] == '\0') {
                test_pass("Comment cleared");
            } else {
                print("  Got: '");
                print(comment);
                print("'\n");
                test_fail("Comment not cleared");
            }
        } else {
            test_fail("Could not read back comment");
        }
    } else {
        test_fail("SetComment returned FALSE");
    }
    
    /* Test 5: Error - non-existent file */
    print("\nTest 5: Non-existent file (should fail)\n");
    success = SetComment((CONST_STRPTR)"nonexistent_xyz.txt", (CONST_STRPTR)"comment");
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
    DeleteFile((CONST_STRPTR)"test_note.txt");
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
