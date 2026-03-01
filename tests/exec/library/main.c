/*
 * Test: exec/library
 *
 * Phase 73: Core Library Resource Management & Stability
 *
 * Tests:
 * 1. OpenLibrary() returns non-NULL for built-in libraries
 * 2. lib_OpenCnt increments on OpenLibrary()
 * 3. lib_OpenCnt decrements on CloseLibrary()
 * 4. Multiple OpenLibrary() calls increment correctly
 * 5. OpenLibrary() for non-existent library returns NULL
 * 6. Version check on OpenLibrary()
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
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
    char *p = buf + sizeof(buf) - 1;
    BOOL neg = FALSE;

    *p = '\0';
    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }
    if (n == 0)
    {
        *--p = '0';
    }
    else
    {
        while (n > 0)
        {
            *--p = '0' + (n % 10);
            n /= 10;
        }
    }
    if (neg)
        *--p = '-';
    print(p);
}

/*
 * Test a single library's reference counting.
 * Returns number of errors.
 */
static int test_library_refcount(const char *name)
{
    int errors = 0;
    struct Library *lib1, *lib2;
    UWORD cnt_before, cnt_after_open1, cnt_after_open2, cnt_after_close1, cnt_after_close2;

    print("--- Testing ");
    print(name);
    print(" ---\n");

    /* Find the library node to read lib_OpenCnt directly */
    /* First, open it to get the base pointer */
    lib1 = OpenLibrary((CONST_STRPTR)name, 0);
    if (lib1 == NULL)
    {
        print("FAIL: OpenLibrary(\"");
        print(name);
        print("\", 0) returned NULL\n");
        return 1;
    }
    print("OK: OpenLibrary() returned non-NULL\n");

    /* Read count after first open */
    cnt_after_open1 = lib1->lib_OpenCnt;
    print("  lib_OpenCnt after 1st open: ");
    print_num(cnt_after_open1);
    print("\n");

    if (cnt_after_open1 < 1)
    {
        print("FAIL: lib_OpenCnt should be >= 1 after OpenLibrary()\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt >= 1 after OpenLibrary()\n");
    }

    /* Open again - count should increment */
    lib2 = OpenLibrary((CONST_STRPTR)name, 0);
    if (lib2 == NULL)
    {
        print("FAIL: 2nd OpenLibrary() returned NULL\n");
        errors++;
        CloseLibrary(lib1);
        return errors;
    }

    cnt_after_open2 = lib2->lib_OpenCnt;
    print("  lib_OpenCnt after 2nd open: ");
    print_num(cnt_after_open2);
    print("\n");

    if (cnt_after_open2 != cnt_after_open1 + 1)
    {
        print("FAIL: lib_OpenCnt should have incremented by 1\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt incremented correctly on 2nd open\n");
    }

    /* Verify same base returned */
    if (lib1 != lib2)
    {
        print("FAIL: 2nd OpenLibrary() returned different base pointer\n");
        errors++;
    }
    else
    {
        print("OK: Same library base returned\n");
    }

    /* Close once - count should decrement */
    CloseLibrary(lib2);
    cnt_after_close1 = lib1->lib_OpenCnt;
    print("  lib_OpenCnt after 1st close: ");
    print_num(cnt_after_close1);
    print("\n");

    if (cnt_after_close1 != cnt_after_open2 - 1)
    {
        print("FAIL: lib_OpenCnt should have decremented by 1\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt decremented correctly on close\n");
    }

    /* Close again */
    CloseLibrary(lib1);
    cnt_after_close2 = lib1->lib_OpenCnt;
    print("  lib_OpenCnt after 2nd close: ");
    print_num(cnt_after_close2);
    print("\n");

    if (cnt_after_close2 != cnt_after_close1 - 1)
    {
        print("FAIL: lib_OpenCnt should have decremented by 1 again\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt decremented correctly on 2nd close\n");
    }

    print("\n");
    return errors;
}

int main(void)
{
    int errors = 0;

    print("=== exec/library Test ===\n\n");

    /* Test 1: Test reference counting for all 6 previously-broken libraries */
    errors += test_library_refcount("graphics.library");
    errors += test_library_refcount("intuition.library");
    errors += test_library_refcount("utility.library");
    errors += test_library_refcount("mathtrans.library");
    errors += test_library_refcount("mathffp.library");
    errors += test_library_refcount("expansion.library");

    /* Test 2: Also test libraries that already work correctly */
    errors += test_library_refcount("reqtools.library");

    /* Test 3: OpenLibrary() for non-existent library */
    print("--- Test: Non-existent library ---\n");
    {
        struct Library *fake = OpenLibrary((CONST_STRPTR)"nonexistent.library", 0);
        if (fake != NULL)
        {
            print("FAIL: OpenLibrary(\"nonexistent.library\") should return NULL\n");
            CloseLibrary(fake);
            errors++;
        }
        else
        {
            print("OK: OpenLibrary(\"nonexistent.library\") correctly returned NULL\n");
        }
    }

    /* Test 4: Version check - request version higher than available */
    print("\n--- Test: Version check ---\n");
    {
        struct Library *lib = OpenLibrary((CONST_STRPTR)"graphics.library", 99);
        if (lib != NULL)
        {
            print("FAIL: OpenLibrary(\"graphics.library\", 99) should return NULL (version too high)\n");
            CloseLibrary(lib);
            errors++;
        }
        else
        {
            print("OK: OpenLibrary() correctly returned NULL for too-high version\n");
        }
    }

    /* ========== Final result ========== */
    print("\n=== Test Results ===\n");
    if (errors == 0)
    {
        print("PASS: All tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: ");
        print_num(errors);
        print(" error(s)\n");
        return 20;
    }
}
