/*
 * DIR command test - Test pattern matching
 * Phase 7.5 test for lxa
 * 
 * Tests the ParsePattern/MatchPattern functions used by DIR
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;

/* Simple output helper */
static void out_str(const char *str)
{
    Write(Output(), (STRPTR)str, strlen(str));
}

static void out_num(int n)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    int neg = (n < 0);
    unsigned int num = neg ? -n : n;
    do {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    if (neg) *--p = '-';
    out_str(p);
}

/* Test pattern matching */
static BOOL test_pattern(const char *pattern, const char *str, BOOL expected)
{
    char parsed[256];
    LONG result;
    BOOL match;
    
    result = ParsePattern((STRPTR)pattern, (STRPTR)parsed, sizeof(parsed));
    
    if (result > 0) {
        /* Has wildcards - use MatchPattern */
        match = MatchPattern((STRPTR)parsed, (STRPTR)str);
    } else if (result == -1) {
        /* Literal string (no wildcards) - case-insensitive compare */
        const char *a = pattern;
        const char *b = str;
        match = TRUE;
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
            if (ca != cb) { match = FALSE; break; }
            a++; b++;
        }
        if (*a || *b) match = FALSE;
    } else {
        /* result == 0 means error */
        out_str("  ParsePattern error (result=0)\n");
        return FALSE;
    }
    
    out_str("  Pattern '");
    out_str(pattern);
    out_str("' vs '");
    out_str(str);
    out_str("': ");
    
    if (match == expected) {
        out_str(match ? "MATCH" : "NO MATCH");
        out_str(" (expected)\n");
        return TRUE;
    } else {
        out_str(match ? "MATCH" : "NO MATCH");
        out_str(" (UNEXPECTED - expected ");
        out_str(expected ? "match" : "no match");
        out_str(")\n");
        return FALSE;
    }
}

int main(int argc, char **argv)
{
    int errors = 0;
    
    (void)argc;
    (void)argv;
    
    out_str("Pattern Matching Test - Phase 7.5\n");
    out_str("==================================\n\n");
    
    /* Test 1: #? (match anything) */
    out_str("Test 1: #? pattern (match anything)\n");
    if (!test_pattern("#?", "hello.txt", TRUE)) errors++;
    if (!test_pattern("#?", "dir", TRUE)) errors++;
    if (!test_pattern("#?", "", TRUE)) errors++;
    out_str("\n");
    
    /* Test 2: Suffix patterns */
    out_str("Test 2: Suffix patterns (#?.txt)\n");
    if (!test_pattern("#?.txt", "hello.txt", TRUE)) errors++;
    if (!test_pattern("#?.txt", "readme.txt", TRUE)) errors++;
    if (!test_pattern("#?.txt", "hello.c", FALSE)) errors++;
    if (!test_pattern("#?.txt", ".txt", TRUE)) errors++;
    out_str("\n");
    
    /* Test 3: Prefix patterns */
    out_str("Test 3: Prefix patterns (test#?)\n");
    if (!test_pattern("test#?", "testing", TRUE)) errors++;
    if (!test_pattern("test#?", "test", TRUE)) errors++;
    if (!test_pattern("test#?", "mytest", FALSE)) errors++;
    out_str("\n");
    
    /* Test 4: Single character wildcard */
    out_str("Test 4: Single character wildcard (?)\n");
    if (!test_pattern("test?", "test1", TRUE)) errors++;
    if (!test_pattern("test?", "testA", TRUE)) errors++;
    if (!test_pattern("test?", "test", FALSE)) errors++;
    if (!test_pattern("test?", "test12", FALSE)) errors++;
    out_str("\n");
    
    /* Test 5: Literal patterns (no wildcards) */
    out_str("Test 5: Literal patterns\n");
    if (!test_pattern("hello.txt", "hello.txt", TRUE)) errors++;
    if (!test_pattern("hello.txt", "HELLO.TXT", TRUE)) errors++;
    if (!test_pattern("hello.txt", "hello.c", FALSE)) errors++;
    out_str("\n");
    
    /* Summary */
    out_str("Results: ");
    out_num(errors);
    out_str(" error(s)\n");
    
    if (errors == 0) {
        out_str("PASS\n");
        return 0;
    } else {
        out_str("FAIL\n");
        return 1;
    }
}
