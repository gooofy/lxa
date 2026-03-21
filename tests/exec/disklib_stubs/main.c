/*
 * Test: exec/disklib_stubs
 *
 * Phase 110: Third-party library stubs
 *
 * Tests that all four third-party disk library stubs can be opened
 * via OpenLibrary() and that their key functions return the expected
 * safe failure values.
 *
 * Libraries tested:
 *   - req.library        (Colin Fox / Bruce Dawson)
 *   - reqtools.library   (Nico Francois)
 *   - powerpacker.library (Nico Francois)
 *   - arp.library         (AmigaDOS Replacement Project)
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

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
    char *p = buf + sizeof(buf) - 1;
    BOOL neg = FALSE;

    *p = '\0';
    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }
    if (n == 0)
    {
        *--p = '0';
    }
    else
    {
        while (n > 0)
        {
            *--p = '0' + (n % 10);
            n /= 10;
        }
    }
    if (neg)
        *--p = '-';
    print(p);
}

static void print_hex(ULONG v)
{
    char buf[12];
    char *p = buf + sizeof(buf) - 1;
    static const char hex[] = "0123456789abcdef";

    *p = '\0';
    if (v == 0)
    {
        *--p = '0';
    }
    else
    {
        while (v > 0)
        {
            *--p = hex[v & 0xf];
            v >>= 4;
        }
    }
    *--p = 'x';
    *--p = '0';
    print(p);
}

/* ========================================================================
 * req.library tests
 *
 * All functions are called through their library jump table offsets.
 * Since there are no system headers for req.library, we call through
 * the base pointer + offset manually.
 * ======================================================================== */

/* req.library function offsets */
#define REQLIB_FileRequester   (-30)
#define REQLIB_ColorRequester  (-36)
#define REQLIB_GetLong         (-42)
#define REQLIB_GetString       (-48)
#define REQLIB_TextRequest     (-54)
#define REQLIB_TwoGadRequest   (-66)

/* Calling convention helpers using inline asm */
static BOOL reqlib_FileRequester(struct Library *base, APTR freq)
{
    register struct Library *a6 __asm("a6") = base;
    register APTR a0 __asm("a0") = freq;
    register BOOL d0 __asm("d0");
    __asm volatile ("jsr -30(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0) : "a1", "d1", "memory");
    return d0;
}

static LONG reqlib_ColorRequester(struct Library *base, LONG initialColor)
{
    register struct Library *a6 __asm("a6") = base;
    register LONG d0_in __asm("d0") = initialColor;
    register LONG d0 __asm("d0");
    __asm volatile ("jsr -36(%%a6)" : "=r"(d0) : "r"(a6), "0"(d0_in) : "a0", "a1", "d1", "memory");
    return d0;
}

static BOOL reqlib_GetLong(struct Library *base, APTR gls)
{
    register struct Library *a6 __asm("a6") = base;
    register APTR a0 __asm("a0") = gls;
    register BOOL d0 __asm("d0");
    __asm volatile ("jsr -42(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0) : "a1", "d1", "memory");
    return d0;
}

static BOOL reqlib_GetString(struct Library *base, APTR gss)
{
    register struct Library *a6 __asm("a6") = base;
    register APTR a0 __asm("a0") = gss;
    register BOOL d0 __asm("d0");
    __asm volatile ("jsr -48(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0) : "a1", "d1", "memory");
    return d0;
}

static LONG reqlib_TextRequest(struct Library *base, APTR tr)
{
    register struct Library *a6 __asm("a6") = base;
    register APTR a0 __asm("a0") = tr;
    register LONG d0 __asm("d0");
    __asm volatile ("jsr -54(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0) : "a1", "d1", "memory");
    return d0;
}

static BOOL reqlib_TwoGadRequest(struct Library *base, APTR tr)
{
    register struct Library *a6 __asm("a6") = base;
    register APTR a0 __asm("a0") = tr;
    register BOOL d0 __asm("d0");
    __asm volatile ("jsr -66(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0) : "a1", "d1", "memory");
    return d0;
}

static int test_req_library(void)
{
    int errors = 0;
    struct Library *base;

    print("--- Test: req.library ---\n");

    base = OpenLibrary((CONST_STRPTR)"req.library", 0);
    if (base == NULL)
    {
        print("FAIL: OpenLibrary(\"req.library\", 0) returned NULL\n");
        return 1;
    }
    print("OK: OpenLibrary(\"req.library\") returned ");
    print_hex((ULONG)base);
    print("\n");

    /* Verify version/revision */
    if (base->lib_Version != 1)
    {
        print("FAIL: lib_Version is ");
        print_num(base->lib_Version);
        print(" expected 1\n");
        errors++;
    }
    else
    {
        print("OK: lib_Version == 1\n");
    }

    /* FileRequester(NULL) should return FALSE */
    {
        BOOL result = reqlib_FileRequester(base, NULL);
        if (result != FALSE)
        {
            print("FAIL: FileRequester(NULL) returned ");
            print_num(result);
            print(" expected FALSE\n");
            errors++;
        }
        else
        {
            print("OK: FileRequester(NULL) returned FALSE\n");
        }
    }

    /* ColorRequester(5) should return -1 */
    {
        LONG result = reqlib_ColorRequester(base, 5);
        if (result != -1)
        {
            print("FAIL: ColorRequester(5) returned ");
            print_num(result);
            print(" expected -1\n");
            errors++;
        }
        else
        {
            print("OK: ColorRequester(5) returned -1\n");
        }
    }

    /* GetLong(NULL) should return FALSE */
    {
        BOOL result = reqlib_GetLong(base, NULL);
        if (result != FALSE)
        {
            print("FAIL: GetLong(NULL) returned ");
            print_num(result);
            print(" expected FALSE\n");
            errors++;
        }
        else
        {
            print("OK: GetLong(NULL) returned FALSE\n");
        }
    }

    /* GetString(NULL) should return FALSE */
    {
        BOOL result = reqlib_GetString(base, NULL);
        if (result != FALSE)
        {
            print("FAIL: GetString(NULL) returned ");
            print_num(result);
            print(" expected FALSE\n");
            errors++;
        }
        else
        {
            print("OK: GetString(NULL) returned FALSE\n");
        }
    }

    /* TextRequest(NULL) should return 0 */
    {
        LONG result = reqlib_TextRequest(base, NULL);
        if (result != 0)
        {
            print("FAIL: TextRequest(NULL) returned ");
            print_num(result);
            print(" expected 0\n");
            errors++;
        }
        else
        {
            print("OK: TextRequest(NULL) returned 0\n");
        }
    }

    /* TwoGadRequest(NULL) should return FALSE */
    {
        BOOL result = reqlib_TwoGadRequest(base, NULL);
        if (result != FALSE)
        {
            print("FAIL: TwoGadRequest(NULL) returned ");
            print_num(result);
            print(" expected FALSE\n");
            errors++;
        }
        else
        {
            print("OK: TwoGadRequest(NULL) returned FALSE\n");
        }
    }

    CloseLibrary(base);
    print("OK: CloseLibrary(req.library) succeeded\n\n");
    return errors;
}

/* ========================================================================
 * powerpacker.library tests
 * ======================================================================== */

#define PP_OPENERR 1

static ULONG pplib_ppLoadData(struct Library *base, STRPTR fileName, ULONG colorType,
                               ULONG memType, UBYTE **buffer, ULONG *length, APTR callback)
{
    register struct Library *a6 __asm("a6") = base;
    register STRPTR a0 __asm("a0") = fileName;
    register ULONG d0_in __asm("d0") = colorType;
    register ULONG d1 __asm("d1") = memType;
    register UBYTE **a1 __asm("a1") = buffer;
    register ULONG *a2 __asm("a2") = length;
    register APTR a3 __asm("a3") = callback;
    register ULONG d0 __asm("d0");
    __asm volatile ("jsr -30(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0), "0"(d0_in), "r"(d1), "r"(a1), "r"(a2), "r"(a3) : "memory");
    return d0;
}

static UWORD pplib_ppCalcChecksum(struct Library *base, UBYTE *buffer)
{
    register struct Library *a6 __asm("a6") = base;
    register UBYTE *a0 __asm("a0") = buffer;
    register UWORD d0 __asm("d0");
    __asm volatile ("jsr -42(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0) : "a1", "d1", "memory");
    return d0;
}

static ULONG pplib_ppCalcPasskey(struct Library *base, STRPTR password)
{
    register struct Library *a6 __asm("a6") = base;
    register STRPTR a0 __asm("a0") = password;
    register ULONG d0 __asm("d0");
    __asm volatile ("jsr -48(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0) : "a1", "d1", "memory");
    return d0;
}

static APTR pplib_ppAllocCrunchInfo(struct Library *base, ULONG efficiency, ULONG speedup,
                                     APTR callback, ULONG memType)
{
    register struct Library *a6 __asm("a6") = base;
    register ULONG d0_in __asm("d0") = efficiency;
    register ULONG d1 __asm("d1") = speedup;
    register APTR a0 __asm("a0") = callback;
    register ULONG d2 __asm("d2") = memType;
    register APTR d0 __asm("d0");
    __asm volatile ("jsr -60(%%a6)" : "=r"(d0) : "r"(a6), "0"(d0_in), "r"(d1), "r"(a0), "r"(d2) : "a1", "memory");
    return d0;
}

static STRPTR pplib_ppErrorMessage(struct Library *base, ULONG errorCode)
{
    register struct Library *a6 __asm("a6") = base;
    register ULONG d0_in __asm("d0") = errorCode;
    register STRPTR d0 __asm("d0");
    __asm volatile ("jsr -90(%%a6)" : "=r"(d0) : "r"(a6), "0"(d0_in) : "a0", "a1", "d1", "memory");
    return d0;
}

static int test_powerpacker_library(void)
{
    int errors = 0;
    struct Library *base;

    print("--- Test: powerpacker.library ---\n");

    base = OpenLibrary((CONST_STRPTR)"powerpacker.library", 0);
    if (base == NULL)
    {
        print("FAIL: OpenLibrary(\"powerpacker.library\", 0) returned NULL\n");
        return 1;
    }
    print("OK: OpenLibrary(\"powerpacker.library\") returned ");
    print_hex((ULONG)base);
    print("\n");

    /* Verify version */
    if (base->lib_Version != 36)
    {
        print("FAIL: lib_Version is ");
        print_num(base->lib_Version);
        print(" expected 36\n");
        errors++;
    }
    else
    {
        print("OK: lib_Version == 36\n");
    }

    /* ppLoadData should return PP_OPENERR (1) */
    {
        UBYTE *buffer = (UBYTE *)0xdeadbeef;
        ULONG length = 0xdeadbeef;
        ULONG result = pplib_ppLoadData(base, (STRPTR)"test.pp", 0, 0, &buffer, &length, NULL);
        if (result != PP_OPENERR)
        {
            print("FAIL: ppLoadData() returned ");
            print_num((LONG)result);
            print(" expected PP_OPENERR (1)\n");
            errors++;
        }
        else
        {
            print("OK: ppLoadData() returned PP_OPENERR\n");
        }
        if (buffer != NULL)
        {
            print("FAIL: ppLoadData() buffer was not set to NULL\n");
            errors++;
        }
        else
        {
            print("OK: ppLoadData() buffer set to NULL\n");
        }
        if (length != 0)
        {
            print("FAIL: ppLoadData() length was not set to 0\n");
            errors++;
        }
        else
        {
            print("OK: ppLoadData() length set to 0\n");
        }
    }

    /* ppCalcChecksum(NULL) should return 0 */
    {
        UWORD result = pplib_ppCalcChecksum(base, NULL);
        if (result != 0)
        {
            print("FAIL: ppCalcChecksum(NULL) returned ");
            print_num((LONG)result);
            print(" expected 0\n");
            errors++;
        }
        else
        {
            print("OK: ppCalcChecksum(NULL) returned 0\n");
        }
    }

    /* ppCalcPasskey(NULL) should return 0 */
    {
        ULONG result = pplib_ppCalcPasskey(base, NULL);
        if (result != 0)
        {
            print("FAIL: ppCalcPasskey(NULL) returned ");
            print_num((LONG)result);
            print(" expected 0\n");
            errors++;
        }
        else
        {
            print("OK: ppCalcPasskey(NULL) returned 0\n");
        }
    }

    /* ppAllocCrunchInfo should return NULL */
    {
        APTR result = pplib_ppAllocCrunchInfo(base, 1, 0, NULL, 0);
        if (result != NULL)
        {
            print("FAIL: ppAllocCrunchInfo() returned non-NULL\n");
            errors++;
        }
        else
        {
            print("OK: ppAllocCrunchInfo() returned NULL\n");
        }
    }

    /* ppErrorMessage should return NULL */
    {
        STRPTR result = pplib_ppErrorMessage(base, PP_OPENERR);
        if (result != NULL)
        {
            print("FAIL: ppErrorMessage(PP_OPENERR) returned non-NULL\n");
            errors++;
        }
        else
        {
            print("OK: ppErrorMessage(PP_OPENERR) returned NULL\n");
        }
    }

    CloseLibrary(base);
    print("OK: CloseLibrary(powerpacker.library) succeeded\n\n");
    return errors;
}

/* ========================================================================
 * reqtools.library tests
 * ======================================================================== */

static APTR rtlib_rtAllocRequestA(struct Library *base, ULONG type, APTR taglist)
{
    register struct Library *a6 __asm("a6") = base;
    register ULONG d0_in __asm("d0") = type;
    register APTR a0 __asm("a0") = taglist;
    register APTR d0 __asm("d0");
    __asm volatile ("jsr -30(%%a6)" : "=r"(d0) : "r"(a6), "0"(d0_in), "r"(a0) : "a1", "d1", "memory");
    return d0;
}

static ULONG rtlib_rtEZRequestA(struct Library *base, STRPTR bodyfmt, STRPTR gadfmt,
                                 APTR reqinfo, APTR argarray, APTR taglist)
{
    register struct Library *a6 __asm("a6") = base;
    register STRPTR a1 __asm("a1") = bodyfmt;
    register STRPTR a2 __asm("a2") = gadfmt;
    register APTR a3 __asm("a3") = reqinfo;
    register APTR a4 __asm("a4") = argarray;
    register APTR a0 __asm("a0") = taglist;
    register ULONG d0 __asm("d0");
    __asm volatile ("jsr -66(%%a6)" : "=r"(d0) : "r"(a6), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a0) : "d1", "memory");
    return d0;
}

static APTR rtlib_rtFileRequestA(struct Library *base, APTR filereq, STRPTR file,
                                  STRPTR title, APTR taglist)
{
    register struct Library *a6 __asm("a6") = base;
    register APTR a1 __asm("a1") = filereq;
    register STRPTR a2 __asm("a2") = file;
    register STRPTR a3 __asm("a3") = title;
    register APTR a0 __asm("a0") = taglist;
    register APTR d0 __asm("d0");
    __asm volatile ("jsr -54(%%a6)" : "=r"(d0) : "r"(a6), "r"(a1), "r"(a2), "r"(a3), "r"(a0) : "d1", "memory");
    return d0;
}

static ULONG rtlib_rtGetStringA(struct Library *base, UBYTE *buffer, ULONG maxchars,
                                 STRPTR title, APTR reqinfo, APTR taglist)
{
    register struct Library *a6 __asm("a6") = base;
    register UBYTE *a1 __asm("a1") = buffer;
    register ULONG d0_in __asm("d0") = maxchars;
    register STRPTR a2 __asm("a2") = title;
    register APTR a3 __asm("a3") = reqinfo;
    register APTR a0 __asm("a0") = taglist;
    register ULONG d0 __asm("d0");
    __asm volatile ("jsr -72(%%a6)" : "=r"(d0) : "r"(a6), "r"(a1), "0"(d0_in), "r"(a2), "r"(a3), "r"(a0) : "d1", "memory");
    return d0;
}

static LONG rtlib_rtPaletteRequestA(struct Library *base, STRPTR title, APTR reqinfo, APTR taglist)
{
    register struct Library *a6 __asm("a6") = base;
    register STRPTR a2 __asm("a2") = title;
    register APTR a3 __asm("a3") = reqinfo;
    register APTR a0 __asm("a0") = taglist;
    register LONG d0 __asm("d0");
    __asm volatile ("jsr -102(%%a6)" : "=r"(d0) : "r"(a6), "r"(a2), "r"(a3), "r"(a0) : "a1", "d1", "memory");
    return d0;
}

static APTR rtlib_rtLockWindow(struct Library *base, APTR win)
{
    register struct Library *a6 __asm("a6") = base;
    register APTR a0 __asm("a0") = win;
    register APTR d0 __asm("d0");
    __asm volatile ("jsr -156(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0) : "a1", "d1", "memory");
    return d0;
}

static int test_reqtools_library(void)
{
    int errors = 0;
    struct Library *base;

    print("--- Test: reqtools.library ---\n");

    base = OpenLibrary((CONST_STRPTR)"reqtools.library", 0);
    if (base == NULL)
    {
        print("FAIL: OpenLibrary(\"reqtools.library\", 0) returned NULL\n");
        return 1;
    }
    print("OK: OpenLibrary(\"reqtools.library\") returned ");
    print_hex((ULONG)base);
    print("\n");

    /* Verify version */
    if (base->lib_Version != 38)
    {
        print("FAIL: lib_Version is ");
        print_num(base->lib_Version);
        print(" expected 38\n");
        errors++;
    }
    else
    {
        print("OK: lib_Version == 38\n");
    }

    /* rtAllocRequestA(0, NULL) should return NULL */
    {
        APTR result = rtlib_rtAllocRequestA(base, 0, NULL);
        if (result != NULL)
        {
            print("FAIL: rtAllocRequestA(0, NULL) returned non-NULL\n");
            errors++;
        }
        else
        {
            print("OK: rtAllocRequestA(0, NULL) returned NULL\n");
        }
    }

    /* rtEZRequestA should return 0 */
    {
        ULONG result = rtlib_rtEZRequestA(base, (STRPTR)"Test", (STRPTR)"OK", NULL, NULL, NULL);
        if (result != 0)
        {
            print("FAIL: rtEZRequestA() returned ");
            print_num((LONG)result);
            print(" expected 0\n");
            errors++;
        }
        else
        {
            print("OK: rtEZRequestA() returned 0\n");
        }
    }

    /* rtFileRequestA should return NULL */
    {
        APTR result = rtlib_rtFileRequestA(base, NULL, NULL, (STRPTR)"Select File", NULL);
        if (result != NULL)
        {
            print("FAIL: rtFileRequestA() returned non-NULL\n");
            errors++;
        }
        else
        {
            print("OK: rtFileRequestA() returned NULL\n");
        }
    }

    /* rtGetStringA should return 0 */
    {
        UBYTE buf[64];
        ULONG result = rtlib_rtGetStringA(base, buf, 64, (STRPTR)"Enter text", NULL, NULL);
        if (result != 0)
        {
            print("FAIL: rtGetStringA() returned ");
            print_num((LONG)result);
            print(" expected 0\n");
            errors++;
        }
        else
        {
            print("OK: rtGetStringA() returned 0\n");
        }
    }

    /* rtPaletteRequestA should return -1 */
    {
        LONG result = rtlib_rtPaletteRequestA(base, (STRPTR)"Select Color", NULL, NULL);
        if (result != -1)
        {
            print("FAIL: rtPaletteRequestA() returned ");
            print_num(result);
            print(" expected -1\n");
            errors++;
        }
        else
        {
            print("OK: rtPaletteRequestA() returned -1\n");
        }
    }

    /* rtLockWindow(NULL) should return NULL */
    {
        APTR result = rtlib_rtLockWindow(base, NULL);
        if (result != NULL)
        {
            print("FAIL: rtLockWindow(NULL) returned non-NULL\n");
            errors++;
        }
        else
        {
            print("OK: rtLockWindow(NULL) returned NULL\n");
        }
    }

    CloseLibrary(base);
    print("OK: CloseLibrary(reqtools.library) succeeded\n\n");
    return errors;
}

/* ========================================================================
 * arp.library tests
 * ======================================================================== */

/* ARP public functions start at -228. We test a representative sample. */

static LONG arplib_Printf(struct Library *base, STRPTR string, APTR stream)
{
    register struct Library *a6 __asm("a6") = base;
    register STRPTR a0 __asm("a0") = string;
    register APTR a1 __asm("a1") = stream;
    register LONG d0 __asm("d0");
    __asm volatile ("jsr -228(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0), "r"(a1) : "d1", "memory");
    return d0;
}

static STRPTR arplib_FileRequest(struct Library *base, APTR filerequester)
{
    register struct Library *a6 __asm("a6") = base;
    register APTR a0 __asm("a0") = filerequester;
    register STRPTR d0 __asm("d0");
    __asm volatile ("jsr -294(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0) : "a1", "d1", "memory");
    return d0;
}

static APTR arplib_DosAllocMem(struct Library *base, LONG size)
{
    register struct Library *a6 __asm("a6") = base;
    register LONG d0_in __asm("d0") = size;
    register APTR d0 __asm("d0");
    __asm volatile ("jsr -342(%%a6)" : "=r"(d0) : "r"(a6), "0"(d0_in) : "a0", "a1", "d1", "memory");
    return d0;
}

static BOOL arplib_PatternMatch(struct Library *base, STRPTR pattern, STRPTR string)
{
    register struct Library *a6 __asm("a6") = base;
    register STRPTR a0 __asm("a0") = pattern;
    register STRPTR a1 __asm("a1") = string;
    register BOOL d0 __asm("d0");
    __asm volatile ("jsr -432(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0), "r"(a1) : "d1", "memory");
    return d0;
}

static struct Library * arplib_ArpOpenLibrary(struct Library *base, STRPTR name, LONG vers)
{
    register struct Library *a6 __asm("a6") = base;
    register STRPTR a1 __asm("a1") = name;
    register LONG d0_in __asm("d0") = vers;
    register struct Library *d0 __asm("d0");
    __asm volatile ("jsr -654(%%a6)" : "=r"(d0) : "r"(a6), "r"(a1), "0"(d0_in) : "a0", "memory");
    return d0;
}

static APTR arplib_ArpAllocFreq(struct Library *base)
{
    register struct Library *a6 __asm("a6") = base;
    register APTR d0 __asm("d0");
    __asm volatile ("jsr -660(%%a6)" : "=r"(d0) : "r"(a6) : "a0", "a1", "d1", "memory");
    return d0;
}

static int test_arp_library(void)
{
    int errors = 0;
    struct Library *base;

    print("--- Test: arp.library ---\n");

    base = OpenLibrary((CONST_STRPTR)"arp.library", 0);
    if (base == NULL)
    {
        print("FAIL: OpenLibrary(\"arp.library\", 0) returned NULL\n");
        return 1;
    }
    print("OK: OpenLibrary(\"arp.library\") returned ");
    print_hex((ULONG)base);
    print("\n");

    /* Verify version */
    if (base->lib_Version != 40)
    {
        print("FAIL: lib_Version is ");
        print_num(base->lib_Version);
        print(" expected 40\n");
        errors++;
    }
    else
    {
        print("OK: lib_Version == 40\n");
    }

    /* Printf should return 0 */
    {
        LONG result = arplib_Printf(base, NULL, NULL);
        if (result != 0)
        {
            print("FAIL: Printf() returned ");
            print_num(result);
            print(" expected 0\n");
            errors++;
        }
        else
        {
            print("OK: Printf() returned 0\n");
        }
    }

    /* FileRequest(NULL) should return NULL */
    {
        STRPTR result = arplib_FileRequest(base, NULL);
        if (result != NULL)
        {
            print("FAIL: FileRequest(NULL) returned non-NULL\n");
            errors++;
        }
        else
        {
            print("OK: FileRequest(NULL) returned NULL\n");
        }
    }

    /* DosAllocMem(100) should return NULL */
    {
        APTR result = arplib_DosAllocMem(base, 100);
        if (result != NULL)
        {
            print("FAIL: DosAllocMem(100) returned non-NULL\n");
            errors++;
        }
        else
        {
            print("OK: DosAllocMem(100) returned NULL\n");
        }
    }

    /* PatternMatch should return FALSE */
    {
        BOOL result = arplib_PatternMatch(base, (STRPTR)"#?", (STRPTR)"test");
        if (result != FALSE)
        {
            print("FAIL: PatternMatch() returned ");
            print_num(result);
            print(" expected FALSE\n");
            errors++;
        }
        else
        {
            print("OK: PatternMatch() returned FALSE\n");
        }
    }

    /* ArpOpenLibrary should return NULL */
    {
        struct Library *result = arplib_ArpOpenLibrary(base, (STRPTR)"fake.library", 0);
        if (result != NULL)
        {
            print("FAIL: ArpOpenLibrary() returned non-NULL\n");
            errors++;
        }
        else
        {
            print("OK: ArpOpenLibrary() returned NULL\n");
        }
    }

    /* ArpAllocFreq should return NULL */
    {
        APTR result = arplib_ArpAllocFreq(base);
        if (result != NULL)
        {
            print("FAIL: ArpAllocFreq() returned non-NULL\n");
            errors++;
        }
        else
        {
            print("OK: ArpAllocFreq() returned NULL\n");
        }
    }

    CloseLibrary(base);
    print("OK: CloseLibrary(arp.library) succeeded\n\n");
    return errors;
}

/* ========================================================================
 * Additional tests: open/close reference counting
 * ======================================================================== */

static int test_refcounting(void)
{
    int errors = 0;
    struct Library *base;
    struct Library *base2;
    UWORD cnt_after_open;
    UWORD cnt_after_close;

    print("--- Test: Reference counting ---\n");

    /* Test with powerpacker.library as representative */
    base = OpenLibrary((CONST_STRPTR)"powerpacker.library", 0);
    if (base == NULL)
    {
        print("FAIL: OpenLibrary(\"powerpacker.library\") returned NULL\n");
        return 1;
    }
    cnt_after_open = base->lib_OpenCnt;

    /* Open again */
    base2 = OpenLibrary((CONST_STRPTR)"powerpacker.library", 0);
    if (base2 == NULL)
    {
        print("FAIL: Second OpenLibrary() returned NULL\n");
        CloseLibrary(base);
        return 1;
    }

    if (base2 != base)
    {
        print("FAIL: Second OpenLibrary() returned different base pointer\n");
        errors++;
    }
    else
    {
        print("OK: Second OpenLibrary() returned same base pointer\n");
    }

    if (base->lib_OpenCnt != cnt_after_open + 1)
    {
        print("FAIL: lib_OpenCnt after second open is ");
        print_num(base->lib_OpenCnt);
        print(" expected ");
        print_num(cnt_after_open + 1);
        print("\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt incremented correctly\n");
    }

    CloseLibrary(base2);
    cnt_after_close = base->lib_OpenCnt;
    if (cnt_after_close != cnt_after_open)
    {
        print("FAIL: lib_OpenCnt after close is ");
        print_num(cnt_after_close);
        print(" expected ");
        print_num(cnt_after_open);
        print("\n");
        errors++;
    }
    else
    {
        print("OK: lib_OpenCnt decremented correctly after CloseLibrary()\n");
    }

    CloseLibrary(base);
    print("OK: Reference counting test complete\n\n");
    return errors;
}

/* ========================================================================
 * Test: Libraries are NOT resident in ROM (they're disk libraries)
 * ======================================================================== */

static int test_not_in_rom(void)
{
    static const char *lib_names[] = {
        "req.library",
        "reqtools.library",
        "powerpacker.library",
        "arp.library"
    };
    int errors = 0;
    ULONG i;

    print("--- Test: Libraries not resident in ROM ---\n");

    for (i = 0; i < 4; i++)
    {
        struct Resident *resident = FindResident((CONST_STRPTR)lib_names[i]);
        struct Node *node;

        print("Checking ");
        print(lib_names[i]);
        print("... ");

        if (resident != NULL)
        {
            print("FAIL: found as ROM resident\n");
            errors++;
        }
        else
        {
            print("OK (not ROM resident)\n");
        }
    }

    print("\n");
    return errors;
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void)
{
    int errors = 0;

    print("=== exec/disklib_stubs Test ===\n\n");

    /* Test 1: Libraries are not resident in ROM */
    errors += test_not_in_rom();

    /* Test 2: req.library */
    errors += test_req_library();

    /* Test 3: powerpacker.library */
    errors += test_powerpacker_library();

    /* Test 4: reqtools.library */
    errors += test_reqtools_library();

    /* Test 5: arp.library */
    errors += test_arp_library();

    /* Test 6: Reference counting */
    errors += test_refcounting();

    /* ========== Final result ========== */
    print("=== Test Results ===\n");
    if (errors == 0)
    {
        print("PASS: All tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: ");
        print_num(errors);
        print(" errors\n");
        return 5;
    }
}
