/*
 * FFPExample.c - Fast Floating Point math library demonstration
 * 
 * Based on RKM Libraries sample, adapted for automated testing.
 * Tests mathffp.library functions for FFP arithmetic.
 * 
 * Uses inline macros to ensure we're calling the correct library
 * (mathffp.library, not mathieeesingbas.library).
 */

#include <exec/types.h>
#include <libraries/mathffp.h>

#include <clib/exec_protos.h>

/* Use inline macros for mathffp to ensure correct library is called */
#define MATHFFP_BASE_NAME MathBase
#include <inline/mathffp.h>

#include <stdio.h>

struct Library *MathBase = NULL;

int main(void)
{
    int success = 0;
    int total_tests = 0;

    printf("FFPExample: Fast Floating Point library demonstration\n\n");

    if ((MathBase = OpenLibrary("mathffp.library", 33)) == NULL)
    {
        printf("ERROR: Can't open mathffp.library v33\n");
        return 1;
    }

    printf("mathffp.library opened successfully\n\n");

    /* Test 1: Integer to FFP conversion */
    printf("=== Test: Integer to FFP (SPFlt) ===\n");
    {
        FLOAT f10, f3, f42, fneg;
        LONG back;
        
        f10 = SPFlt(10);
        f3 = SPFlt(3);
        f42 = SPFlt(42);
        fneg = SPFlt(-25);
        
        /* Convert back to integer to verify */
        back = SPFix(f10);
        printf("  SPFlt(10) -> SPFix = %ld (expected: 10)\n", back);
        
        back = SPFix(f3);
        printf("  SPFlt(3) -> SPFix = %ld (expected: 3)\n", back);
        
        back = SPFix(f42);
        printf("  SPFlt(42) -> SPFix = %ld (expected: 42)\n", back);
        
        back = SPFix(fneg);
        printf("  SPFlt(-25) -> SPFix = %ld (expected: -25)\n", back);
        
        if (SPFix(SPFlt(10)) == 10 && SPFix(SPFlt(3)) == 3 && 
            SPFix(SPFlt(42)) == 42 && SPFix(SPFlt(-25)) == -25)
        {
            printf("  PASS: Integer-FFP conversion roundtrip works\n");
            success++;
        }
        else
        {
            printf("  FAIL: Conversion roundtrip failed\n");
        }
    }
    total_tests++;

    /* Test 2: FFP Addition */
    printf("\n=== Test: FFP Addition (SPAdd) ===\n");
    {
        FLOAT a, b, result;
        LONG int_result;
        
        a = SPFlt(10);
        b = SPFlt(3);
        result = SPAdd(a, b);  /* 10 + 3 = 13 */
        int_result = SPFix(result);
        
        printf("  SPAdd(10, 3) = %ld (expected: 13)\n", int_result);
        
        a = SPFlt(-5);
        b = SPFlt(8);
        result = SPAdd(a, b);  /* -5 + 8 = 3 */
        int_result = SPFix(result);
        
        printf("  SPAdd(-5, 8) = %ld (expected: 3)\n", int_result);
        
        if (SPFix(SPAdd(SPFlt(10), SPFlt(3))) == 13 &&
            SPFix(SPAdd(SPFlt(-5), SPFlt(8))) == 3)
        {
            printf("  PASS: Addition works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Addition returned wrong results\n");
        }
    }
    total_tests++;

    /* Test 3: FFP Subtraction */
    printf("\n=== Test: FFP Subtraction (SPSub) ===\n");
    {
        FLOAT a, b, result;
        LONG int_result;
        
        a = SPFlt(10);
        b = SPFlt(3);
        result = SPSub(a, b);  /* 10 - 3 = 7 */
        int_result = SPFix(result);
        
        printf("  SPSub(10, 3) = %ld (expected: 7)\n", int_result);
        
        a = SPFlt(5);
        b = SPFlt(12);
        result = SPSub(a, b);  /* 5 - 12 = -7 */
        int_result = SPFix(result);
        
        printf("  SPSub(5, 12) = %ld (expected: -7)\n", int_result);
        
        if (SPFix(SPSub(SPFlt(10), SPFlt(3))) == 7 &&
            SPFix(SPSub(SPFlt(5), SPFlt(12))) == -7)
        {
            printf("  PASS: Subtraction works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Subtraction returned wrong results\n");
        }
    }
    total_tests++;

    /* Test 4: FFP Multiplication */
    printf("\n=== Test: FFP Multiplication (SPMul) ===\n");
    {
        FLOAT a, b, result;
        LONG int_result;
        
        a = SPFlt(10);
        b = SPFlt(3);
        result = SPMul(a, b);  /* 10 * 3 = 30 */
        int_result = SPFix(result);
        
        printf("  SPMul(10, 3) = %ld (expected: 30)\n", int_result);
        
        a = SPFlt(-4);
        b = SPFlt(7);
        result = SPMul(a, b);  /* -4 * 7 = -28 */
        int_result = SPFix(result);
        
        printf("  SPMul(-4, 7) = %ld (expected: -28)\n", int_result);
        
        if (SPFix(SPMul(SPFlt(10), SPFlt(3))) == 30 &&
            SPFix(SPMul(SPFlt(-4), SPFlt(7))) == -28)
        {
            printf("  PASS: Multiplication works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Multiplication returned wrong results\n");
        }
    }
    total_tests++;

    /* Test 5: FFP Division */
    printf("\n=== Test: FFP Division (SPDiv) ===\n");
    {
        FLOAT dividend, divisor, result;
        LONG int_result;
        
        /* Note: SPDiv(divisor, dividend) - args are reversed! */
        dividend = SPFlt(30);
        divisor = SPFlt(5);
        result = SPDiv(divisor, dividend);  /* 30 / 5 = 6 */
        int_result = SPFix(result);
        
        printf("  SPDiv(30, 5) = %ld (expected: 6)\n", int_result);
        
        dividend = SPFlt(100);
        divisor = SPFlt(4);
        result = SPDiv(divisor, dividend);  /* 100 / 4 = 25 */
        int_result = SPFix(result);
        
        printf("  SPDiv(100, 4) = %ld (expected: 25)\n", int_result);
        
        if (SPFix(SPDiv(SPFlt(5), SPFlt(30))) == 6 &&
            SPFix(SPDiv(SPFlt(4), SPFlt(100))) == 25)
        {
            printf("  PASS: Division works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Division returned wrong results\n");
        }
    }
    total_tests++;

    /* Test 6: FFP Comparison */
    printf("\n=== Test: FFP Comparison (SPCmp) ===\n");
    {
        FLOAT a, b;
        LONG cmp;
        
        a = SPFlt(10);
        b = SPFlt(3);
        cmp = SPCmp(a, b);
        printf("  SPCmp(10, 3) = %ld (expected: positive)\n", cmp);
        
        a = SPFlt(3);
        b = SPFlt(10);
        cmp = SPCmp(a, b);
        printf("  SPCmp(3, 10) = %ld (expected: negative)\n", cmp);
        
        a = SPFlt(7);
        b = SPFlt(7);
        cmp = SPCmp(a, b);
        printf("  SPCmp(7, 7) = %ld (expected: 0)\n", cmp);
        
        if (SPCmp(SPFlt(10), SPFlt(3)) > 0 &&
            SPCmp(SPFlt(3), SPFlt(10)) < 0 &&
            SPCmp(SPFlt(7), SPFlt(7)) == 0)
        {
            printf("  PASS: Comparison works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Comparison returned wrong results\n");
        }
    }
    total_tests++;

    /* Test 7: FFP Negation */
    printf("\n=== Test: FFP Negation (SPNeg) ===\n");
    {
        FLOAT pos, neg;
        LONG result;
        
        pos = SPFlt(42);
        neg = SPNeg(pos);
        result = SPFix(neg);
        
        printf("  SPNeg(42) = %ld (expected: -42)\n", result);
        
        neg = SPFlt(-17);
        pos = SPNeg(neg);
        result = SPFix(pos);
        
        printf("  SPNeg(-17) = %ld (expected: 17)\n", result);
        
        if (SPFix(SPNeg(SPFlt(42))) == -42 &&
            SPFix(SPNeg(SPFlt(-17))) == 17)
        {
            printf("  PASS: Negation works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Negation returned wrong results\n");
        }
    }
    total_tests++;

    /* Test 8: FFP Absolute Value */
    printf("\n=== Test: FFP Absolute Value (SPAbs) ===\n");
    {
        FLOAT f, abs_result;
        LONG result;
        
        f = SPFlt(-42);
        abs_result = SPAbs(f);
        result = SPFix(abs_result);
        
        printf("  SPAbs(-42) = %ld (expected: 42)\n", result);
        
        f = SPFlt(17);
        abs_result = SPAbs(f);
        result = SPFix(abs_result);
        
        printf("  SPAbs(17) = %ld (expected: 17)\n", result);
        
        if (SPFix(SPAbs(SPFlt(-42))) == 42 &&
            SPFix(SPAbs(SPFlt(17))) == 17)
        {
            printf("  PASS: Absolute value works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Absolute value returned wrong results\n");
        }
    }
    total_tests++;

    /* Test 9: FFP Test (SPTst) */
    printf("\n=== Test: FFP Test (SPTst) ===\n");
    {
        FLOAT zero, pos, neg;
        LONG result;
        
        zero = SPFlt(0);
        pos = SPFlt(42);
        neg = SPFlt(-17);
        
        result = SPTst(zero);
        printf("  SPTst(0) = %ld (expected: 0)\n", result);
        
        result = SPTst(pos);
        printf("  SPTst(42) = %ld (expected: positive)\n", result);
        
        result = SPTst(neg);
        printf("  SPTst(-17) = %ld (expected: negative)\n", result);
        
        if (SPTst(SPFlt(0)) == 0 &&
            SPTst(SPFlt(42)) > 0 &&
            SPTst(SPFlt(-17)) < 0)
        {
            printf("  PASS: Test works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Test returned wrong results\n");
        }
    }
    total_tests++;

    /* Test 10: Test with zero */
    printf("\n=== Test: Zero Handling ===\n");
    {
        FLOAT zero, one;
        LONG result;
        
        zero = SPFlt(0);
        one = SPFlt(1);
        
        result = SPFix(SPAdd(zero, one));
        printf("  0 + 1 = %ld (expected: 1)\n", result);
        
        result = SPFix(SPMul(zero, one));
        printf("  0 * 1 = %ld (expected: 0)\n", result);
        
        result = SPCmp(zero, zero);
        printf("  SPCmp(0, 0) = %ld (expected: 0)\n", result);
        
        if (SPFix(SPAdd(SPFlt(0), SPFlt(1))) == 1 &&
            SPFix(SPMul(SPFlt(0), SPFlt(1))) == 0 &&
            SPCmp(SPFlt(0), SPFlt(0)) == 0)
        {
            printf("  PASS: Zero handling works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Zero handling returned wrong results\n");
        }
    }
    total_tests++;

    /* Cleanup */
    printf("\nClosing mathffp.library\n");
    CloseLibrary(MathBase);

    printf("\n=== Test Summary ===\n");
    printf("Tests passed: %d/%d\n", success, total_tests);

    if (success == total_tests)
    {
        printf("\nFFPExample: ALL TESTS PASSED\n");
        return 0;
    }
    else
    {
        printf("\nFFPExample: SOME TESTS FAILED\n");
        return 1;
    }
}
