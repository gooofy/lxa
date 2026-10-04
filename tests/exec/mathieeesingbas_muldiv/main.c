/*
 * Test for mathieeesingbas.library IEEESPMul / IEEESPDiv (lxa only)
 *
 * On the AmigaOS 3.1 reference machine (FS-UAE A4000 with a 68040 and
 * Workbench 3.1's 68040.library) the ROM IEEESPMul and IEEESPDiv raise a
 * Line-F exception (Software Failure #8000000B, the task is held), while
 * FPU code of our own - including instructions the 68040 FPSP emulates -
 * works there.  On the same ROM with a 68030+68882 FS-UAE configuration
 * both functions return and this test passes unchanged (Phase 222b), so
 * the checks stay lxa only; the rest of the library is covered by the
 * reference-validated Tests/Exec/MathIeeeSingBas.
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

static FLOAT sp_bits(ULONG bits)
{
    union FloatBits fb;
    fb.u = bits;
    return fb.f;
}

int main(void)
{
    out = Output();

    print("Testing mathieeesingbas.library Mul/Div\n\n");

    MathIeeeSingBasBase = OpenLibrary("mathieeesingbas.library", 0);
    if (!MathIeeeSingBasBase) {
        print("FAIL: Cannot open mathieeesingbas.library\n");
        return 20;
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

    CloseLibrary(MathIeeeSingBasBase);

    print("\n");
    if (test_fail == 0) {
        print("PASS: All ");
        print_num(test_pass);
        print(" tests passed!\n");
        return 0;
    }
    print("FAIL: ");
    print_num(test_fail);
    print(" tests failed\n");
    return 20;
}
