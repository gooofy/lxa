#include <exec/types.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <inline/exec.h>
#include <inline/dos.h>

struct Library *MathIeeeDoubBasBase;
#define MATHIEEEDOUBBAS_BASE_NAME MathIeeeDoubBasBase
#include <inline/mathieeedoubbas.h>

struct Library *MathIeeeDoubTransBase;
#define MATHIEEEDOUBTRANS_BASE_NAME MathIeeeDoubTransBase
#include <inline/mathieeedoubtrans.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static BPTR out;
static LONG test_pass;
static LONG test_fail;

union DoubleBits {
    DOUBLE d;
    struct {
        ULONG hi;
        ULONG lo;
    } u;
};

union FloatBits {
    FLOAT f;
    ULONG u;
};

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
        Write(out, (CONST APTR)"-", 1);
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

static DOUBLE dp_from_bits(ULONG hi, ULONG lo)
{
    union DoubleBits bits;
    bits.u.hi = hi;
    bits.u.lo = lo;
    return bits.d;
}

static ULONG double_hi(DOUBLE value)
{
    union DoubleBits bits;
    bits.d = value;
    return bits.u.hi;
}

static FLOAT float_from_bits(ULONG bits)
{
    union FloatBits raw;
    raw.u = bits;
    return raw.f;
}

static ULONG double_lo(DOUBLE value)
{
    union DoubleBits bits;
    bits.d = value;
    return bits.u.lo;
}

static BOOL double_is_nan(DOUBLE value)
{
    ULONG hi = double_hi(value);
    ULONG lo = double_lo(value);

    return ((hi & 0x7ff00000UL) == 0x7ff00000UL) && (((hi & 0x000fffffUL) != 0) || (lo != 0));
}

static BOOL double_is_inf(DOUBLE value)
{
    ULONG hi = double_hi(value);
    ULONG lo = double_lo(value);

    return ((hi & 0x7fffffffUL) == 0x7ff00000UL) && (lo == 0);
}

static BOOL double_signbit(DOUBLE value)
{
    return (double_hi(value) & 0x80000000UL) != 0;
}

static void expect_double_bits(DOUBLE actual, ULONG expected_hi, ULONG expected_lo, const char *name)
{
    expect_true(double_hi(actual) == expected_hi && double_lo(actual) == expected_lo, name);
}

static void expect_float_bits(FLOAT actual, ULONG expected, const char *name)
{
    union FloatBits bits;
    bits.f = actual;
    expect_true(bits.u == expected, name);
}

static void expect_double_close(DOUBLE actual, DOUBLE expected, DOUBLE tolerance, const char *name)
{
    DOUBLE diff = IEEEDPAbs(IEEEDPSub(actual, expected));
    expect_true(IEEEDPCmp(diff, tolerance) <= 0, name);
}

int main(void)
{
    DOUBLE pi = dp_from_bits(0x400921fbUL, 0x54442d18UL);
    DOUBLE half_pi = dp_from_bits(0x3ff921fbUL, 0x54442d18UL);
    DOUBLE quarter_pi = dp_from_bits(0x3fe921fbUL, 0x54442d18UL);
    DOUBLE tol_small = IEEEDPDiv(IEEEDPFlt(1), IEEEDPFlt(1000000));
    DOUBLE tol_trig = IEEEDPDiv(IEEEDPFlt(1), IEEEDPFlt(10000));
    DOUBLE pos_zero = dp_from_bits(0x00000000UL, 0x00000000UL);
    DOUBLE neg_zero = dp_from_bits(0x80000000UL, 0x00000000UL);
    DOUBLE pos_inf = dp_from_bits(0x7ff00000UL, 0x00000000UL);
    DOUBLE neg_inf = dp_from_bits(0xfff00000UL, 0x00000000UL);
    DOUBLE quiet_nan = dp_from_bits(0x7ff80000UL, 0x00000000UL);

    out = Output();

    print("Testing mathieeedoubtrans.library\n\n");

    MathIeeeDoubBasBase = OpenLibrary("mathieeedoubbas.library", 0);
    if (MathIeeeDoubBasBase == NULL)
    {
        print("FAIL: Cannot open mathieeedoubbas.library\n");
        return 20;
    }
    test_ok("OpenLibrary mathieeedoubbas.library");

    MathIeeeDoubTransBase = OpenLibrary("mathieeedoubtrans.library", 0);
    if (MathIeeeDoubTransBase == NULL)
    {
        print("FAIL: Cannot open mathieeedoubtrans.library\n");
        CloseLibrary(MathIeeeDoubBasBase);
        return 20;
    }
    test_ok("OpenLibrary mathieeedoubtrans.library");

    expect_double_bits(IEEEDPSin(pos_zero), 0x00000000UL, 0x00000000UL, "IEEEDPSin(+0) = +0");
    expect_double_bits(IEEEDPSin(neg_zero), 0x80000000UL, 0x00000000UL, "IEEEDPSin(-0) = -0");
    expect_double_bits(IEEEDPCos(pos_zero), 0x3ff00000UL, 0x00000000UL, "IEEEDPCos(0) = 1");
    expect_double_bits(IEEEDPTan(neg_zero), 0x80000000UL, 0x00000000UL, "IEEEDPTan(-0) = -0");
    expect_double_close(IEEEDPSin(half_pi), IEEEDPFlt(1), tol_trig, "IEEEDPSin(pi/2) ~= 1");
    expect_double_close(IEEEDPCos(pi), IEEEDPNeg(IEEEDPFlt(1)), tol_trig, "IEEEDPCos(pi) ~= -1");
    expect_double_close(IEEEDPTan(quarter_pi), IEEEDPFlt(1), tol_trig, "IEEEDPTan(pi/4) ~= 1");
    expect_double_close(IEEEDPAtan(IEEEDPFlt(1)), quarter_pi, tol_trig, "IEEEDPAtan(1) ~= pi/4");

    {
        DOUBLE cos_result = pos_zero;
        DOUBLE sin_result = IEEEDPSincos(&cos_result, half_pi);
        expect_double_close(sin_result, IEEEDPFlt(1), tol_trig, "IEEEDPSincos(pi/2) sin ~= 1");
        expect_double_close(cos_result, pos_zero, tol_trig, "IEEEDPSincos(pi/2) cos ~= 0");
    }

    expect_double_bits(IEEEDPSinh(pos_zero), 0x00000000UL, 0x00000000UL, "IEEEDPSinh(+0) = +0");
    expect_double_bits(IEEEDPSinh(neg_zero), 0x80000000UL, 0x00000000UL, "IEEEDPSinh(-0) = -0");
    expect_double_bits(IEEEDPCosh(pos_zero), 0x3ff00000UL, 0x00000000UL, "IEEEDPCosh(0) = 1");
    expect_double_bits(IEEEDPTanh(pos_zero), 0x00000000UL, 0x00000000UL, "IEEEDPTanh(+0) = +0");
    expect_double_bits(IEEEDPTanh(neg_zero), 0x80000000UL, 0x00000000UL, "IEEEDPTanh(-0) = -0");
    expect_true(double_is_inf(IEEEDPSinh(pos_inf)) && !double_signbit(IEEEDPSinh(pos_inf)), "IEEEDPSinh(+inf) = +inf");
    expect_true(double_is_inf(IEEEDPCosh(pos_inf)) && !double_signbit(IEEEDPCosh(pos_inf)), "IEEEDPCosh(+inf) = +inf");
    expect_double_bits(IEEEDPTanh(pos_inf), 0x3ff00000UL, 0x00000000UL, "IEEEDPTanh(+inf) = 1");
    expect_double_bits(IEEEDPTanh(neg_inf), 0xbff00000UL, 0x00000000UL, "IEEEDPTanh(-inf) = -1");

    expect_double_bits(IEEEDPExp(pos_zero), 0x3ff00000UL, 0x00000000UL, "IEEEDPExp(0) = 1");
    expect_true(double_is_inf(IEEEDPExp(pos_inf)) && !double_signbit(IEEEDPExp(pos_inf)), "IEEEDPExp(+inf) = +inf");
    expect_double_bits(IEEEDPExp(neg_inf), 0x00000000UL, 0x00000000UL, "IEEEDPExp(-inf) = +0");
    expect_double_bits(IEEEDPLog(IEEEDPFlt(1)), 0x00000000UL, 0x00000000UL, "IEEEDPLog(1) = 0");
    expect_true(double_is_inf(IEEEDPLog(pos_zero)) && double_signbit(IEEEDPLog(pos_zero)), "IEEEDPLog(0) = -inf");
    expect_true(double_is_inf(IEEEDPLog(pos_inf)) && !double_signbit(IEEEDPLog(pos_inf)), "IEEEDPLog(+inf) = +inf");
    expect_true(double_is_nan(IEEEDPLog(IEEEDPNeg(IEEEDPFlt(1)))), "IEEEDPLog(-1) = NaN");
    expect_double_bits(IEEEDPLog10(IEEEDPFlt(10)), 0x3ff00000UL, 0x00000000UL, "IEEEDPLog10(10) = 1");
    expect_true(double_is_inf(IEEEDPLog10(pos_zero)) && double_signbit(IEEEDPLog10(pos_zero)), "IEEEDPLog10(0) = -inf");

    expect_double_bits(IEEEDPPow(IEEEDPFlt(10), IEEEDPFlt(2)), 0x40900000UL, 0x00000000UL, "IEEEDPPow(2, 10) = 1024");
    expect_double_bits(IEEEDPPow(IEEEDPFlt(0), IEEEDPFlt(2)), 0x3ff00000UL, 0x00000000UL, "IEEEDPPow(2, 0) = 1");
    expect_true(double_is_nan(IEEEDPPow(IEEEDPDiv(IEEEDPFlt(1), IEEEDPFlt(2)), IEEEDPNeg(IEEEDPFlt(1)))), "IEEEDPPow(-1, 0.5) = NaN");
    expect_double_bits(IEEEDPSqrt(IEEEDPFlt(9)), 0x40080000UL, 0x00000000UL, "IEEEDPSqrt(9) = 3");
    expect_double_bits(IEEEDPSqrt(pos_zero), 0x00000000UL, 0x00000000UL, "IEEEDPSqrt(+0) = +0");
    expect_double_bits(IEEEDPSqrt(neg_zero), 0x80000000UL, 0x00000000UL, "IEEEDPSqrt(-0) = -0");
    expect_true(double_is_nan(IEEEDPSqrt(IEEEDPNeg(IEEEDPFlt(1)))), "IEEEDPSqrt(-1) = NaN");

    expect_double_bits(IEEEDPAsin(pos_zero), 0x00000000UL, 0x00000000UL, "IEEEDPAsin(0) = 0");
    expect_double_close(IEEEDPAsin(IEEEDPFlt(1)), half_pi, tol_trig, "IEEEDPAsin(1) ~= pi/2");
    expect_true(double_is_nan(IEEEDPAsin(IEEEDPFlt(2))), "IEEEDPAsin(2) = NaN");
    expect_double_bits(IEEEDPAcos(IEEEDPFlt(1)), 0x00000000UL, 0x00000000UL, "IEEEDPAcos(1) = 0");
    expect_double_close(IEEEDPAcos(pos_zero), half_pi, tol_trig, "IEEEDPAcos(0) ~= pi/2");
    expect_true(double_is_nan(IEEEDPAcos(IEEEDPFlt(2))), "IEEEDPAcos(2) = NaN");

    expect_float_bits(IEEEDPTieee(pos_zero), 0x00000000UL, "IEEEDPTieee(+0) = +0f");
    expect_float_bits(IEEEDPTieee(neg_zero), 0x80000000UL, "IEEEDPTieee(-0) = -0f");
    expect_float_bits(IEEEDPTieee(IEEEDPFlt(1)), 0x3f800000UL, "IEEEDPTieee(1) = 1.0f");
    expect_double_bits(IEEEDPFieee(1.0f), 0x3ff00000UL, 0x00000000UL, "IEEEDPFieee(1.0f) = 1.0");
    expect_double_bits(IEEEDPFieee(-0.0f), 0x80000000UL, 0x00000000UL, "IEEEDPFieee(-0.0f) = -0.0");
    expect_true(double_is_inf(IEEEDPFieee(float_from_bits(0x7f800000UL))) && !double_signbit(IEEEDPFieee(float_from_bits(0x7f800000UL))), "IEEEDPFieee(+inf) = +inf");
    expect_true(double_is_nan(IEEEDPFieee(float_from_bits(0x7fc00000UL))), "IEEEDPFieee(NaN) = NaN");
    expect_true(double_is_nan(IEEEDPAtan(quiet_nan)), "IEEEDPAtan(NaN) = NaN");

    expect_long(IEEEDPFix(IEEEDPPow(IEEEDPFlt(4), IEEEDPFlt(3))), 81, "IEEEDPFix(IEEEDPPow(3, 4)) = 81");
    expect_long(IEEEDPFix(IEEEDPSqrt(IEEEDPFlt(144))), 12, "IEEEDPFix(IEEEDPSqrt(144)) = 12");
    expect_double_close(IEEEDPExp(IEEEDPLog(IEEEDPFlt(7))), IEEEDPFlt(7), tol_small, "IEEEDPExp(IEEEDPLog(7)) ~= 7");

    CloseLibrary(MathIeeeDoubTransBase);
    test_ok("CloseLibrary mathieeedoubtrans.library");
    CloseLibrary(MathIeeeDoubBasBase);
    test_ok("CloseLibrary mathieeedoubbas.library");

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
