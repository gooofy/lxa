/*
 * Test: exec/null_safety (lxa only, Phase 220)
 *
 * lxa tolerates invalid input that real AmigaOS 3.1 does not survive
 * (NULL semaphores, Cause(NULL), ParseDate() with a NULL DateStamp, which
 * the V38-40 locale.library writes through).  These checks were moved out
 * of the reference-validated programs (Tests/Exec/Sync, Interrupts, Locale)
 * so lxa keeps its tolerance under test; they cannot run on the reference
 * (tests/ref_suite.yaml).
 */

#include <exec/types.h>
#include <exec/semaphores.h>
#include <exec/interrupts.h>
#include <utility/hooks.h>
#include <libraries/locale.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/locale_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/locale.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct LocaleBase *LocaleBase;

static LONG failures;

static void print(const char *s)
{
    LONG len = 0;
    while (s[len])
        len++;
    Write(Output(), (CONST APTR)s, len);
}

static void check(BOOL ok, const char *name)
{
    print(ok ? "  OK: " : "  FAIL: ");
    print(name);
    print("\n");
    if (!ok)
        failures++;
}

struct StrSource
{
    const char *str;
    LONG pos;
};

static ULONG get_char(register struct Hook *hook __asm("a0"),
                      register APTR locale __asm("a2"),
                      register APTR unused __asm("a1"))
{
    struct StrSource *src = (struct StrSource *)hook->h_Data;
    char c = src->str[src->pos];

    (void)locale;
    (void)unused;
    if (c)
        src->pos++;
    return (ULONG)(UBYTE)c;
}

int main(void)
{
    print("NULL-safety tests (lxa only)\n");

    InitSemaphore(NULL);
    check(TRUE, "InitSemaphore(NULL) did not crash");
    ObtainSemaphore(NULL);
    check(TRUE, "ObtainSemaphore(NULL) did not crash");
    ReleaseSemaphore(NULL);
    check(TRUE, "ReleaseSemaphore(NULL) did not crash");
    ObtainSemaphoreShared(NULL);
    check(TRUE, "ObtainSemaphoreShared(NULL) did not crash");
    check(AttemptSemaphoreShared(NULL) == 0, "AttemptSemaphoreShared(NULL) returns FALSE");
    check(AttemptSemaphore(NULL) == 0, "AttemptSemaphore(NULL) returns FALSE");

    Cause(NULL);
    check(TRUE, "Cause(NULL) did not crash");

    LocaleBase = (struct LocaleBase *)OpenLibrary((STRPTR)"locale.library", 38);
    if (LocaleBase)
    {
        struct Locale *locale = OpenLocale(NULL);
        struct StrSource src = { "2020-01-01", 0 };
        struct Hook hook;

        hook.h_Entry = (ULONG (*)())get_char;
        hook.h_SubEntry = NULL;
        hook.h_Data = &src;
        check(ParseDate(locale, NULL, (STRPTR)"%Y-%m-%d", &hook) != FALSE,
              "ParseDate with NULL date validates only");
        CloseLocale(locale);
        CloseLibrary((struct Library *)LocaleBase);
    }
    else
    {
        check(FALSE, "OpenLibrary locale.library");
    }

    if (failures)
    {
        print("FAIL: NULL-safety tests failed\n");
        return 20;
    }
    print("PASS: NULL-safety tests passed\n");
    return 0;
}
