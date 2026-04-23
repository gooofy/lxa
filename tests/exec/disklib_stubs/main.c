/*
 * Test: exec/disklib_stubs (renamed: third-party library loading)
 *
 * Phase 142: Third-party libraries use real binaries from share/lxa/System/Libs/.
 * Stubs have been removed; this test validates that the real libraries can be
 * loaded from disk and have the expected version numbers.
 *
 * Libraries tested:
 *   - req.library        v2  (Colin Fox / Bruce Dawson)
 *   - reqtools.library   v38 (Nico Francois)
 *   - powerpacker.library v37 (Nico Francois)
 *   - arp.library        v40 (AmigaDOS Replacement Project)
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <dos/dos.h>
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

static void print_hex(ULONG v)
{
    char buf[12];
    char *p = buf + sizeof(buf) - 1;
    static const char hex[] = "0123456789abcdef";

    *p = '\0';
    if (v == 0)
    {
        *--p = '0';
    }
    else
    {
        while (v > 0)
        {
            *--p = hex[v & 0xf];
            v >>= 4;
        }
    }
    *--p = 'x';
    *--p = '0';
    print(p);
}

/* ========================================================================
 * Test: Libraries are NOT resident in ROM (they are real disk libraries)
 * ======================================================================== */

static int test_not_in_rom(void)
{
    static const char *lib_names[] = {
        "req.library",
        "reqtools.library",
        "powerpacker.library",
        "arp.library"
    };
    int errors = 0;
    ULONG i;

    print("--- Test: Libraries not resident in ROM ---\n");

    for (i = 0; i < 4; i++)
    {
        struct Resident *resident = FindResident((CONST_STRPTR)lib_names[i]);

        print("Checking ");
        print(lib_names[i]);
        print("... ");

        if (resident != NULL)
        {
            print("FAIL: found as ROM resident (should be disk library)\n");
            errors++;
        }
        else
        {
            print("OK (not ROM resident)\n");
        }
    }

    print("\n");
    return errors;
}

/* ========================================================================
 * Test: Real libraries open and have correct versions
 * ======================================================================== */

typedef struct {
    const char *name;
    UWORD       expected_version;
} LibSpec;

static int test_open_and_version(void)
{
    int errors = 0;
    ULONG i;

    static const LibSpec libs[] = {
        { "req.library",          2  },
        { "reqtools.library",     38 },
        { "powerpacker.library",  37 },
        { "arp.library",          40 },
    };
    const ULONG count = sizeof(libs) / sizeof(libs[0]);

    print("--- Test: Open real libraries and check version ---\n");

    for (i = 0; i < count; i++)
    {
        struct Library *base = OpenLibrary((CONST_STRPTR)libs[i].name, 0);

        print(libs[i].name);
        print(": ");

        if (base == NULL)
        {
            print("FAIL: OpenLibrary() returned NULL\n");
            errors++;
            continue;
        }

        print("opened at ");
        print_hex((ULONG)base);
        print(", version=");
        print_num(base->lib_Version);

        if (base->lib_Version != libs[i].expected_version)
        {
            print(" FAIL: expected ");
            print_num(libs[i].expected_version);
            print("\n");
            errors++;
        }
        else
        {
            print(" OK\n");
        }

        CloseLibrary(base);
    }

    print("\n");
    return errors;
}

/* ========================================================================
 * Test: Reference counting with one of the real libraries
 * ======================================================================== */

static int test_refcounting(void)
{
    int errors = 0;
    struct Library *base;
    struct Library *base2;
    UWORD cnt_after_open;

    print("--- Test: Reference counting (req.library) ---\n");

    base = OpenLibrary((CONST_STRPTR)"req.library", 0);
    if (base == NULL)
    {
        print("FAIL: OpenLibrary(\"req.library\") returned NULL\n");
        return 1;
    }
    cnt_after_open = base->lib_OpenCnt;

    base2 = OpenLibrary((CONST_STRPTR)"req.library", 0);
    if (base2 == NULL)
    {
        print("FAIL: Second OpenLibrary() returned NULL\n");
        CloseLibrary(base);
        return 1;
    }

    if (base2 != base)
    {
        print("FAIL: Second OpenLibrary() returned different base pointer\n");
        errors++;
    }
    else
    {
        print("OK: Second OpenLibrary() returned same base pointer\n");
    }

    if (base->lib_OpenCnt != cnt_after_open + 1)
    {
        print("FAIL: lib_OpenCnt after second open is ");
        print_num(base->lib_OpenCnt);
        print(" expected ");
        print_num(cnt_after_open + 1);
        print("\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt incremented correctly\n");
    }

    CloseLibrary(base2);

    if (base->lib_OpenCnt != cnt_after_open)
    {
        print("FAIL: lib_OpenCnt after CloseLibrary is ");
        print_num(base->lib_OpenCnt);
        print(" expected ");
        print_num(cnt_after_open);
        print("\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt decremented correctly\n");
    }

    CloseLibrary(base);
    print("OK: Reference counting test complete\n\n");
    return errors;
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void)
{
    int errors = 0;

    print("=== Third-party library loading test ===\n\n");

    /* Test 1: Not ROM resident */
    errors += test_not_in_rom();

    /* Test 2: Open + version check */
    errors += test_open_and_version();

    /* Test 3: Reference counting */
    errors += test_refcounting();

    print("=== Test Results ===\n");
    if (errors == 0)
    {
        print("PASS: All tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: ");
        print_num(errors);
        print(" errors\n");
        return 5;
    }
}
