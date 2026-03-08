#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/stdio.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;

static LONG tests_passed = 0;
static LONG tests_failed = 0;

static void print(const char *s)
{
    LONG len = 0;
    while (s[len]) len++;
    Write(Output(), (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[32];
    char *p = buf;
    char tmp[16];
    int i = 0;

    if (n < 0) {
        *p++ = '-';
        n = -n;
    }

    do {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    while (i > 0) *p++ = tmp[--i];
    *p = '\0';
    print(buf);
}

static void test_pass(const char *name)
{
    print("  PASS: ");
    print(name);
    print("\n");
    tests_passed++;
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

static BOOL streq(const char *a, const char *b)
{
    LONG i = 0;
    while (a[i] || b[i]) {
        if (a[i] != b[i])
            return FALSE;
        i++;
    }
    return TRUE;
}

int main(void)
{
    BPTR fh;
    LONG blocks;
    LONG ch;
    UBYTE buf[64];
    LONG args[3];

    print("Buffered DOS I/O Test\n");
    print("=====================\n\n");

    DeleteFile((CONST_STRPTR)"bufferedio.txt");

    print("Test 1: SetVBuf and FWrite/FRead\n");
    fh = Open((CONST_STRPTR)"bufferedio.txt", MODE_NEWFILE);
    if (!fh) {
        test_fail("Open write file", "Open failed");
        return 20;
    }

    if (SetVBuf(fh, NULL, BUF_FULL, 512) == 0)
        test_pass("SetVBuf full buffering");
    else
        test_fail("SetVBuf full buffering", "call failed");

    blocks = FWrite(fh, (CONST APTR)"ABCDEF", 2, 3);
    if (blocks == 3)
        test_pass("FWrite returns written blocks");
    else
        test_fail("FWrite returns written blocks", "wrong block count");

    Close(fh);

    fh = Open((CONST_STRPTR)"bufferedio.txt", MODE_OLDFILE);
    if (!fh) {
        test_fail("Open read file", "Open failed");
        return 20;
    }

    blocks = FRead(fh, buf, 2, 3);
    buf[6] = '\0';
    if (blocks == 3 && streq((const char *)buf, "ABCDEF"))
        test_pass("FRead returns expected blocks");
    else
        test_fail("FRead returns expected blocks", "wrong data");

    Close(fh);

    print("\nTest 2: UnGetC survives buffered mode\n");
    fh = Open((CONST_STRPTR)"bufferedio.txt", MODE_OLDFILE);
    SetVBuf(fh, NULL, BUF_FULL, 512);
    ch = FGetC(fh);
    UnGetC(fh, ch);
    ch = FGetC(fh);
    if (ch == 'A')
        test_pass("UnGetC with buffered stream");
    else
        test_fail("UnGetC with buffered stream", "did not return pushed char");
    Close(fh);

    print("\nTest 3: Line buffering with FPuts and VFPrintf\n");
    fh = Open((CONST_STRPTR)"bufferedio.txt", MODE_NEWFILE);
    SetVBuf(fh, NULL, BUF_LINE, 512);
    FPuts(fh, (CONST_STRPTR)"Line one\n");
    args[0] = 42;
    args[1] = (LONG)"ok";
    args[2] = 0x2A;
    VFPrintf(fh, (CONST_STRPTR)"Value=%ld %s %lx\n", args);
    Close(fh);

    fh = Open((CONST_STRPTR)"bufferedio.txt", MODE_OLDFILE);
    if (fh && FGets(fh, (STRPTR)buf, sizeof(buf)) && streq((const char *)buf, "Line one\n"))
        test_pass("FPuts wrote first line");
    else
        test_fail("FPuts wrote first line", "wrong line content");

    if (fh && FGets(fh, (STRPTR)buf, sizeof(buf)) && streq((const char *)buf, "Value=42 ok 2A\n"))
        test_pass("VFPrintf formatted second line");
    else
        test_fail("VFPrintf formatted second line", "wrong formatted line");

    if (fh)
        Close(fh);

    DeleteFile((CONST_STRPTR)"bufferedio.txt");

    print("\nPassed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 10 : 0;
}
