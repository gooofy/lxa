/*
 * Probe (Phase 237): SuperState()/UserState().  SuperState() enters
 * supervisor mode on the caller's stack and returns the old system stack,
 * UserState() goes back.  Fish ILBM_Killer reads the VBR in between.
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static UWORD read_sr(void)
{
    UWORD sr;
    __asm__ __volatile__ ("move.w sr,%0" : "=d" (sr));   /* privileged on 68010+ */
    return sr;
}

static ULONG read_vbr(void)
{
    register ULONG v __asm("d0");
    __asm__ __volatile__ (".short 0x4e7a,0x0801" : "=r" (v));   /* movec vbr,d0 */
    return v;
}

int main(void)
{
    APTR ssp;
    UWORD sr;
    ULONG vbr;

    P_SECTION("SuperState/UserState");
    ssp = SuperState();
    sr = read_sr();
    vbr = read_vbr();
    UserState(ssp);
    (void)vbr;
    P_NULL("SuperState() result", ssp);
    P_BOOL("supervisor bit set after SuperState", sr & 0x2000);
    P_BOOL("continued after UserState", TRUE);
    return 0;
}
