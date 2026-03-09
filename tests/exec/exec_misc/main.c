/*
 * Exec Miscellaneous Verification Tests (Phase 78-A-6)
 *
 * Tests for remaining Exec miscellaneous functions verified against
 * AROS reference source code:
 *
 *   1. RawDoFmt maxwidth (string precision truncation)
 *   2. RawDoFmt %% at end of format string
 *   3. RawDoFmt %c specifier edge cases
 *   4. RawDoFmt return value (end of data stream)
 *   5. List accessors: GetHead/GetTail/GetSucc/GetPred semantics
 *   6. Alert (recovery alert does not crash)
 *   7. Supervisor (basic call and return)
 *   8. UserState/SuperState round-trip
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/resident.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/alerts.h>
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

/* Print hex number */
static void print_hex(ULONG v)
{
    char buf[9];
    LONG i;
    for (i = 7; i >= 0; i--)
    {
        ULONG nibble = v & 0xF;
        buf[i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        v >>= 4;
    }
    buf[8] = '\0';
    print(buf);
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

/* Simple string compare */
static int streq(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return (*a == *b);
}

/* Helper to format and compare */
static int format_and_check(const char *fmt, APTR args, const char *expected)
{
    RawDoFmt((CONST_STRPTR)fmt, args, (void (*)())putChProc, g_buffer);
    return streq(g_buffer, expected);
}

/*
 * List accessor macros matching AROS exec/lists.h semantics.
 * These are the inline versions that m68k programs would use.
 */
#define GetHead(l)  (((struct List *)(l))->lh_Head->ln_Succ \
                     ? ((struct List *)(l))->lh_Head : (struct Node *)0)

#define GetTail(l)  (((struct List *)(l))->lh_TailPred->ln_Pred \
                     ? ((struct List *)(l))->lh_TailPred : (struct Node *)0)

#define GetSucc(n)  (((n) && ((struct Node *)(n))->ln_Succ && \
                      ((struct Node *)(n))->ln_Succ->ln_Succ) \
                     ? ((struct Node *)(n))->ln_Succ : (struct Node *)0)

#define GetPred(n)  (((n) && ((struct Node *)(n))->ln_Pred && \
                      ((struct Node *)(n))->ln_Pred->ln_Pred) \
                     ? ((struct Node *)(n))->ln_Pred : (struct Node *)0)


/* ===== Supervisor test function ===== */
/* This runs in supervisor mode and returns a magic value */
ULONG superFunc(void);
asm(
".globl _superFunc              \n"
"_superFunc:                    \n"
"       move.l  #0xDEADBEEF, d0 \n"
"       rte                     \n"
);

int main(void)
{
    out = Output();

    print("=== Exec Miscellaneous Tests (Phase 78-A-6) ===\n");

    /* ===== Test 1: RawDoFmt maxwidth (precision) for strings ===== */
    print("\nTest 1: RawDoFmt maxwidth (string precision)\n");
    {
        ULONG sargs[1];

        /* %.3s should truncate "Hello" to "Hel" */
        sargs[0] = (ULONG)"Hello";
        if (format_and_check("%.3s", sargs, "Hel"))
        {
            test_ok("%.3s truncates to 3 chars");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%.3s truncates to 3 chars");
        }

        /* %.0s should output empty string */
        sargs[0] = (ULONG)"Hello";
        if (format_and_check("%.0s", sargs, ""))
        {
            test_ok("%.0s produces empty output");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%.0s produces empty output");
        }

        /* %.10s with short string should output full string */
        sargs[0] = (ULONG)"Hi";
        if (format_and_check("%.10s", sargs, "Hi"))
        {
            test_ok("%.10s with short string outputs full string");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%.10s with short string outputs full string");
        }

        /* Width + precision: %10.3s should right-pad */
        sargs[0] = (ULONG)"Hello";
        if (format_and_check("%10.3s", sargs, "       Hel"))
        {
            test_ok("%10.3s right-pads truncated string");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%10.3s right-pads truncated string");
        }

        /* Left-aligned width + precision: %-10.3s */
        sargs[0] = (ULONG)"Hello";
        if (format_and_check("%-10.3s", sargs, "Hel       "))
        {
            test_ok("%-10.3s left-aligns truncated string");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%-10.3s left-aligns truncated string");
        }
    }

    /* ===== Test 2: RawDoFmt %% edge cases ===== */
    print("\nTest 2: RawDoFmt %% edge cases\n");
    {
        /* Double percent at start */
        if (format_and_check("%%test", NULL, "%test"))
        {
            test_ok("%%%% at start produces %%");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%%% at start produces %%");
        }

        /* Multiple percents */
        if (format_and_check("a%%b%%c", NULL, "a%b%c"))
        {
            test_ok("a%%%%b%%%%c produces a%%b%%c");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("a%%%%b%%%%c produces a%%b%%c");
        }

        /* %% at end */
        if (format_and_check("end%%", NULL, "end%"))
        {
            test_ok("end%%%% produces end%%");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("end%%%% produces end%%");
        }
    }

    /* ===== Test 3: RawDoFmt %c specifier edge cases ===== */
    print("\nTest 3: RawDoFmt %c specifier\n");
    {
        UWORD cargs[1];

        /* Basic character */
        cargs[0] = 'A';
        if (format_and_check("%c", cargs, "A"))
        {
            test_ok("%%c outputs character");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%c outputs character");
        }

        /* Character with width */
        cargs[0] = 'X';
        if (format_and_check("%5c", cargs, "    X"))
        {
            test_ok("%%5c right-pads character");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%5c right-pads character");
        }

        /* Left-aligned character with width */
        cargs[0] = 'Z';
        if (format_and_check("%-5c", cargs, "Z    "))
        {
            test_ok("%%-5c left-aligns character");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("%%-5c left-aligns character");
        }
    }

    /* ===== Test 4: RawDoFmt return value ===== */
    print("\nTest 4: RawDoFmt return value\n");
    {
        /* Return value should point past last consumed argument */
        UWORD args2[2];
        APTR result;

        args2[0] = 42;
        args2[1] = 99;
        result = RawDoFmt((CONST_STRPTR)"%d", args2, (void (*)())putChProc, g_buffer);

        /* After consuming one WORD arg, result should point past it */
        if (result == (APTR)&args2[1])
        {
            test_ok("return value points past last WORD arg");
        }
        else
        {
            print("    Got: 0x"); print_hex((ULONG)result);
            print(", Expected: 0x"); print_hex((ULONG)&args2[1]);
            print("\n");
            test_fail_msg("return value points past last WORD arg");
        }

        /* Two WORD args */
        result = RawDoFmt((CONST_STRPTR)"%d,%d", args2, (void (*)())putChProc, g_buffer);
        if (result == (APTR)&args2[2])
        {
            test_ok("return value after two WORD args");
        }
        else
        {
            print("    Got: 0x"); print_hex((ULONG)result);
            print(", Expected: 0x"); print_hex((ULONG)&args2[2]);
            print("\n");
            test_fail_msg("return value after two WORD args");
        }

        /* LONG arg */
        {
            ULONG largs[2];
            largs[0] = 12345;
            largs[1] = 0xDEAD;
            result = RawDoFmt((CONST_STRPTR)"%ld", largs, (void (*)())putChProc, g_buffer);
            if (result == (APTR)&largs[1])
            {
                test_ok("return value after LONG arg");
            }
            else
            {
                print("    Got: 0x"); print_hex((ULONG)result);
                print(", Expected: 0x"); print_hex((ULONG)&largs[1]);
                print("\n");
                test_fail_msg("return value after LONG arg");
            }
        }

        /* String arg (APTR = 4 bytes consumed) */
        {
            ULONG sargs[2];
            sargs[0] = (ULONG)"hello";
            sargs[1] = 0xBEEF;
            result = RawDoFmt((CONST_STRPTR)"%s", sargs, (void (*)())putChProc, g_buffer);
            if (result == (APTR)&sargs[1])
            {
                test_ok("return value after string arg");
            }
            else
            {
                print("    Got: 0x"); print_hex((ULONG)result);
                print(", Expected: 0x"); print_hex((ULONG)&sargs[1]);
                print("\n");
                test_fail_msg("return value after string arg");
            }
        }
    }

    /* ===== Test 5: List accessors ===== */
    print("\nTest 5: List accessors (GetHead/GetTail/GetSucc/GetPred)\n");
    {
        struct List list;
        struct Node nodeA, nodeB, nodeC;

        /* Test on empty list */
        NewList(&list);

        if (GetHead(&list) == NULL)
        {
            test_ok("GetHead on empty list returns NULL");
        }
        else
        {
            test_fail_msg("GetHead on empty list returns NULL");
        }

        if (GetTail(&list) == NULL)
        {
            test_ok("GetTail on empty list returns NULL");
        }
        else
        {
            test_fail_msg("GetTail on empty list returns NULL");
        }

        /* Add one node */
        nodeA.ln_Name = "A";
        nodeA.ln_Pri = 0;
        AddTail(&list, &nodeA);

        if (GetHead(&list) == &nodeA)
        {
            test_ok("GetHead returns sole node");
        }
        else
        {
            test_fail_msg("GetHead returns sole node");
        }

        if (GetTail(&list) == &nodeA)
        {
            test_ok("GetTail returns sole node");
        }
        else
        {
            test_fail_msg("GetTail returns sole node");
        }

        /* GetSucc/GetPred on sole node should return NULL */
        if (GetSucc(&nodeA) == NULL)
        {
            test_ok("GetSucc on sole node returns NULL");
        }
        else
        {
            test_fail_msg("GetSucc on sole node returns NULL");
        }

        if (GetPred(&nodeA) == NULL)
        {
            test_ok("GetPred on sole node returns NULL");
        }
        else
        {
            test_fail_msg("GetPred on sole node returns NULL");
        }

        /* Add two more nodes */
        nodeB.ln_Name = "B";
        nodeB.ln_Pri = 0;
        AddTail(&list, &nodeB);

        nodeC.ln_Name = "C";
        nodeC.ln_Pri = 0;
        AddTail(&list, &nodeC);

        /* List is now: A -> B -> C */

        if (GetHead(&list) == &nodeA)
        {
            test_ok("GetHead returns first of three");
        }
        else
        {
            test_fail_msg("GetHead returns first of three");
        }

        if (GetTail(&list) == &nodeC)
        {
            test_ok("GetTail returns last of three");
        }
        else
        {
            test_fail_msg("GetTail returns last of three");
        }

        /* GetSucc chain: A -> B -> C -> NULL */
        if (GetSucc(&nodeA) == &nodeB)
        {
            test_ok("GetSucc(A) returns B");
        }
        else
        {
            test_fail_msg("GetSucc(A) returns B");
        }

        if (GetSucc(&nodeB) == &nodeC)
        {
            test_ok("GetSucc(B) returns C");
        }
        else
        {
            test_fail_msg("GetSucc(B) returns C");
        }

        if (GetSucc(&nodeC) == NULL)
        {
            test_ok("GetSucc(C) returns NULL (tail)");
        }
        else
        {
            test_fail_msg("GetSucc(C) returns NULL (tail)");
        }

        /* GetPred chain: C -> B -> A -> NULL */
        if (GetPred(&nodeC) == &nodeB)
        {
            test_ok("GetPred(C) returns B");
        }
        else
        {
            test_fail_msg("GetPred(C) returns B");
        }

        if (GetPred(&nodeB) == &nodeA)
        {
            test_ok("GetPred(B) returns A");
        }
        else
        {
            test_fail_msg("GetPred(B) returns A");
        }

        if (GetPred(&nodeA) == NULL)
        {
            test_ok("GetPred(A) returns NULL (head)");
        }
        else
        {
            test_fail_msg("GetPred(A) returns NULL (head)");
        }

        /* GetSucc/GetPred with NULL input */
        if (GetSucc(NULL) == NULL)
        {
            test_ok("GetSucc(NULL) returns NULL");
        }
        else
        {
            test_fail_msg("GetSucc(NULL) returns NULL");
        }

        if (GetPred(NULL) == NULL)
        {
            test_ok("GetPred(NULL) returns NULL");
        }
        else
        {
            test_fail_msg("GetPred(NULL) returns NULL");
        }

        /* After removing middle node B */
        Remove(&nodeB);
        /* List is now: A -> C */

        if (GetSucc(&nodeA) == &nodeC)
        {
            test_ok("GetSucc(A) returns C after Remove(B)");
        }
        else
        {
            test_fail_msg("GetSucc(A) returns C after Remove(B)");
        }

        if (GetPred(&nodeC) == &nodeA)
        {
            test_ok("GetPred(C) returns A after Remove(B)");
        }
        else
        {
            test_fail_msg("GetPred(C) returns A after Remove(B)");
        }
    }

    /* ===== Test 6: Alert (recovery) ===== */
    print("\nTest 6: Alert (recovery alert)\n");
    {
        /* Recovery alert (bit 31 clear) should not crash */
        Alert(AG_NoMemory | AO_ExecLib);
        test_ok("Recovery alert returned (did not crash)");

        /* Another recovery alert */
        Alert(AG_OpenLib | AO_DOSLib);
        test_ok("AG_OpenLib | AO_DOSLib alert returned");
    }

    /* ===== Test 7: Supervisor ===== */
    print("\nTest 7: Supervisor\n");
    {
        ULONG result;

        result = Supervisor((ULONG (*)())superFunc);
        if (result == 0xDEADBEEF)
        {
            test_ok("Supervisor returned 0xDEADBEEF from superFunc");
        }
        else
        {
            print("    Got: 0x"); print_hex(result); print("\n");
            test_fail_msg("Supervisor returned 0xDEADBEEF from superFunc");
        }
    }

    /* ===== Test 8: RawDoFmt mixed format ===== */
    print("\nTest 8: RawDoFmt mixed format verification\n");
    {
        /* Mixed: string + decimal + hex */
        struct {
            ULONG str;
            UWORD num;
            UWORD hex;
        } __attribute__((packed)) margs;

        margs.str = (ULONG)"foo";
        margs.num = 42;
        margs.hex = 0xFF;

        if (format_and_check("%s=%d (0x%x)", &margs, "foo=42 (0xFF)"))
        {
            test_ok("mixed format: string + decimal + hex");
        }
        else
        {
            print("    Got: '"); print(g_buffer); print("'\n");
            test_fail_msg("mixed format: string + decimal + hex");
        }
    }

    /* ===== Test 9: CachePreDMA / CachePostDMA hosted semantics ===== */
    print("\nTest 9: CachePreDMA / CachePostDMA hosted semantics\n");
    {
        ULONG dma_length = 16;
        ULONG buffer[4] = { 1, 2, 3, 4 };
        APTR dma_addr;

        dma_addr = CachePreDMA(buffer, &dma_length, DMA_ReadFromRAM);
        if (dma_addr == buffer)
        {
            test_ok("CachePreDMA returns original address");
        }
        else
        {
            test_fail_msg("CachePreDMA returns original address");
        }

        if (dma_length == 16)
        {
            test_ok("CachePreDMA preserves length");
        }
        else
        {
            test_fail_msg("CachePreDMA preserves length");
        }

        CachePostDMA(buffer, &dma_length, DMA_NoModify);
        test_ok("CachePostDMA returns without crashing");
    }

    /* ===== Test 10: InitCode resident replay ===== */
    print("\nTest 10: InitCode resident replay\n");
    {
        struct Resident *resident_before;
        struct Resident *resident_after;

        resident_before = FindResident((CONST_STRPTR)"dos.library");
        InitCode(RTF_COLDSTART, 0);
        resident_after = FindResident((CONST_STRPTR)"dos.library");

        if (resident_before != NULL && resident_after == resident_before)
        {
            test_ok("InitCode keeps built-in residents discoverable");
        }
        else
        {
            test_fail_msg("InitCode keeps built-in residents discoverable");
        }
    }

    /* ===== Test 11: RawDoFmt BSTR format ===== */
    print("\nTest 11: RawDoFmt BSTR format\n");
    {
        /*
         * BSTR requires 4-byte aligned memory because BPTR = address >> 2.
         * Stack arrays may not be aligned, so use AllocMem which guarantees
         * longword alignment.
         */
        UBYTE *bstr_data = (UBYTE *)AllocMem(8, MEMF_PUBLIC | MEMF_CLEAR);
        ULONG bargs[1];

        if (bstr_data)
        {
            bstr_data[0] = 5;  /* length */
            bstr_data[1] = 'H';
            bstr_data[2] = 'e';
            bstr_data[3] = 'l';
            bstr_data[4] = 'l';
            bstr_data[5] = 'o';

            /* BPTR = address >> 2 */
            bargs[0] = ((ULONG)bstr_data) >> 2;

            if (format_and_check("%b", bargs, "Hello"))
            {
                test_ok("%%b formats BSTR correctly");
            }
            else
            {
                print("    Got: '"); print(g_buffer); print("'\n");
                test_fail_msg("%%b formats BSTR correctly");
            }

            /* NULL BSTR */
            bargs[0] = 0;
            if (format_and_check("%b", bargs, ""))
            {
                test_ok("%%b NULL BSTR produces empty string");
            }
            else
            {
                print("    Got: '"); print(g_buffer); print("'\n");
                test_fail_msg("%%b NULL BSTR produces empty string");
            }

            /* BSTR with width */
            bstr_data[0] = 2;
            bstr_data[1] = 'O';
            bstr_data[2] = 'K';
            bargs[0] = ((ULONG)bstr_data) >> 2;

            if (format_and_check("[%6b]", bargs, "[    OK]"))
            {
                test_ok("%%6b pads BSTR with spaces");
            }
            else
            {
                print("    Got: '"); print(g_buffer); print("'\n");
                test_fail_msg("%%6b pads BSTR with spaces");
            }

            FreeMem(bstr_data, 8);
        }
        else
        {
            print("    SKIP: Could not allocate BSTR memory\n");
            test_fail_msg("%%b AllocMem failed");
        }
    }

    /* ===== Summary ===== */
    print("\n=============================\n");
    print("Tests passed: ");
    print_num(test_pass);
    print("\n");
    print("Tests failed: ");
    print_num(test_fail);
    print("\n");

    if (test_fail == 0)
    {
        print("\nPASS: All Exec Misc tests passed!\n");
        return 0;
    }
    else
    {
        print("\nFAIL: Some tests failed\n");
        return 20;
    }
}
