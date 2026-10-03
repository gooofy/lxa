/*
 * Probe (Phase 222a): exec.library signals - AllocSignal, FreeSignal,
 * SetSignal, Signal, Wait, and the task's tc_SigAlloc/tc_SigRecvd.
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

int main(void)
{
    struct Task *me = FindTask(NULL);
    ULONG base = me->tc_SigAlloc;
    BYTE s[20];
    int i;

    P_SECTION("allocation");
    P_HEX("tc_SigAlloc low 16 bits (system)", base & 0xffff);
    P_HEX("SysBase->TaskSigAlloc low 16 bits", SysBase->TaskSigAlloc & 0xffff);
    s[0] = AllocSignal(-1);
    s[1] = AllocSignal(-1);
    s[2] = AllocSignal(-1);
    P_LONG("AllocSignal(-1) #1", s[0]);
    P_LONG("AllocSignal(-1) #2", s[1]);
    P_LONG("AllocSignal(-1) #3", s[2]);
    P_HEX("tc_SigAlloc gained", me->tc_SigAlloc & ~base);
    P_LONG("AllocSignal(same) again", AllocSignal(s[0]));
    P_LONG("AllocSignal(4) (system bit)", AllocSignal(4));
    P_LONG("AllocSignal(SIGBREAKB_CTRL_C)", AllocSignal(SIGBREAKB_CTRL_C));
    FreeSignal(s[1]);
    P_HEX("after FreeSignal #2, gained", me->tc_SigAlloc & ~base);
    P_LONG("AllocSignal(-1) reuses", AllocSignal(-1));
    FreeSignal(s[1]);
    P_LONG("AllocSignal(20) specific", AllocSignal(20));
    P_LONG("AllocSignal(20) twice", AllocSignal(20));
    FreeSignal(20);
    FreeSignal(-1);
    P_LONG("FreeSignal(-1) survived", 1);
    for (i = 3; i < 20; i++) {
        s[i] = AllocSignal(-1);
        if (s[i] == -1)
            break;
    }
    P_LONG("allocations until exhausted (incl. 3 above)", i);
    P_HEX("tc_SigAlloc when full, gained", me->tc_SigAlloc & ~base);
    while (--i >= 0)
        FreeSignal(s[i]);
    P_HEX("all freed, gained", me->tc_SigAlloc & ~base);

    P_SECTION("SetSignal/Signal/Wait");
    s[0] = AllocSignal(-1);
    s[1] = AllocSignal(-1);
    SetSignal(0, 0xffff0000UL);
    P_HEX("SetSignal(0,0) after clear (user bits)", SetSignal(0, 0) & 0xffff0000UL);
    P_HEX("SetSignal(set bit #1) old", SetSignal(1UL << s[0], 1UL << s[0]) & 0xffff0000UL);
    P_HEX("SetSignal(0,0) now", SetSignal(0, 0) & 0xffff0000UL);
    P_HEX("tc_SigRecvd", me->tc_SigRecvd & 0xffff0000UL);
    Signal(me, 1UL << s[1]);
    P_HEX("after Signal(self, #2)", SetSignal(0, 0) & 0xffff0000UL);
    P_HEX("Wait(#1|#2) result", Wait((1UL << s[0]) | (1UL << s[1])) & 0xffff0000UL);
    P_HEX("after Wait", SetSignal(0, 0) & 0xffff0000UL);
    Signal(me, (1UL << s[0]) | (1UL << s[1]));
    P_HEX("Wait(#1) result with both pending", Wait(1UL << s[0]) & 0xffff0000UL);
    P_HEX("still pending", SetSignal(0, 0) & 0xffff0000UL);
    P_HEX("SetSignal(0, #2) old", SetSignal(0, 1UL << s[1]) & 0xffff0000UL);
    P_HEX("cleared", SetSignal(0, 0) & 0xffff0000UL);
    Signal(me, SIGBREAKF_CTRL_D);
    P_HEX("Signal CTRL_D -> SetSignal(0,0) & 0xf000", SetSignal(0, 0) & 0xf000);
    P_HEX("Wait(CTRL_D)", Wait(SIGBREAKF_CTRL_D));
    /* signalling an unallocated bit still sets it */
    SetSignal(0, 1UL << 27);
    if (!(me->tc_SigAlloc & (1UL << 27))) {
        Signal(me, 1UL << 27);
        P_HEX("Signal(unallocated bit 27) recorded", SetSignal(0, 1UL << 27) & (1UL << 27));
    }
    FreeSignal(s[0]);
    FreeSignal(s[1]);
    P_HEX("final gained", me->tc_SigAlloc & ~base);
    return 0;
}
