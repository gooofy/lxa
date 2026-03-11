#include <exec/types.h>
#include <libraries/mathffp.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <inline/exec.h>
#include <inline/dos.h>

#define MATHFFP_BASE_NAME MathBase
#include <inline/mathffp.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct Library *MathBase;

static BPTR out;
static LONG test_pass;
static LONG test_fail;

static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;

    while (*p++)
    {
        len++;
    }

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG num)
{
    char buf[16];
    int i = 0;
    BOOL neg = FALSE;

    if (num < 0)
    {
        neg = TRUE;
        num = -num;
    }

    if (num == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        char temp[16];
        int j = 0;

        while (num > 0)
        {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }

        while (j > 0)
        {
            buf[i++] = temp[--j];
        }
    }

    if (neg)
    {
        Write(out, "-", 1);
    }

    buf[i] = '\0';
    print(buf);
}

static void test_ok(const char *name)
{
    print("  OK: ");
    print(name);
    print("\n");
    test_pass++;
}

static void test_fail_msg(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    test_fail++;
}

static void expect_long(LONG actual, LONG expected, const char *name)
{
    if (actual == expected)
    {
        test_ok(name);
    }
    else
    {
        test_fail_msg(name);
        print("    expected ");
        print_num(expected);
        print(", got ");
        print_num(actual);
        print("\n");
    }
}

static void expect_true(BOOL condition, const char *name)
{
    if (condition)
    {
        test_ok(name);
    }
    else
    {
        test_fail_msg(name);
    }
}

int main(void)
{
    LONG i;

    out = Output();

    print("Testing mathffp.library\n\n");

    MathBase = OpenLibrary("mathffp.library", 0);
    if (MathBase == NULL)
    {
        print("FAIL: Cannot open mathffp.library\n");
        return 20;
    }
    test_ok("OpenLibrary mathffp.library");

    expect_long(SPFix(SPFlt(0)), 0, "SPFix(SPFlt(0)) = 0");
    expect_long(SPFix(SPFlt(1)), 1, "SPFix(SPFlt(1)) = 1");
    expect_long(SPFix(SPFlt(-1)), -1, "SPFix(SPFlt(-1)) = -1");
    expect_long(SPFix(SPFlt(42)), 42, "SPFix(SPFlt(42)) = 42");
    expect_long(SPFix(SPFlt(-32768)), -32768, "SPFix(SPFlt(-32768)) = -32768");

    expect_long(SPFix(SPDiv(SPFlt(3), SPFlt(10))), 3, "SPFix(10/3) truncates toward zero");
    expect_long(SPFix(SPDiv(SPFlt(3), SPFlt(-10))), -3, "SPFix(-10/3) truncates toward zero");

    for (i = -100; i <= 100; i++)
    {
        if (SPFix(SPFlt(i)) != i)
        {
            break;
        }
    }
    expect_true(i == 101, "SPFix(SPFlt(N)) round-trips for -100..100");

    expect_long(SPTst(SPFlt(0)), 0, "SPTst(0) = 0");
    expect_long(SPTst(SPFlt(5)), 1, "SPTst(5) = 1");
    expect_long(SPTst(SPFlt(-5)), -1, "SPTst(-5) = -1");

    expect_long(SPCmp(SPFlt(10), SPFlt(20)), -1, "SPCmp(10, 20) = -1");
    expect_long(SPCmp(SPFlt(20), SPFlt(10)), 1, "SPCmp(20, 10) = 1");
    expect_long(SPCmp(SPFlt(-7), SPFlt(-3)), -1, "SPCmp(-7, -3) = -1");
    expect_long(SPCmp(SPFlt(12), SPFlt(12)), 0, "SPCmp(12, 12) = 0");

    expect_long(SPFix(SPAbs(SPFlt(-5))), 5, "SPAbs(-5) = 5");
    expect_long(SPFix(SPAbs(SPFlt(5))), 5, "SPAbs(5) = 5");
    expect_long(SPTst(SPAbs(SPFlt(0))), 0, "SPAbs(0) = 0");

    expect_long(SPFix(SPNeg(SPFlt(7))), -7, "SPNeg(7) = -7");
    expect_long(SPFix(SPNeg(SPFlt(-7))), 7, "SPNeg(-7) = 7");
    expect_long(SPTst(SPNeg(SPFlt(0))), 0, "SPNeg(0) = 0");

    expect_long(SPFix(SPAdd(SPFlt(3), SPFlt(4))), 7, "SPAdd(3, 4) = 7");
    expect_long(SPFix(SPAdd(SPFlt(-10), SPFlt(3))), -7, "SPAdd(-10, 3) = -7");
    expect_long(SPTst(SPAdd(SPFlt(-5), SPFlt(5))), 0, "SPAdd(-5, 5) = 0");
    expect_long(SPFix(SPAdd(SPFlt(9), SPFlt(0))), 9, "SPAdd(9, 0) = 9");

    expect_long(SPFix(SPSub(SPFlt(10), SPFlt(3))), 7, "SPSub(10, 3) = 7");
    expect_long(SPFix(SPSub(SPFlt(3), SPFlt(10))), -7, "SPSub(3, 10) = -7");
    expect_long(SPFix(SPSub(SPFlt(-5), SPFlt(-8))), 3, "SPSub(-5, -8) = 3");
    expect_long(SPTst(SPSub(SPFlt(9), SPFlt(9))), 0, "SPSub(9, 9) = 0");

    expect_long(SPFix(SPMul(SPFlt(6), SPFlt(7))), 42, "SPMul(6, 7) = 42");
    expect_long(SPFix(SPMul(SPFlt(-5), SPFlt(3))), -15, "SPMul(-5, 3) = -15");
    expect_long(SPTst(SPMul(SPFlt(0), SPFlt(99))), 0, "SPMul(0, 99) = 0");

    expect_long(SPFix(SPDiv(SPFlt(6), SPFlt(42))), 7, "SPDiv(6, 42) = 7");
    expect_long(SPFix(SPDiv(SPFlt(4), SPFlt(100))), 25, "SPDiv(4, 100) = 25");
    expect_long(SPFix(SPDiv(SPFlt(-3), SPFlt(21))), -7, "SPDiv(-3, 21) = -7");
    expect_long(SPTst(SPDiv(SPFlt(5), SPFlt(0))), 0, "SPDiv(5, 0) = 0");

    CloseLibrary(MathBase);
    test_ok("CloseLibrary");

    print("\n");
    if (test_fail == 0)
    {
        print("PASS: All ");
        print_num(test_pass);
        print(" tests passed!\n");
        return 0;
    }

    print("FAIL: ");
    print_num(test_fail);
    print(" of ");
    print_num(test_pass + test_fail);
    print(" tests failed\n");
    return 20;
}
