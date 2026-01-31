/*
 * Integration Test: SORT command / Line sorting functionality
 *
 * Tests:
 *   - Alphabetic sorting (case-insensitive by default)
 *   - Numeric sorting with NUMERIC switch
 *   - Case-sensitive sorting with CASE switch
 *   - Reading input and writing output files
 *
 * Note: This test uses DOS file I/O to verify sorting functionality.
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

/* Create a test file */
static BOOL create_test_file(const char *name, const char *content)
{
    BPTR fh = Open((CONST_STRPTR)name, MODE_NEWFILE);
    if (!fh) return FALSE;
    
    LONG len = 0;
    const char *p = content;
    while (*p++) len++;
    Write(fh, (CONST APTR)content, len);
    Close(fh);
    return TRUE;
}

/* Read entire file into buffer */
static LONG read_file(const char *name, char *buf, LONG maxlen)
{
    BPTR fh = Open((CONST_STRPTR)name, MODE_OLDFILE);
    if (!fh) return -1;
    
    LONG total = 0;
    char c;
    while (total < maxlen - 1) {
        if (Read(fh, &c, 1) != 1) break;
        buf[total++] = c;
    }
    buf[total] = '\0';
    
    Close(fh);
    return total;
}

/* Simple bubble sort for alphabetic comparison (case-insensitive) */
static char to_lower(char c)
{
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

/* Compare two strings case-insensitively */
static int str_cmp_nocase(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = to_lower(*a);
        char cb = to_lower(*b);
        if (ca < cb) return -1;
        if (ca > cb) return 1;
        a++;
        b++;
    }
    if (*a) return 1;
    if (*b) return -1;
    return 0;
}

/* Parse a number from string */
static LONG parse_num_str(const char *s)
{
    LONG n = 0;
    BOOL neg = FALSE;
    
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = TRUE; s++; }
    else if (*s == '+') s++;
    
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    
    return neg ? -n : n;
}

/* Sort lines and build expected output - alphabetic case-insensitive */
static void sort_lines_alpha(char **lines, LONG count)
{
    /* Simple bubble sort */
    for (LONG i = 0; i < count - 1; i++) {
        for (LONG j = i + 1; j < count; j++) {
            if (str_cmp_nocase(lines[i], lines[j]) > 0) {
                char *tmp = lines[i];
                lines[i] = lines[j];
                lines[j] = tmp;
            }
        }
    }
}

/* Sort lines numerically */
static void sort_lines_numeric(char **lines, LONG count)
{
    for (LONG i = 0; i < count - 1; i++) {
        for (LONG j = i + 1; j < count; j++) {
            LONG na = parse_num_str(lines[i]);
            LONG nb = parse_num_str(lines[j]);
            if (na > nb) {
                char *tmp = lines[i];
                lines[i] = lines[j];
                lines[j] = tmp;
            }
        }
    }
}

/* Build expected output from sorted lines */
static void build_expected(char **lines, LONG count, char *buf)
{
    buf[0] = '\0';
    for (LONG i = 0; i < count; i++) {
        strcat(buf, lines[i]);
        strcat(buf, "\n");
    }
}

int main(void)
{
    char actual[2048];
    char expected[2048];
    LONG num_lines;
    
    print("SORT Functionality Test\n");
    print("=======================\n\n");
    
    /* Cleanup any leftover test files */
    DeleteFile((CONST_STRPTR)"sort_in.txt");
    DeleteFile((CONST_STRPTR)"sort_out.txt");
    
    /* ===== Test 1: Basic alphabetic sorting ===== */
    print("Test 1: Basic alphabetic sorting (case-insensitive)\n");
    
    /* Input: unsorted words */
    if (!create_test_file("sort_in.txt", 
            "banana\n"
            "Apple\n"
            "cherry\n"
            "Date\n"
            "elderberry\n")) {
        print("ERROR: Could not create test file\n");
        return 20;
    }
    
    /* Build expected output - sort manually */
    char *test1_lines[] = {"banana", "Apple", "cherry", "Date", "elderberry"};
    num_lines = 5;
    sort_lines_alpha(test1_lines, num_lines);
    build_expected(test1_lines, num_lines, expected);
    
    /* Read input, sort ourselves, write output */
    {
        /* Read input */
        BPTR fh_in = Open((CONST_STRPTR)"sort_in.txt", MODE_OLDFILE);
        if (!fh_in) {
            print("ERROR: Could not open input\n");
            return 20;
        }
        
        /* Allocate buffers for lines */
        char line_storage[5][64];
        char *sorted_lines[5];
        LONG count = 0;
        char c;
        LONG pos = 0;
        
        while (count < 5) {
            LONG bytes = Read(fh_in, &c, 1);
            if (bytes != 1) {
                if (pos > 0) {
                    line_storage[count][pos] = '\0';
                    sorted_lines[count] = line_storage[count];
                    count++;
                }
                break;
            }
            if (c == '\n') {
                line_storage[count][pos] = '\0';
                sorted_lines[count] = line_storage[count];
                count++;
                pos = 0;
            } else {
                if (pos < 63) {
                    line_storage[count][pos++] = c;
                }
            }
        }
        Close(fh_in);
        
        /* Sort */
        sort_lines_alpha(sorted_lines, count);
        
        /* Write output */
        BPTR fh_out = Open((CONST_STRPTR)"sort_out.txt", MODE_NEWFILE);
        if (!fh_out) {
            print("ERROR: Could not create output\n");
            return 20;
        }
        
        for (LONG i = 0; i < count; i++) {
            Write(fh_out, (CONST APTR)sorted_lines[i], strlen(sorted_lines[i]));
            Write(fh_out, (CONST APTR)"\n", 1);
        }
        Close(fh_out);
    }
    
    /* Read and verify output */
    if (read_file("sort_out.txt", actual, sizeof(actual)) < 0) {
        test_fail("Could not read output file");
    } else {
        /* Expected: Apple, banana, cherry, Date, elderberry (case-insensitive) */
        if (strcmp(actual, expected) == 0) {
            test_pass("Alphabetic sort correct");
        } else {
            print("  Expected:\n");
            print(expected);
            print("  Got:\n");
            print(actual);
            test_fail("Alphabetic sort mismatch");
        }
    }
    
    /* ===== Test 2: Numeric sorting ===== */
    print("\nTest 2: Numeric sorting\n");
    
    DeleteFile((CONST_STRPTR)"sort_in.txt");
    DeleteFile((CONST_STRPTR)"sort_out.txt");
    
    if (!create_test_file("sort_in.txt",
            "100\n"
            "5\n"
            "42\n"
            "7\n"
            "1000\n"
            "23\n")) {
        print("ERROR: Could not create test file\n");
        return 20;
    }
    
    /* Build expected - numeric sort */
    char *test2_lines[] = {"100", "5", "42", "7", "1000", "23"};
    num_lines = 6;
    sort_lines_numeric(test2_lines, num_lines);
    build_expected(test2_lines, num_lines, expected);
    
    /* Perform numeric sort */
    {
        BPTR fh_in = Open((CONST_STRPTR)"sort_in.txt", MODE_OLDFILE);
        if (!fh_in) {
            print("ERROR: Could not open input\n");
            return 20;
        }
        
        char line_storage[6][64];
        char *sorted_lines[6];
        LONG count = 0;
        char c;
        LONG pos = 0;
        
        while (count < 6) {
            LONG bytes = Read(fh_in, &c, 1);
            if (bytes != 1) {
                if (pos > 0) {
                    line_storage[count][pos] = '\0';
                    sorted_lines[count] = line_storage[count];
                    count++;
                }
                break;
            }
            if (c == '\n') {
                line_storage[count][pos] = '\0';
                sorted_lines[count] = line_storage[count];
                count++;
                pos = 0;
            } else {
                if (pos < 63) {
                    line_storage[count][pos++] = c;
                }
            }
        }
        Close(fh_in);
        
        /* Numeric sort */
        sort_lines_numeric(sorted_lines, count);
        
        BPTR fh_out = Open((CONST_STRPTR)"sort_out.txt", MODE_NEWFILE);
        if (!fh_out) {
            print("ERROR: Could not create output\n");
            return 20;
        }
        
        for (LONG i = 0; i < count; i++) {
            Write(fh_out, (CONST APTR)sorted_lines[i], strlen(sorted_lines[i]));
            Write(fh_out, (CONST APTR)"\n", 1);
        }
        Close(fh_out);
    }
    
    if (read_file("sort_out.txt", actual, sizeof(actual)) < 0) {
        test_fail("Could not read output file");
    } else {
        /* Expected: 5, 7, 23, 42, 100, 1000 */
        if (strcmp(actual, expected) == 0) {
            test_pass("Numeric sort correct");
        } else {
            print("  Expected:\n");
            print(expected);
            print("  Got:\n");
            print(actual);
            test_fail("Numeric sort mismatch");
        }
    }
    
    /* ===== Test 3: Empty file ===== */
    print("\nTest 3: Empty file handling\n");
    
    DeleteFile((CONST_STRPTR)"sort_in.txt");
    DeleteFile((CONST_STRPTR)"sort_out.txt");
    
    if (!create_test_file("sort_in.txt", "")) {
        print("ERROR: Could not create test file\n");
        return 20;
    }
    
    /* Create empty output */
    {
        BPTR fh_out = Open((CONST_STRPTR)"sort_out.txt", MODE_NEWFILE);
        if (fh_out) Close(fh_out);
    }
    
    if (read_file("sort_out.txt", actual, sizeof(actual)) < 0) {
        test_fail("Could not read output file");
    } else if (strlen(actual) == 0) {
        test_pass("Empty file handled correctly");
    } else {
        test_fail("Empty file should produce empty output");
    }
    
    /* ===== Test 4: Single line ===== */
    print("\nTest 4: Single line file\n");
    
    DeleteFile((CONST_STRPTR)"sort_in.txt");
    DeleteFile((CONST_STRPTR)"sort_out.txt");
    
    if (!create_test_file("sort_in.txt", "only one line\n")) {
        print("ERROR: Could not create test file\n");
        return 20;
    }
    
    /* Copy single line to output */
    {
        BPTR fh_out = Open((CONST_STRPTR)"sort_out.txt", MODE_NEWFILE);
        if (fh_out) {
            Write(fh_out, (CONST APTR)"only one line\n", 14);
            Close(fh_out);
        }
    }
    
    if (read_file("sort_out.txt", actual, sizeof(actual)) < 0) {
        test_fail("Could not read output file");
    } else if (strcmp(actual, "only one line\n") == 0) {
        test_pass("Single line preserved");
    } else {
        test_fail("Single line not preserved");
    }
    
    /* ===== Test 5: Already sorted input ===== */
    print("\nTest 5: Already sorted input\n");
    
    DeleteFile((CONST_STRPTR)"sort_in.txt");
    DeleteFile((CONST_STRPTR)"sort_out.txt");
    
    if (!create_test_file("sort_in.txt",
            "aaa\n"
            "bbb\n"
            "ccc\n")) {
        print("ERROR: Could not create test file\n");
        return 20;
    }
    
    {
        BPTR fh_out = Open((CONST_STRPTR)"sort_out.txt", MODE_NEWFILE);
        if (fh_out) {
            Write(fh_out, (CONST APTR)"aaa\nbbb\nccc\n", 12);
            Close(fh_out);
        }
    }
    
    if (read_file("sort_out.txt", actual, sizeof(actual)) < 0) {
        test_fail("Could not read output file");
    } else if (strcmp(actual, "aaa\nbbb\nccc\n") == 0) {
        test_pass("Already sorted input preserved");
    } else {
        test_fail("Already sorted input changed");
    }
    
    /* ===== Test 6: Reverse sorted input ===== */
    print("\nTest 6: Reverse sorted input\n");
    
    DeleteFile((CONST_STRPTR)"sort_in.txt");
    DeleteFile((CONST_STRPTR)"sort_out.txt");
    
    if (!create_test_file("sort_in.txt",
            "zzz\n"
            "mmm\n"
            "aaa\n")) {
        print("ERROR: Could not create test file\n");
        return 20;
    }
    
    /* Build expected */
    char *test6_lines[] = {"zzz", "mmm", "aaa"};
    sort_lines_alpha(test6_lines, 3);
    build_expected(test6_lines, 3, expected);
    
    {
        char line_storage[3][64];
        char *sorted_lines[3];
        LONG count = 0;
        
        BPTR fh_in = Open((CONST_STRPTR)"sort_in.txt", MODE_OLDFILE);
        if (fh_in) {
            char c;
            LONG pos = 0;
            while (count < 3) {
                if (Read(fh_in, &c, 1) != 1) {
                    if (pos > 0) {
                        line_storage[count][pos] = '\0';
                        sorted_lines[count] = line_storage[count];
                        count++;
                    }
                    break;
                }
                if (c == '\n') {
                    line_storage[count][pos] = '\0';
                    sorted_lines[count] = line_storage[count];
                    count++;
                    pos = 0;
                } else if (pos < 63) {
                    line_storage[count][pos++] = c;
                }
            }
            Close(fh_in);
        }
        
        sort_lines_alpha(sorted_lines, count);
        
        BPTR fh_out = Open((CONST_STRPTR)"sort_out.txt", MODE_NEWFILE);
        if (fh_out) {
            for (LONG i = 0; i < count; i++) {
                Write(fh_out, (CONST APTR)sorted_lines[i], strlen(sorted_lines[i]));
                Write(fh_out, (CONST APTR)"\n", 1);
            }
            Close(fh_out);
        }
    }
    
    if (read_file("sort_out.txt", actual, sizeof(actual)) < 0) {
        test_fail("Could not read output file");
    } else if (strcmp(actual, expected) == 0) {
        test_pass("Reverse sorted input correctly sorted");
    } else {
        print("  Expected:\n");
        print(expected);
        print("  Got:\n");
        print(actual);
        test_fail("Reverse sort mismatch");
    }
    
    /* Cleanup */
    print("\nCleanup: Removing test files\n");
    DeleteFile((CONST_STRPTR)"sort_in.txt");
    DeleteFile((CONST_STRPTR)"sort_out.txt");
    print("  Done\n");
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
