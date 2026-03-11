#include <exec/types.h>
#include <libraries/mathffp.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <inline/exec.h>
#include <inline/dos.h>

#define MATHFFP_BASE_NAME MathBase
#include <inline/mathffp.h>

struct Library *MathIeeeSingBasBase;
#define MATHIEEESINGBAS_BASE_NAME MathIeeeSingBasBase
#include <inline/mathieeesingbas.h>

struct Library *MathTransBase;
#define MATHTRANS_BASE_NAME MathTransBase
#include <inline/mathtrans.h>

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

static ULONG float_bits(FLOAT value)
{
    union {
        FLOAT f;
        ULONG u;
    } bits;

    bits.f = value;
    return bits.u;
}

static FLOAT ffp_from_bits(ULONG bits)
{
    union {
        FLOAT f;
        ULONG u;
    } raw;

    raw.u = bits;
    return raw.f;
}

static void expect_ieee_bits(FLOAT actual, ULONG expected, const char *name)
{
    expect_true(float_bits(actual) == expected, name);
}

static void expect_ffp_bits(FLOAT actual, ULONG expected, const char *name)
{
    expect_true(float_bits(actual) == expected, name);
}

static void expect_close(FLOAT actual_ffp, FLOAT expected_ieee, FLOAT tolerance, const char *name)
{
    FLOAT actual_ieee = SPTieee(actual_ffp);
    FLOAT diff = IEEESPAbs(IEEESPSub(actual_ieee, expected_ieee));

    expect_true(IEEESPCmp(diff, tolerance) <= 0, name);
}

int main(void)
{
    LONG i;
    FLOAT pi = ffp_from_bits(0xc90fdb42UL);
    FLOAT half_pi = ffp_from_bits(0xc90fdb41UL);
    FLOAT quarter_pi = ffp_from_bits(0xc90fdb40UL);

    out = Output();

    print("Testing mathffp.library\n\n");

    MathBase = OpenLibrary("mathffp.library", 0);
    if (MathBase == NULL)
    {
        print("FAIL: Cannot open mathffp.library\n");
        return 20;
    }
    test_ok("OpenLibrary mathffp.library");

    MathIeeeSingBasBase = OpenLibrary("mathieeesingbas.library", 0);
    if (MathIeeeSingBasBase == NULL)
    {
        print("FAIL: Cannot open mathieeesingbas.library\n");
        CloseLibrary(MathBase);
        return 20;
    }
    test_ok("OpenLibrary mathieeesingbas.library");

    MathTransBase = OpenLibrary("mathtrans.library", 0);
    if (MathTransBase == NULL)
    {
        print("FAIL: Cannot open mathtrans.library\n");
        CloseLibrary(MathIeeeSingBasBase);
        CloseLibrary(MathBase);
        return 20;
    }
    test_ok("OpenLibrary mathtrans.library");

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

    expect_long(SPFix(SPFloor(SPDiv(SPFlt(10), SPFlt(37)))), 3, "SPFloor(37/10) = 3");
    expect_long(SPFix(SPFloor(SPDiv(SPFlt(10), SPFlt(-37)))), -4, "SPFloor(-37/10) = -4");
    expect_long(SPFix(SPFloor(SPDiv(SPFlt(2), SPFlt(1)))), 0, "SPFloor(1/2) = 0");
    expect_long(SPFix(SPFloor(SPDiv(SPFlt(10), SPFlt(-30)))), -3, "SPFloor(-30/10) = -3");

    expect_long(SPFix(SPCeil(SPDiv(SPFlt(10), SPFlt(37)))), 4, "SPCeil(37/10) = 4");
    expect_long(SPFix(SPCeil(SPDiv(SPFlt(10), SPFlt(-37)))), -3, "SPCeil(-37/10) = -3");
    expect_long(SPFix(SPCeil(SPDiv(SPFlt(2), SPFlt(1)))), 1, "SPCeil(1/2) = 1");
    expect_long(SPFix(SPCeil(SPDiv(SPFlt(10), SPFlt(-30)))), -3, "SPCeil(-30/10) = -3");

    expect_close(SPSin(SPFlt(0)), 0.0f, 0.01f, "SPSin(0) ~= 0");
    expect_close(SPCos(SPFlt(0)), 1.0f, 0.01f, "SPCos(0) ~= 1");
    expect_close(SPSin(half_pi), 1.0f, 0.02f, "SPSin(pi/2) ~= 1");
    expect_close(SPCos(pi), -1.0f, 0.02f, "SPCos(pi) ~= -1");
    expect_close(SPTan(quarter_pi), 1.0f, 0.03f, "SPTan(pi/4) ~= 1");
    expect_close(SPAtan(SPFlt(1)), 0.7853982f, 0.03f, "SPAtan(1) ~= pi/4");
    expect_close(SPAsin(SPFlt(0)), 0.0f, 0.02f, "SPAsin(0) ~= 0");
    expect_close(SPAsin(SPFlt(1)), 1.5707963f, 0.03f, "SPAsin(1) ~= pi/2");
    expect_close(SPAcos(SPFlt(1)), 0.0f, 0.02f, "SPAcos(1) ~= 0");
    expect_close(SPAcos(SPFlt(0)), 1.5707963f, 0.03f, "SPAcos(0) ~= pi/2");

    {
        FLOAT cos_result = SPFlt(0);
        FLOAT sin_result = SPSincos(&cos_result, half_pi);
        expect_close(sin_result, 1.0f, 0.02f, "SPSincos(pi/2) sin ~= 1");
        expect_close(cos_result, 0.0f, 0.02f, "SPSincos(pi/2) cos ~= 0");
    }

    expect_close(SPSinh(SPFlt(0)), 0.0f, 0.01f, "SPSinh(0) ~= 0");
    expect_close(SPCosh(SPFlt(0)), 1.0f, 0.01f, "SPCosh(0) ~= 1");
    expect_close(SPTanh(SPFlt(0)), 0.0f, 0.01f, "SPTanh(0) ~= 0");

    expect_close(SPExp(SPFlt(0)), 1.0f, 0.03f, "SPExp(0) ~= 1");
    expect_close(SPLog(SPFlt(1)), 0.0f, 0.03f, "SPLog(1) ~= 0");
    expect_close(SPLog10(SPFlt(10)), 1.0f, 0.03f, "SPLog10(10) ~= 1");
    expect_close(SPPow(SPFlt(3), SPFlt(2)), 8.0f, 0.05f, "SPPow(3, 2) ~= 8");
    expect_close(SPSqrt(SPFlt(9)), 3.0f, 0.03f, "SPSqrt(9) ~= 3");

    expect_ieee_bits(SPTieee(SPFlt(0)), 0x00000000UL, "SPTieee(0) = 0.0f");
    expect_ieee_bits(SPTieee(SPFlt(1)), 0x3F800000UL, "SPTieee(1) = 1.0f");
    expect_ffp_bits(SPFieee(ffp_from_bits(0x00000000UL)), 0x00000000UL, "SPFieee(+0.0f) = FFP 0");
    expect_ffp_bits(SPFieee(ffp_from_bits(0x80000000UL)), 0x00000000UL, "SPFieee(-0.0f) = FFP 0");
    expect_long(SPFix(SPFieee(1.0f)), 1, "SPFieee(1.0f) = 1");
    expect_long(SPFix(SPFieee(-2.5f)), -2, "SPFieee(-2.5f) truncates to -2");
    expect_ffp_bits(SPFieee(ffp_from_bits(0x7f800000UL)), 0xffffff7fUL, "SPFieee(+inf) saturates to max FFP");
    expect_ffp_bits(SPFieee(ffp_from_bits(0xff800000UL)), 0xffffffffUL, "SPFieee(-inf) saturates to min FFP");
    expect_ffp_bits(SPFieee(ffp_from_bits(0x7fc00000UL)), 0x00000000UL, "SPFieee(NaN) collapses to FFP 0");
    expect_ffp_bits(SPFieee(ffp_from_bits(0x00000001UL)), 0x00000000UL, "SPFieee(subnormal) underflows to FFP 0");

    CloseLibrary(MathTransBase);
    test_ok("CloseLibrary mathtrans.library");
    CloseLibrary(MathIeeeSingBasBase);
    test_ok("CloseLibrary mathieeesingbas.library");
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
