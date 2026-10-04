/*
 * Probe (Phase 238): the system memory list of the reference machine
 * (A4000 with 2 MB chip and Zorro III fast RAM) - the regions' attributes,
 * priorities and order, which region MEMF_ANY allocations come from, and
 * whether fast memory exists at all.  Sizes and addresses are not printed
 * (lxa's fast region is smaller).  AmiBlitz3 refuses its online help and
 * intellisense ("The available memory is low") when AvailMem(MEMF_FAST)
 * is 0.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

int main(void)
{
    struct MemHeader *mh;
    APTR p;
    int n = 0;

    P_SECTION("MemList");
    Forbid();
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ && n < 8;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ, n++) {
        static ULONG attr[8];
        static LONG pri[8];
        static ULONG lower[8];
        attr[n] = mh->mh_Attributes;
        pri[n] = mh->mh_Node.ln_Pri;
        lower[n] = (ULONG)mh->mh_Lower;
        if (mh->mh_Node.ln_Succ->ln_Succ == NULL || n == 7) {
            int i;
            Permit();
            for (i = 0; i <= n; i++) {
                probe_s("region ");
                probe_dec(i);
                probe_s(": attributes ");
                probe_hex(attr[i], 4);
                probe_s(" pri ");
                probe_dec(pri[i]);
                probe_s(lower[i] < 0x200000 ? " (below 2 MB)" : " (above 2 MB)");
                probe_ch('\n');
            }
            Forbid();
        }
    }
    Permit();
    P_LONG("regions", n);

    P_SECTION("where allocations come from");
    p = AllocMem(4096, MEMF_ANY);
    P_HEX("TypeOfMem(AllocMem(MEMF_ANY))", TypeOfMem(p));
    FreeMem(p, 4096);
    p = AllocMem(4096, MEMF_PUBLIC);
    P_HEX("TypeOfMem(AllocMem(MEMF_PUBLIC))", TypeOfMem(p));
    FreeMem(p, 4096);
    p = AllocMem(4096, MEMF_CHIP);
    P_HEX("TypeOfMem(AllocMem(MEMF_CHIP))", TypeOfMem(p));
    FreeMem(p, 4096);
    p = AllocMem(4096, MEMF_FAST);
    P_NULL("AllocMem(MEMF_FAST)", p);
    if (p) {
        P_HEX("TypeOfMem(AllocMem(MEMF_FAST))", TypeOfMem(p));
        FreeMem(p, 4096);
    }

    P_SECTION("AvailMem");
    P_BOOL("AvailMem(MEMF_FAST) > 1 MB", AvailMem(MEMF_FAST) > 0x100000);
    P_BOOL("AvailMem(MEMF_CHIP) < 2 MB", AvailMem(MEMF_CHIP) < 0x200000);
    P_BOOL("AvailMem(MEMF_CHIP) > 1 MB", AvailMem(MEMF_CHIP) > 0x100000);
    P_BOOL("AvailMem(MEMF_TOTAL|MEMF_CHIP) == 2 MB", AvailMem(MEMF_TOTAL | MEMF_CHIP) == 0x200000);
    P_HEX("MaxLocMem", SysBase->MaxLocMem);
    return 0;
}
