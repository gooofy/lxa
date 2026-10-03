/*
 * Probe (Phase 222a): exec.library library/device bookkeeping -
 * OpenLibrary/CloseLibrary (version checks, lib_OpenCnt), MakeLibrary,
 * MakeFunctions (absolute and relative tables), AddLibrary, SumLibrary,
 * SetFunction, RemLibrary, OpenDevice/CloseDevice error paths,
 * OpenResource, FindResident.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <exec/resident.h>
#include <devices/timer.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

/* library vectors: Open returns a6 and counts, Close/Expunge/Null return 0 */
__asm__(
    "    .text\n"
    "    .globl _pl_open\n"
    "_pl_open:\n"
    "    addq.w #1,32(a6)\n"
    "    move.l a6,d0\n"
    "    rts\n"
    "    .globl _pl_close\n"
    "_pl_close:\n"
    "    subq.w #1,32(a6)\n"
    "    moveq #0,d0\n"
    "    rts\n"
    "    .globl _pl_null\n"
    "_pl_null:\n"
    "    moveq #0,d0\n"
    "    rts\n"
    "    .globl _pl_f1\n"
    "_pl_f1:\n"
    "    add.l d0,d0\n"
    "    addq.l #1,d0\n"
    "    rts\n"
    "    .globl _pl_f2\n"
    "_pl_f2:\n"
    "    add.l #1000,d0\n"
    "    rts\n"
    "    .globl _pl_f3\n"
    "_pl_f3:\n"
    "    neg.l d0\n"
    "    rts\n"
    "    .globl _pl_rel\n"
    "_pl_rel:\n"
    "    .word -1\n"
    "    .word _pl_open-_pl_rel\n"
    "    .word _pl_close-_pl_rel\n"
    "    .word _pl_null-_pl_rel\n"
    "    .word _pl_null-_pl_rel\n"
    "    .word _pl_f2-_pl_rel\n"
    "    .word -1\n");
extern WORD pl_rel[];
extern void pl_open(void), pl_close(void), pl_null(void), pl_f1(void), pl_f2(void), pl_f3(void);

static ULONG callf(struct Library *lib, WORD lvo, ULONG arg)
{
    register ULONG d0 __asm("d0") = arg;
    register struct Library *a6 __asm("a6") = lib;
    register LONG off __asm("a0") = lvo;
    __asm volatile("jsr 0(a6,a0.l)" : "+r"(d0), "+r"(off) : "r"(a6) : "d1", "a1", "cc", "memory");
    return d0;
}

/* RemLibrary and MakeFunctions return d0 although some prototypes say VOID */
static ULONG remlibrary(struct Library *lib)
{
    register ULONG d0 __asm("d0");
    register struct Library *a1 __asm("a1") = lib;
    register struct ExecBase *a6 __asm("a6") = SysBase;
    __asm volatile("jsr -402(a6)" : "=r"(d0), "+r"(a1) : "r"(a6) : "d1", "a0", "cc", "memory");
    return d0;
}

static ULONG makefunctions(APTR target, APTR array, APTR base)
{
    register ULONG d0 __asm("d0");
    register APTR a0 __asm("a0") = target;
    register APTR a1 __asm("a1") = array;
    register APTR a2 __asm("a2") = base;
    register struct ExecBase *a6 __asm("a6") = SysBase;
    __asm volatile("jsr -90(a6)" : "=r"(d0), "+r"(a0), "+r"(a1) : "r"(a2), "r"(a6) : "d1", "cc", "memory");
    return d0;
}

static void jumptab(const char *label, struct Library *lib, int n)
{
    int i;
    probe_s(label);
    probe_s(" =");
    for (i = 1; i <= n; i++) {
        UWORD *jt = (UWORD *)((UBYTE *)lib - 6 * i);
        probe_ch(' ');
        probe_hex(jt[0], 4);
    }
    probe_ch('\n');
}

int main(void)
{
    struct Library *lib, *l2;
    ULONG cnt;

    P_SECTION("OpenLibrary");
    lib = OpenLibrary((STRPTR)"exec.library", 0);
    P_BOOL("OpenLibrary(exec) == SysBase", lib == (struct Library *)SysBase);
    if (lib)
        CloseLibrary(lib);
    P_NULL("OpenLibrary(exec, 99)", OpenLibrary((STRPTR)"exec.library", 99));
    P_NULL("OpenLibrary(missing)", OpenLibrary((STRPTR)"lxaprobe-missing.library", 0));
    P_NULL("OpenLibrary(EXEC.LIBRARY)", (lib = OpenLibrary((STRPTR)"EXEC.LIBRARY", 0)));
    if (lib)
        CloseLibrary(lib);
    lib = OpenLibrary((STRPTR)"utility.library", 0);
    if (lib) {
        cnt = lib->lib_OpenCnt;
        l2 = OpenLibrary((STRPTR)"utility.library", 37);
        P_LONG("lib_OpenCnt delta after OpenLibrary", (LONG)(lib->lib_OpenCnt - cnt));
        CloseLibrary(l2);
        P_LONG("lib_OpenCnt delta after CloseLibrary", (LONG)(lib->lib_OpenCnt - cnt));
        P_LONG("utility lib_Version", lib->lib_Version);
        P_LONG("utility ln_Type", lib->lib_Node.ln_Type);
        CloseLibrary(lib);
    }
    P_LONG("exec lib_Version", SysBase->LibNode.lib_Version);
    P_LONG("exec lib_NegSize", SysBase->LibNode.lib_NegSize);
    CloseLibrary(NULL);
    P_LONG("CloseLibrary(NULL) survived", 1);

    P_SECTION("MakeLibrary (absolute vectors)");
    {
        static APTR vecs[] = {(APTR)pl_open, (APTR)pl_close, (APTR)pl_null, (APTR)pl_null,
                              (APTR)pl_f1, (APTR)pl_f2, (APTR)-1};
        APTR old;
        lib = MakeLibrary(vecs, NULL, NULL, sizeof(struct Library) + 6, 0);
        P_NULL("MakeLibrary", lib);
        if (!lib)
            return 20;
        P_LONG("lib_NegSize", lib->lib_NegSize);
        P_LONG("lib_PosSize", lib->lib_PosSize);
        P_LONG("lib_OpenCnt", lib->lib_OpenCnt);
        P_LONG("lib_Flags", lib->lib_Flags);
        P_LONG("ln_Type", lib->lib_Node.ln_Type);
        jumptab("jump opcodes", lib, 6);
        P_BOOL("vector 5 target", *(APTR *)((UBYTE *)lib - 30 + 2) == (APTR)pl_f1);
        P_LONG("call LVO -30 (f1: 2x+1) with 20", callf(lib, -30, 20));
        P_LONG("call LVO -36 (f2: x+1000) with 20", callf(lib, -36, 20));
        lib->lib_Node.ln_Name = (char *)"lxaprobe.library";
        lib->lib_Node.ln_Type = NT_LIBRARY;
        lib->lib_Version = 7;
        lib->lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
        AddLibrary(lib);
        P_LONG("lib_Flags after AddLibrary", lib->lib_Flags);
        {
            UWORD sum0;
            lib->lib_Sum = 0;
            SumLibrary(lib);
            sum0 = lib->lib_Sum;
            P_LONG("lib_Flags after SumLibrary", lib->lib_Flags);
        }
        l2 = OpenLibrary((STRPTR)"lxaprobe.library", 7);
        P_BOOL("OpenLibrary(lxaprobe, 7) via Open vector", l2 == lib);
        P_LONG("lib_OpenCnt", lib->lib_OpenCnt);
        P_NULL("OpenLibrary(lxaprobe, 8)", OpenLibrary((STRPTR)"lxaprobe.library", 8));
        P_LONG("lib_OpenCnt after failed version", lib->lib_OpenCnt);
        old = SetFunction(lib, -30, (APTR)pl_f3);
        P_BOOL("SetFunction returns old vector", old == (APTR)pl_f1);
        P_LONG("lib_Flags after SetFunction", lib->lib_Flags);
        P_LONG("call LVO -30 after SetFunction (neg) with 20", callf(lib, -30, 20));
        SetFunction(lib, -30, old);
        P_LONG("call LVO -30 restored with 20", callf(lib, -30, 20));
        if (l2)
            CloseLibrary(l2);
        P_LONG("lib_OpenCnt after close", lib->lib_OpenCnt);
        P_LONG("RemLibrary (Expunge returns 0)", (LONG)remlibrary(lib));
        Forbid();
        P_BOOL("still in LibList", FindName(&SysBase->LibList, (STRPTR)"lxaprobe.library") == (struct Node *)lib);
        Remove(&lib->lib_Node);
        Permit();
        P_NULL("FindName after Remove", FindName(&SysBase->LibList, (STRPTR)"lxaprobe.library"));
        FreeMem((UBYTE *)lib - lib->lib_NegSize, lib->lib_NegSize + lib->lib_PosSize);
    }

    P_SECTION("MakeLibrary (relative vectors)");
    {
        lib = MakeLibrary(pl_rel, NULL, NULL, sizeof(struct Library), 0);
        P_NULL("MakeLibrary(relative)", lib);
        if (lib) {
            P_LONG("lib_NegSize", lib->lib_NegSize);
            P_LONG("lib_PosSize", lib->lib_PosSize);
            P_LONG("call LVO -30 (f2) with 5", callf(lib, -30, 5));
            FreeMem((UBYTE *)lib - lib->lib_NegSize, lib->lib_NegSize + lib->lib_PosSize);
        }
    }

    P_SECTION("MakeFunctions");
    {
        static APTR vecs[] = {(APTR)pl_f3, (APTR)pl_f1, (APTR)-1};
        UBYTE *mem = AllocMem(64, MEMF_CLEAR);
        if (mem) {
            ULONG n = makefunctions(mem + 32, vecs, NULL);
            P_LONG("MakeFunctions size", (LONG)n);
            P_LONG("call -6 (neg) with 9", callf((struct Library *)(mem + 32), -6, 9));
            P_LONG("call -12 (2x+1) with 9", callf((struct Library *)(mem + 32), -12, 9));
            FreeMem(mem, 64);
        }
    }

    P_SECTION("devices");
    {
        struct MsgPort *mp = CreateMsgPort();
        struct timerequest *tr = (struct timerequest *)CreateIORequest(mp, sizeof(struct timerequest));
        if (tr) {
            BYTE err;
            P_LONG("CreateIORequest ln_Type", tr->tr_node.io_Message.mn_Node.ln_Type);
            P_LONG("CreateIORequest mn_Length", tr->tr_node.io_Message.mn_Length);
            P_LONG("CreateIORequest io_Error", tr->tr_node.io_Error);
            err = OpenDevice((STRPTR)"lxaprobe-missing.device", 0, (struct IORequest *)tr, 0);
            P_LONG("OpenDevice(missing)", err);
            P_LONG("io_Error", tr->tr_node.io_Error);
            P_NULL("io_Device", tr->tr_node.io_Device);
            err = OpenDevice((STRPTR)"timer.device", 99, (struct IORequest *)tr, 0);
            P_LONG("OpenDevice(timer, unit 99)", err);
            P_LONG("io_Error", tr->tr_node.io_Error);
            err = OpenDevice((STRPTR)"timer.device", UNIT_VBLANK, (struct IORequest *)tr, 0);
            P_LONG("OpenDevice(timer, VBLANK)", err);
            P_LONG("io_Error", tr->tr_node.io_Error);
            P_NULL("io_Device", tr->tr_node.io_Device);
            P_LONG("ln_Type of request", tr->tr_node.io_Message.mn_Node.ln_Type);
            if (!err) {
                struct Device *dev = tr->tr_node.io_Device;
                ULONG c0 = dev->dd_Library.lib_OpenCnt;
                P_LONG("device ln_Type", dev->dd_Library.lib_Node.ln_Type);
                P_STR("device name", dev->dd_Library.lib_Node.ln_Name);
                tr->tr_node.io_Command = TR_ADDREQUEST;
                tr->tr_time.tv_secs = 0;
                tr->tr_time.tv_micro = 1000;
                P_LONG("DoIO(TR_ADDREQUEST)", DoIO((struct IORequest *)tr));
                P_LONG("ln_Type after DoIO", tr->tr_node.io_Message.mn_Node.ln_Type);
                tr->tr_node.io_Command = 0x7fff;
                P_LONG("DoIO(bad command)", DoIO((struct IORequest *)tr));
                P_LONG("io_Error", tr->tr_node.io_Error);
                CloseDevice((struct IORequest *)tr);
                P_LONG("lib_OpenCnt delta after CloseDevice", (LONG)dev->dd_Library.lib_OpenCnt - (LONG)c0);
            }
            DeleteIORequest((struct IORequest *)tr);
        }
        P_NULL("CreateIORequest(NULL port)", CreateIORequest(NULL, 48));
        DeleteIORequest(NULL);
        P_LONG("DeleteIORequest(NULL) survived", 1);
        DeleteMsgPort(mp);
    }

    P_SECTION("resources/residents");
    P_NULL("OpenResource(missing)", OpenResource((STRPTR)"lxaprobe.resource"));
    P_NULL("OpenResource(ciaa.resource)", OpenResource((STRPTR)"ciaa.resource"));
    P_NULL("FindResident(exec.library)", FindResident((STRPTR)"exec.library"));
    P_NULL("FindResident(missing)", FindResident((STRPTR)"lxaprobe.library"));
    {
        struct Resident *r = FindResident((STRPTR)"exec.library");
        if (r) {
            P_HEX("rt_MatchWord", r->rt_MatchWord);
            P_BOOL("rt_MatchTag points to itself", r->rt_MatchTag == r);
            P_LONG("rt_Type", r->rt_Type);
            P_LONG("rt_Version", r->rt_Version);
        }
    }
    return 0;
}
