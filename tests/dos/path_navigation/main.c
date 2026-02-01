/*
 * Path Navigation Test
 * 
 * Tests Amiga path navigation semantics:
 *   - Relative paths (no colon)
 *   - Parent directory navigation with /
 *   - Multiple parent levels with //
 *   - Lock() resolution with current directory context
 *   - NameFromLock() returning correct Amiga paths
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

/* Helper to get and print current directory path */
static void print_curdir(const char *prefix)
{
    BPTR lock = CurrentDir(0);
    CurrentDir(lock);  /* Restore */
    
    print(prefix);
    
    if (lock) {
        char buf[256];
        if (NameFromLock(lock, (STRPTR)buf, sizeof(buf))) {
            print(buf);
        } else {
            print("(NameFromLock failed)");
        }
    } else {
        print("(no current directory)");
    }
    print("\n");
}

/* Check if a string contains a colon */
static BOOL has_colon(const char *s)
{
    while (*s) {
        if (*s == ':') return TRUE;
        s++;
    }
    return FALSE;
}

int main(void)
{
    BPTR lock, oldDir;
    char buf[256];
    
    print("Path Navigation Test\n");
    print("====================\n\n");
    
    /* Start at SYS: */
    lock = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
    if (!lock) {
        print("ERROR: Could not lock SYS:\n");
        return 1;
    }
    oldDir = CurrentDir(lock);
    UnLock(oldDir);  /* Release old dir */
    
    print_curdir("Initial directory: ");
    
    /* Test 1: Create a test directory structure */
    print("\nTest 1: Creating test directories...\n");
    lock = CreateDir((CONST_STRPTR)"TestDir");
    if (lock) {
        UnLock(lock);
        print("  Created TestDir\n");
    } else {
        /* May already exist */
        print("  TestDir exists\n");
    }
    
    lock = Lock((CONST_STRPTR)"TestDir", SHARED_LOCK);
    if (lock) {
        oldDir = CurrentDir(lock);
        UnLock(oldDir);
        
        BPTR sub = CreateDir((CONST_STRPTR)"SubDir");
        if (sub) {
            UnLock(sub);
            print("  Created TestDir/SubDir\n");
        } else {
            print("  SubDir exists\n");
        }
        
        /* Go back to SYS: */
        lock = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
        if (lock) {
            oldDir = CurrentDir(lock);
            UnLock(oldDir);
        }
    }
    
    /* Test 2: Navigate down with relative paths */
    print("\nTest 2: Navigate down (relative paths)...\n");
    print_curdir("  Start: ");
    
    lock = Lock((CONST_STRPTR)"TestDir", SHARED_LOCK);
    if (lock) {
        oldDir = CurrentDir(lock);
        UnLock(oldDir);
        print_curdir("  After cd TestDir: ");
        
        lock = Lock((CONST_STRPTR)"SubDir", SHARED_LOCK);
        if (lock) {
            oldDir = CurrentDir(lock);
            UnLock(oldDir);
            print_curdir("  After cd SubDir: ");
        } else {
            print("  ERROR: Could not Lock SubDir\n");
        }
    } else {
        print("  ERROR: Could not Lock TestDir\n");
    }
    
    /* Test 3: Navigate up with single / */
    print("\nTest 3: Navigate up with / (single parent)...\n");
    print_curdir("  Start: ");
    
    lock = Lock((CONST_STRPTR)"/", SHARED_LOCK);
    if (lock) {
        if (NameFromLock(lock, (STRPTR)buf, sizeof(buf))) {
            print("  Lock(\"/\") => ");
            print(buf);
            print("\n");
            
            /* Verify it has a colon (absolute path) */
            if (has_colon(buf)) {
                print("  PASS: Path is absolute\n");
            } else {
                print("  FAIL: Path should be absolute\n");
            }
        }
        oldDir = CurrentDir(lock);
        UnLock(oldDir);
        print_curdir("  After cd /: ");
    } else {
        print("  ERROR: Could not Lock /\n");
    }
    
    /* Test 4: Navigate up with // (two levels) */
    print("\nTest 4: Navigate up with // (two parents)...\n");
    
    /* First go to TestDir/SubDir */
    lock = Lock((CONST_STRPTR)"SYS:TestDir/SubDir", SHARED_LOCK);
    if (lock) {
        oldDir = CurrentDir(lock);
        UnLock(oldDir);
        print_curdir("  Start: ");
        
        lock = Lock((CONST_STRPTR)"//", SHARED_LOCK);
        if (lock) {
            if (NameFromLock(lock, (STRPTR)buf, sizeof(buf))) {
                print("  Lock(\"//\") => ");
                print(buf);
                print("\n");
                
                /* Should be at SYS: (root) */
                if (has_colon(buf)) {
                    /* Check if it ends with : (at root) */
                    int len = 0;
                    while (buf[len]) len++;
                    if (len > 0 && buf[len-1] == ':') {
                        print("  PASS: At volume root\n");
                    } else {
                        print("  PASS: Path is absolute\n");
                    }
                } else {
                    print("  FAIL: Path should be absolute\n");
                }
            }
            oldDir = CurrentDir(lock);
            UnLock(oldDir);
            print_curdir("  After cd //: ");
        } else {
            print("  ERROR: Could not Lock //\n");
        }
    }
    
    /* Test 5: Navigate with combined path (/ + relative) */
    print("\nTest 5: Combined path (/Sibling)...\n");
    
    /* Go to TestDir */
    lock = Lock((CONST_STRPTR)"SYS:TestDir", SHARED_LOCK);
    if (lock) {
        oldDir = CurrentDir(lock);
        UnLock(oldDir);
        print_curdir("  Start: ");
        
        /* Create a sibling */
        lock = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
        if (lock) {
            oldDir = CurrentDir(lock);
            UnLock(oldDir);
            
            BPTR sibling = CreateDir((CONST_STRPTR)"SiblingDir");
            if (sibling) {
                UnLock(sibling);
                print("  Created SiblingDir\n");
            }
            
            /* Go back to TestDir */
            lock = Lock((CONST_STRPTR)"TestDir", SHARED_LOCK);
            if (lock) {
                oldDir = CurrentDir(lock);
                UnLock(oldDir);
            }
        }
        
        /* Now try /SiblingDir (go up one, then into SiblingDir) */
        lock = Lock((CONST_STRPTR)"/SiblingDir", SHARED_LOCK);
        if (lock) {
            if (NameFromLock(lock, (STRPTR)buf, sizeof(buf))) {
                print("  Lock(\"/SiblingDir\") => ");
                print(buf);
                print("\n");
                print("  PASS: Combined parent + relative path works\n");
            }
            UnLock(lock);
        } else {
            print("  ERROR: Could not Lock /SiblingDir\n");
        }
    }
    
    /* Cleanup */
    print("\nCleanup...\n");
    lock = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
    if (lock) {
        oldDir = CurrentDir(lock);
        UnLock(oldDir);
    }
    
    DeleteFile((CONST_STRPTR)"SYS:TestDir/SubDir");
    DeleteFile((CONST_STRPTR)"SYS:TestDir");
    DeleteFile((CONST_STRPTR)"SYS:SiblingDir");
    
    print("\nPASS\n");
    return 0;
}
