/*
 * Pattern Matching Test
 * Tests ParsePattern and MatchPattern DOS functions
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

/* Simple print function using Write() */
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
    char buf[32];
    char *p = buf;
    
    if (n < 0) {
        *p++ = '-';
        n = -n;
    }
    
    char tmp[16];
    int i = 0;
    do {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    
    while (i > 0) *p++ = tmp[--i];
    *p = '\0';
    
    print(buf);
}

static void test_match(const char *pattern, const char *str, BOOL expected, const char *desc)
{
    UBYTE pat_buf[256];
    LONG pat_result;
    BOOL match_result;
    
    /* Parse the pattern */
    pat_result = ParsePattern((CONST_STRPTR)pattern, pat_buf, sizeof(pat_buf));
    
    if (pat_result == 0) {
        print("FAIL [");
        print(desc);
        print("]: ParsePattern failed for '");
        print(pattern);
        print("'\n");
        tests_failed++;
        return;
    }
    
    /* Try to match */
    match_result = MatchPattern(pat_buf, (STRPTR)str);
    
    if (match_result == expected) {
        print("PASS [");
        print(desc);
        print("]: '");
        print(pattern);
        print("' ");
        print(expected ? "matches" : "doesn't match");
        print(" '");
        print(str);
        print("'\n");
        tests_passed++;
    } else {
        print("FAIL [");
        print(desc);
        print("]: '");
        print(pattern);
        print("' ");
        print(match_result ? "matches" : "doesn't match");
        print(" '");
        print(str);
        print("' (expected ");
        print(expected ? "match" : "no match");
        print(")\n");
        tests_failed++;
    }
}

static void test_parse_pattern(const char *pattern, LONG expected_type, const char *desc)
{
    UBYTE pat_buf[256];
    LONG result;
    
    result = ParsePattern((CONST_STRPTR)pattern, pat_buf, sizeof(pat_buf));
    
    if (expected_type < 0) {
        /* Expecting literal (no wildcards) */
        if (result < 0) {
            print("PASS [");
            print(desc);
            print("]: '");
            print(pattern);
            print("' parsed as literal\n");
            tests_passed++;
        } else {
            print("FAIL [");
            print(desc);
            print("]: '");
            print(pattern);
            print("' expected literal, got ");
            print_num(result);
            print("\n");
            tests_failed++;
        }
    } else if (expected_type == 0) {
        /* Expecting error */
        if (result == 0) {
            print("PASS [");
            print(desc);
            print("]: '");
            print(pattern);
            print("' returned error as expected\n");
            tests_passed++;
        } else {
            print("FAIL [");
            print(desc);
            print("]: '");
            print(pattern);
            print("' expected error, got ");
            print_num(result);
            print("\n");
            tests_failed++;
        }
    } else {
        /* Expecting wildcard pattern */
        if (result > 0) {
            print("PASS [");
            print(desc);
            print("]: '");
            print(pattern);
            print("' parsed as wildcard pattern\n");
            tests_passed++;
        } else {
            print("FAIL [");
            print(desc);
            print("]: '");
            print(pattern);
            print("' expected wildcard, got ");
            print_num(result);
            print("\n");
            tests_failed++;
        }
    }
}

int main(void)
{
    print("=== Pattern Matching Tests ===\n\n");
    
    /* Test 1: ParsePattern return values */
    print("--- ParsePattern Return Values ---\n");
    test_parse_pattern("hello", -1, "literal string");
    test_parse_pattern("hello.txt", -1, "literal with dot");
    test_parse_pattern("#?", 1, "any string wildcard");
    test_parse_pattern("*.c", 1, "star wildcard");
    test_parse_pattern("test?", 1, "single char wildcard");
    test_parse_pattern("file#?", 1, "hash question wildcard");
    print("\n");
    
    /* Test 2: Exact match (no wildcards) */
    print("--- Exact Match (No Wildcards) ---\n");
    test_match("hello", "hello", TRUE, "exact match");
    test_match("hello", "world", FALSE, "exact no match");
    test_match("hello", "HELLO", FALSE, "case sensitive");
    test_match("", "", TRUE, "empty strings");
    test_match("test.txt", "test.txt", TRUE, "with dot");
    print("\n");
    
    /* Test 3: Single character wildcard (?) */
    print("--- Single Char Wildcard (?) ---\n");
    test_match("h?llo", "hello", TRUE, "? in middle");
    test_match("?ello", "hello", TRUE, "? at start");
    test_match("hell?", "hello", TRUE, "? at end");
    test_match("?????", "hello", TRUE, "all ?");
    test_match("h?llo", "hllo", FALSE, "? needs char");
    test_match("h?llo", "heello", FALSE, "? only one char");
    print("\n");
    
    /* Test 4: Amiga #? wildcard (any string) */
    print("--- #? Wildcard (Any String) ---\n");
    test_match("#?", "", TRUE, "#? matches empty");
    test_match("#?", "a", TRUE, "#? matches single");
    test_match("#?", "hello", TRUE, "#? matches word");
    test_match("#?", "hello world", TRUE, "#? matches phrase");
    test_match("file#?", "file", TRUE, "#? matches zero chars");
    test_match("file#?", "file.txt", TRUE, "#? matches extension");
    test_match("#?.c", ".c", TRUE, "#? at start matches empty");
    test_match("#?.c", "test.c", TRUE, "#? at start matches name");
    test_match("#?.c", "test.txt", FALSE, "#? respects suffix");
    print("\n");
    
    /* Test 5: Star wildcard (*) - treated like #? */
    print("--- Star Wildcard (*) ---\n");
    test_match("*", "", TRUE, "* matches empty");
    test_match("*", "hello", TRUE, "* matches word");
    test_match("*.c", "test.c", TRUE, "*.c matches");
    test_match("test*", "test", TRUE, "test* matches test");
    test_match("test*", "testing", TRUE, "test* matches testing");
    test_match("test*.c", "test.c", TRUE, "test*.c matches test.c");
    test_match("test*.c", "test123.c", TRUE, "test*.c matches test123.c");
    print("\n");
    
    /* Test 6: Repeated character wildcard (#c) */
    print("--- Repeated Char Wildcard (#c) ---\n");
    test_match("#a", "", TRUE, "#a matches zero a's");
    test_match("#a", "a", TRUE, "#a matches one a");
    test_match("#a", "aaa", TRUE, "#a matches multiple a's");
    test_match("#a", "b", FALSE, "#a doesn't match b");
    test_match("te#st", "tet", TRUE, "#s matches zero s's");
    test_match("te#st", "test", TRUE, "#s matches one s");
    test_match("te#st", "tesst", TRUE, "#s matches two s's");
    print("\n");
    
    /* Test 7: Complex patterns */
    print("--- Complex Patterns ---\n");
    test_match("#?.c", "main.c", TRUE, "source file");
    test_match("#?.c", "test_utils.c", TRUE, "underscore in name");
    test_match("test#?", "test", TRUE, "test prefix only");
    test_match("test#?", "test123", TRUE, "test prefix with numbers");
    test_match("test#?", "testing", TRUE, "test prefix with letters");
    test_match("#?test#?", "test", TRUE, "surrounded wildcard");
    test_match("#?test#?", "mytest", TRUE, "prefix only match");
    test_match("#?test#?", "testing", TRUE, "suffix only match");
    test_match("#?test#?", "mytesting", TRUE, "both prefix and suffix");
    print("\n");
    
    /* Test 8: Edge cases */
    print("--- Edge Cases ---\n");
    test_match("?", "a", TRUE, "single ? single char");
    test_match("?", "", FALSE, "single ? empty string");
    test_match("?", "ab", FALSE, "single ? two chars");
    test_match("??", "ab", TRUE, "double ? two chars");
    test_match("#?#?", "hello", TRUE, "double #? pattern");
    print("\n");
    
    /* Summary */
    print("=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\n");
    print("Failed: ");
    print_num(tests_failed);
    print("\n");
    print("Total:  ");
    print_num(tests_passed + tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
