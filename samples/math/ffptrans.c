/* ffptrans.c - FFP Transcendental math library sample
 * Adapted from RKM Libraries Manual.
 *
 * Demonstrates mathtrans.library:
 * - SPSin, SPCos (sine/cosine via CORDIC)
 * - SPSqrt (square root)
 * - SPExp, SPLog (exponential and logarithm)
 * - SPPow (power function)
 *
 * Modified for automated testing (auto-exits with verification).
 */

#include <exec/types.h>
#include <libraries/mathffp.h>

#include <clib/exec_protos.h>
#include <clib/mathffp_protos.h>
#include <clib/mathtrans_protos.h>

#include <stdio.h>

struct Library *MathBase = NULL;
struct Library *MathTransBase = NULL;

/* Helper to compare FFP values with tolerance */
static int compare_ffp(FLOAT a, FLOAT b, FLOAT tolerance)
{
    FLOAT diff = SPSub(b, a);
    FLOAT zero = SPFlt(0);
    LONG cmp = SPCmp(zero, diff);
    if (cmp > 0) diff = SPNeg(diff);  /* abs(diff) */
    return SPCmp(tolerance, diff) >= 0;
}

int main(void)
{
    FLOAT num, result, expected;
    FLOAT tolerance;
    int errors = 0;

    printf("FFPTrans: mathtrans.library transcendental function test\n\n");

    /* Open mathffp.library first (required for basic FFP operations) */
    MathBase = OpenLibrary("mathffp.library", 0);
    if (!MathBase)
    {
        printf("ERROR: Can't open mathffp.library\n");
        return 1;
    }

    MathTransBase = OpenLibrary("mathtrans.library", 0);
    if (!MathTransBase)
    {
        printf("ERROR: Can't open mathtrans.library\n");
        CloseLibrary(MathBase);
        return 1;
    }

    /* Set tolerance for comparisons (FFP has limited precision) */
    tolerance = SPDiv(SPFlt(100), SPFlt(1));  /* 0.01 tolerance */

    /* Compute pi for angle tests */
    FLOAT pi = SPDiv(SPFlt(10000), SPFlt(31416));  /* 31416 / 10000 = 3.1416 */

    /* Test SPSin */
    printf("Testing SPSin()...\n");
    
    /* sin(0) = 0 */
    num = SPFlt(0);
    result = SPSin(num);
    expected = SPFlt(0);
    if (compare_ffp(result, expected, tolerance))
        printf("  sin(0) = 0: PASS\n");
    else
    {
        printf("  sin(0) = 0: FAIL\n");
        errors++;
    }

    /* sin(pi/6) = 0.5 (30 degrees) */
    num = SPDiv(SPFlt(6), pi);  /* pi / 6 */
    result = SPSin(num);
    expected = SPDiv(SPFlt(2), SPFlt(1));  /* 1/2 = 0.5 */
    if (compare_ffp(result, expected, tolerance))
        printf("  sin(pi/6) ~= 0.5: PASS\n");
    else
    {
        printf("  sin(pi/6) ~= 0.5: FAIL\n");
        errors++;
    }

    /* Test SPCos */
    printf("\nTesting SPCos()...\n");
    
    /* cos(0) = 1 */
    num = SPFlt(0);
    result = SPCos(num);
    expected = SPFlt(1);
    if (compare_ffp(result, expected, tolerance))
        printf("  cos(0) = 1: PASS\n");
    else
    {
        printf("  cos(0) = 1: FAIL\n");
        errors++;
    }

    /* cos(pi/3) = 0.5 (60 degrees) */
    num = SPDiv(SPFlt(3), pi);  /* pi / 3 */
    result = SPCos(num);
    expected = SPDiv(SPFlt(2), SPFlt(1));  /* 1/2 = 0.5 */
    if (compare_ffp(result, expected, tolerance))
        printf("  cos(pi/3) ~= 0.5: PASS\n");
    else
    {
        printf("  cos(pi/3) ~= 0.5: FAIL\n");
        errors++;
    }

    /* Test SPSqrt */
    printf("\nTesting SPSqrt()...\n");
    
    /* sqrt(4) = 2 */
    num = SPFlt(4);
    result = SPSqrt(num);
    expected = SPFlt(2);
    if (compare_ffp(result, expected, tolerance))
        printf("  sqrt(4) = 2: PASS\n");
    else
    {
        printf("  sqrt(4) = 2: FAIL\n");
        errors++;
    }

    /* sqrt(16) = 4 */
    num = SPFlt(16);
    result = SPSqrt(num);
    expected = SPFlt(4);
    if (compare_ffp(result, expected, tolerance))
        printf("  sqrt(16) = 4: PASS\n");
    else
    {
        printf("  sqrt(16) = 4: FAIL\n");
        errors++;
    }

    /* Test SPExp */
    printf("\nTesting SPExp()...\n");
    
    /* e^0 = 1 */
    result = SPExp(SPFlt(0));
    expected = SPFlt(1);
    if (compare_ffp(result, expected, tolerance))
        printf("  e^0 = 1: PASS\n");
    else
    {
        printf("  e^0 = 1: FAIL\n");
        errors++;
    }

    /* e^1 ~= 2.718 (test against known FFP value) */
    result = SPExp(SPFlt(1));
    /* e in FFP is 0xadf85442 */
    expected = SPDiv(SPFlt(10000), SPFlt(27183));  /* 2.7183 approximation */
    if (compare_ffp(result, expected, tolerance))
        printf("  e^1 ~= 2.718: PASS\n");
    else
    {
        printf("  e^1 ~= 2.718: FAIL\n");
        errors++;
    }

    /* Test SPLog */
    printf("\nTesting SPLog()...\n");
    
    /* ln(1) = 0 */
    result = SPLog(SPFlt(1));
    expected = SPFlt(0);
    if (compare_ffp(result, expected, tolerance))
        printf("  ln(1) = 0: PASS\n");
    else
    {
        printf("  ln(1) = 0: FAIL\n");
        errors++;
    }

    /* ln(e) ~= 1 */
    FLOAT e_val = SPDiv(SPFlt(10000), SPFlt(27183));  /* 2.7183 */
    result = SPLog(e_val);
    expected = SPFlt(1);
    if (compare_ffp(result, expected, tolerance))
        printf("  ln(e) ~= 1: PASS\n");
    else
    {
        printf("  ln(e) ~= 1: FAIL\n");
        errors++;
    }

    /* Test SPPow */
    printf("\nTesting SPPow()...\n");
    
    /* 2^3 = 8 */
    num = SPFlt(2);
    expected = SPFlt(8);
    result = SPPow(SPFlt(3), num);  /* SPPow(exponent, base) */
    if (compare_ffp(result, expected, tolerance))
        printf("  2^3 = 8: PASS\n");
    else
    {
        printf("  2^3 = 8: FAIL\n");
        errors++;
    }

    /* 3^2 = 9 */
    num = SPFlt(3);
    expected = SPFlt(9);
    result = SPPow(SPFlt(2), num);  /* SPPow(exponent, base) */
    if (compare_ffp(result, expected, tolerance))
        printf("  3^2 = 9: PASS\n");
    else
    {
        printf("  3^2 = 9: FAIL\n");
        errors++;
    }

    /* 10^2 = 100 */
    num = SPFlt(10);
    expected = SPFlt(100);
    result = SPPow(SPFlt(2), num);  /* SPPow(exponent, base) */
    /* Use larger tolerance for 10^2 due to FFP precision limits */
    FLOAT large_tolerance = SPDiv(SPFlt(10), SPFlt(1));  /* 0.1 tolerance */
    if (compare_ffp(result, expected, large_tolerance))
        printf("  10^2 = 100: PASS\n");
    else
    {
        printf("  10^2 = 100: FAIL\n");
        errors++;
    }

    /* Cleanup */
    CloseLibrary(MathTransBase);
    CloseLibrary(MathBase);

    /* Report results */
    printf("\nFFPTrans: Test complete.\n");
    if (errors == 0)
        printf("FFPTrans: All tests PASSED!\n");
    else
        printf("FFPTrans: %d test(s) FAILED!\n", errors);

    return errors;
}
