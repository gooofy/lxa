/*
 * Integration Test: CreateDir permission behavior
 *
 * Tests:
 *   - Default permissions on created directories
 *   - Amiga protection bits mapping
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

static int tests_passed = 0;
static int tests_failed = 0;

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

int main(void)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    
    print("CreateDir Permission Test\n");
    print("=========================\n\n");
    
    /* Test 1: Create directory and check permissions */
    print("Test 1: Default directory permissions\n");
    
    /* Clean up first */
    DeleteFile((CONST_STRPTR)"T:perm_test_dir");
    
    lock = CreateDir((CONST_STRPTR)"T:perm_test_dir");
    if (lock) {
        UnLock(lock);
        
        /* Now examine the directory */
        lock = Lock((CONST_STRPTR)"T:perm_test_dir", SHARED_LOCK);
        if (lock) {
            fib = AllocDosObject(DOS_FIB, NULL);
            if (fib) {
                if (Examine(lock, fib)) {
                    print("  Protection bits: ");
                    print_num(fib->fib_Protection);
                    print("\n");
                    
                    /* On Amiga, default is usually 0 (all permissions enabled)
                     * Protection bits are inverted: 0 = enabled, 1 = disabled
                     * HSPARWED bits
                     */
                    if (fib->fib_DirEntryType > 0) {
                        test_pass("Created directory has protection bits");
                    } else {
                        test_fail("Permission check", "not a directory");
                    }
                } else {
                    test_fail("Permission check", "Examine failed");
                }
                FreeDosObject(DOS_FIB, fib);
            } else {
                test_fail("Permission check", "could not allocate FIB");
            }
            UnLock(lock);
        } else {
            test_fail("Permission check", "could not lock created dir");
        }
        
        /* Cleanup */
        DeleteFile((CONST_STRPTR)"T:perm_test_dir");
    } else {
        test_fail("CreateDir", "failed to create directory");
    }
    
    /* Test 2: Create file and check permissions */
    print("\nTest 2: Default file permissions\n");
    
    /* Clean up first */
    DeleteFile((CONST_STRPTR)"T:perm_test_file.txt");
    
    BPTR fh = Open((CONST_STRPTR)"T:perm_test_file.txt", MODE_NEWFILE);
    if (fh) {
        Write(fh, (CONST APTR)"test", 4);
        Close(fh);
        
        lock = Lock((CONST_STRPTR)"T:perm_test_file.txt", SHARED_LOCK);
        if (lock) {
            fib = AllocDosObject(DOS_FIB, NULL);
            if (fib) {
                if (Examine(lock, fib)) {
                    print("  Protection bits: ");
                    print_num(fib->fib_Protection);
                    print("\n");
                    
                    if (fib->fib_DirEntryType < 0) {
                        test_pass("Created file has protection bits");
                    } else {
                        test_fail("File check", "not a file");
                    }
                } else {
                    test_fail("File check", "Examine failed");
                }
                FreeDosObject(DOS_FIB, fib);
            }
            UnLock(lock);
        }
        
        /* Cleanup */
        DeleteFile((CONST_STRPTR)"T:perm_test_file.txt");
    } else {
        test_fail("Create file", "failed to create file");
    }
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
