/*
 * ieeedpexample.c - IEEE Double Precision math library demonstration
 * 
 * Tests mathieeedoubbas.library and mathieeedoubtrans.library functions.
 * Uses inline macros to call library functions.
 * 
 * Phase 61: mathieeedoubbas.library/mathieeedoubtrans.library Implementation
 */

#include <exec/types.h>

#include <clib/exec_protos.h>

/* Use inline macros for mathieeedoubbas */
#define MATHIEEEDOUBBAS_BASE_NAME MathIeeeDoubBasBase
#include <inline/mathieeedoubbas.h>

/* Use inline macros for mathieeedoubtrans */
#define MATHIEEEDOUBTRANS_BASE_NAME MathIeeeDoubTransBase
#include <inline/mathieeedoubtrans.h>

#include <stdio.h>

struct Library *MathIeeeDoubBasBase = NULL;
struct Library *MathIeeeDoubTransBase = NULL;

/* PI constant for angle tests */
#define PI 3.14159265358979323846

int main(void)
{
    int success = 0;
    int total_tests = 0;

    printf("IEEEDPExample: IEEE Double Precision library demonstration\n\n");

    /* Open mathieeedoubbas.library */
    if ((MathIeeeDoubBasBase = OpenLibrary("mathieeedoubbas.library", 33)) == NULL)
    {
        printf("ERROR: Can't open mathieeedoubbas.library v33\n");
        return 1;
    }
    printf("mathieeedoubbas.library opened successfully\n");

    /* Open mathieeedoubtrans.library */
    if ((MathIeeeDoubTransBase = OpenLibrary("mathieeedoubtrans.library", 33)) == NULL)
    {
        printf("ERROR: Can't open mathieeedoubtrans.library v33\n");
        CloseLibrary(MathIeeeDoubBasBase);
        return 1;
    }
    printf("mathieeedoubtrans.library opened successfully\n\n");

    /* Test 1: Integer to Double conversion */
    printf("=== Test: Integer to Double (IEEEDPFlt/IEEEDPFix) ===\n");
    {
        DOUBLE d10, d3, d42, dneg;
        LONG back;
        
        d10 = IEEEDPFlt(10);
        d3 = IEEEDPFlt(3);
        d42 = IEEEDPFlt(42);
        dneg = IEEEDPFlt(-25);
        
        /* Convert back to integer to verify */
        back = IEEEDPFix(d10);
        printf("  IEEEDPFlt(10) -> IEEEDPFix = %ld (expected: 10)\n", (long)back);
        
        back = IEEEDPFix(d3);
        printf("  IEEEDPFlt(3) -> IEEEDPFix = %ld (expected: 3)\n", (long)back);
        
        back = IEEEDPFix(d42);
        printf("  IEEEDPFlt(42) -> IEEEDPFix = %ld (expected: 42)\n", (long)back);
        
        back = IEEEDPFix(dneg);
        printf("  IEEEDPFlt(-25) -> IEEEDPFix = %ld (expected: -25)\n", (long)back);
        
        if (IEEEDPFix(IEEEDPFlt(10)) == 10 && IEEEDPFix(IEEEDPFlt(3)) == 3 && 
            IEEEDPFix(IEEEDPFlt(42)) == 42 && IEEEDPFix(IEEEDPFlt(-25)) == -25)
        {
            printf("  PASS: Integer-Double conversion roundtrip works\n");
            success++;
        }
        else
        {
            printf("  FAIL: Conversion roundtrip failed\n");
        }
    }
    total_tests++;

    /* Test 2: Double Addition */
    printf("\n=== Test: Double Addition (IEEEDPAdd) ===\n");
    {
        DOUBLE a, b, result;
        LONG int_result;
        
        a = IEEEDPFlt(10);
        b = IEEEDPFlt(3);
        result = IEEEDPAdd(a, b);  /* 10 + 3 = 13 */
        int_result = IEEEDPFix(result);
        
        printf("  IEEEDPAdd(10, 3) = %ld (expected: 13)\n", (long)int_result);
        
        a = IEEEDPFlt(-5);
        b = IEEEDPFlt(8);
        result = IEEEDPAdd(a, b);  /* -5 + 8 = 3 */
        int_result = IEEEDPFix(result);
        
        printf("  IEEEDPAdd(-5, 8) = %ld (expected: 3)\n", (long)int_result);
        
        if (IEEEDPFix(IEEEDPAdd(IEEEDPFlt(10), IEEEDPFlt(3))) == 13 &&
            IEEEDPFix(IEEEDPAdd(IEEEDPFlt(-5), IEEEDPFlt(8))) == 3)
        {
            printf("  PASS: Addition works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Addition failed\n");
        }
    }
    total_tests++;

    /* Test 3: Double Subtraction */
    printf("\n=== Test: Double Subtraction (IEEEDPSub) ===\n");
    {
        DOUBLE a, b, result;
        LONG int_result;
        
        a = IEEEDPFlt(10);
        b = IEEEDPFlt(3);
        result = IEEEDPSub(a, b);  /* 10 - 3 = 7 */
        int_result = IEEEDPFix(result);
        
        printf("  IEEEDPSub(10, 3) = %ld (expected: 7)\n", (long)int_result);
        
        if (IEEEDPFix(IEEEDPSub(IEEEDPFlt(10), IEEEDPFlt(3))) == 7)
        {
            printf("  PASS: Subtraction works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Subtraction failed\n");
        }
    }
    total_tests++;

    /* Test 4: Double Multiplication */
    printf("\n=== Test: Double Multiplication (IEEEDPMul) ===\n");
    {
        DOUBLE a, b, result;
        LONG int_result;
        
        a = IEEEDPFlt(6);
        b = IEEEDPFlt(7);
        result = IEEEDPMul(a, b);  /* 6 * 7 = 42 */
        int_result = IEEEDPFix(result);
        
        printf("  IEEEDPMul(6, 7) = %ld (expected: 42)\n", (long)int_result);
        
        if (IEEEDPFix(IEEEDPMul(IEEEDPFlt(6), IEEEDPFlt(7))) == 42)
        {
            printf("  PASS: Multiplication works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Multiplication failed\n");
        }
    }
    total_tests++;

    /* Test 5: Double Division */
    printf("\n=== Test: Double Division (IEEEDPDiv) ===\n");
    {
        DOUBLE a, b, result;
        LONG int_result;
        
        a = IEEEDPFlt(42);
        b = IEEEDPFlt(7);
        result = IEEEDPDiv(a, b);  /* 42 / 7 = 6 */
        int_result = IEEEDPFix(result);
        
        printf("  IEEEDPDiv(42, 7) = %ld (expected: 6)\n", (long)int_result);
        
        if (IEEEDPFix(IEEEDPDiv(IEEEDPFlt(42), IEEEDPFlt(7))) == 6)
        {
            printf("  PASS: Division works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Division failed\n");
        }
    }
    total_tests++;

    /* Test 6: Double Comparison */
    printf("\n=== Test: Double Comparison (IEEEDPCmp) ===\n");
    {
        DOUBLE a, b;
        LONG cmp;
        
        a = IEEEDPFlt(10);
        b = IEEEDPFlt(5);
        cmp = IEEEDPCmp(a, b);  /* 10 > 5 -> +1 */
        printf("  IEEEDPCmp(10, 5) = %ld (expected: 1)\n", (long)cmp);
        
        a = IEEEDPFlt(5);
        b = IEEEDPFlt(10);
        cmp = IEEEDPCmp(a, b);  /* 5 < 10 -> -1 */
        printf("  IEEEDPCmp(5, 10) = %ld (expected: -1)\n", (long)cmp);
        
        a = IEEEDPFlt(7);
        b = IEEEDPFlt(7);
        cmp = IEEEDPCmp(a, b);  /* 7 == 7 -> 0 */
        printf("  IEEEDPCmp(7, 7) = %ld (expected: 0)\n", (long)cmp);
        
        if (IEEEDPCmp(IEEEDPFlt(10), IEEEDPFlt(5)) > 0 &&
            IEEEDPCmp(IEEEDPFlt(5), IEEEDPFlt(10)) < 0 &&
            IEEEDPCmp(IEEEDPFlt(7), IEEEDPFlt(7)) == 0)
        {
            printf("  PASS: Comparison works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Comparison failed\n");
        }
    }
    total_tests++;

    /* Test 7: Square Root (transcendental) */
    printf("\n=== Test: Square Root (IEEEDPSqrt) ===\n");
    {
        DOUBLE a, result;
        LONG int_result;
        
        a = IEEEDPFlt(25);
        result = IEEEDPSqrt(a);  /* sqrt(25) = 5 */
        int_result = IEEEDPFix(result);
        
        printf("  IEEEDPSqrt(25) = %ld (expected: 5)\n", (long)int_result);
        
        a = IEEEDPFlt(144);
        result = IEEEDPSqrt(a);  /* sqrt(144) = 12 */
        int_result = IEEEDPFix(result);
        
        printf("  IEEEDPSqrt(144) = %ld (expected: 12)\n", (long)int_result);
        
        if (IEEEDPFix(IEEEDPSqrt(IEEEDPFlt(25))) == 5 &&
            IEEEDPFix(IEEEDPSqrt(IEEEDPFlt(144))) == 12)
        {
            printf("  PASS: Square root works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Square root failed\n");
        }
    }
    total_tests++;

    /* Test 8: Power function (transcendental) */
    printf("\n=== Test: Power (IEEEDPPow) ===\n");
    {
        DOUBLE base, exponent, result;
        LONG int_result;
        
        base = IEEEDPFlt(2);
        exponent = IEEEDPFlt(10);
        result = IEEEDPPow(exponent, base);  /* 2^10 = 1024 */
        int_result = IEEEDPFix(result);
        
        printf("  IEEEDPPow(2, 10) = %ld (expected: 1024)\n", (long)int_result);
        
        base = IEEEDPFlt(3);
        exponent = IEEEDPFlt(4);
        result = IEEEDPPow(exponent, base);  /* 3^4 = 81 */
        int_result = IEEEDPFix(result);
        
        printf("  IEEEDPPow(3, 4) = %ld (expected: 81)\n", (long)int_result);
        
        if (IEEEDPFix(IEEEDPPow(IEEEDPFlt(10), IEEEDPFlt(2))) == 1024 &&
            IEEEDPFix(IEEEDPPow(IEEEDPFlt(4), IEEEDPFlt(3))) == 81)
        {
            printf("  PASS: Power function works correctly\n");
            success++;
        }
        else
        {
            printf("  FAIL: Power function failed\n");
        }
    }
    total_tests++;

    /* Summary */
    printf("\n========================================\n");
    printf("SUMMARY: %d/%d tests passed\n", success, total_tests);
    printf("========================================\n");

    CloseLibrary(MathIeeeDoubTransBase);
    CloseLibrary(MathIeeeDoubBasBase);

    return (success == total_tests) ? 0 : 1;
}
