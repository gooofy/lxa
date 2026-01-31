/*
 * Integration Test: SEARCH command / String search functionality
 *
 * Tests:
 *   - Case-insensitive string search in files
 *   - File I/O and line reading
 *   - Multiple file handling
 *
 * Note: This test uses DOS file I/O to verify text searching functionality.
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

/* Case-insensitive substring search */
static const char *stristr(const char *haystack, const char *needle)
{
    if (!*needle) return haystack;
    
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        
        while (*h && *n) {
            char hc = *h;
            char nc = *n;
            if (hc >= 'A' && hc <= 'Z') hc += 32;
            if (nc >= 'A' && nc <= 'Z') nc += 32;
            if (hc != nc) break;
            h++;
            n++;
        }
        
        if (!*n) return haystack;
    }
    return NULL;
}

/* Read a line from file */
static LONG read_line(BPTR fh, char *buf, LONG maxlen)
{
    LONG i = 0;
    char c;
    
    while (i < maxlen - 1) {
        if (Read(fh, &c, 1) != 1) break;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

/* Count matches in file */
static LONG count_matches(const char *filename, const char *search)
{
    BPTR fh = Open((CONST_STRPTR)filename, MODE_OLDFILE);
    if (!fh) return -1;
    
    char line[256];
    LONG count = 0;
    
    while (1) {
        LONG len = read_line(fh, line, sizeof(line));
        if (len == 0) {
            LONG pos = Seek(fh, 0, OFFSET_CURRENT);
            LONG end = Seek(fh, 0, OFFSET_END);
            Seek(fh, pos, OFFSET_BEGINNING);
            if (pos >= end) break;
        }
        
        if (stristr(line, search)) {
            count++;
        }
    }
    
    Close(fh);
    return count;
}

int main(void)
{
    LONG matches;
    
    print("SEARCH Functionality Test\n");
    print("=========================\n\n");
    
    /* Cleanup any leftover test files */
    DeleteFile((CONST_STRPTR)"search_test1.txt");
    DeleteFile((CONST_STRPTR)"search_test2.txt");
    
    /* Create test files */
    print("Setup: Creating test files\n");
    
    if (!create_test_file("search_test1.txt", 
            "This is the first line.\n"
            "The quick brown fox jumps.\n"
            "This line has FOX in uppercase.\n"
            "No match here.\n"
            "Another FOX appears!\n")) {
        print("ERROR: Could not create test file 1\n");
        return 20;
    }
    print("  Created search_test1.txt\n");
    
    if (!create_test_file("search_test2.txt",
            "Second file content.\n"
            "A fox is a clever animal.\n"
            "The end.\n")) {
        print("ERROR: Could not create test file 2\n");
        return 20;
    }
    print("  Created search_test2.txt\n");
    
    /* Test 1: Simple search */
    print("\nTest 1: Search for 'fox' (case-insensitive)\n");
    matches = count_matches("search_test1.txt", "fox");
    print("  Found ");
    print_num(matches);
    print(" matches\n");
    if (matches == 3) {
        test_pass("Found 3 matches for 'fox'");
    } else {
        test_fail("Wrong match count");
    }
    
    /* Test 2: Uppercase search */
    print("\nTest 2: Search for 'FOX' (should find same as lowercase)\n");
    matches = count_matches("search_test1.txt", "FOX");
    if (matches == 3) {
        test_pass("Case-insensitive search works");
    } else {
        test_fail("Case-insensitive search failed");
    }
    
    /* Test 3: Search for non-existent string */
    print("\nTest 3: Search for 'elephant' (no matches)\n");
    matches = count_matches("search_test1.txt", "elephant");
    if (matches == 0) {
        test_pass("Correctly found 0 matches");
    } else {
        test_fail("Should have found 0 matches");
    }
    
    /* Test 4: Search second file */
    print("\nTest 4: Search in second file\n");
    matches = count_matches("search_test2.txt", "fox");
    if (matches == 1) {
        test_pass("Found 1 match in second file");
    } else {
        print("  Found ");
        print_num(matches);
        print(" matches\n");
        test_fail("Wrong match count in second file");
    }
    
    /* Test 5: Search for multi-word pattern */
    print("\nTest 5: Search for 'quick brown'\n");
    matches = count_matches("search_test1.txt", "quick brown");
    if (matches == 1) {
        test_pass("Multi-word search works");
    } else {
        test_fail("Multi-word search failed");
    }
    
    /* Test 6: Non-existent file */
    print("\nTest 6: Search non-existent file\n");
    matches = count_matches("nonexistent_xyz.txt", "test");
    if (matches == -1) {
        test_pass("Correctly handled non-existent file");
    } else {
        test_fail("Should have returned error");
    }
    
    /* Cleanup */
    print("\nCleanup: Removing test files\n");
    DeleteFile((CONST_STRPTR)"search_test1.txt");
    DeleteFile((CONST_STRPTR)"search_test2.txt");
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
