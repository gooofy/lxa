/*
 * Probe (Phase 237): SuperState()/UserState().  SuperState() enters
 * supervisor mode on the caller's stack and returns the old system stack,
 * UserState() goes back.  Fish ILBM_Killer reads the VBR in between;
 * where it points is printed as a class.
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
    P_NULL("SuperState() result", ssp);
    P_BOOL("supervisor bit set after SuperState", sr & 0x2000);
    P_BOOL("continued after UserState", TRUE);
    /* Phase 237b: where the exception vectors live - programs write through
     * NULL pointers into low memory (Fish 60or80 sets a window title at
     * NULL+32, i.e. the privilege violation vector at VBR 0) */
    probe_s("VBR ");
    probe_s(vbr == 0 ? "0" : (vbr < 0x00200000 ? "chip RAM" : (vbr >= 0x00f80000 && vbr < 0x01000000 ? "ROM" : "fast RAM")));
    probe_ch('\n');
    return 0;
}
