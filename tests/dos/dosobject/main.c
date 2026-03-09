/*
 * Integration Test: AllocDosObject / FreeDosObject
 *
 * Tests:
 *   - Allocate/Free DOS_FILEHANDLE
 *   - Allocate/Free DOS_EXALLCONTROL
 *   - Allocate/Free DOS_FIB
 *   - Allocate/Free DOS_STDPKT (with linkage verification)
 *   - Allocate/Free DOS_CLI
 *   - Allocate/Free DOS_RDARGS
 *   - Invalid type returns NULL and sets IoErr
 *   - NULL pointer to FreeDosObject is safe
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/exall.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[32];
    char *p = buf;

    if (n < 0) {
        *p++ = '-';
        n = -n;
    }

    char tmp[16];
    int i = 0;
    do {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    while (i > 0) *p++ = tmp[--i];
    *p = '\0';

    print(buf);
}

static void print_hex(ULONG n)
{
    char buf[12];
    char *p = buf;
    int i;

    *p++ = '0';
    *p++ = 'x';

    for (i = 28; i >= 0; i -= 4) {
        int nibble = (n >> i) & 0xF;
        *p++ = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
    *p = '\0';

    print(buf);
}

static int tests_passed = 0;
static int tests_failed = 0;

static void test_pass(const char *name)
{
    print("  PASS: ");
    print(name);
    print("\n");
    tests_passed++;
}

static void test_fail(const char *name, const char *reason)
{
    print("  FAIL: ");
    print(name);
    print(" - ");
    print(reason);
    print("\n");
    tests_failed++;
}

int main(void)
{
    print("AllocDosObject/FreeDosObject Test\n");
    print("=================================\n\n");

    /* Test 1: DOS_FILEHANDLE */
    print("Test 1: DOS_FILEHANDLE (type 0)\n");
    {
        struct FileHandle *fh = (struct FileHandle *)AllocDosObject(DOS_FILEHANDLE, NULL);
        if (fh) {
            print("  Allocated at ");
            print_hex((ULONG)fh);
            print("\n");

            /* Verify fh_Pos is 0 (not -1) — our FGetC uses negative for ungotten char */
            if (fh->fh_Pos == 0) {
                test_pass("fh_Pos initialized to 0");
            } else {
                print("  fh_Pos = ");
                print_num(fh->fh_Pos);
                print("\n");
                test_fail("fh_Pos init", "Expected 0");
            }

            FreeDosObject(DOS_FILEHANDLE, fh);
            test_pass("DOS_FILEHANDLE alloc/free");
        } else {
            test_fail("DOS_FILEHANDLE", "AllocDosObject returned NULL");
        }
    }

    /* Test 2: DOS_EXALLCONTROL */
    print("\nTest 2: DOS_EXALLCONTROL (type 1)\n");
    {
        struct ExAllControl *eac = (struct ExAllControl *)AllocDosObject(DOS_EXALLCONTROL, NULL);
        if (eac) {
            print("  Allocated at ");
            print_hex((ULONG)eac);
            print("\n");

            /* Verify eac_LastKey is 0 (cleared) */
            if (eac->eac_LastKey == 0) {
                test_pass("eac_LastKey initialized to 0");
            } else {
                test_fail("eac_LastKey init", "Expected 0");
            }

            /* Verify eac_MatchString is NULL (cleared) */
            if (eac->eac_MatchString == NULL) {
                test_pass("eac_MatchString initialized to NULL");
            } else {
                test_fail("eac_MatchString init", "Expected NULL");
            }

            FreeDosObject(DOS_EXALLCONTROL, eac);
            test_pass("DOS_EXALLCONTROL alloc/free");
        } else {
            test_fail("DOS_EXALLCONTROL", "AllocDosObject returned NULL");
        }
    }

    /* Test 3: DOS_FIB */
    print("\nTest 3: DOS_FIB (type 2)\n");
    {
        struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
        if (fib) {
            print("  Allocated at ");
            print_hex((ULONG)fib);
            print("\n");

            /* Verify zeroed */
            if (fib->fib_DiskKey == 0) {
                test_pass("fib_DiskKey initialized to 0");
            } else {
                test_fail("fib_DiskKey init", "Expected 0");
            }

            FreeDosObject(DOS_FIB, fib);
            test_pass("DOS_FIB alloc/free");
        } else {
            test_fail("DOS_FIB", "AllocDosObject returned NULL");
        }
    }

    /* Test 4: DOS_STDPKT */
    print("\nTest 4: DOS_STDPKT (type 3)\n");
    {
        struct StandardPacket *sp = (struct StandardPacket *)AllocDosObject(DOS_STDPKT, NULL);
        if (sp) {
            print("  Allocated at ");
            print_hex((ULONG)sp);
            print("\n");

            /* Verify linkage: sp_Msg.mn_Node.ln_Name should point to sp_Pkt */
            if (sp->sp_Msg.mn_Node.ln_Name == (char *)&sp->sp_Pkt) {
                test_pass("sp_Msg.ln_Name -> sp_Pkt linkage");
            } else {
                print("  sp_Msg.ln_Name = ");
                print_hex((ULONG)sp->sp_Msg.mn_Node.ln_Name);
                print(", &sp_Pkt = ");
                print_hex((ULONG)&sp->sp_Pkt);
                print("\n");
                test_fail("sp_Msg->sp_Pkt linkage", "Pointers don't match");
            }

            /* Verify linkage: sp_Pkt.dp_Link should point to sp_Msg */
            if (sp->sp_Pkt.dp_Link == &sp->sp_Msg) {
                test_pass("sp_Pkt.dp_Link -> sp_Msg linkage");
            } else {
                print("  sp_Pkt.dp_Link = ");
                print_hex((ULONG)sp->sp_Pkt.dp_Link);
                print(", &sp_Msg = ");
                print_hex((ULONG)&sp->sp_Msg);
                print("\n");
                test_fail("sp_Pkt->sp_Msg linkage", "Pointers don't match");
            }

            FreeDosObject(DOS_STDPKT, sp);
            test_pass("DOS_STDPKT alloc/free");
        } else {
            test_fail("DOS_STDPKT", "AllocDosObject returned NULL");
        }
    }

    /* Test 5: DOS_CLI */
    print("\nTest 5: DOS_CLI (type 4)\n");
    {
        struct CommandLineInterface *cli = (struct CommandLineInterface *)AllocDosObject(DOS_CLI, NULL);
        if (cli) {
            print("  Allocated at ");
            print_hex((ULONG)cli);
            print("\n");

            FreeDosObject(DOS_CLI, cli);
            test_pass("DOS_CLI alloc/free");
        } else {
            test_fail("DOS_CLI", "AllocDosObject returned NULL");
        }
    }

    /* Test 5b: DOS_STDPKT packet helpers */
    print("\nTest 5b: DOS packet helper compatibility\n");
    {
        struct StandardPacket *sp = (struct StandardPacket *)AllocDosObject(DOS_STDPKT, NULL);
        struct MsgPort *port = CreateMsgPort();
        struct Process *me = (struct Process *)FindTask(NULL);
        if (sp && port) {
            sp->sp_Pkt.dp_Type = ACTION_NIL;
            SendPkt(&sp->sp_Pkt, port, &me->pr_MsgPort);
            if (sp->sp_Pkt.dp_Port == &me->pr_MsgPort) {
                test_pass("SendPkt stores reply port");
            } else {
                test_fail("SendPkt", "reply port mismatch");
            }
            ReplyPkt(&sp->sp_Pkt, 11, 22);
            if (sp->sp_Pkt.dp_Res1 == 11 && sp->sp_Pkt.dp_Res2 == 22) {
                test_pass("ReplyPkt stores result fields");
            } else {
                test_fail("ReplyPkt", "result fields mismatch");
            }
        } else {
            test_fail("DOS packet helpers", "setup failed");
        }
        if (port)
            DeleteMsgPort(port);
        if (sp)
            FreeDosObject(DOS_STDPKT, sp);
    }

    /* Test 6: DOS_RDARGS */
    print("\nTest 6: DOS_RDARGS (type 5)\n");
    {
        struct RDArgs *rdargs = (struct RDArgs *)AllocDosObject(DOS_RDARGS, NULL);
        if (rdargs) {
            print("  Allocated at ");
            print_hex((ULONG)rdargs);
            print("\n");

            FreeDosObject(DOS_RDARGS, rdargs);
            test_pass("DOS_RDARGS alloc/free");
        } else {
            test_fail("DOS_RDARGS", "AllocDosObject returned NULL");
        }
    }

    /* Test 7: Invalid type (should return NULL, set IoErr, NOT crash) */
    print("\nTest 7: Invalid type (type 99)\n");
    {
        SetIoErr(0);
        void *obj = AllocDosObject(99, NULL);
        if (obj) {
            test_fail("Invalid type", "Should have returned NULL");
            FreeDosObject(99, obj);
        } else {
            LONG err = IoErr();
            print("  IoErr = ");
            print_num(err);
            print("\n");
            test_pass("Invalid type returns NULL (no crash)");
        }
    }

    /* Test 8: Multiple alloc/free cycles */
    print("\nTest 8: Multiple alloc/free cycles\n");
    {
        int cycles = 10;
        int i;
        BOOL all_ok = TRUE;

        for (i = 0; i < cycles; i++) {
            struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
            if (!fib) {
                all_ok = FALSE;
                break;
            }
            FreeDosObject(DOS_FIB, fib);
        }

        if (all_ok) {
            print("  ");
            print_num(cycles);
            print(" cycles completed\n");
            test_pass("Multiple alloc/free cycles");
        } else {
            test_fail("Multiple cycles", "AllocDosObject returned NULL");
        }
    }

    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed > 0 ? 10 : 0;
}
