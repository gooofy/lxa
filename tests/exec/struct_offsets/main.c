/*
 * AmigaOS Struct Offset Verification Tests
 *
 * Phase 78-W: Structural Verification — OS Data Structure Offsets
 *
 * Verifies that all key AmigaOS data structures have correct sizes and
 * field offsets by comparing compiled offsetof() results against the
 * NDK assembly definitions (.i files).
 *
 * This test catches:
 *   - Struct packing/alignment issues
 *   - Field ordering mistakes
 *   - Cross-compiler ABI deviations
 *   - NDK header vs assembly offset mismatches
 */

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <exec/libraries.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <exec/execbase.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <stddef.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;

static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

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

/*
 * CHECK_SIZEOF: verify struct size matches expected value from NDK .i file
 */
#define CHECK_SIZEOF(structname, expected) \
    do { \
        LONG actual = (LONG)sizeof(struct structname); \
        if (actual == (expected)) \
        { \
            test_pass++; \
        } \
        else \
        { \
            print("  FAIL: sizeof(" #structname ") = "); \
            print_num(actual); \
            print(", expected "); \
            print_num(expected); \
            print("\n"); \
            test_fail++; \
        } \
    } while (0)

/*
 * CHECK_OFFSET: verify field offset matches expected value from NDK .i file
 */
#define CHECK_OFFSET(structname, field, expected) \
    do { \
        LONG actual = (LONG)offsetof(struct structname, field); \
        if (actual == (expected)) \
        { \
            test_pass++; \
        } \
        else \
        { \
            print("  FAIL: offsetof(" #structname "." #field ") = "); \
            print_num(actual); \
            print(", expected "); \
            print_num(expected); \
            print("\n"); \
            test_fail++; \
        } \
    } while (0)

/* ================================================================
 * Exec Library Structs
 * ================================================================ */

static void test_node(void)
{
    print("--- Node / MinNode ---\n");

    /* MinNode: MLN_SIZE = 8 */
    CHECK_SIZEOF(MinNode, 8);
    CHECK_OFFSET(MinNode, mln_Succ, 0);
    CHECK_OFFSET(MinNode, mln_Pred, 4);

    /* Node: LN_SIZE = 14 */
    CHECK_SIZEOF(Node, 14);
    CHECK_OFFSET(Node, ln_Succ, 0);
    CHECK_OFFSET(Node, ln_Pred, 4);
    CHECK_OFFSET(Node, ln_Type, 8);
    CHECK_OFFSET(Node, ln_Pri, 9);
    CHECK_OFFSET(Node, ln_Name, 10);
}

static void test_list(void)
{
    print("--- List / MinList ---\n");

    /* MinList: MLH_SIZE = 12 */
    CHECK_SIZEOF(MinList, 12);
    CHECK_OFFSET(MinList, mlh_Head, 0);
    CHECK_OFFSET(MinList, mlh_Tail, 4);
    CHECK_OFFSET(MinList, mlh_TailPred, 8);

    /* List: LH_SIZE = 14 */
    CHECK_SIZEOF(List, 14);
    CHECK_OFFSET(List, lh_Head, 0);
    CHECK_OFFSET(List, lh_Tail, 4);
    CHECK_OFFSET(List, lh_TailPred, 8);
    CHECK_OFFSET(List, lh_Type, 12);
    CHECK_OFFSET(List, l_pad, 13);
}

static void test_msgport(void)
{
    print("--- MsgPort / Message ---\n");

    /* Message: MN_SIZE = 20 */
    CHECK_SIZEOF(Message, 20);
    CHECK_OFFSET(Message, mn_Node, 0);
    CHECK_OFFSET(Message, mn_ReplyPort, 14);
    CHECK_OFFSET(Message, mn_Length, 18);

    /* MsgPort: MP_SIZE = 34 */
    CHECK_SIZEOF(MsgPort, 34);
    CHECK_OFFSET(MsgPort, mp_Node, 0);
    CHECK_OFFSET(MsgPort, mp_Flags, 14);
    CHECK_OFFSET(MsgPort, mp_SigBit, 15);
    CHECK_OFFSET(MsgPort, mp_SigTask, 16);
    CHECK_OFFSET(MsgPort, mp_MsgList, 20);
}

static void test_task(void)
{
    print("--- Task ---\n");

    /* Task: TC_SIZE = 92 */
    CHECK_SIZEOF(Task, 92);
    CHECK_OFFSET(Task, tc_Node, 0);
    CHECK_OFFSET(Task, tc_Flags, 14);
    CHECK_OFFSET(Task, tc_State, 15);
    CHECK_OFFSET(Task, tc_IDNestCnt, 16);
    CHECK_OFFSET(Task, tc_TDNestCnt, 17);
    CHECK_OFFSET(Task, tc_SigAlloc, 18);
    CHECK_OFFSET(Task, tc_SigWait, 22);
    CHECK_OFFSET(Task, tc_SigRecvd, 26);
    CHECK_OFFSET(Task, tc_SigExcept, 30);
    CHECK_OFFSET(Task, tc_TrapAlloc, 34);
    CHECK_OFFSET(Task, tc_TrapAble, 36);
    CHECK_OFFSET(Task, tc_ExceptData, 38);
    CHECK_OFFSET(Task, tc_ExceptCode, 42);
    CHECK_OFFSET(Task, tc_TrapData, 46);
    CHECK_OFFSET(Task, tc_TrapCode, 50);
    CHECK_OFFSET(Task, tc_SPReg, 54);
    CHECK_OFFSET(Task, tc_SPLower, 58);
    CHECK_OFFSET(Task, tc_SPUpper, 62);
    CHECK_OFFSET(Task, tc_Switch, 66);
    CHECK_OFFSET(Task, tc_Launch, 70);
    CHECK_OFFSET(Task, tc_MemEntry, 74);
    CHECK_OFFSET(Task, tc_UserData, 88);
}

static void test_library(void)
{
    print("--- Library ---\n");

    /* Library: LIB_SIZE = 34 */
    CHECK_SIZEOF(Library, 34);
    CHECK_OFFSET(Library, lib_Node, 0);
    CHECK_OFFSET(Library, lib_Flags, 14);
    CHECK_OFFSET(Library, lib_pad, 15);
    CHECK_OFFSET(Library, lib_NegSize, 16);
    CHECK_OFFSET(Library, lib_PosSize, 18);
    CHECK_OFFSET(Library, lib_Version, 20);
    CHECK_OFFSET(Library, lib_Revision, 22);
    CHECK_OFFSET(Library, lib_IdString, 24);
    CHECK_OFFSET(Library, lib_Sum, 28);
    CHECK_OFFSET(Library, lib_OpenCnt, 32);
}

static void test_io(void)
{
    print("--- IORequest / IOStdReq ---\n");

    /* IORequest: IO_SIZE = 32 */
    CHECK_SIZEOF(IORequest, 32);
    CHECK_OFFSET(IORequest, io_Message, 0);
    CHECK_OFFSET(IORequest, io_Device, 20);
    CHECK_OFFSET(IORequest, io_Unit, 24);
    CHECK_OFFSET(IORequest, io_Command, 28);
    CHECK_OFFSET(IORequest, io_Flags, 30);
    CHECK_OFFSET(IORequest, io_Error, 31);

    /* IOStdReq: IOSTD_SIZE = 48 */
    CHECK_SIZEOF(IOStdReq, 48);
    CHECK_OFFSET(IOStdReq, io_Message, 0);
    CHECK_OFFSET(IOStdReq, io_Device, 20);
    CHECK_OFFSET(IOStdReq, io_Unit, 24);
    CHECK_OFFSET(IOStdReq, io_Command, 28);
    CHECK_OFFSET(IOStdReq, io_Flags, 30);
    CHECK_OFFSET(IOStdReq, io_Error, 31);
    CHECK_OFFSET(IOStdReq, io_Actual, 32);
    CHECK_OFFSET(IOStdReq, io_Length, 36);
    CHECK_OFFSET(IOStdReq, io_Data, 40);
    CHECK_OFFSET(IOStdReq, io_Offset, 44);
}

static void test_memory(void)
{
    print("--- MemChunk / MemHeader ---\n");

    /* MemChunk: MC_SIZE = 8 */
    CHECK_SIZEOF(MemChunk, 8);
    CHECK_OFFSET(MemChunk, mc_Next, 0);
    CHECK_OFFSET(MemChunk, mc_Bytes, 4);

    /* MemHeader: MH_SIZE = 32 */
    CHECK_SIZEOF(MemHeader, 32);
    CHECK_OFFSET(MemHeader, mh_Node, 0);
    CHECK_OFFSET(MemHeader, mh_Attributes, 14);
    CHECK_OFFSET(MemHeader, mh_First, 16);
    CHECK_OFFSET(MemHeader, mh_Lower, 20);
    CHECK_OFFSET(MemHeader, mh_Upper, 24);
    CHECK_OFFSET(MemHeader, mh_Free, 28);
}

static void test_interrupt(void)
{
    print("--- Interrupt / IntVector / SoftIntList ---\n");

    /* Interrupt: IS_SIZE = 22 */
    CHECK_SIZEOF(Interrupt, 22);
    CHECK_OFFSET(Interrupt, is_Node, 0);
    CHECK_OFFSET(Interrupt, is_Data, 14);
    CHECK_OFFSET(Interrupt, is_Code, 18);

    /* IntVector: IV_SIZE = 12 */
    CHECK_SIZEOF(IntVector, 12);
    CHECK_OFFSET(IntVector, iv_Data, 0);
    CHECK_OFFSET(IntVector, iv_Code, 4);
    CHECK_OFFSET(IntVector, iv_Node, 8);

    /* SoftIntList: SH_SIZE = 16 */
    CHECK_SIZEOF(SoftIntList, 16);
    CHECK_OFFSET(SoftIntList, sh_List, 0);
    CHECK_OFFSET(SoftIntList, sh_Pad, 14);
}

static void test_execbase(void)
{
    print("--- ExecBase ---\n");

    /* ExecBase: SYSBASESIZE = 632 */
    CHECK_SIZEOF(ExecBase, 632);
    CHECK_OFFSET(ExecBase, LibNode, 0);
    CHECK_OFFSET(ExecBase, SoftVer, 34);
    CHECK_OFFSET(ExecBase, LowMemChkSum, 36);
    CHECK_OFFSET(ExecBase, ChkBase, 38);
    CHECK_OFFSET(ExecBase, ColdCapture, 42);
    CHECK_OFFSET(ExecBase, CoolCapture, 46);
    CHECK_OFFSET(ExecBase, WarmCapture, 50);
    CHECK_OFFSET(ExecBase, SysStkUpper, 54);
    CHECK_OFFSET(ExecBase, SysStkLower, 58);
    CHECK_OFFSET(ExecBase, MaxLocMem, 62);
    CHECK_OFFSET(ExecBase, DebugEntry, 66);
    CHECK_OFFSET(ExecBase, DebugData, 70);
    CHECK_OFFSET(ExecBase, AlertData, 74);
    CHECK_OFFSET(ExecBase, MaxExtMem, 78);
    CHECK_OFFSET(ExecBase, ChkSum, 82);

    /* IntVects: 16 x IntVector(12) = 192 bytes at offset 84 */
    CHECK_OFFSET(ExecBase, IntVects, 84);

    CHECK_OFFSET(ExecBase, ThisTask, 276);
    CHECK_OFFSET(ExecBase, IdleCount, 280);
    CHECK_OFFSET(ExecBase, DispCount, 284);
    CHECK_OFFSET(ExecBase, Quantum, 288);
    CHECK_OFFSET(ExecBase, Elapsed, 290);
    CHECK_OFFSET(ExecBase, SysFlags, 292);
    CHECK_OFFSET(ExecBase, IDNestCnt, 294);
    CHECK_OFFSET(ExecBase, TDNestCnt, 295);
    CHECK_OFFSET(ExecBase, AttnFlags, 296);
    CHECK_OFFSET(ExecBase, AttnResched, 298);
    CHECK_OFFSET(ExecBase, ResModules, 300);
    CHECK_OFFSET(ExecBase, TaskTrapCode, 304);
    CHECK_OFFSET(ExecBase, TaskExceptCode, 308);
    CHECK_OFFSET(ExecBase, TaskExitCode, 312);
    CHECK_OFFSET(ExecBase, TaskSigAlloc, 316);
    CHECK_OFFSET(ExecBase, TaskTrapAlloc, 320);

    /* System lists: 8 x List(14) = 112 bytes starting at 322 */
    CHECK_OFFSET(ExecBase, MemList, 322);
    CHECK_OFFSET(ExecBase, ResourceList, 336);
    CHECK_OFFSET(ExecBase, DeviceList, 350);
    CHECK_OFFSET(ExecBase, IntrList, 364);
    CHECK_OFFSET(ExecBase, LibList, 378);
    CHECK_OFFSET(ExecBase, PortList, 392);
    CHECK_OFFSET(ExecBase, TaskReady, 406);
    CHECK_OFFSET(ExecBase, TaskWait, 420);

    /* SoftInts: 5 x SoftIntList(16) = 80 bytes starting at 434 */
    CHECK_OFFSET(ExecBase, SoftInts, 434);

    /* LastAlert: 4 x LONG = 16 bytes at 514 */
    CHECK_OFFSET(ExecBase, LastAlert, 514);

    CHECK_OFFSET(ExecBase, VBlankFrequency, 530);
    CHECK_OFFSET(ExecBase, PowerSupplyFrequency, 531);

    CHECK_OFFSET(ExecBase, SemaphoreList, 532);

    CHECK_OFFSET(ExecBase, KickMemPtr, 546);
    CHECK_OFFSET(ExecBase, KickTagPtr, 550);
    CHECK_OFFSET(ExecBase, KickCheckSum, 554);

    CHECK_OFFSET(ExecBase, ex_Pad0, 558);
    CHECK_OFFSET(ExecBase, ex_LaunchPoint, 560);
    CHECK_OFFSET(ExecBase, ex_RamLibPrivate, 564);
    CHECK_OFFSET(ExecBase, ex_EClockFrequency, 568);
    CHECK_OFFSET(ExecBase, ex_CacheControl, 572);
    CHECK_OFFSET(ExecBase, ex_TaskID, 576);

    CHECK_OFFSET(ExecBase, ex_MemHandlers, 616);
    CHECK_OFFSET(ExecBase, ex_MemHandler, 628);
}

static void test_semaphore(void)
{
    print("--- SignalSemaphore ---\n");

    CHECK_OFFSET(SignalSemaphore, ss_Link, 0);
    CHECK_OFFSET(SignalSemaphore, ss_NestCount, 14);
    CHECK_OFFSET(SignalSemaphore, ss_WaitQueue, 16);
    CHECK_OFFSET(SignalSemaphore, ss_MultipleLink, 28);
    CHECK_OFFSET(SignalSemaphore, ss_Owner, 40);
    CHECK_OFFSET(SignalSemaphore, ss_QueueCount, 44);
}

/* ================================================================
 * DOS Library Structs
 * ================================================================ */

static void test_datestamp(void)
{
    print("--- DateStamp ---\n");

    CHECK_SIZEOF(DateStamp, 12);
    CHECK_OFFSET(DateStamp, ds_Days, 0);
    CHECK_OFFSET(DateStamp, ds_Minute, 4);
    CHECK_OFFSET(DateStamp, ds_Tick, 8);
}

static void test_fib(void)
{
    print("--- FileInfoBlock ---\n");

    CHECK_SIZEOF(FileInfoBlock, 260);
    CHECK_OFFSET(FileInfoBlock, fib_DiskKey, 0);
    CHECK_OFFSET(FileInfoBlock, fib_DirEntryType, 4);
    CHECK_OFFSET(FileInfoBlock, fib_FileName, 8);
    CHECK_OFFSET(FileInfoBlock, fib_Protection, 116);
    CHECK_OFFSET(FileInfoBlock, fib_EntryType, 120);
    CHECK_OFFSET(FileInfoBlock, fib_Size, 124);
    CHECK_OFFSET(FileInfoBlock, fib_NumBlocks, 128);
    CHECK_OFFSET(FileInfoBlock, fib_Date, 132);
    CHECK_OFFSET(FileInfoBlock, fib_Comment, 144);
    CHECK_OFFSET(FileInfoBlock, fib_OwnerUID, 224);
    CHECK_OFFSET(FileInfoBlock, fib_OwnerGID, 226);
}

static void test_filelock(void)
{
    print("--- FileLock ---\n");

    CHECK_SIZEOF(FileLock, 20);
    CHECK_OFFSET(FileLock, fl_Link, 0);
    CHECK_OFFSET(FileLock, fl_Key, 4);
    CHECK_OFFSET(FileLock, fl_Access, 8);
    CHECK_OFFSET(FileLock, fl_Task, 12);
    CHECK_OFFSET(FileLock, fl_Volume, 16);
}

static void test_process(void)
{
    print("--- Process ---\n");

    CHECK_OFFSET(Process, pr_Task, 0);
    CHECK_OFFSET(Process, pr_MsgPort, 92);
    CHECK_OFFSET(Process, pr_Pad, 126);
    CHECK_OFFSET(Process, pr_SegList, 128);
    CHECK_OFFSET(Process, pr_StackSize, 132);
    CHECK_OFFSET(Process, pr_GlobVec, 136);
    CHECK_OFFSET(Process, pr_TaskNum, 140);
    CHECK_OFFSET(Process, pr_StackBase, 144);
    CHECK_OFFSET(Process, pr_Result2, 148);
    CHECK_OFFSET(Process, pr_CurrentDir, 152);
    CHECK_OFFSET(Process, pr_CIS, 156);
    CHECK_OFFSET(Process, pr_COS, 160);
    CHECK_OFFSET(Process, pr_ConsoleTask, 164);
    CHECK_OFFSET(Process, pr_FileSystemTask, 168);
    CHECK_OFFSET(Process, pr_CLI, 172);
    CHECK_OFFSET(Process, pr_ReturnAddr, 176);
    CHECK_OFFSET(Process, pr_PktWait, 180);
    CHECK_OFFSET(Process, pr_WindowPtr, 184);
    CHECK_OFFSET(Process, pr_HomeDir, 188);
    CHECK_OFFSET(Process, pr_Flags, 192);
    CHECK_OFFSET(Process, pr_ExitCode, 196);
    CHECK_OFFSET(Process, pr_ExitData, 200);
    CHECK_OFFSET(Process, pr_Arguments, 204);
    CHECK_OFFSET(Process, pr_LocalVars, 208);
    CHECK_OFFSET(Process, pr_ShellPrivate, 220);
    CHECK_OFFSET(Process, pr_CES, 224);
}

static void test_cli(void)
{
    print("--- CommandLineInterface ---\n");

    CHECK_SIZEOF(CommandLineInterface, 64);
    CHECK_OFFSET(CommandLineInterface, cli_Result2, 0);
    CHECK_OFFSET(CommandLineInterface, cli_SetName, 4);
    CHECK_OFFSET(CommandLineInterface, cli_CommandDir, 8);
    CHECK_OFFSET(CommandLineInterface, cli_ReturnCode, 12);
    CHECK_OFFSET(CommandLineInterface, cli_CommandName, 16);
    CHECK_OFFSET(CommandLineInterface, cli_FailLevel, 20);
    CHECK_OFFSET(CommandLineInterface, cli_Prompt, 24);
    CHECK_OFFSET(CommandLineInterface, cli_StandardInput, 28);
    CHECK_OFFSET(CommandLineInterface, cli_CurrentInput, 32);
    CHECK_OFFSET(CommandLineInterface, cli_CommandFile, 36);
    CHECK_OFFSET(CommandLineInterface, cli_Interactive, 40);
    CHECK_OFFSET(CommandLineInterface, cli_Background, 44);
    CHECK_OFFSET(CommandLineInterface, cli_CurrentOutput, 48);
    CHECK_OFFSET(CommandLineInterface, cli_DefaultStack, 52);
    CHECK_OFFSET(CommandLineInterface, cli_StandardOutput, 56);
    CHECK_OFFSET(CommandLineInterface, cli_Module, 60);
}

static void test_dos_public(void)
{
    print("--- DosLibrary / RootNode / CliProcList / DosInfo / Segment ---\n");

    CHECK_SIZEOF(DosLibrary, 70);
    CHECK_OFFSET(DosLibrary, dl_lib, 0);
    CHECK_OFFSET(DosLibrary, dl_Root, 34);
    CHECK_OFFSET(DosLibrary, dl_GV, 38);
    CHECK_OFFSET(DosLibrary, dl_A2, 42);
    CHECK_OFFSET(DosLibrary, dl_A5, 46);
    CHECK_OFFSET(DosLibrary, dl_A6, 50);
    CHECK_OFFSET(DosLibrary, dl_Errors, 54);
    CHECK_OFFSET(DosLibrary, dl_TimeReq, 58);
    CHECK_OFFSET(DosLibrary, dl_UtilityBase, 62);
    CHECK_OFFSET(DosLibrary, dl_IntuitionBase, 66);

    CHECK_SIZEOF(RootNode, 56);
    CHECK_OFFSET(RootNode, rn_TaskArray, 0);
    CHECK_OFFSET(RootNode, rn_ConsoleSegment, 4);
    CHECK_OFFSET(RootNode, rn_Time, 8);
    CHECK_OFFSET(RootNode, rn_RestartSeg, 20);
    CHECK_OFFSET(RootNode, rn_Info, 24);
    CHECK_OFFSET(RootNode, rn_FileHandlerSegment, 28);
    CHECK_OFFSET(RootNode, rn_CliList, 32);
    CHECK_OFFSET(RootNode, rn_BootProc, 44);
    CHECK_OFFSET(RootNode, rn_ShellSegment, 48);
    CHECK_OFFSET(RootNode, rn_Flags, 52);

    CHECK_SIZEOF(CliProcList, 16);
    CHECK_OFFSET(CliProcList, cpl_Node, 0);
    CHECK_OFFSET(CliProcList, cpl_First, 8);
    CHECK_OFFSET(CliProcList, cpl_Array, 12);

    CHECK_SIZEOF(DosInfo, 158);
    CHECK_OFFSET(DosInfo, di_McName, 0);
    CHECK_OFFSET(DosInfo, di_DevInfo, 4);
    CHECK_OFFSET(DosInfo, di_Devices, 8);
    CHECK_OFFSET(DosInfo, di_Handlers, 12);
    CHECK_OFFSET(DosInfo, di_NetHand, 16);
    CHECK_OFFSET(DosInfo, di_DevLock, 20);
    CHECK_OFFSET(DosInfo, di_EntryLock, 66);
    CHECK_OFFSET(DosInfo, di_DeleteLock, 112);

    CHECK_SIZEOF(Segment, 16);
    CHECK_OFFSET(Segment, seg_Next, 0);
    CHECK_OFFSET(Segment, seg_UC, 4);
    CHECK_OFFSET(Segment, seg_Seg, 8);
    CHECK_OFFSET(Segment, seg_Name, 12);
}

static void test_dospacket(void)
{
    print("--- DosPacket ---\n");

    CHECK_SIZEOF(DosPacket, 48);
    CHECK_OFFSET(DosPacket, dp_Link, 0);
    CHECK_OFFSET(DosPacket, dp_Port, 4);
    CHECK_OFFSET(DosPacket, dp_Type, 8);
    CHECK_OFFSET(DosPacket, dp_Res1, 12);
    CHECK_OFFSET(DosPacket, dp_Res2, 16);
    CHECK_OFFSET(DosPacket, dp_Arg1, 20);
    CHECK_OFFSET(DosPacket, dp_Arg2, 24);
    CHECK_OFFSET(DosPacket, dp_Arg3, 28);
    CHECK_OFFSET(DosPacket, dp_Arg4, 32);
    CHECK_OFFSET(DosPacket, dp_Arg5, 36);
    CHECK_OFFSET(DosPacket, dp_Arg6, 40);
    CHECK_OFFSET(DosPacket, dp_Arg7, 44);
}

/* ================================================================
 * Graphics Library Structs
 * ================================================================ */

static void test_bitmap(void)
{
    print("--- BitMap ---\n");

    CHECK_SIZEOF(BitMap, 40);
    CHECK_OFFSET(BitMap, BytesPerRow, 0);
    CHECK_OFFSET(BitMap, Rows, 2);
    CHECK_OFFSET(BitMap, Flags, 4);
    CHECK_OFFSET(BitMap, Depth, 5);
    CHECK_OFFSET(BitMap, pad, 6);
    CHECK_OFFSET(BitMap, Planes, 8);
}

static void test_gfxbase(void)
{
    print("--- GfxBase ---\n");

    CHECK_OFFSET(GfxBase, ActiView, 34);
    CHECK_OFFSET(GfxBase, MaxDisplayRow, 212);
    CHECK_OFFSET(GfxBase, MaxDisplayColumn, 214);
    CHECK_OFFSET(GfxBase, NormalDisplayRows, 216);
    CHECK_OFFSET(GfxBase, NormalDisplayColumns, 218);
    CHECK_OFFSET(GfxBase, ActiViewCprSemaphore, 412);
}

static void test_view_structs(void)
{
    print("--- ViewPort / RasInfo / ColorMap ---\n");

    CHECK_SIZEOF(ViewPort, 40);
    CHECK_OFFSET(ViewPort, Next, 0);
    CHECK_OFFSET(ViewPort, ColorMap, 4);
    CHECK_OFFSET(ViewPort, DspIns, 8);
    CHECK_OFFSET(ViewPort, SprIns, 12);
    CHECK_OFFSET(ViewPort, ClrIns, 16);
    CHECK_OFFSET(ViewPort, UCopIns, 20);
    CHECK_OFFSET(ViewPort, DWidth, 24);
    CHECK_OFFSET(ViewPort, DHeight, 26);
    CHECK_OFFSET(ViewPort, DxOffset, 28);
    CHECK_OFFSET(ViewPort, DyOffset, 30);
    CHECK_OFFSET(ViewPort, Modes, 32);
    CHECK_OFFSET(ViewPort, SpritePriorities, 34);
    CHECK_OFFSET(ViewPort, ExtendedModes, 35);
    CHECK_OFFSET(ViewPort, RasInfo, 36);

    CHECK_SIZEOF(RasInfo, 12);
    CHECK_OFFSET(RasInfo, Next, 0);
    CHECK_OFFSET(RasInfo, BitMap, 4);
    CHECK_OFFSET(RasInfo, RxOffset, 8);
    CHECK_OFFSET(RasInfo, RyOffset, 10);

    CHECK_SIZEOF(ColorMap, 52);
    CHECK_OFFSET(ColorMap, Flags, 0);
    CHECK_OFFSET(ColorMap, Type, 1);
    CHECK_OFFSET(ColorMap, Count, 2);
    CHECK_OFFSET(ColorMap, ColorTable, 4);
    CHECK_OFFSET(ColorMap, PalExtra, 40);
    CHECK_OFFSET(ColorMap, SpriteBase_Even, 44);
    CHECK_OFFSET(ColorMap, SpriteBase_Odd, 46);
    CHECK_OFFSET(ColorMap, Bp_0_base, 48);
    CHECK_OFFSET(ColorMap, Bp_1_base, 50);
}

static void test_graphics_helpers(void)
{
    print("--- AreaInfo / TmpRas / GelsInfo ---\n");

    CHECK_SIZEOF(AreaInfo, 24);
    CHECK_OFFSET(AreaInfo, VctrTbl, 0);
    CHECK_OFFSET(AreaInfo, VctrPtr, 4);
    CHECK_OFFSET(AreaInfo, FlagTbl, 8);
    CHECK_OFFSET(AreaInfo, FlagPtr, 12);
    CHECK_OFFSET(AreaInfo, Count, 16);
    CHECK_OFFSET(AreaInfo, MaxCount, 18);
    CHECK_OFFSET(AreaInfo, FirstX, 20);
    CHECK_OFFSET(AreaInfo, FirstY, 22);

    CHECK_SIZEOF(TmpRas, 8);
    CHECK_OFFSET(TmpRas, RasPtr, 0);
    CHECK_OFFSET(TmpRas, Size, 4);

    CHECK_SIZEOF(GelsInfo, 38);
    CHECK_OFFSET(GelsInfo, sprRsrvd, 0);
    CHECK_OFFSET(GelsInfo, Flags, 1);
    CHECK_OFFSET(GelsInfo, gelHead, 2);
    CHECK_OFFSET(GelsInfo, gelTail, 6);
    CHECK_OFFSET(GelsInfo, nextLine, 10);
    CHECK_OFFSET(GelsInfo, lastColor, 14);
    CHECK_OFFSET(GelsInfo, collHandler, 18);
    CHECK_OFFSET(GelsInfo, leftmost, 22);
    CHECK_OFFSET(GelsInfo, rightmost, 24);
    CHECK_OFFSET(GelsInfo, topmost, 26);
    CHECK_OFFSET(GelsInfo, bottommost, 28);
    CHECK_OFFSET(GelsInfo, firstBlissObj, 30);
    CHECK_OFFSET(GelsInfo, lastBlissObj, 34);
}

static void test_rastport(void)
{
    print("--- RastPort ---\n");

    CHECK_SIZEOF(RastPort, 100);
    CHECK_OFFSET(RastPort, Layer, 0);
    CHECK_OFFSET(RastPort, BitMap, 4);
    CHECK_OFFSET(RastPort, AreaPtrn, 8);
    CHECK_OFFSET(RastPort, TmpRas, 12);
    CHECK_OFFSET(RastPort, AreaInfo, 16);
    CHECK_OFFSET(RastPort, GelsInfo, 20);
    CHECK_OFFSET(RastPort, Mask, 24);
    CHECK_OFFSET(RastPort, FgPen, 25);
    CHECK_OFFSET(RastPort, BgPen, 26);
    CHECK_OFFSET(RastPort, AOlPen, 27);
    CHECK_OFFSET(RastPort, DrawMode, 28);
    CHECK_OFFSET(RastPort, AreaPtSz, 29);
    CHECK_OFFSET(RastPort, linpatcnt, 30);
    CHECK_OFFSET(RastPort, dummy, 31);
    CHECK_OFFSET(RastPort, Flags, 32);
    CHECK_OFFSET(RastPort, LinePtrn, 34);
    CHECK_OFFSET(RastPort, cp_x, 36);
    CHECK_OFFSET(RastPort, cp_y, 38);
    CHECK_OFFSET(RastPort, minterms, 40);
    CHECK_OFFSET(RastPort, PenWidth, 48);
    CHECK_OFFSET(RastPort, PenHeight, 50);
    CHECK_OFFSET(RastPort, Font, 52);
    CHECK_OFFSET(RastPort, AlgoStyle, 56);
    CHECK_OFFSET(RastPort, TxFlags, 57);
    CHECK_OFFSET(RastPort, TxHeight, 58);
    CHECK_OFFSET(RastPort, TxWidth, 60);
    CHECK_OFFSET(RastPort, TxBaseline, 62);
    CHECK_OFFSET(RastPort, TxSpacing, 64);
    CHECK_OFFSET(RastPort, RP_User, 66);
    CHECK_OFFSET(RastPort, longreserved, 70);
    CHECK_OFFSET(RastPort, wordreserved, 78);
    CHECK_OFFSET(RastPort, reserved, 92);
}

static void test_textfont(void)
{
    print("--- TextAttr / TextFont ---\n");

    /* TextAttr: ta_SIZEOF = 8 */
    CHECK_SIZEOF(TextAttr, 8);
    CHECK_OFFSET(TextAttr, ta_Name, 0);
    CHECK_OFFSET(TextAttr, ta_YSize, 4);
    CHECK_OFFSET(TextAttr, ta_Style, 6);
    CHECK_OFFSET(TextAttr, ta_Flags, 7);

    /* TextFont: tf_SIZEOF = 52 */
    CHECK_SIZEOF(TextFont, 52);
    CHECK_OFFSET(TextFont, tf_Message, 0);
    CHECK_OFFSET(TextFont, tf_YSize, 20);
    CHECK_OFFSET(TextFont, tf_Style, 22);
    CHECK_OFFSET(TextFont, tf_Flags, 23);
    CHECK_OFFSET(TextFont, tf_XSize, 24);
    CHECK_OFFSET(TextFont, tf_Baseline, 26);
    CHECK_OFFSET(TextFont, tf_BoldSmear, 28);
    CHECK_OFFSET(TextFont, tf_Accessors, 30);
    CHECK_OFFSET(TextFont, tf_LoChar, 32);
    CHECK_OFFSET(TextFont, tf_HiChar, 33);
    CHECK_OFFSET(TextFont, tf_CharData, 34);
    CHECK_OFFSET(TextFont, tf_Modulo, 38);
    CHECK_OFFSET(TextFont, tf_CharLoc, 40);
    CHECK_OFFSET(TextFont, tf_CharSpace, 44);
    CHECK_OFFSET(TextFont, tf_CharKern, 48);
}

/* ================================================================
 * Intuition Library Structs
 * ================================================================ */

static void test_intuitionbase(void)
{
    print("--- IntuitionBase ---\n");

    CHECK_SIZEOF(IntuitionBase, 80);
    CHECK_OFFSET(IntuitionBase, LibNode, 0);
    CHECK_OFFSET(IntuitionBase, ViewLord, 34);
    CHECK_OFFSET(IntuitionBase, ActiveWindow, 52);
    CHECK_OFFSET(IntuitionBase, ActiveScreen, 56);
    CHECK_OFFSET(IntuitionBase, FirstScreen, 60);
    CHECK_OFFSET(IntuitionBase, Flags, 64);
    CHECK_OFFSET(IntuitionBase, MouseY, 68);
    CHECK_OFFSET(IntuitionBase, MouseX, 70);
    CHECK_OFFSET(IntuitionBase, Seconds, 72);
    CHECK_OFFSET(IntuitionBase, Micros, 76);
}

static void test_screen(void)
{
    print("--- Screen ---\n");

    CHECK_SIZEOF(Screen, 346);
    CHECK_OFFSET(Screen, NextScreen, 0);
    CHECK_OFFSET(Screen, FirstWindow, 4);
    CHECK_OFFSET(Screen, LeftEdge, 8);
    CHECK_OFFSET(Screen, TopEdge, 10);
    CHECK_OFFSET(Screen, Width, 12);
    CHECK_OFFSET(Screen, Height, 14);
    CHECK_OFFSET(Screen, MouseY, 16);
    CHECK_OFFSET(Screen, MouseX, 18);
    CHECK_OFFSET(Screen, Flags, 20);
    CHECK_OFFSET(Screen, Title, 22);
    CHECK_OFFSET(Screen, DefaultTitle, 26);
    CHECK_OFFSET(Screen, BarHeight, 30);
    CHECK_OFFSET(Screen, BarVBorder, 31);
    CHECK_OFFSET(Screen, BarHBorder, 32);
    CHECK_OFFSET(Screen, MenuVBorder, 33);
    CHECK_OFFSET(Screen, MenuHBorder, 34);
    CHECK_OFFSET(Screen, WBorTop, 35);
    CHECK_OFFSET(Screen, WBorLeft, 36);
    CHECK_OFFSET(Screen, WBorRight, 37);
    CHECK_OFFSET(Screen, WBorBottom, 38);
    CHECK_OFFSET(Screen, Font, 40);
    CHECK_OFFSET(Screen, ViewPort, 44);
    CHECK_OFFSET(Screen, RastPort, 84);
    CHECK_OFFSET(Screen, BitMap, 184);
    CHECK_OFFSET(Screen, LayerInfo, 224);
    CHECK_OFFSET(Screen, FirstGadget, 326);
    CHECK_OFFSET(Screen, DetailPen, 330);
    CHECK_OFFSET(Screen, BlockPen, 331);
    CHECK_OFFSET(Screen, SaveColor0, 332);
    CHECK_OFFSET(Screen, BarLayer, 334);
    CHECK_OFFSET(Screen, ExtData, 338);
    CHECK_OFFSET(Screen, UserData, 342);
}

static void test_requester(void)
{
    print("--- Requester ---\n");

    CHECK_SIZEOF(Requester, 112);
    CHECK_OFFSET(Requester, OlderRequest, 0);
    CHECK_OFFSET(Requester, LeftEdge, 4);
    CHECK_OFFSET(Requester, TopEdge, 6);
    CHECK_OFFSET(Requester, Width, 8);
    CHECK_OFFSET(Requester, Height, 10);
    CHECK_OFFSET(Requester, RelLeft, 12);
    CHECK_OFFSET(Requester, RelTop, 14);
    CHECK_OFFSET(Requester, ReqGadget, 16);
    CHECK_OFFSET(Requester, ReqBorder, 20);
    CHECK_OFFSET(Requester, ReqText, 24);
    CHECK_OFFSET(Requester, Flags, 28);
    CHECK_OFFSET(Requester, BackFill, 30);
    CHECK_OFFSET(Requester, ReqLayer, 32);
    CHECK_OFFSET(Requester, ImageBMap, 68);
    CHECK_OFFSET(Requester, RWindow, 72);
    CHECK_OFFSET(Requester, ReqImage, 76);
}

static void test_menu(void)
{
    print("--- Menu / MenuItem ---\n");

    /* Menu: mu_SIZEOF = 30 */
    CHECK_SIZEOF(Menu, 30);
    CHECK_OFFSET(Menu, NextMenu, 0);
    CHECK_OFFSET(Menu, LeftEdge, 4);
    CHECK_OFFSET(Menu, TopEdge, 6);
    CHECK_OFFSET(Menu, Width, 8);
    CHECK_OFFSET(Menu, Height, 10);
    CHECK_OFFSET(Menu, Flags, 12);
    CHECK_OFFSET(Menu, MenuName, 14);
    CHECK_OFFSET(Menu, FirstItem, 18);
    CHECK_OFFSET(Menu, JazzX, 22);
    CHECK_OFFSET(Menu, JazzY, 24);
    CHECK_OFFSET(Menu, BeatX, 26);
    CHECK_OFFSET(Menu, BeatY, 28);

    /* MenuItem: mi_SIZEOF = 34 */
    CHECK_SIZEOF(MenuItem, 34);
    CHECK_OFFSET(MenuItem, NextItem, 0);
    CHECK_OFFSET(MenuItem, LeftEdge, 4);
    CHECK_OFFSET(MenuItem, TopEdge, 6);
    CHECK_OFFSET(MenuItem, Width, 8);
    CHECK_OFFSET(MenuItem, Height, 10);
    CHECK_OFFSET(MenuItem, Flags, 12);
    CHECK_OFFSET(MenuItem, MutualExclude, 14);
    CHECK_OFFSET(MenuItem, ItemFill, 18);
    CHECK_OFFSET(MenuItem, SelectFill, 22);
    CHECK_OFFSET(MenuItem, Command, 26);
    CHECK_OFFSET(MenuItem, SubItem, 28);
    CHECK_OFFSET(MenuItem, NextSelect, 32);
}

static void test_gadget(void)
{
    print("--- Gadget ---\n");

    /* Gadget: gg_SIZEOF = 44 */
    CHECK_SIZEOF(Gadget, 44);
    CHECK_OFFSET(Gadget, NextGadget, 0);
    CHECK_OFFSET(Gadget, LeftEdge, 4);
    CHECK_OFFSET(Gadget, TopEdge, 6);
    CHECK_OFFSET(Gadget, Width, 8);
    CHECK_OFFSET(Gadget, Height, 10);
    CHECK_OFFSET(Gadget, Flags, 12);
    CHECK_OFFSET(Gadget, Activation, 14);
    CHECK_OFFSET(Gadget, GadgetType, 16);
    CHECK_OFFSET(Gadget, GadgetRender, 18);
    CHECK_OFFSET(Gadget, SelectRender, 22);
    CHECK_OFFSET(Gadget, GadgetText, 26);
    CHECK_OFFSET(Gadget, MutualExclude, 30);
    CHECK_OFFSET(Gadget, SpecialInfo, 34);
    CHECK_OFFSET(Gadget, GadgetID, 38);
    CHECK_OFFSET(Gadget, UserData, 40);
}

static void test_propinfo(void)
{
    print("--- PropInfo ---\n");

    /* PropInfo: pi_SIZEOF = 22 */
    CHECK_SIZEOF(PropInfo, 22);
    CHECK_OFFSET(PropInfo, Flags, 0);
    CHECK_OFFSET(PropInfo, HorizPot, 2);
    CHECK_OFFSET(PropInfo, VertPot, 4);
    CHECK_OFFSET(PropInfo, HorizBody, 6);
    CHECK_OFFSET(PropInfo, VertBody, 8);
    CHECK_OFFSET(PropInfo, CWidth, 10);
    CHECK_OFFSET(PropInfo, CHeight, 12);
    CHECK_OFFSET(PropInfo, HPotRes, 14);
    CHECK_OFFSET(PropInfo, VPotRes, 16);
    CHECK_OFFSET(PropInfo, LeftBorder, 18);
    CHECK_OFFSET(PropInfo, TopBorder, 20);
}

static void test_stringinfo(void)
{
    print("--- StringInfo ---\n");

    /* StringInfo: si_SIZEOF = 36 */
    CHECK_SIZEOF(StringInfo, 36);
    CHECK_OFFSET(StringInfo, Buffer, 0);
    CHECK_OFFSET(StringInfo, UndoBuffer, 4);
    CHECK_OFFSET(StringInfo, BufferPos, 8);
    CHECK_OFFSET(StringInfo, MaxChars, 10);
    CHECK_OFFSET(StringInfo, DispPos, 12);
    CHECK_OFFSET(StringInfo, UndoPos, 14);
    CHECK_OFFSET(StringInfo, NumChars, 16);
    CHECK_OFFSET(StringInfo, DispCount, 18);
    CHECK_OFFSET(StringInfo, CLeft, 20);
    CHECK_OFFSET(StringInfo, CTop, 22);
    CHECK_OFFSET(StringInfo, Extension, 24);
    CHECK_OFFSET(StringInfo, LongInt, 28);
    CHECK_OFFSET(StringInfo, AltKeyMap, 32);
}

static void test_intuitext(void)
{
    print("--- IntuiText ---\n");

    /* IntuiText: it_SIZEOF = 20 */
    CHECK_SIZEOF(IntuiText, 20);
    CHECK_OFFSET(IntuiText, FrontPen, 0);
    CHECK_OFFSET(IntuiText, BackPen, 1);
    CHECK_OFFSET(IntuiText, DrawMode, 2);
    CHECK_OFFSET(IntuiText, LeftEdge, 4);
    CHECK_OFFSET(IntuiText, TopEdge, 6);
    CHECK_OFFSET(IntuiText, ITextFont, 8);
    CHECK_OFFSET(IntuiText, IText, 12);
    CHECK_OFFSET(IntuiText, NextText, 16);
}

static void test_border(void)
{
    print("--- Border ---\n");

    /* Border: bd_SIZEOF = 16 */
    CHECK_SIZEOF(Border, 16);
    CHECK_OFFSET(Border, LeftEdge, 0);
    CHECK_OFFSET(Border, TopEdge, 2);
    CHECK_OFFSET(Border, FrontPen, 4);
    CHECK_OFFSET(Border, BackPen, 5);
    CHECK_OFFSET(Border, DrawMode, 6);
    CHECK_OFFSET(Border, Count, 7);
    CHECK_OFFSET(Border, XY, 8);
    CHECK_OFFSET(Border, NextBorder, 12);
}

static void test_image(void)
{
    print("--- Image ---\n");

    /* Image: ig_SIZEOF = 20 */
    CHECK_SIZEOF(Image, 20);
    CHECK_OFFSET(Image, LeftEdge, 0);
    CHECK_OFFSET(Image, TopEdge, 2);
    CHECK_OFFSET(Image, Width, 4);
    CHECK_OFFSET(Image, Height, 6);
    CHECK_OFFSET(Image, Depth, 8);
    CHECK_OFFSET(Image, ImageData, 10);
    CHECK_OFFSET(Image, PlanePick, 14);
    CHECK_OFFSET(Image, PlaneOnOff, 15);
    CHECK_OFFSET(Image, NextImage, 16);
}

static void test_intuimessage(void)
{
    print("--- IntuiMessage ---\n");

    /* IntuiMessage: im_SIZEOF = 52 */
    CHECK_SIZEOF(IntuiMessage, 52);
    CHECK_OFFSET(IntuiMessage, ExecMessage, 0);
    CHECK_OFFSET(IntuiMessage, Class, 20);
    CHECK_OFFSET(IntuiMessage, Code, 24);
    CHECK_OFFSET(IntuiMessage, Qualifier, 26);
    CHECK_OFFSET(IntuiMessage, IAddress, 28);
    CHECK_OFFSET(IntuiMessage, MouseX, 32);
    CHECK_OFFSET(IntuiMessage, MouseY, 34);
    CHECK_OFFSET(IntuiMessage, Seconds, 36);
    CHECK_OFFSET(IntuiMessage, Micros, 40);
    CHECK_OFFSET(IntuiMessage, IDCMPWindow, 44);
    CHECK_OFFSET(IntuiMessage, SpecialLink, 48);
}

static void test_window(void)
{
    print("--- Window ---\n");

    /* Window: key public field offsets */
    CHECK_OFFSET(Window, NextWindow, 0);
    CHECK_OFFSET(Window, LeftEdge, 4);
    CHECK_OFFSET(Window, TopEdge, 6);
    CHECK_OFFSET(Window, Width, 8);
    CHECK_OFFSET(Window, Height, 10);
    CHECK_OFFSET(Window, MouseY, 12);
    CHECK_OFFSET(Window, MouseX, 14);
    CHECK_OFFSET(Window, MinWidth, 16);
    CHECK_OFFSET(Window, MinHeight, 18);
    CHECK_OFFSET(Window, MaxWidth, 20);
    CHECK_OFFSET(Window, MaxHeight, 22);
    CHECK_OFFSET(Window, Flags, 24);
    CHECK_OFFSET(Window, MenuStrip, 28);
    CHECK_OFFSET(Window, Title, 32);
    CHECK_OFFSET(Window, FirstRequest, 36);
    CHECK_OFFSET(Window, DMRequest, 40);
    CHECK_OFFSET(Window, ReqCount, 44);
    CHECK_OFFSET(Window, WScreen, 46);
    CHECK_OFFSET(Window, RPort, 50);
    CHECK_OFFSET(Window, BorderLeft, 54);
    CHECK_OFFSET(Window, BorderTop, 55);
    CHECK_OFFSET(Window, BorderRight, 56);
    CHECK_OFFSET(Window, BorderBottom, 57);
    CHECK_OFFSET(Window, BorderRPort, 58);
    CHECK_OFFSET(Window, FirstGadget, 62);
    CHECK_OFFSET(Window, Parent, 66);
    CHECK_OFFSET(Window, Descendant, 70);
    CHECK_OFFSET(Window, Pointer, 74);
    CHECK_OFFSET(Window, PtrHeight, 78);
    CHECK_OFFSET(Window, PtrWidth, 79);
    CHECK_OFFSET(Window, XOffset, 80);
    CHECK_OFFSET(Window, YOffset, 81);
    CHECK_OFFSET(Window, IDCMPFlags, 82);
    CHECK_OFFSET(Window, UserPort, 86);
    CHECK_OFFSET(Window, WindowPort, 90);
    CHECK_OFFSET(Window, MessageKey, 94);
    CHECK_OFFSET(Window, DetailPen, 98);
    CHECK_OFFSET(Window, BlockPen, 99);
    CHECK_OFFSET(Window, CheckMark, 100);
    CHECK_OFFSET(Window, ScreenTitle, 104);
    CHECK_OFFSET(Window, GZZMouseX, 108);
    CHECK_OFFSET(Window, GZZMouseY, 110);
    CHECK_OFFSET(Window, GZZWidth, 112);
    CHECK_OFFSET(Window, GZZHeight, 114);
    CHECK_OFFSET(Window, ExtData, 116);
    CHECK_OFFSET(Window, UserData, 120);
    CHECK_OFFSET(Window, WLayer, 124);
    CHECK_OFFSET(Window, IFont, 128);
    CHECK_OFFSET(Window, MoreFlags, 132);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void)
{
    out = Output();

    print("AmigaOS Struct Offset Verification\n");
    print("==================================\n\n");

    /* Exec structs */
    test_node();
    test_list();
    test_msgport();
    test_task();
    test_library();
    test_io();
    test_memory();
    test_interrupt();
    test_execbase();
    test_semaphore();

    /* DOS structs */
    test_datestamp();
    test_fib();
    test_filelock();
    test_process();
    test_cli();
    test_dospacket();
    test_dos_public();

    /* Graphics structs */
    test_bitmap();
    test_gfxbase();
    test_view_structs();
    test_graphics_helpers();
    test_rastport();
    test_textfont();

    /* Intuition structs */
    test_intuitionbase();
    test_screen();
    test_requester();
    test_menu();
    test_gadget();
    test_propinfo();
    test_stringinfo();
    test_intuitext();
    test_border();
    test_image();
    test_intuimessage();
    test_window();

    /* Summary */
    print("\n==================================\n");
    print("Checks passed: ");
    print_num(test_pass);
    print("\n");
    print("Checks failed: ");
    print_num(test_fail);
    print("\n");

    if (test_fail == 0)
    {
        print("\nPASS: All struct offset checks passed!\n");
        return 0;
    }
    else
    {
        print("\nFAIL: Some struct offset checks failed\n");
        return 20;
    }
}
