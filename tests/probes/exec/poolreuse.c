/*
 * Probe (Phase 238): FreePooled() returns memory to its pool - many
 * AllocPooled()/FreePooled() cycles do not grow the pool, a puddle that
 * becomes empty goes back to the system, large blocks (above the
 * threshold) are freed at once, CreatePool() with threshSize > puddleSize
 * fails.  (lxa's FreePooled() was a no-op; AmiBlitz3 cycles >100000
 * pooled blocks at startup.)
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
    APTR pool, p[16];
    ULONG before, after;
    LONG i, k;

    P_SECTION("CreatePool");
    pool = CreatePool(MEMF_ANY, 1024, 2048);
    P_NULL("CreatePool(thresh 2048 > puddle 1024)", pool);
    if (pool)
        DeletePool(pool);

    pool = CreatePool(MEMF_ANY | MEMF_CLEAR, 4096, 1024);
    P_NULL("CreatePool(4096, 1024)", pool);
    if (!pool)
        return 20;

    P_SECTION("alloc/free cycles");
    p[0] = AllocPooled(pool, 64);           /* keep one puddle alive */
    before = AvailMem(MEMF_ANY);
    for (i = 0; i < 500; i++) {
        for (k = 1; k < 16; k++)
            p[k] = AllocPooled(pool, 16 + k * 8);
        for (k = 1; k < 16; k++)
            FreePooled(pool, p[k], 16 + k * 8);
    }
    after = AvailMem(MEMF_ANY);
    P_BOOL("AvailMem unchanged after 7500 pooled allocations", before == after);

    P_SECTION("large blocks");
    before = AvailMem(MEMF_ANY);
    p[1] = AllocPooled(pool, 100000);
    P_NULL("AllocPooled(100000)", p[1]);
    P_BOOL("AvailMem dropped by about 100000", before - AvailMem(MEMF_ANY) >= 100000 &&
                                                before - AvailMem(MEMF_ANY) < 100100);
    FreePooled(pool, p[1], 100000);
    P_BOOL("AvailMem restored after FreePooled", AvailMem(MEMF_ANY) == before);

    P_SECTION("empty puddle returns to the system");
    FreePooled(pool, p[0], 64);
    after = AvailMem(MEMF_ANY);
    P_BOOL("AvailMem grew by the puddle", after > before && after - before >= 4096);

    P_SECTION("MEMF_CLEAR pool clears reused memory");
    p[2] = AllocPooled(pool, 200);
    for (k = 0; k < 200; k++)
        ((UBYTE *)p[2])[k] = 0x5a;
    FreePooled(pool, p[2], 200);
    p[3] = AllocPooled(pool, 200);
    {
        LONG sum = 0;
        for (k = 0; k < 200; k++)
            sum += ((UBYTE *)p[3])[k];
        P_LONG("byte sum of a reused block", sum);
    }
    DeletePool(pool);
    return 0;
}
