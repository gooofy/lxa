/*
 * Probe (Phase 238): exec.library AddMemHandler/RemMemHandler - when an
 * AllocMem() fails, the low-memory handlers are called (A0 = MemHandlerData,
 * A1 = is_Data, A6 = ExecBase) in priority order: request size and flags,
 * MEMHF_RECYCLE after MEM_TRY_AGAIN, MEM_ALL_DONE / MEM_DID_NOTHING,
 * MEMF_NO_EXPUNGE, and no calls after RemMemHandler().
 * (Scout's identify.library adds a handler at init: lxa's uninitialised
 * ex_MemHandlers list corrupted exec's library list.)
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

#define HUGE_SIZE 0x7f000000UL      /* never satisfiable */

struct Rec
{
    char tag;               /* handler name */
    LONG calls;
    LONG ret_first;         /* result of the first call */
    LONG ret_next;          /* result of later calls */
    char log[64];           /* call order: tag + 'r' if MEMHF_RECYCLE */
    LONG loglen;
    ULONG size;
    ULONG flags;
};

static struct Rec *shared_log;      /* both handlers append their tag here */

static LONG handler(register struct MemHandlerData *mhd __asm("a0"),
                    register struct Rec *r __asm("a1"))
{
    struct Rec *g = shared_log;

    r->calls++;
    r->size = mhd->memh_RequestSize;
    r->flags = mhd->memh_RequestFlags;
    if (g->loglen < 60) {
        g->log[g->loglen++] = r->tag;
        if (mhd->memh_Flags & MEMHF_RECYCLE)
            g->log[g->loglen++] = 'r';
    }
    return r->calls == 1 ? r->ret_first : r->ret_next;
}

static struct Rec ra, rb, logrec;
static struct Interrupt ia, ib;

static void reset(LONG a1, LONG an, LONG b1, LONG bn)
{
    ra.calls = rb.calls = 0;
    ra.ret_first = a1; ra.ret_next = an;
    rb.ret_first = b1; rb.ret_next = bn;
    ra.size = rb.size = 0;
    ra.flags = rb.flags = 0;
    logrec.loglen = 0;
}

static void report(const char *label, APTR mem)
{
    logrec.log[logrec.loglen] = 0;
    probe_s(label);
    probe_s(": mem=");
    probe_s(mem ? "non-NULL" : "NULL");
    probe_s(" A.calls=");
    probe_dec(ra.calls);
    probe_s(" B.calls=");
    probe_dec(rb.calls);
    probe_s(" order=");
    probe_s(logrec.loglen ? logrec.log : "-");
    probe_ch('\n');
}

int main(void)
{
    APTR m;

    ra.tag = 'A';
    rb.tag = 'B';
    shared_log = &logrec;

    ia.is_Node.ln_Type = NT_INTERRUPT;
    ia.is_Node.ln_Pri = 120;            /* before any system handler */
    ia.is_Node.ln_Name = (char *)"probe A";
    ia.is_Data = &ra;
    ia.is_Code = (VOID (*)())handler;

    ib.is_Node.ln_Type = NT_INTERRUPT;
    ib.is_Node.ln_Pri = 110;
    ib.is_Node.ln_Name = (char *)"probe B";
    ib.is_Data = &rb;
    ib.is_Code = (VOID (*)())handler;

    P_SECTION("AddMemHandler (B first, then A with a higher priority)");
    AddMemHandler(&ib);
    AddMemHandler(&ia);
    P_LONG("ex_MemHandlers head is A", SysBase->ex_MemHandlers.mlh_Head == (struct MinNode *)&ia);

    P_SECTION("AllocMem failure calls the handlers");
    reset(MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING);
    m = AllocMem(HUGE_SIZE, MEMF_PUBLIC);
    report("did nothing", m);
    P_HEX("A memh_RequestSize", ra.size);
    P_HEX("A memh_RequestFlags", ra.flags);

    reset(MEM_TRY_AGAIN, MEM_TRY_AGAIN, MEM_DID_NOTHING, MEM_DID_NOTHING);
    ra.ret_next = MEM_DID_NOTHING;
    m = AllocMem(HUGE_SIZE, MEMF_PUBLIC | MEMF_CLEAR);
    report("A try again once", m);
    P_HEX("A memh_RequestFlags", ra.flags);

    reset(MEM_TRY_AGAIN, MEM_TRY_AGAIN, MEM_DID_NOTHING, MEM_DID_NOTHING);
    ra.ret_next = MEM_ALL_DONE;
    m = AllocMem(HUGE_SIZE, MEMF_PUBLIC);
    report("A try again, then all done", m);

    reset(MEM_ALL_DONE, MEM_ALL_DONE, MEM_ALL_DONE, MEM_ALL_DONE);
    m = AllocMem(HUGE_SIZE, MEMF_PUBLIC);
    report("all done", m);

    reset(MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING);
    m = AllocMem(HUGE_SIZE, MEMF_PUBLIC | MEMF_NO_EXPUNGE);
    report("MEMF_NO_EXPUNGE", m);

    reset(MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING);
    m = AllocMem(64, MEMF_PUBLIC);
    report("successful AllocMem", m);
    if (m)
        FreeMem(m, 64);

    reset(MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING);
    m = AllocVec(HUGE_SIZE, MEMF_ANY);
    report("AllocVec failure", m);
    P_HEX("A memh_RequestSize", ra.size);

    P_SECTION("RemMemHandler");
    RemMemHandler(&ia);
    reset(MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING);
    m = AllocMem(HUGE_SIZE, MEMF_PUBLIC);
    report("A removed", m);
    RemMemHandler(&ib);
    reset(MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING, MEM_DID_NOTHING);
    m = AllocMem(HUGE_SIZE, MEMF_PUBLIC);
    report("both removed", m);

    return 0;
}
