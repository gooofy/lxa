/*
 * Test for mathieeesingbas.library - IEEE single-precision math
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/mathieeesingbas.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct Library *MathIeeeSingBasBase;

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;

static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG num)
{
    char buf[16];
    int i = 0;
    BOOL neg = FALSE;

    if (num < 0) {
        neg = TRUE;
        num = -num;
    }

    if (num == 0) {
        buf[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (num > 0) {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }
        while (j > 0)
            buf[i++] = temp[--j];
    }
    if (neg) {
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

/* Union for examining IEEE SP float bits */
union FloatBits {
    FLOAT f;
    ULONG u;
};

int main(void)
{
    out = Output();

    print("Testing mathieeesingbas.library\n\n");

    /* Open the library */
    MathIeeeSingBasBase = OpenLibrary("mathieeesingbas.library", 0);
    if (!MathIeeeSingBasBase) {
        print("FAIL: Cannot open mathieeesingbas.library\n");
        return 20;
    }
    test_ok("OpenLibrary mathieeesingbas.library");

    /* Test IEEESPFlt - integer to float conversion */
    {
        union FloatBits fb;

        fb.f = IEEESPFlt(0);
        if (fb.u == 0x00000000)
            test_ok("IEEESPFlt(0) = 0.0");
        else
            test_fail_msg("IEEESPFlt(0)");

        fb.f = IEEESPFlt(1);
        if (fb.u == 0x3F800000)  /* 1.0f */
            test_ok("IEEESPFlt(1) = 1.0");
        else
            test_fail_msg("IEEESPFlt(1)");

        fb.f = IEEESPFlt(-1);
        if (fb.u == 0xBF800000)  /* -1.0f */
            test_ok("IEEESPFlt(-1) = -1.0");
        else
            test_fail_msg("IEEESPFlt(-1)");

        fb.f = IEEESPFlt(42);
        if (fb.u == 0x42280000)  /* 42.0f */
            test_ok("IEEESPFlt(42) = 42.0");
        else
            test_fail_msg("IEEESPFlt(42)");
    }

    /* Test IEEESPFix - float to integer conversion */
    {
        union FloatBits fb;
        LONG result;

        fb.u = 0x3F800000;  /* 1.0f */
        result = IEEESPFix(fb.f);
        if (result == 1)
            test_ok("IEEESPFix(1.0) = 1");
        else
            test_fail_msg("IEEESPFix(1.0)");

        fb.u = 0x41A80000;  /* 21.0f */
        result = IEEESPFix(fb.f);
        if (result == 21)
            test_ok("IEEESPFix(21.0) = 21");
        else
            test_fail_msg("IEEESPFix(21.0)");

        fb.u = 0xBF800000;  /* -1.0f */
        result = IEEESPFix(fb.f);
        if (result == -1)
            test_ok("IEEESPFix(-1.0) = -1");
        else
            test_fail_msg("IEEESPFix(-1.0)");

        result = IEEESPFix(IEEESPFlt(0));
        if (result == 0)
            test_ok("IEEESPFix(IEEESPFlt(0)) = 0");
        else
            test_fail_msg("IEEESPFix(IEEESPFlt(0))");
    }

    /* Test IEEESPTst */
    {
        LONG result;

        result = IEEESPTst(IEEESPFlt(5));
        if (result > 0)
            test_ok("IEEESPTst(5.0) > 0");
        else
            test_fail_msg("IEEESPTst(5.0)");

        result = IEEESPTst(IEEESPFlt(0));
        if (result == 0)
            test_ok("IEEESPTst(0.0) == 0");
        else
            test_fail_msg("IEEESPTst(0.0)");

        result = IEEESPTst(IEEESPFlt(-3));
        if (result < 0)
            test_ok("IEEESPTst(-3.0) < 0");
        else
            test_fail_msg("IEEESPTst(-3.0)");
    }

    /* Test IEEESPCmp */
    {
        LONG result;
        FLOAT a = IEEESPFlt(10);
        FLOAT b = IEEESPFlt(20);

        result = IEEESPCmp(a, b);
        if (result < 0)
            test_ok("IEEESPCmp(10, 20) < 0");
        else
            test_fail_msg("IEEESPCmp(10, 20)");

        result = IEEESPCmp(b, a);
        if (result > 0)
            test_ok("IEEESPCmp(20, 10) > 0");
        else
            test_fail_msg("IEEESPCmp(20, 10)");

        result = IEEESPCmp(a, a);
        if (result == 0)
            test_ok("IEEESPCmp(10, 10) == 0");
        else
            test_fail_msg("IEEESPCmp(10, 10)");
    }

    /* Test IEEESPAbs */
    {
        FLOAT result;
        union FloatBits fb;

        result = IEEESPAbs(IEEESPFlt(-5));
        fb.f = result;
        if (fb.u == 0x40A00000)  /* 5.0f */
            test_ok("IEEESPAbs(-5.0) = 5.0");
        else
            test_fail_msg("IEEESPAbs(-5.0)");

        result = IEEESPAbs(IEEESPFlt(5));
        fb.f = result;
        if (fb.u == 0x40A00000)  /* 5.0f */
            test_ok("IEEESPAbs(5.0) = 5.0");
        else
            test_fail_msg("IEEESPAbs(5.0)");
    }

    /* Test IEEESPNeg */
    {
        FLOAT result;
        union FloatBits fb;

        result = IEEESPNeg(IEEESPFlt(7));
        fb.f = result;
        if (fb.u == 0xC0E00000)  /* -7.0f */
            test_ok("IEEESPNeg(7.0) = -7.0");
        else
            test_fail_msg("IEEESPNeg(7.0)");

        result = IEEESPNeg(IEEESPFlt(-7));
        fb.f = result;
        if (fb.u == 0x40E00000)  /* 7.0f */
            test_ok("IEEESPNeg(-7.0) = 7.0");
        else
            test_fail_msg("IEEESPNeg(-7.0)");
    }

    /* Test IEEESPAdd */
    {
        LONG result;

        result = IEEESPFix(IEEESPAdd(IEEESPFlt(3), IEEESPFlt(4)));
        if (result == 7)
            test_ok("IEEESPAdd(3, 4) = 7");
        else
            test_fail_msg("IEEESPAdd(3, 4)");

        result = IEEESPFix(IEEESPAdd(IEEESPFlt(-10), IEEESPFlt(3)));
        if (result == -7)
            test_ok("IEEESPAdd(-10, 3) = -7");
        else
            test_fail_msg("IEEESPAdd(-10, 3)");
    }

    /* Test IEEESPSub */
    {
        LONG result;

        result = IEEESPFix(IEEESPSub(IEEESPFlt(10), IEEESPFlt(3)));
        if (result == 7)
            test_ok("IEEESPSub(10, 3) = 7");
        else
            test_fail_msg("IEEESPSub(10, 3)");

        result = IEEESPFix(IEEESPSub(IEEESPFlt(3), IEEESPFlt(10)));
        if (result == -7)
            test_ok("IEEESPSub(3, 10) = -7");
        else
            test_fail_msg("IEEESPSub(3, 10)");
    }

    /* Test IEEESPMul */
    {
        LONG result;

        result = IEEESPFix(IEEESPMul(IEEESPFlt(6), IEEESPFlt(7)));
        if (result == 42)
            test_ok("IEEESPMul(6, 7) = 42");
        else
            test_fail_msg("IEEESPMul(6, 7)");

        result = IEEESPFix(IEEESPMul(IEEESPFlt(-5), IEEESPFlt(3)));
        if (result == -15)
            test_ok("IEEESPMul(-5, 3) = -15");
        else
            test_fail_msg("IEEESPMul(-5, 3)");
    }

    /* Test IEEESPDiv */
    {
        LONG result;

        result = IEEESPFix(IEEESPDiv(IEEESPFlt(42), IEEESPFlt(6)));
        if (result == 7)
            test_ok("IEEESPDiv(42, 6) = 7");
        else
            test_fail_msg("IEEESPDiv(42, 6)");

        result = IEEESPFix(IEEESPDiv(IEEESPFlt(100), IEEESPFlt(4)));
        if (result == 25)
            test_ok("IEEESPDiv(100, 4) = 25");
        else
            test_fail_msg("IEEESPDiv(100, 4)");
    }

    /* Test IEEESPFloor */
    {
        LONG result;

        /* Floor of 3.7 should be 3 */
        result = IEEESPFix(IEEESPFloor(IEEESPDiv(IEEESPFlt(37), IEEESPFlt(10))));
        if (result == 3)
            test_ok("IEEESPFloor(3.7) = 3");
        else {
            test_fail_msg("IEEESPFloor(3.7)");
            print("    got: "); print_num(result); print("\n");
        }

        /* Floor of -2.3 should be -3 */
        result = IEEESPFix(IEEESPFloor(IEEESPDiv(IEEESPFlt(-23), IEEESPFlt(10))));
        if (result == -3)
            test_ok("IEEESPFloor(-2.3) = -3");
        else {
            test_fail_msg("IEEESPFloor(-2.3)");
            print("    got: "); print_num(result); print("\n");
        }

        /* Floor of 5.0 should be 5 */
        result = IEEESPFix(IEEESPFloor(IEEESPFlt(5)));
        if (result == 5)
            test_ok("IEEESPFloor(5.0) = 5");
        else
            test_fail_msg("IEEESPFloor(5.0)");
    }

    /* Test IEEESPCeil */
    {
        LONG result;

        /* Ceil of 3.2 should be 4 */
        result = IEEESPFix(IEEESPCeil(IEEESPDiv(IEEESPFlt(32), IEEESPFlt(10))));
        if (result == 4)
            test_ok("IEEESPCeil(3.2) = 4");
        else {
            test_fail_msg("IEEESPCeil(3.2)");
            print("    got: "); print_num(result); print("\n");
        }

        /* Ceil of -2.7 should be -2 */
        result = IEEESPFix(IEEESPCeil(IEEESPDiv(IEEESPFlt(-27), IEEESPFlt(10))));
        if (result == -2)
            test_ok("IEEESPCeil(-2.7) = -2");
        else {
            test_fail_msg("IEEESPCeil(-2.7)");
            print("    got: "); print_num(result); print("\n");
        }

        /* Ceil of 5.0 should be 5 */
        result = IEEESPFix(IEEESPCeil(IEEESPFlt(5)));
        if (result == 5)
            test_ok("IEEESPCeil(5.0) = 5");
        else
            test_fail_msg("IEEESPCeil(5.0)");
    }

    /* Roundtrip test: IEEESPFix(IEEESPFlt(N)) == N */
    {
        LONG i;
        BOOL all_ok = TRUE;
        for (i = -100; i <= 100; i++) {
            if (IEEESPFix(IEEESPFlt(i)) != i) {
                all_ok = FALSE;
                break;
            }
        }
        if (all_ok)
            test_ok("Roundtrip IEEESPFix(IEEESPFlt(N)) for -100..100");
        else {
            test_fail_msg("Roundtrip failed");
            print("    at i="); print_num(i); print("\n");
        }
    }

    CloseLibrary(MathIeeeSingBasBase);
    test_ok("CloseLibrary");

    print("\n");
    if (test_fail == 0) {
        print("PASS: All ");
        print_num(test_pass);
        print(" tests passed!\n");
        return 0;
    } else {
        print("FAIL: ");
        print_num(test_fail);
        print(" of ");
        print_num(test_pass + test_fail);
        print(" tests failed\n");
        return 20;
    }
}
