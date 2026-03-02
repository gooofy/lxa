/*
 * Integration Test: UnLoadSeg
 *
 * Tests:
 *   - LoadSeg followed by UnLoadSeg (no crash, no leak)
 *   - UnLoadSeg(NULL) is safe (no crash)
 *   - Multiple LoadSeg/UnLoadSeg cycles
 *   - Verify seglist is a valid BPTR chain after LoadSeg
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

static void print_hex(ULONG n)
{
    char buf[12];
    char *p = buf;
    int i;

    *p++ = '0';
    *p++ = 'x';

    for (i = 28; i >= 0; i -= 4) {
        int nibble = (n >> i) & 0xF;
        *p++ = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
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

int main(void)
{
    BPTR seg;
    int count;

    print("UnLoadSeg Test\n");
    print("==============\n\n");

    /* Test 1: UnLoadSeg(NULL) should be safe */
    print("Test 1: UnLoadSeg(NULL)\n");
    UnLoadSeg(0);
    test_pass("UnLoadSeg(NULL) did not crash");

    /* Test 2: LoadSeg a known program, verify seglist, then UnLoadSeg */
    print("\nTest 2: LoadSeg + verify seglist + UnLoadSeg\n");

    seg = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/HelloWorld");
    if (!seg) {
        test_fail("LoadSeg HelloWorld", "LoadSeg returned NULL");
    } else {
        print("  LoadSeg returned seglist: ");
        print_hex((ULONG)seg);
        print("\n");

        /* Walk the seglist chain to count hunks */
        count = 0;
        BPTR s = seg;
        while (s) {
            BPTR *sp = (BPTR *)((ULONG)s << 2); /* BADDR */
            count++;
            print("  Hunk ");
            print_num(count);
            print(" at ");
            print_hex((ULONG)sp);
            print(", next=");
            print_hex((ULONG)sp[0]);
            print("\n");

            if (count > 100) {
                test_fail("Seglist walk", "Chain too long, possible corruption");
                break;
            }
            s = sp[0]; /* next BPTR */
        }

        if (count > 0 && count <= 100) {
            print("  Seglist has ");
            print_num(count);
            print(" hunk(s)\n");
            test_pass("Seglist chain is valid");
        }

        /* Now unload it */
        UnLoadSeg(seg);
        test_pass("UnLoadSeg completed without crash");
    }

    /* Test 3: Multiple LoadSeg/UnLoadSeg cycles */
    print("\nTest 3: Multiple LoadSeg/UnLoadSeg cycles\n");
    {
        int cycles = 5;
        int i;
        BOOL all_ok = TRUE;

        for (i = 0; i < cycles; i++) {
            seg = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/HelloWorld");
            if (!seg) {
                print("  Cycle ");
                print_num(i + 1);
                print(": LoadSeg failed\n");
                all_ok = FALSE;
                break;
            }
            UnLoadSeg(seg);
        }

        if (all_ok) {
            print("  ");
            print_num(cycles);
            print(" cycles completed\n");
            test_pass("Multiple LoadSeg/UnLoadSeg cycles");
        } else {
            test_fail("Multiple cycles", "LoadSeg failed on a cycle");
        }
    }

    /* Test 4: LoadSeg non-existent file, verify NULL */
    print("\nTest 4: LoadSeg non-existent file\n");
    seg = LoadSeg((CONST_STRPTR)"SYS:NonExistent_XYZ_12345");
    if (seg) {
        test_fail("LoadSeg non-existent", "Should have returned NULL");
        UnLoadSeg(seg);
    } else {
        test_pass("LoadSeg non-existent returns NULL");
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
