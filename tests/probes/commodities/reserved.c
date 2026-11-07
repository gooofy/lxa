/*
 * Probe (Phase 251): commodities.library's reserved slot -108 (CxReserved0
 * in the NDK's SFD, between RemoveCxObj and SetTranslate).  What does 3.1
 * do when a program calls it?  The probe records the scratch registers it
 * returns, whether it shares its code with another reserved vector
 * (ExtFunc -24), the library's vector count, and whether broker state
 * changes.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <libraries/commodities.h>
#include <clib/exec_protos.h>
#include <clib/commodities_protos.h>
#include <inline/exec.h>
#include <inline/commodities.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *CxBase = NULL;

static ULONG g_regs[4];

static void call_reserved(void)
{
    register struct Library *a6 __asm("a6") = CxBase;
    register ULONG *a2 __asm("a2") = g_regs;

    __asm__ volatile(
        "move.l #0x11111111,d0\n\t"
        "move.l #0x22222222,d1\n\t"
        "move.l #0x33333333,a0\n\t"
        "move.l #0x44444444,a1\n\t"
        "jsr -108(a6)\n\t"
        "move.l d0,(a2)\n\t"
        "move.l d1,4(a2)\n\t"
        "move.l a0,8(a2)\n\t"
        "move.l a1,12(a2)\n"
        : : "r"(a6), "r"(a2) : "d0", "d1", "a0", "a1", "cc", "memory");
}

/* the jump target of a library vector (jmp abs.l) */
static ULONG vec_target(LONG lvo)
{
    UBYTE *v = (UBYTE *)CxBase + lvo;
    return (v[0] == 0x4e && v[1] == 0xf9) ? *(ULONG *)(v + 2) : 0;
}

int main(void)
{
    struct NewBroker nb;
    struct MsgPort *port;
    CxObj *broker;
    LONG err, i;

    CxBase = OpenLibrary((STRPTR)"commodities.library", 37);
    if (!CxBase)
        return 20;

    P_SECTION("library");
    P_LONG("vectors", CxBase->lib_NegSize / 6);
    P_BOOL("-108 is a jmp", vec_target(-108) != 0);
    P_BOOL("-108 shares code with -24 (ExtFunc)", vec_target(-108) == vec_target(-24));
    for (i = 186; i <= 198; i += 6)
        P_BOOL("-108 shares code with a private controller slot", vec_target(-108) == vec_target(-i));

    P_SECTION("call without brokers");
    call_reserved();
    P_HEX("d0", g_regs[0]);
    P_HEX("d1", g_regs[1]);
    P_HEX("a0", g_regs[2]);
    P_HEX("a1", g_regs[3]);

    port = CreateMsgPort();
    nb.nb_Version = NB_VERSION;
    nb.nb_Name = (STRPTR)"ProbeReserved";
    nb.nb_Title = (STRPTR)"Probe reserved";
    nb.nb_Descr = (STRPTR)"CxReserved0";
    nb.nb_Unique = NBU_UNIQUE;
    nb.nb_Flags = 0;
    nb.nb_Pri = 0;
    nb.nb_Port = port;
    nb.nb_ReservedChannel = 0;
    broker = CxBroker(&nb, &err);
    P_SECTION("call with a broker");
    P_NULL("broker", broker);
    call_reserved();
    P_HEX("d0", g_regs[0]);
    P_HEX("d1", g_regs[1]);
    P_HEX("a0", g_regs[2]);
    P_HEX("a1", g_regs[3]);
    P_LONG("CxObjError", CxObjError(broker));
    P_LONG("ActivateCxObj (was inactive)", ActivateCxObj(broker, TRUE));
    P_LONG("ActivateCxObj (was active)", ActivateCxObj(broker, FALSE));
    P_LONG("CxObjType", CxObjType(broker));
    DeleteCxObjAll(broker);
    DeleteMsgPort(port);
    CloseLibrary(CxBase);
    return 0;
}
