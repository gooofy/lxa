/*
 * Exec AROS Verification Tests (Phase 78-A-1)
 *
 * Tests for bugs identified by systematic comparison of exec.c
 * against AROS reference source code. Each test verifies a specific
 * fix applied in Phase 78-A-1.
 *
 * Tests:
 *   1. AllocMem MEMF_CLEAR returns zeroed memory
 *   2. AllocVec/FreeVec round-trip (correct size tracking)
 *   3. AllocSignal can allocate signal 0
 *   4. RawDoFmt %x produces uppercase hex
 *   5. RawDoFmt NULL string produces empty output
 *   6. RawDoFmt negative zero-padding (-0005 not 000-5)
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;

/* Buffer for RawDoFmt output */
static char g_buffer[256];

/* Helper to print a string */
static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

/* Simple number to string for test output */
static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + 15;
    BOOL neg = FALSE;

    *p = '\0';

    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }

    do
    {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    if (neg) *--p = '-';

    print(p);
}

static void test_ok(const char *name)
{
    print("  OK: ");
    print(name);
    print("\n");
    test_pass++;
}

static void test_fail_msg(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    test_fail++;
}

/* putChProc callback for RawDoFmt */
void putChProc(void);
asm(
".globl _putChProc              \n"
"_putChProc:                    \n"
"       move.b  d0, (a3)+       \n"
"       rts                     \n"
);

/* Helper to format and compare */
static int format_and_check(const char *fmt, APTR args, const char *expected)
{
    RawDoFmt((CONST_STRPTR)fmt, args, (void (*)())putChProc, g_buffer);

    /* Compare strings */
    const char *a = g_buffer;
    const char *b = expected;
    while (*a && *b)
    {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

/* Helper: compare memory region to zero */
static int mem_is_zero(UBYTE *mem, ULONG size)
{
    ULONG i;
    for (i = 0; i < size; i++)
    {
        if (mem[i] != 0)
            return 0;
    }
    return 1;
}

static ULONG library_checksum_reference(void)
{
    struct ExecBase *sys = SysBase;
    ULONG checksum = 0;
    BOOL has_data = FALSE;

#ifndef RESLIST_NEXT
#define RESLIST_NEXT 0x80000000UL
#endif

    if (sys->KickTagPtr)
    {
        ULONG *list = (ULONG *)sys->KickTagPtr;

        while (*list)
        {
            checksum += (ULONG)*list;

            if (*list & RESLIST_NEXT)
            {
                list = (ULONG *)((ULONG)*list & ~RESLIST_NEXT);
                continue;
            }

            list++;
            has_data = TRUE;
        }
    }

    if (sys->KickMemPtr)
    {
        struct MemList *mem_list = (struct MemList *)sys->KickMemPtr;

        while (mem_list)
        {
            UBYTE i;
            ULONG *p = (ULONG *)mem_list;

            for (i = 0; i < sizeof(struct MemList) / sizeof(ULONG); i++)
                checksum += p[i];

            mem_list = (struct MemList *)mem_list->ml_Node.ln_Succ;
            has_data = TRUE;
        }
    }

    if (has_data && !checksum)
        checksum--;

    return checksum;
}

int main(void)
{
    out = Output();

    print("Exec AROS Verification Tests (Phase 78-A-1)\n");
    print("============================================\n\n");

    /* ===== Test 1: AllocMem MEMF_CLEAR ===== */
    print("Test 1: AllocMem MEMF_CLEAR\n");
    {
        /* Allocate with MEMF_CLEAR and verify all bytes are zero */
        UBYTE *mem = (UBYTE *)AllocMem(256, MEMF_PUBLIC | MEMF_CLEAR);
        if (mem)
        {
            if (mem_is_zero(mem, 256))
            {
                test_ok("AllocMem MEMF_CLEAR 256 bytes zeroed");
            }
            else
            {
                test_fail_msg("AllocMem MEMF_CLEAR 256 bytes NOT zeroed");
            }
            FreeMem(mem, 256);
        }
        else
        {
            test_fail_msg("AllocMem MEMF_CLEAR returned NULL");
        }

        /* Test with larger allocation */
        mem = (UBYTE *)AllocMem(4096, MEMF_PUBLIC | MEMF_CLEAR);
        if (mem)
        {
            if (mem_is_zero(mem, 4096))
            {
                test_ok("AllocMem MEMF_CLEAR 4096 bytes zeroed");
            }
            else
            {
                test_fail_msg("AllocMem MEMF_CLEAR 4096 bytes NOT zeroed");
            }
            FreeMem(mem, 4096);
        }
        else
        {
            test_fail_msg("AllocMem MEMF_CLEAR 4096 returned NULL");
        }

        /* Dirty memory, re-allocate with CLEAR, verify it's clean */
        mem = (UBYTE *)AllocMem(128, MEMF_PUBLIC);
        if (mem)
        {
            ULONG i;
            /* Fill with garbage */
            for (i = 0; i < 128; i++)
                mem[i] = 0xAA;
            FreeMem(mem, 128);

            /* Re-allocate same size with CLEAR */
            UBYTE *mem2 = (UBYTE *)AllocMem(128, MEMF_PUBLIC | MEMF_CLEAR);
            if (mem2)
            {
                if (mem_is_zero(mem2, 128))
                {
                    test_ok("AllocMem MEMF_CLEAR after dirty re-alloc");
                }
                else
                {
                    test_fail_msg("AllocMem MEMF_CLEAR after dirty re-alloc NOT zeroed");
                }
                FreeMem(mem2, 128);
            }
            else
            {
                test_fail_msg("AllocMem MEMF_CLEAR re-alloc returned NULL");
            }
        }
        else
        {
            test_fail_msg("AllocMem for dirty fill returned NULL");
        }
    }

    /* ===== Test 2: AllocVec/FreeVec round-trip ===== */
    print("\nTest 2: AllocVec/FreeVec round-trip\n");
    {
        /* Basic AllocVec/FreeVec should not crash or leak */
        APTR vec = AllocVec(100, MEMF_PUBLIC);
        if (vec)
        {
            test_ok("AllocVec 100 bytes succeeded");
            FreeVec(vec);
            test_ok("FreeVec completed without crash");
        }
        else
        {
            test_fail_msg("AllocVec 100 bytes returned NULL");
        }

        /* AllocVec with MEMF_CLEAR */
        UBYTE *cvec = (UBYTE *)AllocVec(64, MEMF_PUBLIC | MEMF_CLEAR);
        if (cvec)
        {
            if (mem_is_zero(cvec, 64))
            {
                test_ok("AllocVec MEMF_CLEAR zeroed");
            }
            else
            {
                test_fail_msg("AllocVec MEMF_CLEAR NOT zeroed");
            }
            FreeVec(cvec);
        }
        else
        {
            test_fail_msg("AllocVec MEMF_CLEAR returned NULL");
        }

        /* Multiple AllocVec/FreeVec cycles to check for size tracking */
        {
            LONG i;
            LONG alloc_ok = 1;
            for (i = 0; i < 10; i++)
            {
                APTR m = AllocVec(200 + i * 10, MEMF_PUBLIC);
                if (!m)
                {
                    alloc_ok = 0;
                    break;
                }
                FreeVec(m);
            }
            if (alloc_ok)
            {
                test_ok("AllocVec/FreeVec 10 cycles OK");
            }
            else
            {
                test_fail_msg("AllocVec/FreeVec cycle failed");
            }
        }

        /* AllocVec(0) should return NULL */
        {
            APTR m = AllocVec(0, MEMF_PUBLIC);
            if (m == NULL)
            {
                test_ok("AllocVec(0) returns NULL");
            }
            else
            {
                test_fail_msg("AllocVec(0) should return NULL");
                FreeVec(m);
            }
        }

        /* FreeVec(NULL) should be safe */
        FreeVec(NULL);
        test_ok("FreeVec(NULL) is safe");
    }

    /* ===== Test 3: AllocSignal signal 0 ===== */
    print("\nTest 3: AllocSignal signal 0\n");
    {
        struct Task *me = FindTask(NULL);

        /* First, free all signals we can (except system-reserved ones) */
        /* Allocate all free signals to exhaust them, then check if signal 0
         * can be explicitly allocated */

        /* Try explicit allocation of signal 0 */
        /* First check if signal 0 is free */
        ULONG origAlloc = me->tc_SigAlloc;

        if (!(origAlloc & 1))
        {
            /* Signal 0 is free, try to allocate it */
            BYTE sig = AllocSignal(0);
            if (sig == 0)
            {
                test_ok("AllocSignal(0) explicit allocation");
                FreeSignal(0);
            }
            else
            {
                print("    Got signal: ");
                print_num(sig);
                print("\n");
                test_fail_msg("AllocSignal(0) explicit allocation");
                if (sig >= 0) FreeSignal(sig);
            }
        }
        else
        {
            /* Signal 0 already allocated — skip explicit test but note it */
            test_ok("AllocSignal(0) skipped (already allocated)");
        }

        /* Test auto-allocation: exhaust all signals, verify signal 0
         * can be auto-allocated when it's the last one free */
        {
            /* Save current state */
            /* Allocate all free signals and track them */
            BYTE allocated[32];
            LONG count = 0;
            BYTE sig;

            while ((sig = AllocSignal(-1)) >= 0)
            {
                allocated[count++] = sig;
            }

            /* All signals should be allocated now */
            if (me->tc_SigAlloc == 0xFFFFFFFF)
            {
                test_ok("All 32 signals allocated (including signal 0)");
            }
            else
            {
                print("    tc_SigAlloc = 0x");
                /* Print hex manually */
                {
                    ULONG v = me->tc_SigAlloc;
                    char hexbuf[9];
                    LONG i;
                    for (i = 7; i >= 0; i--)
                    {
                        ULONG nibble = v & 0xF;
                        hexbuf[i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
                        v >>= 4;
                    }
                    hexbuf[8] = '\0';
                    print(hexbuf);
                }
                print("\n");
                test_fail_msg("Not all 32 signals allocated");
            }

            /* Free all the signals we allocated */
            {
                LONG i;
                for (i = 0; i < count; i++)
                {
                    FreeSignal(allocated[i]);
                }
            }
        }
    }

    /* ===== Test 4: RawDoFmt %x uppercase ===== */
    print("\nTest 4: RawDoFmt %x uppercase hex\n");
    {
        UWORD args[1];

        args[0] = 0xABCD;
        if (format_and_check("%x", args, "ABCD"))
        {
            test_ok("%%x produces uppercase");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%x produces uppercase");
        }

        args[0] = 0xDEAD;
        if (format_and_check("%x", args, "DEAD"))
        {
            test_ok("%%x 0xDEAD uppercase");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%x 0xDEAD uppercase");
        }

        /* %lx with 32-bit value */
        {
            ULONG largs[1] = { 0xCAFEBABE };
            if (format_and_check("%lx", largs, "CAFEBABE"))
            {
                test_ok("%%lx 0xCAFEBABE uppercase");
            }
            else
            {
                print("    Got: '"); print(g_buffer); print("'\n");
                test_fail_msg("%%lx 0xCAFEBABE uppercase");
            }
        }
    }

    /* ===== Test 5: RawDoFmt NULL string ===== */
    print("\nTest 5: RawDoFmt NULL string\n");
    {
        ULONG sargs[1] = { (ULONG)NULL };
        if (format_and_check("%s", sargs, ""))
        {
            test_ok("%%s NULL produces empty string");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%s NULL produces empty string");
        }

        /* NULL with width padding */
        if (format_and_check("[%10s]", sargs, "[          ]"))
        {
            test_ok("%%10s NULL with width padding");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%10s NULL with width padding");
        }
    }

    /* ===== Test 6: RawDoFmt negative zero-padding ===== */
    print("\nTest 6: RawDoFmt negative zero-padding\n");
    {
        UWORD args[1];

        args[0] = (UWORD)-5;
        if (format_and_check("%05d", args, "-0005"))
        {
            test_ok("%%05d -5 = '-0005'");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%05d -5 = '-0005'");
        }

        args[0] = (UWORD)-42;
        if (format_and_check("%08d", args, "-0000042"))
        {
            test_ok("%%08d -42 = '-0000042'");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%08d -42 = '-0000042'");
        }

        /* Negative with space padding should still work */
        args[0] = (UWORD)-7;
        if (format_and_check("%5d", args, "   -7"))
        {
            test_ok("%%5d -7 = '   -7' (space pad)");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%5d -7 = '   -7' (space pad)");
        }

        /* Negative left-aligned */
        args[0] = (UWORD)-3;
        if (format_and_check("%-5d", args, "-3   "))
        {
            test_ok("%%-5d -3 = '-3   ' (left align)");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%-5d -3 = '-3   ' (left align)");
        }

        /* Long negative zero-padding */
        {
            LONG largs[1];
            largs[0] = -12345;
            if (format_and_check("%010ld", largs, "-000012345"))
            {
                test_ok("%%010ld -12345 = '-000012345'");
            }
            else
            {
                print("    Got: '"); print(g_buffer); print("'\n");
                test_fail_msg("%%010ld -12345 = '-000012345'");
            }
        }
    }

    /* ===== Summary ===== */
    print("\nTest 7: SumKickData empty state\n");
    {
        APTR old_kick_tag = SysBase->KickTagPtr;
        APTR old_kick_mem = SysBase->KickMemPtr;
        ULONG sum;

        SysBase->KickTagPtr = NULL;
        SysBase->KickMemPtr = NULL;

        sum = SumKickData();
        if (sum == 0)
        {
            test_ok("SumKickData returns 0 with no kick data");
        }
        else
        {
            test_fail_msg("SumKickData should return 0 with no kick data");
        }

        SysBase->KickTagPtr = old_kick_tag;
        SysBase->KickMemPtr = old_kick_mem;
    }

    print("\nTest 8: SumKickData with KickTagPtr list\n");
    {
        APTR old_kick_tag = SysBase->KickTagPtr;
        APTR old_kick_mem = SysBase->KickMemPtr;
        ULONG tags[3];
        ULONG sum;
        ULONG expected;

        tags[0] = 0x12345678;
        tags[1] = 0x00000002;
        tags[2] = 0;

        SysBase->KickTagPtr = tags;
        SysBase->KickMemPtr = NULL;

        expected = library_checksum_reference();
        sum = SumKickData();
        if (sum == expected)
        {
            test_ok("SumKickData sums KickTagPtr list");
        }
        else
        {
            test_fail_msg("SumKickData KickTagPtr sum mismatch");
        }

        SysBase->KickTagPtr = old_kick_tag;
        SysBase->KickMemPtr = old_kick_mem;
    }

    print("\nTest 9: SumKickData with KickMemPtr list\n");
    {
        APTR old_kick_tag = SysBase->KickTagPtr;
        APTR old_kick_mem = SysBase->KickMemPtr;
        struct
        {
            struct MemList ml;
            struct MemEntry extra;
        } kick_mem;
        ULONG sum;
        ULONG expected;

        kick_mem.ml.ml_Node.ln_Succ = NULL;
        kick_mem.ml.ml_Node.ln_Pred = NULL;
        kick_mem.ml.ml_Node.ln_Type = NT_MEMORY;
        kick_mem.ml.ml_Node.ln_Pri = 0;
        kick_mem.ml.ml_Node.ln_Name = NULL;
        kick_mem.ml.ml_NumEntries = 2;
        kick_mem.ml.ml_ME[0].me_Addr = (APTR)0x1000;
        kick_mem.ml.ml_ME[0].me_Length = 0x2000;
        kick_mem.extra.me_Addr = (APTR)0x4000;
        kick_mem.extra.me_Length = 0x1000;

        SysBase->KickTagPtr = NULL;
        SysBase->KickMemPtr = &kick_mem.ml;

        expected = library_checksum_reference();
        sum = SumKickData();
        if (sum == expected)
        {
            test_ok("SumKickData sums KickMemPtr list");
        }
        else
        {
            test_fail_msg("SumKickData KickMemPtr sum mismatch");
        }

        SysBase->KickTagPtr = old_kick_tag;
        SysBase->KickMemPtr = old_kick_mem;
    }

    print("\n=============================\n");
    print("Tests passed: ");
    print_num(test_pass);
    print("\n");
    print("Tests failed: ");
    print_num(test_fail);
    print("\n");

    if (test_fail == 0)
    {
        print("\nPASS: All Exec Verify tests passed!\n");
        return 0;
    }
    else
    {
        print("\nFAIL: Some tests failed\n");
        return 20;
    }
}
