/*
 * Probe (Phase 222a): utility.library integer math - SMult32, UMult32,
 * SMult64, UMult64, SDivMod32, UDivMod32 (quotient and remainder).
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *UtilityBase;

/* d0/d1 results are read directly from the registers */
static void call2(WORD lvo, ULONG a, ULONG b, ULONG *r0, ULONG *r1)
{
    register ULONG d0 __asm("d0") = a;
    register ULONG d1 __asm("d1") = b;
    register struct Library *a6 __asm("a6") = UtilityBase;
    register LONG off __asm("a0") = lvo;
    __asm volatile("jsr 0(a6,a0.l)"
                   : "+r"(d0), "+r"(d1), "+r"(off)
                   : "r"(a6)
                   : "a1", "cc", "memory");
    *r0 = d0;
    *r1 = d1;
}

#define LVO_SMult32 (-138)
#define LVO_UMult32 (-144)
#define LVO_SDivMod32 (-150)
#define LVO_UDivMod32 (-156)
#define LVO_SMult64 (-198)
#define LVO_UMult64 (-204)

static void show(const char *name, WORD lvo, ULONG a, ULONG b, int both)
{
    ULONG r0, r1;
    call2(lvo, a, b, &r0, &r1);
    probe_s(name);
    probe_ch('(');
    probe_hex(a, 8);
    probe_s(", ");
    probe_hex(b, 8);
    probe_s(") = ");
    probe_hex(r0, 8);
    if (both) {
        probe_s(" d1 ");
        probe_hex(r1, 8);
    }
    probe_ch('\n');
}

static const ULONG vals[][2] = {
    {0, 0}, {1, 1}, {7, 3}, {0xfffffff9, 3}, {7, 0xfffffffd}, {0xfffffff9, 0xfffffffd},
    {0x7fffffff, 2}, {0x80000000, 0xffffffff}, {0x80000000, 1}, {0xffffffff, 0xffffffff},
    {0x10000, 0x10000}, {0x12345678, 0x9abcdef0}, {100, 0xffff}, {0xfffe0001, 0xffff},
    {123456789, 10}, {1, 0x80000000},
};

int main(void)
{
    int i;
    UtilityBase = OpenLibrary((STRPTR)"utility.library", 39);
    if (!UtilityBase)
        return 20;

    P_SECTION("32-bit multiply (d0)");
    for (i = 0; i < (int)(sizeof(vals) / sizeof(vals[0])); i++) {
        show("SMult32", LVO_SMult32, vals[i][0], vals[i][1], 0);
        show("UMult32", LVO_UMult32, vals[i][0], vals[i][1], 0);
    }

    P_SECTION("64-bit multiply (d0 = high, d1 = low)");
    for (i = 0; i < (int)(sizeof(vals) / sizeof(vals[0])); i++) {
        show("SMult64", LVO_SMult64, vals[i][0], vals[i][1], 1);
        show("UMult64", LVO_UMult64, vals[i][0], vals[i][1], 1);
    }

    P_SECTION("division (d0 = quotient, d1 = remainder)");
    for (i = 0; i < (int)(sizeof(vals) / sizeof(vals[0])); i++) {
        if (vals[i][1] == 0)
            continue;
        if (vals[i][0] == 0x80000000 && vals[i][1] == 0xffffffff)
            continue;               /* overflow: undefined */
        show("SDivMod32", LVO_SDivMod32, vals[i][0], vals[i][1], 1);
        show("UDivMod32", LVO_UDivMod32, vals[i][0], vals[i][1], 1);
    }

    CloseLibrary(UtilityBase);
    return 0;
}
