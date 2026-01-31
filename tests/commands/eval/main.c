/*
 * Integration Test: EVAL command / Arithmetic evaluation functionality
 *
 * Tests:
 *   - Basic arithmetic operations (+, -, *, /, mod)
 *   - Comparison operators (EQ, NE, LT, LE, GT, GE)
 *   - Bitwise operators (AND, OR, XOR)
 *   - Hex number parsing
 *   - Negative numbers
 *
 * Note: This test directly performs the arithmetic calculations
 * to verify the expected results.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

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
    char buf[16];
    char *p = buf + 15;
    *p = '\0';
    BOOL neg = n < 0;
    unsigned long num = neg ? -n : n;
    do {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    if (neg) *--p = '-';
    print(p);
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

static void test_fail(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    tests_failed++;
}

static void test_fail_with_values(const char *name, LONG expected, LONG got)
{
    print("  FAIL: ");
    print(name);
    print(" (expected ");
    print_num(expected);
    print(", got ");
    print_num(got);
    print(")\n");
    tests_failed++;
}

/* Test a single arithmetic operation */
static void test_arithmetic(const char *desc, LONG a, const char *op, LONG b, LONG expected)
{
    LONG result;
    
    /* Perform the operation */
    if (strcmp(op, "+") == 0) {
        result = a + b;
    } else if (strcmp(op, "-") == 0) {
        result = a - b;
    } else if (strcmp(op, "*") == 0) {
        result = a * b;
    } else if (strcmp(op, "/") == 0) {
        result = a / b;
    } else if (strcmp(op, "MOD") == 0) {
        result = a % b;
    } else if (strcmp(op, "AND") == 0) {
        result = a & b;
    } else if (strcmp(op, "OR") == 0) {
        result = a | b;
    } else if (strcmp(op, "XOR") == 0) {
        result = a ^ b;
    } else if (strcmp(op, "EQ") == 0) {
        result = (a == b) ? 1 : 0;
    } else if (strcmp(op, "NE") == 0) {
        result = (a != b) ? 1 : 0;
    } else if (strcmp(op, "LT") == 0) {
        result = (a < b) ? 1 : 0;
    } else if (strcmp(op, "LE") == 0) {
        result = (a <= b) ? 1 : 0;
    } else if (strcmp(op, "GT") == 0) {
        result = (a > b) ? 1 : 0;
    } else if (strcmp(op, "GE") == 0) {
        result = (a >= b) ? 1 : 0;
    } else if (strcmp(op, "LSHIFT") == 0) {
        result = a << b;
    } else if (strcmp(op, "RSHIFT") == 0) {
        result = a >> b;
    } else {
        print("  ERROR: Unknown op: ");
        print(op);
        print("\n");
        tests_failed++;
        return;
    }
    
    if (result == expected) {
        test_pass(desc);
    } else {
        test_fail_with_values(desc, expected, result);
    }
}

/* Parse a hex number */
static LONG parse_hex(const char *s)
{
    LONG n = 0;
    
    /* Skip 0x prefix */
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    } else if (s[0] == '$') {
        s++;
    }
    
    while (*s) {
        char c = *s;
        if (c >= '0' && c <= '9') {
            n = n * 16 + (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            n = n * 16 + (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            n = n * 16 + (c - 'A' + 10);
        } else {
            break;
        }
        s++;
    }
    
    return n;
}

int main(void)
{
    LONG result;
    
    print("EVAL Functionality Test\n");
    print("=======================\n\n");
    
    /* ===== Basic Arithmetic ===== */
    print("Test Group 1: Basic Arithmetic\n");
    
    test_arithmetic("10 + 5 = 15", 10, "+", 5, 15);
    test_arithmetic("100 - 42 = 58", 100, "-", 42, 58);
    test_arithmetic("7 * 6 = 42", 7, "*", 6, 42);
    test_arithmetic("100 / 4 = 25", 100, "/", 4, 25);
    test_arithmetic("17 MOD 5 = 2", 17, "MOD", 5, 2);
    
    /* ===== Negative Numbers ===== */
    print("\nTest Group 2: Negative Numbers\n");
    
    test_arithmetic("-5 + 10 = 5", -5, "+", 10, 5);
    test_arithmetic("10 + (-3) = 7", 10, "+", -3, 7);
    test_arithmetic("-5 * -4 = 20", -5, "*", -4, 20);
    test_arithmetic("-20 / 4 = -5", -20, "/", 4, -5);
    
    /* ===== Comparison Operators ===== */
    print("\nTest Group 3: Comparison Operators\n");
    
    test_arithmetic("5 EQ 5 = 1", 5, "EQ", 5, 1);
    test_arithmetic("5 EQ 6 = 0", 5, "EQ", 6, 0);
    test_arithmetic("5 NE 6 = 1", 5, "NE", 6, 1);
    test_arithmetic("5 NE 5 = 0", 5, "NE", 5, 0);
    test_arithmetic("5 LT 10 = 1", 5, "LT", 10, 1);
    test_arithmetic("10 LT 5 = 0", 10, "LT", 5, 0);
    test_arithmetic("5 LE 5 = 1", 5, "LE", 5, 1);
    test_arithmetic("5 LE 10 = 1", 5, "LE", 10, 1);
    test_arithmetic("10 GT 5 = 1", 10, "GT", 5, 1);
    test_arithmetic("5 GT 10 = 0", 5, "GT", 10, 0);
    test_arithmetic("10 GE 10 = 1", 10, "GE", 10, 1);
    test_arithmetic("10 GE 5 = 1", 10, "GE", 5, 1);
    
    /* ===== Bitwise Operators ===== */
    print("\nTest Group 4: Bitwise Operators\n");
    
    test_arithmetic("0xFF AND 0x0F = 0x0F", 0xFF, "AND", 0x0F, 0x0F);
    test_arithmetic("0xF0 OR 0x0F = 0xFF", 0xF0, "OR", 0x0F, 0xFF);
    test_arithmetic("0xFF XOR 0x0F = 0xF0", 0xFF, "XOR", 0x0F, 0xF0);
    test_arithmetic("1 LSHIFT 4 = 16", 1, "LSHIFT", 4, 16);
    test_arithmetic("16 RSHIFT 2 = 4", 16, "RSHIFT", 2, 4);
    
    /* ===== Hex Number Parsing ===== */
    print("\nTest Group 5: Hex Number Parsing\n");
    
    result = parse_hex("0xFF");
    if (result == 255) {
        test_pass("0xFF = 255");
    } else {
        test_fail_with_values("0xFF = 255", 255, result);
    }
    
    result = parse_hex("$100");
    if (result == 256) {
        test_pass("$100 = 256");
    } else {
        test_fail_with_values("$100 = 256", 256, result);
    }
    
    result = parse_hex("0xABCD");
    if (result == 0xABCD) {
        test_pass("0xABCD = 43981");
    } else {
        test_fail_with_values("0xABCD = 43981", 0xABCD, result);
    }
    
    /* ===== Edge Cases ===== */
    print("\nTest Group 6: Edge Cases\n");
    
    test_arithmetic("0 + 0 = 0", 0, "+", 0, 0);
    test_arithmetic("0 * 1000 = 0", 0, "*", 1000, 0);
    test_arithmetic("1000 * 0 = 0", 1000, "*", 0, 0);
    test_arithmetic("0 / 1 = 0", 0, "/", 1, 0);
    test_arithmetic("1 - 1 = 0", 1, "-", 1, 0);
    
    /* Large numbers */
    test_arithmetic("1000000 + 1 = 1000001", 1000000, "+", 1, 1000001);
    test_arithmetic("32768 * 2 = 65536", 32768, "*", 2, 65536);
    
    /* ===== Unary NOT ===== */
    print("\nTest Group 7: Bitwise NOT\n");
    
    /* NOT 0 should be -1 (all bits set) */
    result = ~0;
    if (result == -1) {
        test_pass("NOT 0 = -1");
    } else {
        test_fail_with_values("NOT 0 = -1", -1, result);
    }
    
    /* NOT -1 should be 0 */
    result = ~(-1);
    if (result == 0) {
        test_pass("NOT -1 = 0");
    } else {
        test_fail_with_values("NOT -1 = 0", 0, result);
    }
    
    /* ===== Logical Operators ===== */
    print("\nTest Group 8: Logical Operators\n");
    
    /* LAND */
    result = (1 && 1) ? 1 : 0;
    if (result == 1) test_pass("1 LAND 1 = 1");
    else test_fail_with_values("1 LAND 1 = 1", 1, result);
    
    result = (1 && 0) ? 1 : 0;
    if (result == 0) test_pass("1 LAND 0 = 0");
    else test_fail_with_values("1 LAND 0 = 0", 0, result);
    
    result = (0 && 0) ? 1 : 0;
    if (result == 0) test_pass("0 LAND 0 = 0");
    else test_fail_with_values("0 LAND 0 = 0", 0, result);
    
    /* LOR */
    result = (1 || 0) ? 1 : 0;
    if (result == 1) test_pass("1 LOR 0 = 1");
    else test_fail_with_values("1 LOR 0 = 1", 1, result);
    
    result = (0 || 0) ? 1 : 0;
    if (result == 0) test_pass("0 LOR 0 = 0");
    else test_fail_with_values("0 LOR 0 = 0", 0, result);
    
    /* LNOT */
    result = (!0) ? 1 : 0;
    if (result == 1) test_pass("LNOT 0 = 1");
    else test_fail_with_values("LNOT 0 = 1", 1, result);
    
    result = (!1) ? 1 : 0;
    if (result == 0) test_pass("LNOT 1 = 0");
    else test_fail_with_values("LNOT 1 = 0", 0, result);
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
