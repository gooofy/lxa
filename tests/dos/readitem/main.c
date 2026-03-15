#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;

static int tests_failed = 0;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[32];
    char tmp[16];
    char *p = buf;
    int i = 0;

    if (n < 0)
    {
        *p++ = '-';
        n = -n;
    }

    do
    {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    while (i > 0)
        *p++ = tmp[--i];

    *p = '\0';
    print(buf);
}

static void test_pass(const char *name)
{
    print("  PASS: ");
    print(name);
    print("\n");
}

static void test_fail(const char *name, const char *reason)
{
    print("  FAIL: ");
    print(name);
    print(" - ");
    print(reason);
    print("\n");
    tests_failed++;
}

static BOOL streq(CONST_STRPTR a, CONST_STRPTR b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return FALSE;
        a++;
        b++;
    }

    return *a == *b;
}

int main(void)
{
    struct CSource source;
    LONG result;
    LONG old_err;
    char buffer[32];
    char zero_buf[4] = { 'X', 'Y', 'Z', '\0' };
    char tiny[4];
    static const char escaped_input[] = "\"A*nB*eC***\"\" tail\n";

    print("ReadItem Test\n");
    print("=============\n\n");

    print("Test 1: Unquoted items consume space separators\n");
    source.CS_Buffer = (STRPTR)"alpha beta\n";
    source.CS_Length = 11;
    source.CS_CurChr = 0;
    result = ReadItem((CONST_STRPTR)buffer, sizeof(buffer), &source);
    if (result == ITEM_UNQUOTED && streq((CONST_STRPTR)buffer, (CONST_STRPTR)"alpha") && source.CS_CurChr == 6)
        test_pass("Unquoted token before space");
    else
        test_fail("Unquoted token before space", "Unexpected token text or separator position");

    print("\nTest 2: Quoted items convert documented escapes\n");
    source.CS_Buffer = (STRPTR)escaped_input;
    source.CS_Length = 18;
    source.CS_CurChr = 0;
    result = ReadItem((CONST_STRPTR)buffer, sizeof(buffer), &source);
    if (result == ITEM_QUOTED &&
        buffer[0] == 'A' &&
        buffer[1] == '\n' &&
        buffer[2] == 'B' &&
        buffer[3] == 0x1b &&
        buffer[4] == 'C' &&
        buffer[5] == '*' &&
        buffer[6] == '"' &&
        buffer[7] == '\0')
        test_pass("Quoted escapes converted");
    else
        test_fail("Quoted escapes converted", "Escape handling behaved incorrectly");

    print("\nTest 3: Equal sign is returned directly\n");
    source.CS_Buffer = (STRPTR)"=value\n";
    source.CS_Length = 7;
    source.CS_CurChr = 0;
    buffer[0] = 'X';
    result = ReadItem((CONST_STRPTR)buffer, sizeof(buffer), &source);
    if (result == ITEM_EQUAL && buffer[0] == '\0' && source.CS_CurChr == 1)
        test_pass("Equal sign detected");
    else
        test_fail("Equal sign detected", "Did not report ITEM_EQUAL correctly");

    print("\nTest 4: Newline produces ITEM_NOTHING and is unread\n");
    source.CS_Buffer = (STRPTR)"\n";
    source.CS_Length = 1;
    source.CS_CurChr = 0;
    result = ReadItem((CONST_STRPTR)buffer, sizeof(buffer), &source);
    if (result == ITEM_NOTHING && buffer[0] == '\0' && source.CS_CurChr == 0)
        test_pass("Newline unread on ITEM_NOTHING");
    else
        test_fail("Newline unread on ITEM_NOTHING", "Unexpected end-of-line handling");

    print("\nTest 5: EOF bug rewinds CSource after unquoted item\n");
    source.CS_Buffer = (STRPTR)"abc";
    source.CS_Length = 3;
    source.CS_CurChr = 0;
    result = ReadItem((CONST_STRPTR)buffer, sizeof(buffer), &source);
    if (result == ITEM_UNQUOTED && streq((CONST_STRPTR)buffer, (CONST_STRPTR)"abc") && source.CS_CurChr == 2)
        test_pass("EOF unread bug preserved");
    else
        test_fail("EOF unread bug preserved", "EOF handling drifted from documented compatibility");

    print("\nTest 6: Unterminated quoted item reports ITEM_ERROR\n");
    source.CS_Buffer = (STRPTR)"\"abc\n";
    source.CS_Length = 5;
    source.CS_CurChr = 0;
    old_err = 4321;
    SetIoErr(old_err);
    result = ReadItem((CONST_STRPTR)buffer, sizeof(buffer), &source);
    if (result == ITEM_ERROR && streq((CONST_STRPTR)buffer, (CONST_STRPTR)"abc") && source.CS_CurChr == 4 && IoErr() == old_err)
        test_pass("Quoted parse error preserves IoErr");
    else
        test_fail("Quoted parse error preserves IoErr", "Error return or IoErr preservation was wrong");

    print("\nTest 7: Zero-length buffer returns ITEM_NOTHING and writes NUL\n");
    source.CS_Buffer = (STRPTR)"ignored\n";
    source.CS_Length = 8;
    source.CS_CurChr = 0;
    result = ReadItem((CONST_STRPTR)zero_buf, 0, &source);
    if (result == ITEM_NOTHING && zero_buf[0] == '\0' && source.CS_CurChr == 0)
        test_pass("Zero maxchars handled");
    else
        test_fail("Zero maxchars handled", "Zero-length buffer semantics were wrong");

    print("\nTest 8: NULL buffer returns ITEM_NOTHING\n");
    source.CS_Buffer = (STRPTR)"alpha\n";
    source.CS_Length = 6;
    source.CS_CurChr = 0;
    result = ReadItem((CONST_STRPTR)0, sizeof(buffer), &source);
    if (result == ITEM_NOTHING && source.CS_CurChr == 0)
        test_pass("NULL buffer ignored");
    else
        test_fail("NULL buffer ignored", "NULL buffer handling was wrong");

    print("\nTest 9: Unquoted overflow returns ITEM_ERROR\n");
    source.CS_Buffer = (STRPTR)"alphabet\n";
    source.CS_Length = 9;
    source.CS_CurChr = 0;
    tiny[0] = '?';
    tiny[1] = '?';
    tiny[2] = '?';
    tiny[3] = '?';
    result = ReadItem((CONST_STRPTR)tiny, sizeof(tiny), &source);
    if (result == ITEM_ERROR && streq((CONST_STRPTR)tiny, (CONST_STRPTR)"alp"))
        test_pass("Unquoted overflow detected");
    else
        test_fail("Unquoted overflow detected", "Overflow did not follow compatibility behavior");

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
