/*
 * Probe (Phase 237): the exception vector table as programs see it.
 *
 * Old programs read address 0 and the 68000 vector table directly:
 *  - Manx C stack checks compare sp with the longword at address 0
 *    (`cmpa.l 0.w,sp; bcc ok; jmp 0`), so it must read 0 (Fish FontEdit,
 *    Cycles);
 *  - programs that dereference a NULL library base read the vector
 *    table, which AmigaOS fills completely.
 * Only classes are printed (0 / ROM / RAM), never addresses.
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static const char *cls(ULONG v)
{
    if (v == 0)
        return "0";
    if (v >= 0x00f80000 && v <= 0x00ffffff)
        return "rom";
    return "ram";
}

/* through a volatile pointer variable: GCC turns a visible NULL
 * dereference into a trap */
static volatile ULONG *volatile lowmem_base;

int main(void)
{
    volatile ULONG *lm = lowmem_base;
    int i;

    P_SECTION("address 0 and 4");
    P_HEX("(0)", lm[0]);
    P_BOOL("(4) == SysBase", lm[1] == (ULONG)SysBase);

    /* vectors 2 (bus error) .. 63; the trap #15 vector is not printed:
     * lxa uses it for its emulator calls */
    P_SECTION("vector table");
    for (i = 2; i < 64; i++) {
        if (i == 47)
            continue;
        probe_s("vector ");
        probe_dec(i);
        probe_s(" = ");
        probe_s(cls(lm[i]));
        probe_ch('\n');
    }

    /* the user vectors 64..255 (Phase 237b: Fish Window calls through
     * the longword at $100), as runs of equal class */
    P_SECTION("user vectors");
    {
        int start = 64;
        for (i = 65; i <= 256; i++) {
            if (i == 256 || cls(lm[i]) != cls(lm[start])) {
                probe_s("vectors ");
                probe_dec(start);
                probe_s("..");
                probe_dec(i - 1);
                probe_s(" = ");
                probe_s(cls(lm[start]));
                probe_ch('\n');
                start = i;
            }
        }
    }
    return 0;
}
