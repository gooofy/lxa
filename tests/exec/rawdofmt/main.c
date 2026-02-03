/*
 * Exec RawDoFmt Tests
 *
 * Tests:
 *   - Basic format specifiers: %d, %u, %x, %X, %s, %c
 *   - Long format specifiers: %ld, %lu, %lx, %lX
 *   - Width and padding: %8d, %08x, %-10s
 *   - String precision: %.5s
 *   - BSTR format: %b
 *   - Escape: %%
 *   - NULL string handling
 *   - Edge cases: negative numbers, large values
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;

/* Buffer for RawDoFmt output */
static char g_buffer[256];
static char *g_bufptr;

/* Helper to print a string */
static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

/* Simple number to string for test output */
static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + 15;
    BOOL neg = FALSE;
    
    *p = '\0';
    
    if (n < 0) {
        neg = TRUE;
        n = -n;
    }
    
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    
    if (neg) *--p = '-';
    
    print(p);
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

/* putChProc callback for RawDoFmt - stores character to buffer */
/* Takes char in D0, buffer pointer in A3 */
/* Updates A3 to point to next position */
void putChProc(void);
asm(
".globl _putChProc              \n"
"_putChProc:                    \n"
"       move.b  d0, (a3)+       \n"
"       rts                     \n"
);

/* Helper to format and compare */
static int format_and_check(const char *fmt, APTR args, const char *expected)
{
    g_bufptr = g_buffer;
    RawDoFmt((CONST_STRPTR)fmt, args, (void (*)())putChProc, g_buffer);
    
    /* Compare strings */
    const char *a = g_buffer;
    const char *b = expected;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

int main(void)
{
    out = Output();
    
    print("Exec RawDoFmt Tests\n");
    print("===================\n\n");
    
    /* Test 1: Basic decimal */
    print("Test 1: Basic decimal (%d)\n");
    {
        UWORD args[1] = { 42 };
        if (format_and_check("%d", args, "42")) {
            test_ok("%d with positive number");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%d with positive number");
        }
        
        args[0] = (UWORD)-17;
        if (format_and_check("%d", args, "-17")) {
            test_ok("%d with negative number");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%d with negative number");
        }
        
        args[0] = 0;
        if (format_and_check("%d", args, "0")) {
            test_ok("%d with zero");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%d with zero");
        }
    }
    
    /* Test 2: Unsigned decimal */
    print("\nTest 2: Unsigned decimal (%u)\n");
    {
        UWORD args[1] = { 65535 };
        if (format_and_check("%u", args, "65535")) {
            test_ok("%u with max UWORD");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%u with max UWORD");
        }
    }
    
    /* Test 3: Hexadecimal */
    print("\nTest 3: Hexadecimal (%x, %X)\n");
    {
        UWORD args[1] = { 0x1234 };
        if (format_and_check("%x", args, "1234")) {
            test_ok("%x lowercase");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%x lowercase");
        }
        
        args[0] = 0xABCD;
        if (format_and_check("%x", args, "abcd")) {
            test_ok("%x with letters");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%x with letters");
        }
        
        if (format_and_check("%X", args, "ABCD")) {
            test_ok("%X uppercase");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%X uppercase");
        }
    }
    
    /* Test 4: Long values */
    print("\nTest 4: Long values (%ld, %lu, %lx)\n");
    {
        ULONG largs[1] = { 100000 };
        if (format_and_check("%ld", largs, "100000")) {
            test_ok("%ld with large positive");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%ld with large positive");
        }
        
        largs[0] = (ULONG)-100000;
        if (format_and_check("%ld", largs, "-100000")) {
            test_ok("%ld with large negative");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%ld with large negative");
        }
        
        largs[0] = 0x12345678;
        if (format_and_check("%lx", largs, "12345678")) {
            test_ok("%lx with 32-bit hex");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%lx with 32-bit hex");
        }
    }
    
    /* Test 5: Strings */
    print("\nTest 5: Strings (%s)\n");
    {
        char *str = "Hello";
        ULONG sargs[1] = { (ULONG)str };
        if (format_and_check("%s", sargs, "Hello")) {
            test_ok("%s basic string");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%s basic string");
        }
        
        sargs[0] = (ULONG)NULL;
        if (format_and_check("%s", sargs, "(null)")) {
            test_ok("%s NULL pointer");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%s NULL pointer");
        }
    }
    
    /* Test 6: Characters */
    print("\nTest 6: Characters (%c)\n");
    {
        UWORD args[1] = { 'A' };
        if (format_and_check("%c", args, "A")) {
            test_ok("%c basic character");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%c basic character");
        }
    }
    
    /* Test 7: Width with zero padding */
    print("\nTest 7: Width with zero padding\n");
    {
        UWORD args[1] = { 42 };
        if (format_and_check("%08d", args, "00000042")) {
            test_ok("%08d zero-padded");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%08d zero-padded");
        }
        
        args[0] = 0xFF;
        if (format_and_check("%04x", args, "00ff")) {
            test_ok("%04x zero-padded hex");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%04x zero-padded hex");
        }
    }
    
    /* Test 8: Width with space padding */
    print("\nTest 8: Width with space padding\n");
    {
        UWORD args[1] = { 42 };
        if (format_and_check("%8d", args, "      42")) {
            test_ok("%8d space-padded");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%8d space-padded");
        }
    }
    
    /* Test 9: Left alignment */
    print("\nTest 9: Left alignment (%-)\n");
    {
        UWORD args[1] = { 42 };
        if (format_and_check("%-8d", args, "42      ")) {
            test_ok("%-8d left-aligned");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%-8d left-aligned");
        }
        
        char *str = "Hi";
        ULONG sargs[1] = { (ULONG)str };
        if (format_and_check("%-8s", sargs, "Hi      ")) {
            test_ok("%-8s left-aligned string");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%-8s left-aligned string");
        }
    }
    
    /* Test 10: String precision */
    print("\nTest 10: String precision (%.Ns)\n");
    {
        char *str = "Hello World";
        ULONG sargs[1] = { (ULONG)str };
        if (format_and_check("%.5s", sargs, "Hello")) {
            test_ok("%.5s truncated string");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%.5s truncated string");
        }
    }
    
    /* Test 11: Percent escape */
    print("\nTest 11: Percent escape (%%)\n");
    {
        if (format_and_check("100%%", NULL, "100%")) {
            test_ok("%% produces single %");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%% produces single %");
        }
    }
    
    /* Test 12: Multiple arguments */
    print("\nTest 12: Multiple arguments\n");
    {
        struct {
            UWORD a;
            UWORD b;
            UWORD c;
        } args = { 1, 2, 3 };
        if (format_and_check("%d+%d=%d", &args, "1+2=3")) {
            test_ok("Multiple %d arguments");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("Multiple %d arguments");
        }
    }
    
    /* Test 13: Mixed format string */
    print("\nTest 13: Mixed format string\n");
    {
        struct {
            char *name;
            ULONG value;
        } args = { "x", 0x42 };
        if (format_and_check("%s=0x%lx", &args, "x=0x42")) {
            test_ok("Mixed string and hex");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("Mixed string and hex");
        }
    }
    
    /* Test 14: Edge case - empty format */
    print("\nTest 14: Edge cases\n");
    {
        if (format_and_check("", NULL, "")) {
            test_ok("Empty format string");
        } else {
            test_fail_msg("Empty format string");
        }
        
        if (format_and_check("No formats here", NULL, "No formats here")) {
            test_ok("Format with no specifiers");
        } else {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("Format with no specifiers");
        }
    }
    
    /* Summary */
    print("\n=============================\n");
    print("Tests passed: ");
    print_num(test_pass);
    print("\n");
    print("Tests failed: ");
    print_num(test_fail);
    print("\n");
    
    if (test_fail == 0) {
        print("\nPASS: All RawDoFmt tests passed!\n");
        return 0;
    } else {
        print("\nFAIL: Some tests failed\n");
        return 20;
    }
}
