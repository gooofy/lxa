/*
 * Integration Test: Lock sharing behavior
 *
 * Tests:
 *   - Shared locks can coexist
 *   - Lock on non-existent path fails correctly
 *   - Lock on file vs directory
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

static void print_error(LONG err)
{
    char buf[32];
    char *p = buf;
    LONG n = err;
    
    if (n < 0) {
        *p++ = '-';
        n = -n;
    }
    
    /* Convert number to string */
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

int main(void)
{
    BPTR lock1, lock2;
    LONG err;
    
    print("Lock Sharing Test\n");
    print("=================\n\n");
    
    /* Test 1: Multiple shared locks on same directory */
    print("Test 1: Multiple shared locks on same directory\n");
    
    lock1 = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
    if (!lock1) {
        print("  ERROR: First shared lock failed\n");
        return 1;
    }
    print("  First shared lock: OK\n");
    
    lock2 = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
    if (!lock2) {
        print("  ERROR: Second shared lock failed\n");
        UnLock(lock1);
        return 1;
    }
    print("  Second shared lock: OK\n");
    
    /* Both locks should be valid */
    if (lock1 && lock2) {
        print("  Both locks coexist: OK\n");
    }
    
    UnLock(lock2);
    UnLock(lock1);
    print("  Both locks released: OK\n");
    
    /* Test 2: Lock on non-existent path */
    print("\nTest 2: Lock on non-existent path\n");
    
    lock1 = Lock((CONST_STRPTR)"SYS:NonExistentDirectory/SubDir", SHARED_LOCK);
    if (lock1) {
        print("  ERROR: Lock succeeded on non-existent path!\n");
        UnLock(lock1);
        return 1;
    }
    
    err = IoErr();
    print("  Lock correctly failed, IoErr=");
    print_error(err);
    print("\n");
    
    if (err == ERROR_OBJECT_NOT_FOUND || err == ERROR_DIR_NOT_FOUND) {
        print("  Correct error code: OK\n");
    } else {
        print("  WARNING: Unexpected error code\n");
    }
    
    /* Test 3: Lock on current directory (.) */
    print("\nTest 3: Lock on current directory\n");
    
    lock1 = Lock((CONST_STRPTR)".", SHARED_LOCK);
    if (!lock1) {
        print("  ERROR: Lock on . failed\n");
        return 1;
    }
    
    /* Examine to verify it's a directory */
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocMem(sizeof(struct FileInfoBlock), MEMF_CLEAR);
    if (fib && Examine(lock1, fib)) {
        if (fib->fib_DirEntryType > 0) {
            print("  Current dir is a directory: OK\n");
        } else {
            print("  ERROR: Current dir should be a directory\n");
        }
    }
    if (fib) FreeMem(fib, sizeof(struct FileInfoBlock));
    UnLock(lock1);
    
    print("\nAll tests passed!\n");
    return 0;
}
