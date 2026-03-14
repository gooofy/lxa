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
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#undef SetFunction

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static const char g_test_library_name[] = "phase78test.library";

static struct Library *test_init(register struct Library *lib __asm("d0"),
                                 register BPTR seglist __asm("a0"),
                                 register struct ExecBase *sys_base __asm("a6"))
{
    (void)seglist;
    (void)sys_base;

    lib->lib_Node.ln_Type = NT_LIBRARY;
    lib->lib_Node.ln_Name = (char *)g_test_library_name;
    lib->lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    lib->lib_Version = 5;
    lib->lib_Revision = 1;
    return lib;
}

static struct Library *test_open(register ULONG version __asm("d0"),
                                 register struct Library *lib __asm("a6"))
{
    (void)version;
    lib->lib_OpenCnt++;
    lib->lib_Flags &= ~LIBF_DELEXP;
    return lib;
}

static BPTR test_close(register struct Library *lib __asm("a6"))
{
    lib->lib_OpenCnt--;
    return 0;
}

static BPTR test_expunge(register struct Library *lib __asm("a6"))
{
    if (lib->lib_OpenCnt == 0)
    {
        Remove(&lib->lib_Node);
        return (BPTR)1;
    }

    lib->lib_Flags |= LIBF_DELEXP;
    return 0;
}

static ULONG test_ext(void)
{
    return 0;
}

static ULONG test_old_function(void)
{
    return 0x11111111;
}

static ULONG test_new_function(void)
{
    return 0x22222222;
}

static APTR g_test_library_vectors[] = {
    (APTR)test_open,
    (APTR)test_close,
    (APTR)test_expunge,
    (APTR)test_ext,
    (APTR)test_old_function,
    (APTR)-1
};

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
    UWORD cnt_after_open1, cnt_after_open2, cnt_after_close1, cnt_after_close2;

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

static int test_exec_library_helpers(void)
{
    int errors = 0;
    struct Library *lib;
    struct Library *opened;
    struct Node *found;
    APTR old_function;
    ULONG old_sum;
    UBYTE *lib_mem;
    ULONG lib_size;

    print("--- Test: Exec library helpers ---\n");

    lib = MakeLibrary(g_test_library_vectors, NULL, (ULONG (*)())test_init, sizeof(struct Library), 0);
    if (!lib)
    {
        print("FAIL: MakeLibrary returned NULL\n\n");
        return 1;
    }
    print("OK: MakeLibrary returned non-NULL\n");

    if (lib->lib_NegSize == 30)
    {
        print("OK: MakeLibrary computed 5 vectors of negative size\n");
    }
    else
    {
        print("FAIL: MakeLibrary negative size incorrect\n");
        errors++;
    }

    AddLibrary(lib);
    found = FindName(&SysBase->LibList, (CONST_STRPTR)g_test_library_name);
    if (found == &lib->lib_Node)
    {
        print("OK: AddLibrary inserted library into LibList\n");
    }
    else
    {
        print("FAIL: AddLibrary did not insert library into LibList\n");
        errors++;
    }

    opened = OpenLibrary((CONST_STRPTR)g_test_library_name, 5);
    if (opened == lib)
    {
        print("OK: OpenLibrary found custom library\n");
    }
    else
    {
        print("FAIL: OpenLibrary did not return custom library\n");
        errors++;
    }

    if (OpenLibrary((CONST_STRPTR)g_test_library_name, 6) == NULL)
    {
        print("OK: OpenLibrary rejected too-high version\n");
    }
    else
    {
        print("FAIL: OpenLibrary should reject too-high version\n");
        errors++;
    }

    old_sum = lib->lib_Sum;
    old_function = SetFunction(lib, -30, (ULONG (*)())test_new_function);
    if (old_function == (APTR)test_old_function)
    {
        print("OK: SetFunction returned previous function pointer\n");
    }
    else
    {
        print("FAIL: SetFunction returned wrong previous function pointer\n");
        errors++;
    }

    if (lib->lib_Sum != old_sum)
    {
        print("OK: SumLibrary updated checksum after SetFunction\n");
    }
    else
    {
        print("FAIL: SumLibrary did not update checksum after SetFunction\n");
        errors++;
    }

    CloseLibrary(opened);
    if (lib->lib_OpenCnt == 0)
    {
        print("OK: CloseLibrary decremented custom library open count\n");
    }
    else
    {
        print("FAIL: CloseLibrary did not decrement custom library open count\n");
        errors++;
    }

    RemLibrary(lib);
    found = FindName(&SysBase->LibList, (CONST_STRPTR)g_test_library_name);
    if (found == NULL)
    {
        print("OK: RemLibrary expunged custom library from LibList\n");
    }
    else
    {
        print("FAIL: RemLibrary did not expunge custom library\n");
        errors++;
    }

    lib_mem = (UBYTE *)lib - lib->lib_NegSize;
    lib_size = lib->lib_NegSize + lib->lib_PosSize;
    FreeMem(lib_mem, lib_size);

    CloseLibrary(NULL);
    print("OK: CloseLibrary(NULL) did not crash\n\n");
    return errors;
}

static int test_external_library_scope(void)
{
    static const char *external_libs[] = {
        "commodities.library",
        "rexxsyslib.library",
        "datatypes.library",
        "dopus.library",
        "powerpacker.library",
        "arp.library",
        "iff.library",
        "Explode.Library",
        "Icon.Library"
    };
    int errors = 0;
    ULONG i;

    print("--- Test: External library scope ---\n");

    for (i = 0; i < (sizeof(external_libs) / sizeof(external_libs[0])); i++)
    {
        const char *name = external_libs[i];
        struct Resident *resident = FindResident((CONST_STRPTR)name);
        struct Node *node = FindName(&SysBase->LibList, (CONST_STRPTR)name);

        print("Checking ");
        print(name);
        print("...\n");

        if (resident != NULL)
        {
            print("FAIL: library should not be resident in ROM\n");
            errors++;
        }
        else
        {
            print("OK: library is not resident in ROM\n");
        }

        if (node != NULL)
        {
            print("FAIL: library should not be pre-registered in LibList\n");
            errors++;
        }
        else
        {
            print("OK: library is not pre-registered in LibList\n");
        }
    }

    print("\n");
    return errors;
}

static int test_dos_library_init_state(void)
{
    int errors = 0;
    ULONG *task_array;

    print("--- Test: DOS InitLib state ---\n");

    if (DOSBase == NULL)
    {
        print("FAIL: DOSBase should be available\n\n");
        return 1;
    }

    if (DOSBase->dl_Root != NULL)
    {
        print("OK: dos.library initialized RootNode\n");
    }
    else
    {
        print("FAIL: dos.library RootNode is NULL\n");
        errors++;
    }

    task_array = DOSBase->dl_Root ? (ULONG *)BADDR(DOSBase->dl_Root->rn_TaskArray) : NULL;
    if (task_array != NULL)
    {
        print("OK: dos.library initialized RootNode task array\n");
        if (task_array[0] >= 8)
        {
            print("OK: dos.library task array has expected initial capacity\n");
        }
        else
        {
            print("FAIL: dos.library task array capacity is too small\n");
            errors++;
        }
    }
    else
    {
        print("FAIL: dos.library task array is NULL\n");
        errors++;
    }

    if (DOSBase->dl_Root && DOSBase->dl_Root->rn_Info != 0)
    {
        print("OK: dos.library initialized DosInfo\n");
    }
    else
    {
        print("FAIL: dos.library DosInfo is NULL\n");
        errors++;
    }

    if (DOSBase->dl_Errors == NULL &&
        DOSBase->dl_TimeReq == NULL &&
        DOSBase->dl_UtilityBase == NULL &&
        DOSBase->dl_IntuitionBase == NULL)
    {
        print("OK: dos.library private startup pointers stay cleared\n");
    }
    else
    {
        print("FAIL: dos.library private startup pointers should be cleared\n");
        errors++;
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

    /* Test 2: OpenLibrary() for non-existent library */
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

    /* Test 3: Version check - request version higher than available */
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

    /* Test 4: Verify MakeLibrary/SetFunction/SumLibrary/AddLibrary/RemLibrary */
    errors += test_exec_library_helpers();

    /* Test 5: Verify third-party libraries stay off the built-in ROM surface */
    errors += test_external_library_scope();

    /* Test 6: Verify dos.library InitLib initialized the public DOS base state */
    errors += test_dos_library_init_state();

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
