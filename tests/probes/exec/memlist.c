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

    P_SECTION("MemList (lxa has one fast region, the reference two)");
    {
        ULONG chip_attr = 0, first_attr = 0;
        LONG chip_pri = 999, min_fast_pri = 999, first_pri = 0;
        Forbid();
        for (mh = (struct MemHeader *)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ;
             mh = (struct MemHeader *)mh->mh_Node.ln_Succ, n++) {
            if (n == 0) {
                first_attr = mh->mh_Attributes;
                first_pri = mh->mh_Node.ln_Pri;
            }
            if (mh->mh_Attributes & MEMF_CHIP) {
                chip_attr = mh->mh_Attributes;
                chip_pri = mh->mh_Node.ln_Pri;
            } else if (mh->mh_Node.ln_Pri < min_fast_pri)
                min_fast_pri = mh->mh_Node.ln_Pri;
        }
        Permit();
        P_HEX("first region attributes", first_attr);
        P_LONG("first region pri", first_pri);
        P_HEX("chip region attributes", chip_attr);
        P_LONG("chip region pri", chip_pri);
        P_BOOL("fast regions before chip", min_fast_pri != 999 && min_fast_pri > chip_pri);
    }

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
