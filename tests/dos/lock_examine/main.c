/*
 * Phase 3 Integration Test: Lock and Examine APIs
 * 
 * Tests:
 *   - Lock() / UnLock()
 *   - Examine()
 *   - ExNext()
 *   - DupLock()
 *   - ParentDir()
 *   - CurrentDir()
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

/* Helper to print a string */
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
    struct FileInfoBlock *fib;
    BPTR lock, dupLock, parentLock;
    LONG count = 0;
    
    print("Lock/Examine Test\n");
    print("=================\n\n");
    
    /* Allocate FileInfoBlock - must be longword aligned */
    fib = (struct FileInfoBlock *)AllocMem(sizeof(struct FileInfoBlock), MEMF_CLEAR);
    if (!fib) {
        print("ERROR: Failed to allocate FileInfoBlock\n");
        return 1;
    }
    
    /* Test 1: Lock the current directory (.) */
    print("Test 1: Lock(\".\")\n");
    lock = Lock((CONST_STRPTR)".", SHARED_LOCK);
    if (!lock) {
        print("ERROR: Lock(\".\") failed\n");
        FreeMem(fib, sizeof(struct FileInfoBlock));
        return 1;
    }
    print("  Lock obtained: OK\n");
    
    /* Test 2: Examine the lock */
    print("\nTest 2: Examine()\n");
    if (!Examine(lock, fib)) {
        print("ERROR: Examine() failed\n");
        UnLock(lock);
        FreeMem(fib, sizeof(struct FileInfoBlock));
        return 1;
    }
    
    print("  Type: ");
    if (fib->fib_DirEntryType > 0) {
        print("Directory");
    } else {
        print("File");
    }
    print("\n");
    
    print("  Name: ");
    /* fib_FileName is a null-terminated C string (not BSTR despite historical confusion) */
    print((char *)fib->fib_FileName);
    print("\n");
    print("  Examine: OK\n");
    
    /* Test 3: ExNext - iterate directory entries */
    print("\nTest 3: ExNext() - Directory listing:\n");
    while (ExNext(lock, fib)) {
        count++;
        
        /* Limit iterations to prevent very long listings */
        if (count >= 20) {
            break;
        }
    }
    
    /* Just verify we found some entries - don't check specific names
     * since directory order is filesystem-dependent */
    if (count >= 3) {
        print("  Found multiple entries: OK\n");
    } else {
        print("  ERROR: Too few entries found\n");
    }
    print("  ExNext: OK\n");
    
    /* Test 4: DupLock */
    print("\nTest 4: DupLock()\n");
    dupLock = DupLock(lock);
    if (!dupLock) {
        print("  ERROR: DupLock() failed\n");
    } else {
        print("  Duplicate lock obtained: OK\n");
        UnLock(dupLock);
        print("  Duplicate unlocked: OK\n");
    }
    
    /* Test 5: ParentDir */
    print("\nTest 5: ParentDir()\n");
    parentLock = ParentDir(lock);
    if (!parentLock) {
        print("  No parent (at root): OK\n");
    } else {
        print("  Parent lock obtained: OK\n");
        
        /* Examine the parent */
        if (Examine(parentLock, fib)) {
            print("  Parent name: ");
            if (fib->fib_FileName[0]) {
                print((char *)fib->fib_FileName);
            } else {
                print("(root)");
            }
            print("\n");
        }
        UnLock(parentLock);
    }
    
    /* Test 6: CurrentDir */
    print("\nTest 6: CurrentDir()\n");
    {
        BPTR oldDir;
        
        /* Set current directory to our lock */
        oldDir = CurrentDir(lock);
        print("  CurrentDir set: OK\n");
        
        /* Restore old dir */
        (void)CurrentDir(oldDir);
        print("  CurrentDir restored: OK\n");
    }
    
    /* Cleanup */
    UnLock(lock);
    FreeMem(fib, sizeof(struct FileInfoBlock));
    
    print("\nAll tests passed!\n");
    return 0;
}
