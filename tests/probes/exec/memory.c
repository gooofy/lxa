/*
 * Probe (Phase 222a): exec.library memory - Allocate/Deallocate on a
 * private MemHeader (offsets, mh_Free, chunk list), AllocMem/AllocVec
 * edge cases, MEMF_CLEAR, AllocEntry/FreeEntry, pools, CopyMem,
 * CopyMemQuick, InitStruct.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static struct MemHeader mh;
static UBYTE *base;

static void chunks(const char *label)
{
    struct MemChunk *c;
    int n = 0;
    probe_s(label);
    probe_s(": free ");
    probe_dec((LONG)mh.mh_Free);
    probe_s(" chunks");
    for (c = mh.mh_First; c; c = c->mc_Next) {
        probe_s(" @");
        probe_dec((LONG)((UBYTE *)c - base));
        probe_ch('+');
        probe_dec((LONG)c->mc_Bytes);
        if (++n > 20)
            break;
    }
    probe_ch('\n');
}

static LONG off(APTR p)
{
    return p ? (LONG)((UBYTE *)p - base) : -1;
}

int main(void)
{
    UBYTE *raw;
    APTR a, b, c, d, e;
    int i;

    P_SECTION("Allocate/Deallocate (private MemHeader, 1024 bytes)");
    raw = AllocMem(1024 + 16, MEMF_PUBLIC);
    if (!raw)
        return 20;
    base = (UBYTE *)(((ULONG)raw + 7) & ~7UL);
    mh.mh_Node.ln_Type = NT_MEMORY;
    mh.mh_Node.ln_Name = (char *)"probe";
    mh.mh_Attributes = MEMF_PUBLIC;
    mh.mh_First = (struct MemChunk *)base;
    mh.mh_First->mc_Next = NULL;
    mh.mh_First->mc_Bytes = 1024;
    mh.mh_Lower = base;
    mh.mh_Upper = base + 1024;
    mh.mh_Free = 1024;
    chunks("initial");
    a = Allocate(&mh, 10);
    P_LONG("Allocate(10) offset", off(a));
    chunks("after 10");
    b = Allocate(&mh, 8);
    P_LONG("Allocate(8) offset", off(b));
    c = Allocate(&mh, 1);
    P_LONG("Allocate(1) offset", off(c));
    d = Allocate(&mh, 100);
    P_LONG("Allocate(100) offset", off(d));
    chunks("after 4 allocations");
    P_LONG("Allocate(0) offset", off(Allocate(&mh, 0)));
    P_LONG("Allocate(2000) offset", off(Allocate(&mh, 2000)));
    P_LONG("Allocate(1024) offset", off(Allocate(&mh, 1024)));
    chunks("after failed allocations");
    Deallocate(&mh, b, 8);
    chunks("Deallocate b");
    Deallocate(&mh, a, 10);
    chunks("Deallocate a (merges with b)");
    e = Allocate(&mh, 24);
    P_LONG("Allocate(24) offset", off(e));
    chunks("after Allocate(24)");
    a = Allocate(&mh, 16);
    P_LONG("Allocate(16) offset (first fit)", off(a));
    chunks("after Allocate(16)");
    Deallocate(&mh, c, 1);
    Deallocate(&mh, d, 100);
    chunks("Deallocate c, d");
    Deallocate(&mh, a, 16);
    chunks("Deallocate last");
    Deallocate(&mh, e, 24);
    chunks("Deallocate the 24 bytes");
    a = Allocate(&mh, 1024);
    P_LONG("Allocate(1024) whole", off(a));
    chunks("full");
    P_LONG("Allocate(8) from full", off(Allocate(&mh, 8)));
    Deallocate(&mh, base + 64, 64);
    Deallocate(&mh, base + 256, 32);
    Deallocate(&mh, base + 128, 64);
    chunks("Deallocate 64@64, 32@256, 64@128 (out of order)");
    Deallocate(&mh, base + 192, 64);
    chunks("Deallocate 64@192 (bridges)");
    FreeMem(raw, 1024 + 16);

    P_SECTION("AllocMem/AllocVec");
    P_NULL("AllocMem(0)", (a = AllocMem(0, MEMF_ANY)));
    if (a)
        FreeMem(a, 0);
    P_NULL("AllocVec(0)", (a = AllocVec(0, MEMF_ANY)));
    if (a)
        FreeVec(a);
    P_NULL("AllocMem(0x7ffffff0)", AllocMem(0x7ffffff0, MEMF_ANY));
    P_NULL("AllocVec(0x7ffffff0)", AllocVec(0x7ffffff0, MEMF_ANY));
    a = AllocMem(64, MEMF_CLEAR);
    if (a) {
        ULONG sum = 0;
        for (i = 0; i < 64; i++)
            sum += ((UBYTE *)a)[i];
        P_LONG("AllocMem(64, MEMF_CLEAR) byte sum", (LONG)sum);
        P_LONG("AllocMem alignment (addr & 7)", (LONG)((ULONG)a & 7));
        FreeMem(a, 64);
    }
    a = AllocVec(100, MEMF_CLEAR | MEMF_PUBLIC);
    if (a) {
        ULONG sum = 0;
        for (i = 0; i < 100; i++)
            sum += ((UBYTE *)a)[i];
        P_LONG("AllocVec(100, MEMF_CLEAR) byte sum", (LONG)sum);
        P_LONG("AllocVec alignment (addr & 3)", (LONG)((ULONG)a & 3));
        P_LONG("AllocVec size longword before block", (LONG)((ULONG *)a)[-1]);
        FreeVec(a);
    }
    FreeVec(NULL);
    P_LONG("FreeVec(NULL) survived", 1);

    P_SECTION("AllocEntry/FreeEntry");
    {
        struct {
            struct MemList ml;
            struct MemEntry extra[2];
        } req;
        struct MemList *got;
        req.ml.ml_NumEntries = 3;
        req.ml.ml_ME[0].me_Reqs = MEMF_CLEAR;
        req.ml.ml_ME[0].me_Length = 30;
        req.ml.ml_ME[1].me_Reqs = MEMF_PUBLIC;
        req.ml.ml_ME[1].me_Length = 8;
        req.ml.ml_ME[2].me_Reqs = MEMF_ANY;
        req.ml.ml_ME[2].me_Length = 0;
        got = AllocEntry(&req.ml);
        P_BOOL("AllocEntry ok (bit 31 clear)", !((ULONG)got & 0x80000000UL));
        if (!((ULONG)got & 0x80000000UL)) {
            ULONG sum = 0;
            P_LONG("ml_NumEntries", got->ml_NumEntries);
            P_LONG("ln_Type", got->ml_Node.ln_Type);
            for (i = 0; i < 3; i++) {
                probe_s("entry ");
                probe_dec(i);
                probe_s(": length ");
                probe_dec((LONG)got->ml_ME[i].me_Length);
                probe_s(", addr ");
                probe_s(got->ml_ME[i].me_Addr ? "non-NULL" : "NULL");
                probe_ch('\n');
            }
            for (i = 0; i < 30; i++)
                sum += ((UBYTE *)got->ml_ME[0].me_Addr)[i];
            P_LONG("entry 0 MEMF_CLEAR sum", (LONG)sum);
            FreeEntry(got);
        }
        req.ml.ml_NumEntries = 2;
        req.ml.ml_ME[0].me_Reqs = MEMF_PUBLIC;
        req.ml.ml_ME[0].me_Length = 16;
        req.ml.ml_ME[1].me_Reqs = MEMF_PUBLIC | MEMF_CLEAR;
        req.ml.ml_ME[1].me_Length = 0x7ffffff0;
        got = AllocEntry(&req.ml);
        P_HEX("AllocEntry failure result", (ULONG)got);
    }

    P_SECTION("pools");
    {
        APTR pool = CreatePool(MEMF_CLEAR | MEMF_PUBLIC, 1024, 256);
        APTR p[8];
        P_NULL("CreatePool", pool);
        if (pool) {
            ULONG sum = 0;
            for (i = 0; i < 8; i++)
                p[i] = AllocPooled(pool, 16 + i * 100);
            for (i = 0; i < 8; i++) {
                int k;
                if (!p[i]) {
                    sum += 1000000;
                    continue;
                }
                for (k = 0; k < 16 + i * 100; k++)
                    sum += ((UBYTE *)p[i])[k];
                for (k = 0; k < 16 + i * 100; k++)
                    ((UBYTE *)p[i])[k] = (UBYTE)k;
            }
            P_LONG("AllocPooled x8 (MEMF_CLEAR) byte sum", (LONG)sum);
            for (i = 0; i < 8; i += 2)
                FreePooled(pool, p[i], 16 + i * 100);
            a = AllocPooled(pool, 2000);
            P_NULL("AllocPooled(2000) (> puddle)", a);
            if (a) {
                sum = 0;
                for (i = 0; i < 2000; i++)
                    sum += ((UBYTE *)a)[i];
                P_LONG("large AllocPooled byte sum", (LONG)sum);
            }
            DeletePool(pool);
        }
        DeletePool(NULL);
        P_LONG("DeletePool(NULL) survived", 1);
    }

    P_SECTION("CopyMem/CopyMemQuick");
    {
        UBYTE src[64], dst[80];
        int s, l;
        ULONG h = 0;
        for (i = 0; i < 64; i++)
            src[i] = (UBYTE)(i * 7 + 1);
        for (s = 0; s < 4; s++)
            for (l = 0; l < 40; l += 3) {
                for (i = 0; i < 80; i++)
                    dst[i] = 0xee;
                CopyMem(src + s, dst + 3 - s + 1, l);
                h = h * 31 + probe_hash(dst, 80);
            }
        P_HEX("CopyMem alignment/length matrix hash", h);
        {
            ULONG qs[8], qd[10];
            for (i = 0; i < 8; i++)
                qs[i] = 0x01020304UL * (i + 1);
            for (i = 0; i < 10; i++)
                qd[i] = 0;
            CopyMemQuick(qs, qd + 1, 32);
            P_BYTES("CopyMemQuick 32", qd, 40);
            CopyMemQuick(qs, qd, 0);
            P_LONG("CopyMemQuick 0 first long unchanged", qd[0] == 0);
        }
    }

    P_SECTION("InitStruct");
    {
        static const UBYTE table[] __attribute__((aligned(2))) = {
            0xa0, 2, 0x11,                      /* byte at offset 2 */
            0xa0, 3, 0x12,                      /* byte at offset 3 */
            0x90, 4, 0x22, 0x33,                /* word at offset 4 */
            0x80, 8, 0x44, 0x55, 0x66, 0x77,    /* long at offset 8 */
            0x21, 0xaa, 0xbb,                   /* copy 2 bytes at 12 */
            0x52, 0x12, 0x34,                   /* repeat word 3x at 14 */
            0xc0, 0, 0, 24, 0xde, 0xad, 0xbe, 0xef, /* long at 24-bit offset 24 */
            0x20, 0x99,                         /* copy 1 byte at 28 */
            0x00
        };
        UBYTE out[40];
        for (i = 0; i < 40; i++)
            out[i] = 0xee;
        InitStruct((APTR)table, out, 40);
        P_BYTES("InitStruct", out, 40);
    }
    return 0;
}
