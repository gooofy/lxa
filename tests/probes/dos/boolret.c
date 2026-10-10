/*
 * Probe (Phase 244): the full 32-bit D0 of dos.library functions that
 * return a BOOL.  AmiBlitz3 tests the whole register after
 * MatchPatternNoCase(); lxa left garbage in the upper word of a "no match"
 * and listed StormWizard_App.wizard as a template.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

#define RAW(fn, lvo) \
static ULONG fn(ULONG a, ULONG b) \
{ \
    register ULONG d0 __asm("d0") = 0xDEADBEEF; \
    register ULONG d1 __asm("d1") = a; \
    register ULONG d2 __asm("d2") = b; \
    register struct DosLibrary *a6 __asm("a6") = DOSBase; \
    __asm volatile ("jsr " #lvo "(a6)" : "+r"(d0), "+r"(d1), "+r"(d2) : "r"(a6) \
                    : "a0", "a1", "cc", "memory"); \
    return d0; \
}

RAW(raw_matchnocase, -972)
RAW(raw_match, -846)

static char tok[128];

static void match(const char *pat, const char *str)
{
    ParsePatternNoCase((STRPTR)pat, (STRPTR)tok, sizeof(tok));
    probe_s("MatchPatternNoCase(\"");
    probe_s(pat);
    probe_s("\", \"");
    probe_s(str);
    probe_s("\") = ");
    probe_hex(raw_matchnocase( (ULONG)tok, (ULONG)str), 8);
    probe_ch('\n');
    ParsePattern((STRPTR)pat, (STRPTR)tok, sizeof(tok));
    probe_s("MatchPattern(\"");
    probe_s(pat);
    probe_s("\", \"");
    probe_s(str);
    probe_s("\") = ");
    probe_hex(raw_match( (ULONG)tok, (ULONG)str), 8);
    probe_ch('\n');
}

int main(void)
{
    match("#?.(bb2|ab2|ab3|asc)", "StormWizard_App.wizard");
    match("#?.(bb2|ab2|ab3|asc)", "StormWizard_App.ab3");
    match("#?.(bb2|ab2|ab3|asc)", "x.AB3");
    match("a#?", "abc");
    match("a#?", "xbc");
    return 0;
}
