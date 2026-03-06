# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project!

---

## Current Status

**Version: 0.6.61** | **Phase 77 Complete** | **38/38 Tests Passing (GTest-only)**

Phase 77: Missing Libraries & Devices — complete.

**Current Status**:
- 38/38 ctest entries (all GTest), 4 new test cases (mathieeesingbas, trackdisk, audio, locale)
- **mathieeesingbas.library**: Full IEEE single-precision math (12 functions: Fix/Flt/Cmp/Tst/Abs/Neg/Add/Sub/Mul/Div/Floor/Ceil) via EMU_CALL. Fixed d2 register clobbering bug in two-operand asm stubs.
- **trackdisk.device**: Stub device with DD 3.5" floppy geometry, TD_MOTOR/TD_CHANGENUM/TD_CHANGESTATE/TD_PROTSTATUS/TD_GETGEOMETRY support.
- **audio.device**: Fixed Open() to succeed (was returning ADIOERR_NOALLOCATION).
- **diskfont.library**: Disk font loading from FONTS: directory, AFF_DISK support in AvailFonts.
- **locale.library**: FormatDate (all format codes), FormatString (printf-like with Amiga conventions), ParseDate.
- **mathieeedoubbas.library**: Fixed latent d2/d3/d4 register clobbering bug in all 11 asm stubs (same issue found and fixed in mathieeesingbas).

---

## Core Philosophy & Boundaries

### WINE-like DOS Approach
Instead of emulating hardware-level disk controllers and running Amiga-native filesystems (like OFS/FFS) on top, `lxa` maps Amiga logical drives directly to Linux directories.

- **Drives as Directories**: `DH0:`, `DF0:`, etc., are backed by standard Linux directories.
- **Host Interception**: DOS calls (`Open`, `Read`, `Lock`, etc.) are handled by the host `lxa` process using Linux system calls.
- **Auto-Provisioning**: On first run, `lxa` creates a default `~/.lxa` structure with `SYS:`, `S:`, `C:`, etc.

### Support Boundaries

| Feature | Status | Note |
| :--- | :--- | :--- |
| **Exec Multitasking** | ✅ Full | Signal-based multitasking, Message Ports, Semaphores |
| **DOS Filesystem** | ✅ Full | Mapped to Linux via VFS layer |
| **Interactive Shell** | ✅ Full | Amiga-compatible Shell with prompt, history, scripting |
| **Assigns** | ✅ Full | Logical assigns, multi-directory assigns (Paths) |
| **Graphics/Intuition** | ✅ 95% | SDL2-based rendering, windows, screens, gadgets |
| **Floppy Emulation** | ✅ Basic | Floppies appear as directories |

---

## Completed Phases (1-72)

### Foundation & Transitions (Phases 1-63)
- ✅ **Phases 1-62**: Core Exec, DOS, Graphics, Intuition, and Application support implementation.
- ✅ **Phase 63**: **Google Test & liblxa Transition** - Successfully migrated the core test suite to Google Test and integrated host-side drivers for major applications (ASM-One, Devpac, KickPascal, MaxonBASIC, Cluster2, DPaint V). Replaced legacy `test_inject.h` with robust, automated verification using `liblxa`.

### Test Suite Stabilization & Hardening (Phases 64-70a)
- ✅ **Phase 64**: Fixed standalone test drivers (updatestrgad_test, simplegtgadget_test) with interactive input/output matching.
- ✅ **Phase 65**: Fixed RawKey & input handling — IDCMP RAWKEY events now correctly delivered in GTest environment.
- ✅ **Phase 66**: Fixed Intuition & Console regressions — console.device timing, InputConsole/InputInject wait-for-marker logic, ConsoleAdvanced output.
- ✅ **Phase 67**: Fixed test environment & app paths — all app tests find their binaries correctly.
- ✅ **Phase 68**: Rewrote `Delay()` to use `timer.device` (UNIT_MICROHZ) instead of busy-yield loop. Optimized stress test parameters for reliable CI execution. Fixed ExecTest.Multitask timing.
- ✅ **Phase 69**: Fixed apps_misc_gtest — restructured DirectoryOpus test to handle missing dopus.library gracefully, converted KickPascal2 to load-only test (interactive testing via kickpascal_gtest), fixed SysInfo background-process timeout handling. All 3 app tests now pass.
- ✅ **Phase 70**: **Test Suite Hardening & Expansion** — GadTools visual rendering (bevel borders, text labels, 13 pixel/functional tests), palette pipeline fix (SetRGB4/LoadRGB4/SetRGB32/SetRGB32CM/LoadRGB32), string gadget rawkey/input fixes, rewrote 3 RKM samples, added 3 graphics clipping tests + 10 DOS VFS corner-case tests. DOS: 14→24, Graphics: 14→17. 49/49 ctest entries.
- ✅ **Phase 70a**: **Fix Issues Identified by Manual Tests** — All issues found by comparing RKM sample programs to running originals on a real Amiga have been fixed. GadToolsGadgets (underscore labels, slider, resize/depth/close gadgets, double-bevel borders, TAB cycling), SimpleGTGadget, SimpleGad, UpdateStrGad, IntuiText, SimpleImage, EasyRequest, SimpleMenu, MenuLayout, FileReq, FontReq. 54/54 ctest entries.

### Performance & Infrastructure (Phase 71)
- ✅ **Phase 71**: Fix ClipBlit layer cleanup crash (A5 frame pointer clobbering). `cpu_instr_callback` ~80x speedup via `g_debug_active` fast-path. Finalized GTest migration (removed 16 legacy C test drivers, 54→38 tests). 38/38 tests passing.

### Application Compatibility & Analysis (Phase 72)
- ✅ **Phase 72**: Implemented `dopus.library` stub (97 API functions). Extended KickPascal2 test to verify window opening. Fixed `console.device` CONU_LIBRARY handling. Added 68040 MMU register stubs (TC, ITT0/1, DTT0/1, MMUSR, URP, SRP in MOVEC handlers). Silenced PFLUSH stderr output. Comprehensive codebase audit: 93 FIXME/TODO markers catalogued (see audit below). 38/38 tests passing.

### Core Library Resource Management & Stability (Phase 73)
- ✅ **Phase 73**: Fixed library reference counting in all 6 libraries (graphics, intuition, utility, mathtrans, mathffp, expansion) — `lib_OpenCnt++` in Open, `lib_OpenCnt--` in Close, `LIBF_DELEXP` clearing. Implemented `Exception()` (LVO -66) with full tc_ExceptCode signal dispatch loop. Fixed `exceptions.s`: tc_Switch/tc_Launch callbacks, TF_EXCEPT handling in Schedule/Dispatch. Process initialization: pr_SegList, pr_ConsoleTask, pr_FileSystemTask inheritance, cli_Prompt/cli_CommandDir inheritance from parent CLI. Bootstrap FIXME cleanup. New Library ref counting test. 38/38 tests passing.

### DOS & VFS Hardening (Phase 74)
- ✅ **Phase 74**: Implemented `UnLoadSeg` (seglist chain walking with `FreeVec`) and `InternalUnLoadSeg` (custom freefunc). Added `DOS_EXALLCONTROL` and `DOS_STDPKT` to `AllocDosObject`/`FreeDosObject`. Full `errno2Amiga` mapping (12+ errno→AmigaOS codes). Case-insensitive path resolution via `vfs_resolve_case_path()`. 3 new m68k test programs. 35/35 tests passing.

### Advanced Graphics & Layers (Phase 75)
- ✅ **Phase 75**: Scan-line polygon fill in `AreaEnd` (even-odd fill rule, multi-polygon). Implemented all four pixel array functions (`ReadPixelLine8`, `ReadPixelArray8`, `WritePixelLine8`, `WritePixelArray8`) replacing `assert(FALSE)` stubs, with correct `UWORD` parameter types. `ScrollLayer` for non-SuperBitMap layers. 9 layers.library functions implemented (`BeginUpdate`, `EndUpdate`, `SwapBitsRastPortClipRect`, `MoveLayerInFrontOf`, `InstallClipRegion`, `SortLayerCR`, `DoHookClipRects`, `ShowLayer`). 3 new m68k test programs. 38/38 tests passing.

### Hardware Blitter Emulation & CLI Assigns (Phase 75b)
- ✅ **Phase 75b**: Full OCS/ECS hardware blitter emulation for applications that bypass graphics.library and program the blitter directly (e.g., Cluster2 Oberon-2 IDE). Implemented all blitter registers (BLTCON0/1, BLTAFWM/BLTALWM, BLTxPTH/L, BLTxMOD, BLTxDAT, BLTSIZE, BLTSIZV/H), `_blitter_execute()` with all 256 minterms, A/B barrel shift, first/last word masks, ascending/descending direction, inclusive-OR and exclusive-OR fill modes. Fixed `m68k_write_memory_32()` to route custom chip area writes as two 16-bit writes. Added `-a name=path` CLI flag for command-line assigns. New hw_blitter m68k test program (7 tests). 38/38 tests passing.

### Intuition & BOOPSI Enhancements (Phase 76)
- ✅ **Phase 76**: Full BOOPSI inter-object communication (icclass/modelclass/gadgetclass dispatchers with embedded ICData, `_boopsi_do_notify()` notification pipeline with ICA_TARGET/ICA_MAP tag mapping, loop prevention). propgclass/strgclass rewritten with INST_DATA for PropGData/StrGData. SetGadgetAttrsA/DoGadgetMethodA implemented (were stubs/crashes). ActivateWindow with proper deactivation. ZipWindow with WA_Zoom toggle. AutoRequest modal requester. 38/38 tests passing.

---

## Codebase Audit Results (Phase 72.5)

**93 markers total**: 83 in ROM code (`src/rom/`), 10 in host code (`src/lxa/`).

### Top Issues by Priority

1. ~~**Library Reference Counting Broken** (8 FIXMEs across 6 libraries)~~ — **Fixed in Phase 73**. `lib_OpenCnt` incremented/decremented, `LIBF_DELEXP` cleared in Open handlers.
2. ~~**Intuition Heavily Stubbed** (29 markers) — BOOPSI gadgets, PropGadget/StringGadget allocation, AutoRequest rendering, ZipWindow, requesters.~~ — **Fixed in Phase 76**. BOOPSI IC system (icclass/modelclass/gadgetclass with ICData), propgclass/strgclass rewritten, SetGadgetAttrsA/DoGadgetMethodA implemented, ActivateWindow/ZipWindow/AutoRequest all functional.
3. ~~**Layers Library Stubbed** (10 TODOs) — ScrollLayer, ClipRects, damage tracking, LAYERSMART all unimplemented.~~ — **Fixed in Phase 75**. ScrollLayer, BeginUpdate, EndUpdate, SwapBitsRastPortClipRect, MoveLayerInFrontOf, InstallClipRegion, SortLayerCR, DoHookClipRects, ShowLayer all implemented.
4. ~~**Exception/Task Switching Incomplete** (5 FIXMEs in exceptions.s)~~ — **Fixed in Phase 73**. tc_Switch, tc_Launch, TF_EXCEPT, Exception() all implemented.
5. ~~**DOS Memory Leaks** (7 FIXMEs) — UnLoadSeg doesn't free memory, AllocDosObject/FreeDosObject incomplete.~~ — **Fixed in Phase 74**. UnLoadSeg frees seglist chain, AllocDosObject/FreeDosObject complete for all 6 types.
6. ~~**Process/CLI Initialization Gaps** (5 FIXMEs)~~ — **Fixed in Phase 73**. cli_CommandDir, pr_ConsoleTask, pr_FileSystemTask, pr_SegList, prompt inheritance all implemented.
7. ~~**Case-Insensitive Paths Missing** (1 FIXME in lxa.c) — Amiga paths are case-insensitive but host I/O is case-sensitive.~~ — **Fixed in Phase 74**. Legacy fallback path uses `vfs_resolve_case_path()` for case-insensitive resolution.
8. ~~**Graphics Pixel Array Stubs** (4 TODOs) — ReadPixelLine8, ReadPixelArray8, WritePixelLine8 are stubs/assert(FALSE).~~ — **Fixed in Phase 75**. All four pixel array functions fully implemented with correct UWORD parameter types.

---

## Next Steps

### Phase 73: Core Library Resource Management & Stability ✅
**Goal**: Fix critical library and process lifecycle issues identified in the codebase audit.
**DONE**:
- [x] Fix `lib_OpenCnt++` and `LIBF_DELEXP` flag clearing in all 6 affected library Open handlers (graphics, intuition, utility, mathtrans, mathffp, expansion). Implement proper `CloseLibrary` reference counting and delayed expunge.
- [x] Fix `exceptions.s` FIXMEs: properly handle `tc_Flags`, `Sysflags`, `tc_Switch` callback, and task launch exceptions. Implemented `Exception()` (LVO -66) with full signal dispatch loop.
- [x] Fix process initialization in `util.c`: set `pr_SegList`, `pr_ConsoleTask`, `pr_FileSystemTask`. Also inherits `cli_Prompt` and `cli_CommandDir` from parent CLI.
- [x] Fix `exec.c` bootstrap FIXMEs: `NP_HomeDir`, `pr_TaskNum`, `cli_CommandDir`. Cleaned dead code block comments.

### Phase 74: DOS & VFS Hardening ✅
**Goal**: Complete missing DOS/VFS features for 100% application compatibility.
**DONE**:
- [x] Fix `UnLoadSeg` to properly free hunk memory (walks seglist chain, `FreeVec` each hunk). Also implemented `InternalUnLoadSeg` with custom freefunc support.
- [x] Implement missing `AllocDosObject` types (`DOS_EXALLCONTROL`, `DOS_STDPKT` with msg/pkt linkage) and fix `FreeDosObject` for unknown types (graceful error instead of `assert(FALSE)`).
- [x] Fix case-insensitive path mapping in `lxa.c` — added `vfs_resolve_case_path()` wrapper, legacy fallback uses case-insensitive resolution, fixed `g_sysroot` NULL crash.
- [x] Fix errno-to-AmigaOS error code mapping in host I/O — full switch/case mapping (12+ errno→AmigaOS error codes), fixed double-call bug in `_dos_open`.

### Phase 75: Advanced Graphics & Layers ✅
**Goal**: Fill in missing graphical operations and layer functions.
**DONE**:
- [x] Implement proper polygon filling using scan-line algorithm (`AreaFill` in `lxa_graphics.c`). Even-odd fill rule, multi-polygon support, fill-then-outline. Added Tests 7 & 8 to area_fill test.
- [x] Implement `ReadPixelLine8`, `ReadPixelArray8`, `WritePixelLine8`, `WritePixelArray8` (replaced `assert(FALSE)` stubs). Fixed NDK parameter types (`UWORD` not `ULONG`). New pixel_array8 test (6 tests).
- [x] Implement `ScrollLayer` — calls `ScrollRaster` for non-SuperBitMap layers, then updates scroll offsets. New scroll_layer test (5 tests).
- [x] Implement DamageList tracking, ClipRects rebuilds, and `LAYERSMART` support: `BeginUpdate` (damage-based ClipRects), `EndUpdate` (frees DamageList + damage ClipRects), `SwapBitsRastPortClipRect` (3-step bitmap swap), `MoveLayerInFrontOf` (z-order reordering), `InstallClipRegion`, `SortLayerCR` (direction-based insertion sort), `DoHookClipRects`, `ShowLayer`.

### Phase 76: Intuition & BOOPSI Enhancements ✅
**Goal**: Complete Intuition elements and finalize BOOPSI support.
**DONE**:
- [x] Implement BOOPSI gadget system and Inter-Object Communication (ICA_MAP/ICA_TARGET). Full icclass, modelclass, gadgetclass dispatchers with embedded ICData, `_boopsi_do_notify()` notification pipeline, loop prevention. propgclass/strgclass rewritten with INST_DATA for PropGData/StrGData.
- [x] Fix Zoom gadget (`ZipWindow` behavior). Uses `struct ZoomData` stored in `window->ExtData` to toggle between normal and WA_Zoom alternate position/size.
- [x] Implement `SetGadgetAttrsA` (was stub) and `DoGadgetMethodA` (was `assert(FALSE)` crash).
- [x] Implement `ActivateWindow` deactivation of previous window (clears flag, sends IDCMP_INACTIVEWINDOW/ACTIVEWINDOW, re-renders frames).
- [x] Implement `AutoRequest` with proper text measurement, gadget creation, IntuiText rendering, and modal event loop.

### Phase 77: Missing Libraries & Devices ✅
**Goal**: Implement essential missing libraries and devices required for full userland.
**DONE**:
- [x] Implement `mathieeesingbas.library` — Full IEEE single-precision math (12 functions via EMU_CALL). Fixed d2 register clobbering bug in two-operand asm stubs.
- [x] Implement `trackdisk.device` — Stub device with DD 3.5" floppy geometry and basic command dispatch (TD_MOTOR, TD_CHANGENUM, TD_CHANGESTATE, TD_PROTSTATUS, TD_GETGEOMETRY).
- [x] Fix `audio.device` — Open() now succeeds (was returning ADIOERR_NOALLOCATION).
- [x] Implement `diskfont.library` — Disk font loading from FONTS: directory, AFF_DISK support in AvailFonts, binary `.font` contents file parsing.
- [x] Implement `locale.library` formatting — FormatDate (all format codes), FormatString (printf-like with Amiga conventions), ParseDate.
- [x] Fix `mathieeedoubbas.library` — Fixed latent d2/d3/d4 register clobbering bug in all 11 asm stubs (same issue as mathieeesingbas).
- [x] Tests: mathieeesingbas (35 tests), trackdisk, audio, locale (31 tests) — all passing.

### Phase 78: AROS Compatibility Verification & FS-UAE Analysis

**Goal**: Systematically compare `lxa` against AROS (reference source in `others/AROS-20231016-source/`) and
analyse FS-UAE (`others/fs-uae/`) for algorithmic insights applicable to lxa's WINE-like approach.

> **Note**: AROS source lives under `others/AROS-20231016-source/rom/`.  
> Key sub-dirs: `exec/`, `dos/`, `graphics/`, `hyperlayers/`, `intuition/`, `utility/`, `timer/`, `keymap/`,
> `expansion/`, `task/`, `devs/console/`, `devs/input/`, `devs/trackdisk/`, `devs/gameport/`, `devs/keyboard/`,
> `compiler/alib/` (GadTools, ASL alib wrappers).

---

#### 78-A: Exec Library (`src/rom/exec.c` vs `others/AROS-20231016-source/rom/exec/`)

**Data Structures** (verify offsets/sizes against `exec/exec_init.c`, `exec/exec_util.c`, NDK `exec/types.i`):
- [ ] `ExecBase` — verify all public field offsets (VBlankFrequency, PowerSupplyFrequency, KickMemPtr, etc.)
- [ ] `Task` — tc_Node, tc_Flags, tc_State, tc_IDNestCnt, tc_TDNestCnt, tc_SigAlloc, tc_SigWait, tc_SigRecvd, tc_SigExcept, tc_TrapAlloc, tc_TrapAble, tc_ExceptData, tc_ExceptCode, tc_TrapData, tc_TrapCode, tc_SPReg, tc_SPLower, tc_SPUpper, tc_Switch, tc_Launch, tc_MemEntry, tc_UserData
- [ ] `MsgPort` — mp_Node, mp_Flags, mp_SigBit, mp_SigTask, mp_MsgList
- [ ] `Message` — mn_Node, mn_ReplyPort, mn_Length
- [ ] `IORequest` / `IOStdReq` — io_Message, io_Device, io_Unit, io_Command, io_Flags, io_Error, io_Actual, io_Length, io_Data, io_Offset
- [ ] `Library` — lib_Node, lib_Flags, lib_pad, lib_NegSize, lib_PosSize, lib_Version, lib_Revision, lib_IdString, lib_Sum, lib_OpenCnt
- [ ] `Device` / `Unit` structure offsets
- [ ] `Interrupt` — is_Node, is_Data, is_Code
- [ ] `IntVector` (ExecBase.IntVects array)
- [ ] `MemChunk` / `MemHeader` — mh_Node, mh_Attributes, mh_First, mh_Lower, mh_Upper, mh_Free
- [ ] `SemaphoreRequest` / `SignalSemaphore` — ss_Link, ss_NestCount, ss_Owner, ss_QueueCount, ss_WaitQueue
- [ ] `List` / `MinList` / `Node` / `MinNode` offsets

**Memory Management** (vs `exec/memory.c`, `exec/mementry.c`):
- [ ] `AllocMem` — MEMF_CLEAR, MEMF_CHIP, MEMF_FAST, MEMF_ANY, MEMF_LARGEST, alignment guarantees
- [ ] `FreeMem` — size parameter must match allocated size; verify behaviour on NULL pointer
- [ ] `AllocVec` / `FreeVec` — hidden size word prepended; MEMF_CLEAR
- [ ] `AllocEntry` / `FreeEntry` — MemList allocation loop, partial failure rollback
- [ ] `AvailMem` — MEMF_TOTAL, MEMF_LARGEST
- [ ] `TypeOfMem` — returns attribute flags for address; returns 0 for unknown addresses
- [ ] `AddMemList` / `AddMemHandler` / `RemMemHandler`
- [ ] `CopyMem` / `CopyMemQuick` — alignment optimizations; behaviour on overlap

**List Operations** (vs `exec/lists.c`):
- [ ] `AddHead` / `AddTail` / `Remove` / `RemHead` / `RemTail` — correct sentinel node handling
- [ ] `Insert` — position node, predecessor check
- [ ] `FindName` — case-sensitive string compare, returns first match
- [ ] `Enqueue` — priority-based insertion (ln_Pri, higher=closer to head)

**Task Management** (vs `exec/tasks.c`, `exec/task/`, `exec/schedule.c`):
- [ ] `AddTask` — NP_* tags, stack allocation, initial PC/stack setup
- [ ] `RemTask` — can remove self (via Wait loop exit) or other task; cleanups
- [ ] `FindTask` — NULL returns current task; string search
- [ ] `SetTaskPri` — returns old priority; reschedule if needed
- [ ] `GetCC` / `SetSR`
- [ ] Task state machine: TS_RUN, TS_READY, TS_WAIT, TS_EXCEPT, TS_INVALID

**Signals & Semaphores** (vs `exec/semaphores.c`):
- [ ] `AllocSignal` / `FreeSignal` — signal bit allocation from tc_SigAlloc
- [ ] `Signal` — set bits in tc_SigRecvd; wake from wait if bits match tc_SigWait
- [ ] `Wait` — clear tc_SigWait on return; SIGBREAKF_CTRL_C handling
- [ ] `SetSignal` — set/clear signal bits without scheduling
- [ ] `InitSemaphore` / `ObtainSemaphore` / `ReleaseSemaphore` — nest count, queue
- [ ] `ObtainSemaphoreShared` / `AttemptSemaphore` / `AttemptSemaphoreShared`
- [ ] `AddSemaphore` / `FindSemaphore` / `RemSemaphore`

**I/O & Message Passing** (vs `exec/io.c`, `exec/ports.c`):
- [ ] `CreateMsgPort` / `DeleteMsgPort` — signal bit allocation, mp_SigBit, mp_SigTask
- [ ] `CreateIORequest` / `DeleteIORequest`
- [ ] `OpenDevice` / `CloseDevice` — device open/close counting
- [ ] `DoIO` / `SendIO` / `AbortIO` / `WaitIO` / `CheckIO` — sync vs async I/O
- [ ] `GetMsg` / `PutMsg` / `ReplyMsg` / `WaitPort`

**Interrupts** (vs `exec/interrupts.c`):
- [ ] `AddIntServer` / `RemIntServer` — INTB_* interrupt vectors; priority insertion
- [ ] `Cause` — software interrupt dispatch
- [ ] `Disable` / `Enable` — nesting via IDNestCnt
- [ ] `Forbid` / `Permit` — nesting via TDNestCnt

**Library Management** (vs `exec/libraries.c`, `exec/resident.c`):
- [ ] `OpenLibrary` / `CloseLibrary` — version check, lib_OpenCnt, expunge on close
- [ ] `MakeLibrary` / `MakeFunctions` / `SumLibrary`
- [ ] `AddLibrary` / `RemLibrary` / `FindName` in SysBase->LibList
- [ ] `SetFunction` — replace LVO function, return old pointer
- [ ] `SumKickData`

**Miscellaneous Exec**:
- [ ] `RawDoFmt` — format string parsing (`%s`, `%d`, `%ld`, `%x`, `%lx`, `%b`, `%c`), PutChProc callback; verify against AROS `exec/rawfmt.c`
- [ ] `NewRawDoFmt` — extended version
- [ ] `Debug` — serial output stub
- [ ] `Alert` — deadlock/crash alert display
- [ ] `Supervisor` — enter supervisor mode
- [ ] `UserState` — return to user mode
- [ ] `GetHead` / `GetTail` / `GetSucc` / `GetPred` — inline accessors

---

#### 78-B: DOS Library (`src/rom/lxa_dos.c` vs `others/AROS-20231016-source/rom/dos/`)

**Data Structures** (verify vs `dos/dos_init.c`, NDK `dos/dos.h`):
- [ ] `DOSBase` — public fields: RootNode, TimerBase, SegList
- [ ] `RootNode` — rn_TaskArray, rn_ConsoleSegment, rn_Time, rn_RestartSeg, rn_Info, rn_RequestList, rn_BootProc, rn_CliList, rn_boot_pkt_port
- [ ] `DosInfo` — di_McName, di_DevInfo, di_Devices, di_Handlers, di_NetHand, di_DevLock, di_EntryLock, di_DeleteLock
- [ ] `FileLock` — fl_Link, fl_Key, fl_Access, fl_Task, fl_Volume (BPTR chaining)
- [ ] `FileHandle` — fh_Link, fh_Port, fh_Type, fh_Buf, fh_Pos, fh_End, fh_Funcs, fh_Func2, fh_Func3, fh_Arg1, fh_Arg2
- [ ] `FileInfoBlock` — fib_DiskKey, fib_DirEntryType, fib_FileName[108], fib_Protection, fib_EntryType, fib_Size, fib_NumBlocks, fib_Date, fib_Comment[80], fib_OwnerUID, fib_OwnerGID
- [ ] `InfoData` — id_NumSoftErrors, id_UnitNumber, id_DiskState, id_NumBlocks, id_NumBlocksUsed, id_BytesPerBlock, id_DiskType, id_VolumeNode, id_InUse
- [ ] `DateStamp` — ds_Days, ds_Minute, ds_Tick
- [ ] `DosPacket` — dp_Link, dp_Port, dp_Type, dp_Res1, dp_Res2, dp_Arg1..dp_Arg7
- [ ] `ProcessWindowNode` / `CliProcList` for multi-CLI
- [ ] `Segment` (seglist chain: BPTR links)

**File I/O** (vs `dos/open.c`, `dos/read.c`, `dos/write.c`, `dos/close.c`):
- [ ] `Open` — MODE_OLDFILE, MODE_NEWFILE, MODE_READWRITE; returns BPTR FileHandle or NULL (IoErr set)
- [ ] `Close` — flush, return success/failure; NULL handle no-op
- [ ] `Read` — returns bytes read, 0=EOF, -1=error
- [ ] `Write` — returns bytes written or -1
- [ ] `Seek` — OFFSET_BEGINNING, OFFSET_CURRENT, OFFSET_END; returns old position or -1
- [ ] `Flush` — flush file handle buffers
- [ ] `SetFileSize` — truncate/extend file (AROS: `dos/setfilesize.c`)
- [ ] `ChangeFilePosition` / `GetFilePosition`

**Lock & Examine** (vs `dos/lock.c`, `dos/examine.c`):
- [ ] `Lock` — SHARED_LOCK (ACCESS_READ), EXCLUSIVE_LOCK (ACCESS_WRITE); returns BPTR or NULL
- [ ] `UnLock` — NULL is no-op
- [ ] `DupLock` / `DupLockFromFH`
- [ ] `Examine` / `ExNext` — FIB population; fib_DirEntryType positive=dir, negative=file
- [ ] `ExamineFH`
- [ ] `ExAll` / `ExAllEnd` — EXALL_TYPE filter, ED_NAME/ED_TYPE/ED_SIZE/ED_PROTECTION/ED_DATE/ED_COMMENT/ED_OWNER
- [ ] `SameLock` — compare two locks (same volume+key)
- [ ] `Info` — populate InfoData for a lock's volume

**Directory & Path Operations** (vs `dos/dir.c`, `dos/path.c`):
- [ ] `CreateDir` — returns exclusive lock on new dir or NULL
- [ ] `DeleteFile` — file or empty directory; IOERR_DELETEOBJECT on in-use
- [ ] `Rename` — cross-directory; error on cross-volume
- [ ] `SetProtection` — FIBF_* bits; maps to host `chmod`
- [ ] `SetComment` — file comment (extended attribute or ignore)
- [ ] `SetFileDate` — set datestamp
- [ ] `MakeLink` — hard link (type 0) or soft link (type 1)
- [ ] `ReadLink` — read soft link target
- [ ] `NameFromLock` / `NameFromFH` — build full Amiga path string
- [ ] `FilePart` / `PathPart` — string operations only (no I/O)
- [ ] `AddPart` — append name to path buffer

**Assigns** (vs `dos/assign.c`):
- [ ] `AssignLock` / `AssignLate` / `AssignPath` / `AssignAdd`
- [ ] `RemAssignList` / `GetDevProc` / `FreeDevProc`
- [ ] Multi-directory assigns (path list iteration)

**Pattern Matching** (vs `dos/pattern.c`):
- [ ] `MatchPattern` / `MatchPatternNoCase` — `#?`, `?`, `*`, `(a|b)`, `[a-z]` tokens
- [ ] `ParsePattern` / `ParsePatternNoCase` — tokenize to internal form
- [ ] `MatchFirst` / `MatchNext` / `MatchEnd` — directory iteration with pattern

**Environment, CLI, Variables** (vs `dos/env.c`, `dos/cli.c`):
- [ ] `GetVar` / `SetVar` / `DeleteVar` — GVF_LOCAL_VAR, GVF_GLOBAL_VAR, GVF_BINARY_VAR; ENV: assigns
- [ ] `GetCurrentDirName` / `SetCurrentDirName`
- [ ] `GetProgramName` / `SetProgramName`
- [ ] `CurrentDir` — set process current dir, return old lock
- [ ] `GetPrompt` / `SetPrompt`
- [ ] `SetVBuf` — set file buffering

**Notification** (vs `dos/notify.c`):
- [ ] `StartNotify` / `EndNotify` — `NotifyRequest` structure; NRF_SEND_MESSAGE / NRF_SEND_SIGNAL

**Date & Time** (vs `dos/datetime.c`):
- [ ] `DateStamp` — fill with current time
- [ ] `Delay` — TICKS_PER_SECOND = 50; uses timer.device
- [ ] `WaitForChar` — wait up to timeout for character on file handle
- [ ] `DateToStr` / `StrToDate` — locale-independent

**Formatting** (vs `dos/dospkt.c`, `dos/vfprintf.c`):
- [ ] `FPuts` / `FPrintf` / `VFPrintf` / `FRead` / `FWrite`
- [ ] `SPrintf` / `VSPrintf`
- [ ] `PrintFault` — print IoErr message to file handle

**Program Loading** (vs `dos/loadseg.c`):
- [ ] `LoadSeg` — HUNK_HEADER, HUNK_CODE/DATA/BSS, HUNK_RELOC32, HUNK_END; returns BPTR seglist
- [ ] `UnLoadSeg` — walk seglist, FreeVec each hunk ✅ (Phase 74)
- [ ] `InternalLoadSeg` / `InternalUnLoadSeg` ✅ (Phase 74)
- [ ] `NewLoadSeg` — with tags
- [ ] `CreateProc` / `CreateNewProc` / `RunCommand`
- [ ] `Execute` — run shell command string
- [ ] `System` — run with I/O redirection
- [ ] `GetSegListInfo`

**Error Handling**:
- [ ] `IoErr` / `SetIoErr` — per-process error code (pr_Result2)
- [ ] `Fault` — IOERR_* code to string

---

#### 78-C: Graphics Library (`src/rom/lxa_graphics.c` vs `others/AROS-20231016-source/rom/graphics/`)

**Data Structures** (vs `graphics/gfx_init.c`, NDK `graphics/gfx.h`):
- [ ] `GfxBase` — ActiView, ActiViewCprSemaphore, ActualDPMSR, NormalDisplayRows/Columns, MaxDisplayRow/Col
- [ ] `RastPort` — Layer, BitMap, AreaInfo, TmpRas, GelsInfo, Mask, FgPen, BgPen, AOlPen, DrawMode, AreaPtSz, linpatcnt, dummy, Flags, LinePtrn, cp_x, cp_y, minterms[8], PenWidth, PenHeight, Font, AlgoStyle, TxFlags, TxHeight, TxWidth, TxBaseline, TxSpacing, RP_User, longreserved[2], workreserved[7], patterned[2], Rect
- [ ] `BitMap` — BytesPerRow, Rows, Flags, Depth, pad, Planes[8]
- [ ] `ViewPort` — ColorMap, DspIns, SprIns, ClrIns, Next, DWidth, DHeight, DxOffset, DyOffset, Modes, SpritePriorities, ExtendedModes, RasInfo
- [ ] `RasInfo` — Next, BitMap, RxOffset, RyOffset
- [ ] `ColorMap` — Count, ColorTable, cm_Entry, PalExtra, SpritePens, Flags
- [ ] `AreaInfo` — VctrTbl, VctrPtr, FlagTbl, FlagPtr, Count, MaxCount, FirstX, FirstY
- [ ] `TmpRas` — RasPtr, Size
- [ ] `GelsInfo` structure
- [ ] `TextFont` — tf_Message, tf_YSize, tf_Style, tf_Flags, tf_XSize, tf_Baseline, tf_BoldSmear, tf_Accessors, tf_LoChar, tf_HiChar, tf_CharData, tf_Modulo, tf_CharLoc, tf_CharSpace, tf_CharKern

**Drawing Operations** (vs `graphics/draw.c`, `graphics/clip.c`):
- [ ] `Move` / `Draw` — update cp_x/cp_y; JAM1/JAM2/COMPLEMENT/INVERSVID draw modes
- [ ] `DrawEllipse` — Bresenham ellipse; clip to rp->Layer
- [ ] `AreaEllipse` — add ellipse to area fill
- [ ] `RectFill` — fills with FgPen; clips to layer
- [ ] `SetRast` — fill entire bitmap (not clipped to layer)
- [ ] `WritePixel` / `ReadPixel`
- [ ] `PolyDraw` — MoveTo first, DrawTo remaining
- [ ] `Flood` — FLOOD_COMPLEMENT, FLOOD_FILL
- [ ] `BltBitMap` — minterm, mask; all 4 ABC masks; BLTDEST only for clears
- [ ] `BltBitMapRastPort` — clip to layer ClipRect chain
- [ ] `ClipBlit` — layer-aware BltBitMapRastPort
- [ ] `BltTemplate` — expand 1-bit template to pen colors with minterm
- [ ] `BltPattern` — fill with pattern
- [ ] `BltMaskBitMapRastPort`
- [ ] `ReadPixelLine8` / `ReadPixelArray8` / `WritePixelLine8` / `WritePixelArray8` ✅ (Phase 75)

**Text Rendering** (vs `graphics/text.c`):
- [ ] `Text` — JAM1/JAM2/COMPLEMENT styles; tf_CharLoc offsets; clip to layer
- [ ] `TextLength` — no rendering, measure only
- [ ] `SetFont` / `AskFont`
- [ ] `OpenFont` — search GfxBase->TextFonts list; match name+ysize
- [ ] `CloseFont`
- [ ] `SetSoftStyle` — FSF_BOLD, FSF_ITALIC, FSF_UNDERLINED, FSF_EXTENDED via algorithmic transforms

**Area Fill** (vs `graphics/areafill.c`):
- [ ] `InitArea` / `AreaMove` / `AreaDraw` / `AreaEnd` — scan-line fill ✅ (Phase 75); verify against AROS even-odd rule
- [ ] `AreaCircle` — shorthand for AreaEllipse

**Blitter** (vs `graphics/blitter.c`, `graphics/blit.c`):
- [ ] `QBlit` / `QBSBlit` — blitter queue with semaphore
- [ ] `WaitBlit` / `OwnBlitter` / `DisownBlitter`
- [ ] `InitBitMap` — initialise BitMap struct (does NOT allocate planes)
- [ ] `AllocBitMap` / `FreeBitMap` — allocate planes from CHIP mem
- [ ] `AllocRaster` / `FreeRaster` — allocate single plane row

**Regions** (vs `graphics/regions.c`):
- [ ] `NewRegion` / `DisposeRegion` / `ClearRegion`
- [ ] `OrRectRegion` / `AndRectRegion` / `XorRectRegion` / `ClearRectRegion`
- [ ] `OrRegionRegion` / `AndRegionRegion` / `XorRegionRegion`
- [ ] `RectInRegion` / `PointInRegion`

**Display & ViewPort** (vs `graphics/view.c`):
- [ ] `InitView` / `InitVPort`
- [ ] `LoadView` — switch to new View (Intuition-level only in lxa)
- [ ] `MakeVPort` / `MrgCop` / `FreeVPortCopLists` / `FreeCopList`
- [ ] `VideoControl` — VTAG_* tags; verify tag list handling
- [ ] `GetDisplayInfoData` — DisplayInfo, DimensionInfo, MonitorInfo, NameInfo
- [ ] `FindDisplayInfo` / `NextDisplayInfo`

**Sprites & GELs** (vs `graphics/sprite.c`, `graphics/gel.c`):
- [ ] `GetSprite` / `FreeSprite` / `ChangeSprite` / `MoveSprite`
- [ ] `InitGels` / `AddBob` / `AddVSprite` / `RemBob` / `RemVSprite` / `DoBlitVSprite` / `DrawGList` / `SortGList`

**Pens & Colors** (vs `graphics/color.c`):
- [ ] `SetRGB4` / `SetRGB32` / `SetRGB4CM` / `SetRGB32CM` / `GetRGB32`
- [ ] `LoadRGB4` / `LoadRGB32`
- [ ] `ObtainPen` / `ReleasePen` / `FindColor`

**BitMap Utilities**:
- [ ] `CopyRgn` — region-clipped copy
- [ ] `ScalerDiv` — fast integer scale helper
- [ ] `BitMapScale` — hardware-accelerated bitmap scale via blitter
- [ ] `ExtendFont` / `StripFont` — tf_Extension for proportional/kerning
- [ ] `AddFont` / `RemFont`
- [ ] `GfxFree` / `GfxNew` / `GfxAssociate` / `GfxLookUp`

---

#### 78-D: Layers Library (`src/rom/lxa_layers.c` vs `others/AROS-20231016-source/rom/hyperlayers/`)

- [ ] `InitLayers` — initialise LayerInfo struct
- [ ] `NewLayerInfo` / `DisposeLayerInfo`
- [ ] `CreateUpfrontLayer` / `CreateBehindLayer` / `CreateUpfrontHookLayer` / `CreateBehindHookLayer` — with and without SuperBitMap
- [ ] `DeleteLayer` — release ClipRects, damage list, region, bitmap
- [ ] `MoveLayer` — move by delta; update ClipRect chain
- [ ] `SizeLayer` — resize layer; rebuild ClipRects
- [ ] `BehindLayer` / `UpfrontLayer` — z-order without moving
- [ ] `MoveLayerInFrontOf` ✅ (Phase 75)
- [ ] `ScrollLayer` ✅ (Phase 75)
- [ ] `BeginUpdate` / `EndUpdate` ✅ (Phase 75)
- [ ] `SwapBitsRastPortClipRect` ✅ (Phase 75)
- [ ] `InstallClipRegion` ✅ (Phase 75)
- [ ] `SortLayerCR` ✅ (Phase 75)
- [ ] `DoHookClipRects` ✅ (Phase 75)
- [ ] `ShowLayer` ✅ (Phase 75)
- [ ] `LockLayer` / `UnlockLayer` / `LockLayers` / `UnlockLayers` / `LockLayerRom` / `UnlockLayerRom`
- [ ] `WhichLayer` — hit-test point to top-most layer
- [ ] `FattenLayerInfo` / `ThinLayerInfo`
- [ ] `AddLayerInfoTag` / `CreateLayerTagList`

---

#### 78-E: Intuition Library (`src/rom/lxa_intuition.c` vs `others/AROS-20231016-source/rom/intuition/`)

**Data Structures** (vs `intuition/iobsolete.h`, NDK `intuition/intuition.h`):
- [ ] `IntuitionBase` — FirstScreen, ActiveScreen, ActiveWindow, MouseX/Y, Seconds/Micros, ActiveGadget, MenuVerifyTimeout
- [ ] `Screen` — NextScreen, FirstWindow, LeftEdge/TopEdge/Width/Height, MouseY/X, Flags, Title, DefaultTitle, BarHeight, BarVBorder, BarHBorder, MenuVBorder, MenuHBorder, WBorTop/Left/Right/Bottom, Font, ViewPort, RastPort, BitMap, LayerInfo, FirstGadget, DetailPen, BlockPen, SaveColor0, ViewPortAddress, Depth, WBorLeft, WBorRight, WBorTop, WBorBottom, Title, BitMap
- [ ] `Window` — NextWindow, LeftEdge/TopEdge/Width/Height, MouseX/Y, MinWidth/MinHeight/MaxWidth/MaxHeight, Flags, Title, Screen, RPort, BorderLeft/Top/Right/Bottom, FirstGadget, Parent/Descendant, Pointer/PtrHeight/PtrWidth/XOffset/YOffset, IDCMPFlags, UserPort, WindowPort, MessageKey, DetailPen, BlockPen, CheckMark, ScreenTitle, GZZWidth, GZZHeight, ExtData, UserData, WLayer, IFont, MoreFlags
- [ ] `IntuiMessage` — ExecMessage, Class, Code, Qualifier, IAddress, MouseX/Y, Seconds/Micros, IDCMPWindow, SpecialLink
- [ ] `Gadget` — NextGadget, LeftEdge/TopEdge/Width/Height, Flags, Activation, GadgetType, GadgetRender/SelectRender, GadgetText, MutualExclude, SpecialInfo, GadgetID, UserData
- [ ] `StringInfo` — Buffer, UndoBuffer, BufferPos, MaxChars, DispPos, UndoPos, NumChars, DispCount, CLeft, CTop, LayerPtr, LongInt, AltKeyMap
- [ ] `PropInfo` — Flags, HorizPot/VertPot, HorizBody/VertBody, CWidth/CHeight, HPotRes/VPotRes, LeftBorder/TopBorder
- [ ] `Image` — LeftEdge, TopEdge, Width, Height, Depth, ImageData, PlanePick, PlaneOnOff, NextImage
- [ ] `Border` — LeftEdge, TopEdge, FrontPen, BackPen, DrawMode, Count, XY, NextBorder
- [ ] `IntuiText` — FrontPen, BackPen, DrawMode, LeftEdge, TopEdge, ITextFont, IText, NextText
- [ ] `MenuItem` — NextItem, LeftEdge/TopEdge/Width/Height, Flags, MutualExclude, ItemFill, SelectFill, Command, SubItem, NextSelect
- [ ] `Menu` — NextMenu, LeftEdge/TopEdge/Width/Height, Flags, MenuName, FirstItem
- [ ] `Requester` — OlderRequest, LeftEdge/TopEdge/Width/Height, RelLeft/RelTop, ReqGadget, ReqBorder, ReqText, Flags, BackFill, ReqLayer, ReqPad1, ImageBMap, RWindow, ReqImage, ReqPad2, special

**Screens** (vs `intuition/screens.c`):
- [ ] `OpenScreen` / `CloseScreen` / `OpenScreenTagList`
- [ ] `LockPubScreen` / `UnlockPubScreen` / `LockPubScreenList` / `UnlockPubScreenList`
- [ ] `GetScreenData` — fill ScreenBuffer from screen
- [ ] `MoveScreen` — panning
- [ ] `ScreenToFront` / `ScreenToBack`
- [ ] `ShowTitle` — SA_ShowTitle toggle
- [ ] `PubScreenStatus`
- [ ] `SetDefaultScreenFont`

**Windows** (vs `intuition/windows.c`):
- [ ] `OpenWindow` / `CloseWindow` / `OpenWindowTagList`
- [ ] `MoveWindow` / `SizeWindow` / `ChangeWindowBox`
- [ ] `WindowToFront` / `WindowToBack`
- [ ] `ActivateWindow` ✅ (Phase 76)
- [ ] `ZipWindow` ✅ (Phase 76)
- [ ] `SetWindowTitles` — NULL means no change; ~0 means blank
- [ ] `RefreshWindowFrame`
- [ ] `WindowLimits` — min/max width/height
- [ ] `BeginRefresh` / `EndRefresh` — WFlg damage tracking
- [ ] `ModifyIDCMP`
- [ ] `GetDefaultPubScreen`

**Gadgets** (vs `intuition/gadgets.c`, `intuition/boopsi.c`):
- [ ] `AddGadget` / `RemoveGadget` / `AddGList` / `RemoveGList`
- [ ] `RefreshGadgets` / `RefreshGList`
- [ ] `ActivateGadget`
- [ ] `SetGadgetAttrsA` ✅ (Phase 76)
- [ ] `DoGadgetMethodA` ✅ (Phase 76)
- [ ] `OnGadget` / `OffGadget` — GADGDISABLED flag; re-render
- [ ] `SizeWindow` interaction with GFLG_RELRIGHT/RELBOTTOM relative gadgets

**BOOPSI / OOP** (vs `intuition/boopsi.c`, `intuition/classes.c`):
- [ ] `NewObjectA` / `DisposeObject` — class dispatch, tag list
- [ ] `GetAttr` / `SetAttrsA` / `CoerceMethodA`
- [ ] `DoMethodA` — OM_NEW, OM_SET, OM_GET, OM_DISPOSE, OM_ADDMEMBER, OM_REMMEMBER, OM_NOTIFY, OM_UPDATE
- [ ] `MakeClass` / `FreeClass` / `AddClass` / `RemoveClass`
- [ ] `ICA_TARGET / ICA_MAP` notification pipeline ✅ (Phase 76)
- [ ] Standard classes: rootclass, imageclass, frameiclass, sysiclass, fillrectclass, gadgetclass, propgclass, strgclass, buttongclass, frbuttonclass, groupgclass, icclass, modelclass ✅ (Phase 76)
- [ ] `GetScreenDepth` / `GetScreenRect`

**Menus** (vs `intuition/menus.c`):
- [ ] `SetMenuStrip` / `ClearMenuStrip` / `ResetMenuStrip`
- [ ] `OnMenu` / `OffMenu`
- [ ] `ItemAddress` — menu number decode (MENUNUM/ITEMNUM/SUBNUM macros)

**Requesters** (vs `intuition/requesters.c`):
- [ ] `Request` / `EndRequest`
- [ ] `BuildSysRequest` / `FreeSysRequest` / `SysReqHandler`
- [ ] `AutoRequest` ✅ (Phase 76)
- [ ] `EasyRequestArgs` / `BuildEasyRequestArgs` ✅ (Phase 70a)

**Pointers & Input** (vs `intuition/pointer.c`):
- [ ] `SetPointer` / `ClearPointer` — sprite data, xoffset/yoffset
- [ ] `SetMouseQueue` — limit pending MOUSEMOVE messages
- [ ] `CurrentTime` — fill seconds/micros from timer
- [ ] `DisplayAlert` / `DisplayBeep`

**Drawing in Intuition**:
- [ ] `DrawBorder` — linked Border chain rendering
- [ ] `DrawImage` / `DrawImageState` — Image chain, IMGS_* state for 3D look
- [ ] `PrintIText` / `IntuiTextLength`
- [ ] `EraseImage`
- [ ] `DrawBoopsiObject`

---

#### 78-F: GadTools Library (`src/rom/lxa_gadtools.c` vs `others/AROS-20231016-source/rom/compiler/alib/`)

- [ ] `GT_CreateContext` / `GT_FreeContext` — context gadget; verify head/tail sentinel nodes
- [ ] `GT_CreateGadgetA` — all kinds: BUTTON_KIND, CHECKBOX_KIND, CYCLE_KIND, INTEGER_KIND, LISTVIEW_KIND, MX_KIND, NUMBER_KIND, PALETTE_KIND, SCROLLER_KIND, SLIDER_KIND, STRING_KIND, TEXT_KIND
- [ ] `GT_FreeGadget` / `GT_FreeGadgets`
- [ ] `GT_SetGadgetAttrsA` / `GT_GetGadgetAttrs`
- [ ] `GT_ReplyIMsg` / `GT_GetIMsg` — thin wrappers around Intuition IDCMP
- [ ] `GT_RefreshWindow` — paint gadget list
- [ ] `GT_BeginRefresh` / `GT_EndRefresh`
- [ ] `GT_PostFilterIMsg` / `GT_FilterIMsg`
- [ ] `CreateMenusA` / `FreeMenus` / `LayoutMenuItemsA` / `LayoutMenusA`
- [ ] `DrawBevelBoxA` — raised/recessed bevel rendering

---

#### 78-G: ASL Library (`src/rom/lxa_asl.c` vs `others/AROS-20231016-source/rom/compiler/alib/`)

- [ ] `AllocAslRequest` / `FreeAslRequest` — ASL_FileRequest, ASL_FontRequest, ASL_ScreenModeRequest
- [ ] `AslRequestTags` / `AslRequest` — run modal requester, return TRUE/FALSE
- [ ] FileRequester tags: ASLFR_Window, ASLFR_TitleText, ASLFR_InitialFile, ASLFR_InitialDrawer, ASLFR_InitialPattern, ASLFR_Flags1 (FRF_DOSAVEMODE, FRF_DOMULTISELECT, FRF_DOPATTERNS)
- [ ] FontRequester tags: ASLFO_Window, ASLFO_TitleText, ASLFO_InitialName, ASLFO_InitialSize, ASLFO_InitialStyle, ASLFO_Flags (FOF_DOFIXEDWIDTHONLY, FOF_DOSTYLEBUTTONS)
- [ ] ScreenMode requester tags
- [ ] `fr_File` / `fr_Drawer` / `fr_NumArgs` / `fr_ArgList` output fields
- [ ] `fo_Attr` / `fo_TAttr` output fields ✅ (Phase 70a)

---

#### 78-H: Utility Library (`src/rom/lxa_utility.c` vs `others/AROS-20231016-source/rom/utility/`)

- [ ] `FindTagItem` — scan TagList for tag; handle TAG_MORE, TAG_SKIP, TAG_IGNORE, TAG_END
- [ ] `GetTagData` — return ti_Data or default
- [ ] `TagInArray` — check if tag present in array
- [ ] `FilterTagItems` — remove non-listed tags
- [ ] `MapTags` — remap tag values
- [ ] `AllocateTagItems` / `FreeTagItems` / `CloneTagItems`
- [ ] `RefreshTagItemClones`
- [ ] `NextTagItem` — iterate with state pointer
- [ ] `CallHookPkt` — invoke Hook with A0=hook, A2=object, A1=message
- [ ] `HookEntry` — assembly trampoline (hook calling convention)
- [ ] `CheckDate` / `Amiga2Date` / `Date2Amiga` — struct ClockData ↔ seconds-since-epoch
- [ ] `SMult32` / `UMult32` / `SDivMod32` / `UDivMod32` — 32-bit multiply/divide
- [ ] `PackStructureTags` / `UnpackStructureTags` — TagItem ↔ struct mapping via PackTable
- [ ] `NamedObjectA` / `AllocNamedObjectA` / `FreeNamedObject` / `AddNamedObject` / `RemNamedObject` / `FindNamedObject`
- [ ] `GetUniqueID`
- [ ] `MakeDosPatternA` / `MatchDosPatternA`

---

#### 78-I: Locale Library (`src/rom/lxa_locale.c` vs NDK `libraries/locale.h`)

- [ ] `OpenLocale` / `CloseLocale` — NULL = default locale
- [ ] `OpenCatalog` / `CloseCatalog` — .catalog file loading from LOCALE:Catalogs/
- [ ] `GetCatalogStr` / `GetLocaleStr`
- [ ] `IsAlpha` / `IsDigit` / `IsSpace` / `IsUpper` / `IsLower` / `IsAlNum` / `IsGraph` / `IsCntrl` / `IsPrint` / `IsPunct` / `IsXDigit`
- [ ] `ToUpper` / `ToLower`
- [ ] `StrnCmp` — locale-sensitive string comparison (with strength level)
- [ ] `ConvToUpper` / `ConvToLower`
- [ ] `FormatDate` ✅ (Phase 77) — all % codes; verify against AROS `locale/formatdate.c`
- [ ] `FormatString` ✅ (Phase 77) — `%s`/`%d`/`%ld`/`%u`/`%lu`/`%x`/`%lx` with PutChProc callback
- [ ] `ParseDate` ✅ (Phase 77)

---

#### 78-J: IFFParse Library (`src/rom/lxa_iffparse.c` vs NDK `libraries/iffparse.h`)

- [ ] `AllocIFF` / `FreeIFF`
- [ ] `InitIFFasDOS` / `InitIFFasClip`
- [ ] `OpenIFF` / `CloseIFF` — IFFF_READ, IFFF_WRITE
- [ ] `ParseIFF` — IFFPARSE_RAWSTEP, IFFPARSE_SCAN, IFFPARSE_STEP; return codes
- [ ] `StopChunk` / `StopOnExit` / `EntryHandler` / `ExitHandler`
- [ ] `PushChunk` / `PopChunk` — write support
- [ ] `ReadChunkBytes` / `ReadChunkRecords` / `WriteChunkBytes` / `WriteChunkRecords`
- [ ] `CurrentChunk` / `ParentChunk` / `FindProp` / `FindCollection` / `FindPropContext`
- [ ] `CollectionChunk` / `PropChunk`
- [ ] `OpenClipboard` / `CloseClipboard` / `ReadClipboard` / `WriteClipboard`

---

#### 78-K: Keymap Library (`src/rom/lxa_keymap.c` vs `others/AROS-20231016-source/rom/keymap/`)

- [ ] `SetKeyMap` / `AskKeyMapDefault`
- [ ] `MapANSI` — convert ANSI string to rawkey+qualifier sequence
- [ ] `MapRawKey` — rawkey+qualifier+keymap → character(s); qualifier processing (shift, alt, ctrl)
- [ ] `KeyMap` structure — km_LoKeyMap/km_LoKeyMapTypes/km_HiKeyMap/km_HiKeyMapTypes; KCF_* type flags
- [ ] Dead-key sequences (diaeresis, accent, tilde)

---

#### 78-L: Timer Device (`src/rom/lxa_dev_timer.c` vs `others/AROS-20231016-source/rom/timer/`)

- [ ] `TR_ADDREQUEST` — UNIT_MICROHZ (≥50 ms), UNIT_VBLANK (VBL aligned), UNIT_ECLOCK, UNIT_WAITUNTIL
- [ ] `TR_GETSYSTIME` / `TR_SETSYSTIME`
- [ ] `AbortIO` for timer requests
- [ ] `GetSysTime` / `AddTime` / `SubTime` / `CmpTime` — `struct timeval`
- [ ] EClockVal reading

---

#### 78-M: Console Device (`src/rom/lxa_dev_console.c` vs `others/AROS-20231016-source/rom/devs/console/`)

- [ ] `CMD_WRITE` — ANSI escape sequences: cursor movement, SGR attributes (bold/reverse/underline), erase, color (foreground/background 30-37/40-47)
- [ ] `CMD_READ` — read character/line; CRAF_* flags
- [ ] `CD_ASKKEYMAP` / `CD_SETKEYMAP`
- [ ] `CONU_LIBRARY` unit (-1) handling ✅ (Phase 72)
- [ ] Window resize events (IDCMP_NEWSIZE) → console resize
- [ ] Scroll-back buffer management
- [ ] Verify ANSI CSI sequences: `\e[H` (home), `\e[J` (erase display), `\e[K` (erase line), `\e[?25l/h` (cursor hide/show)

---

#### 78-N: Input Device (`src/rom/lxa_dev_input.c` vs `others/AROS-20231016-source/rom/devs/input/`)

- [ ] `IND_ADDHANDLER` / `IND_REMHANDLER` — input handler chain; priority ordering
- [ ] `IND_SETTHRESH` / `IND_SETPERIOD` — key repeat
- [ ] `IND_SETMPORT` / `IND_SETMTYPE`
- [ ] `InputEvent` chain processing; IE_RAWKEY, IE_BUTTON, IE_POINTER, IE_TIMER, IE_NEWACTIVE, IE_NEWSIZE, IE_REFRESH
- [ ] Qualifier accumulation: IEQUALIFIER_LSHIFT, RSHIFT, CAPSLOCK, CONTROL, LALT, RALT, LCOMMAND, RCOMMAND, NUMERICPAD, REPEAT, INTERRUPT, MULTIBROADCAST, MIDBUTTON, RBUTTON, LEFTBUTTON
- [ ] `FreeInputHandlerList`

---

#### 78-O: Audio Device (`src/rom/lxa_dev_audio.c`)

- [ ] `ADCMD_ALLOCATE` / `ADCMD_FREE` — channel allocation bitmask (channels 0-3)
- [ ] `ADCMD_SETPREC` — allocation precedence
- [ ] `CMD_WRITE` — DMA audio playback: period/volume/data/length
- [ ] `ADCMD_PERVOL` — change period and volume of running sound
- [ ] `ADCMD_FINISH` / `ADCMD_WAITCYCLE`
- [ ] Audio interrupt (end-of-sample notification)
- [ ] SDL2 audio stream mapping to Amiga channel model

---

#### 78-P: Trackdisk Device (`src/rom/lxa_dev_trackdisk.c` vs `others/AROS-20231016-source/rom/devs/trackdisk/`)

- [ ] `TD_READ` / `TD_WRITE` / `TD_FORMAT` / `TD_SEEK` — map to host file I/O on `.adf` image
- [ ] `TD_MOTOR` ✅ (Phase 77)
- [ ] `TD_CHANGENUM` / `TD_CHANGESTATE` / `TD_PROTSTATUS` / `TD_GETGEOMETRY` ✅ (Phase 77)
- [ ] `TD_ADDCHANGEINT` / `TD_REMCHANGEINT` — disk change notification via interrupt
- [ ] `ETD_READ` / `ETD_WRITE` — extended commands with `TDU_PUBFLAGS`
- [ ] Error codes: TDERR_NotSpecified, TDERR_NoSecHdr, TDERR_BadSecPreamble, TDERR_TooFewSecs, TDERR_NoDisk

---

#### 78-Q: Expansion Library (`src/rom/lxa_expansion.c` vs `others/AROS-20231016-source/rom/expansion/`)

- [ ] `FindConfigDev` ✅ — verify ConfigDev chain search
- [ ] `AddConfigDev` / `RemConfigDev`
- [ ] `AllocBoardMem` / `FreeBoardMem`
- [ ] `AllocExpansionMem` / `FreeExpansionMem`
- [ ] `ConfigBoard` / `ConfigChain`
- [ ] `MakeDosNode` / `AddDosNode` — bootnode creation
- [ ] `ObtainConfigBinding` / `ReleaseConfigBinding`
- [ ] `SetCurrentBinding` / `GetCurrentBinding`

---

#### 78-R: Math Libraries

**mathffp** (`src/rom/lxa_mathffp.c` vs NDK `math.h`):
- [ ] `SPFix` / `SPFlt` / `SPCmp` / `SPTst` / `SPAbs` / `SPNeg` — single-precision fast float
- [ ] `SPAdd` / `SPSub` / `SPMul` / `SPDiv`
- [ ] `SPFloor` / `SPCeil`
- [ ] FFP format: 8-bit exponent (biased by 0x40), 24-bit mantissa, 1 sign bit
- [ ] Edge cases: NaN, Inf, zero, denormals

**mathtrans** (`src/rom/lxa_mathtrans.c`):
- [ ] `SPSin` / `SPCos` / `SPTan` / `SPAtan`
- [ ] `SPSinh` / `SPCosh` / `SPTanh`
- [ ] `SPExp` / `SPLog` / `SPLog10`
- [ ] `SPPow` / `SPSqrt` / `SPTieee` / `SPFieee`

**mathieeesingbas** ✅ (Phase 77) — all 12 functions verified.

**mathieeedoubbas** (`src/rom/lxa_mathieeedoubbas.c`) ✅ (Phase 77) — d2/d3/d4 clobber fixed.

**mathieeedoubtrans** (`src/rom/lxa_mathieeedoubtrans.c`):
- [ ] `IEEEDPSin` / `IEEEDPCos` / `IEEEDPTan` / `IEEEDPAtan`
- [ ] `IEEEDPSinh` / `IEEEDPCosh` / `IEEEDPTanh`
- [ ] `IEEEDPExp` / `IEEEDPLog` / `IEEEDPLog10`
- [ ] `IEEEDPPow` / `IEEEDPSqrt` / `IEEEDPFieee` / `IEEEDPTieee`
- [ ] Verify against glibc results for edge values

---

#### 78-S: Diskfont Library (`src/rom/lxa_diskfont.c`)

- [ ] `OpenDiskFont` ✅ (Phase 77) — verify against AROS disk font format: contents file, `.font` binary format (header, chardata, charloc, charspace, charkern)
- [ ] `AvailFonts` ✅ (Phase 77) — AFF_MEMORY, AFF_DISK; verify AvailFontsHeader + AvailFonts array packing
- [ ] `DisposeFontContents` — free AvailFonts result
- [ ] `NewFontContents` — build AF list for single drawer
- [ ] Proportional font support (TF_PROPORTIONAL, tf_CharSpace, tf_CharKern arrays)
- [ ] Multi-plane fonts (depth > 1)

---

#### 78-T: Workbench & Icon Libraries (`src/rom/lxa_workbench.c`, `src/rom/lxa_icon.c`)

- [ ] `GetDiskObjectNew` / `GetDiskObject` / `PutDiskObject` / `FreeDiskObject` — `.info` file parsing
- [ ] `DiskObject` structure — do_Magic, do_Version, do_Gadget, do_Type, do_DefaultTool, do_ToolArray, do_StackSize, do_ToolTypes
- [ ] `FindToolType` / `MatchToolValue` — ToolTypes array search
- [ ] `AddAppWindow` / `RemoveAppWindow` / `AddAppIcon` / `RemoveAppIcon` / `AddAppMenuItem` / `RemoveAppMenuItem`
- [ ] `WBInfo` — info requester
- [ ] `MakeWorkbenchGadgetA` — custom icon rendering via BOOPSI
- [ ] `OpenWorkbench` / `CloseWorkbench`
- [ ] `ChangeWorkbench` notification

---

#### 78-U: Commodities, RexxSysLib, ReqTools, Datatypes

**Commodities** (`src/rom/lxa_commodities.c`):
- [ ] `CreateCxObj` — CX_FILTER, CX_TYPEFILTER, CX_SEND, CX_SIGNAL, CX_TRANSLATE, CX_BROKER, CX_DEBUG, CX_INVALID
- [ ] `CxBroker` — broker registration, NBU_UNIQUE/NBU_NOTIFY
- [ ] `ActivateCxObj` / `DeleteCxObj` / `DeleteCxObjList`
- [ ] `AttachCxObj` / `EnqueueCxObj` / `InsertCxObj` / `RemoveCxObj`
- [ ] `SetFilter` / `SetFilterIX` / `SetTranslate`
- [ ] `CxMsgType` / `CxMsgID` / `DisposeCxMsg`
- [ ] `InvertKeyMap` — hotkey string → InputXpression
- [ ] `MatchIX` — test InputEvent against InputXpression
- [ ] `GetCxObj` / `CxObjError` / `ClearCxObjError`

**RexxSysLib** (`src/rom/lxa_rexxsyslib.c`) — ARexx stub:
- [ ] `CreateArgstring` / `DeleteArgstring`
- [ ] `CreateRexxMsg` / `DeleteRexxMsg`
- [ ] `IsRexxMsg`
- [ ] `SetRexxLocalVar` / `GetRexxLocalVar`

**ReqTools** (`src/rom/lxa_reqtools.c`):
- [ ] `rtAllocRequestA` / `rtFreeRequest`
- [ ] `rtFileRequestA` — file requester dialog
- [ ] `rtFontRequestA` — font requester dialog
- [ ] `rtPaletteRequestA` / `rtScreenModeRequestA`
- [ ] `rtEZRequestA` / `rtGetStringA` / `rtGetLongA`

**Datatypes** (`src/rom/lxa_datatypes.c`):
- [ ] `ObtainDataTypeA` / `ReleaseDataType`
- [ ] `NewDTObjectA` / `DisposeDTObject`
- [ ] `GetDTMethods` / `GetDTAttrs` / `SetDTAttrsA`
- [ ] `AddDTObject` / `RemoveDTObject`
- [ ] `DrawDTObjectA`
- [ ] DTST_FILE / DTST_CLIPBOARD / DTST_RAM source types

---

#### 78-V: FS-UAE Emulation Analysis

> **Purpose**: Identify algorithmic patterns from FS-UAE (`others/fs-uae/`) that lxa can adopt or learn from,
> while staying true to lxa's WINE-like philosophy (OS-level API compatibility, NOT hardware emulation).

**CPU Emulation** (`others/fs-uae/newcpu.cpp`, `newcpu_common.cpp`, `cpuemu_*.cpp`):
- [ ] Study JIT compiler infrastructure (`compemu.cpp`, `compemu_support.cpp`) — understand code block caching strategy; consider whether m68kops.c can benefit from a simple basic-block cache
- [ ] Study exception/trap dispatch (`newcpu.cpp: Exception()`) — compare with lxa's `exceptions.s` for correctness of vector table offsets, SR manipulation
- [ ] Study division-by-zero / CHK / TRAPV exception handling — verify lxa handles these
- [ ] Study address-error exception generation (odd-address word/longword access)
- [ ] Study MOVE to SR / MOVE from SR / MOVE USP supervisor checks
- [ ] Study RTE (return from exception) SR restore semantics
- [ ] Study STOP instruction — verify lxa's STOP wakes on interrupt (INTENA/INTREQ)
- [ ] Study cycle counting per instruction — check if lxa's m68k cycle budget is representative

**Blitter** (`others/fs-uae/blitter.cpp`):
- [ ] ✅ (Phase 75b) — lxa has full blitter emulation; verify: line mode (BLTCON1 bit 0), exclusive-fill, inclusive-fill, ascending/descending direction matches FS-UAE
- [ ] Study FS-UAE's blitter nasty / CPU wait interaction (`custom.cpp: blitter_done()`) — verify lxa properly delays CPU when blitter running with DMACON BLTNASTY
- [ ] Study FS-UAE's blitter immediate vs deferred execution model vs lxa's synchronous model

**Custom Chips / DMA** (`others/fs-uae/custom.cpp`):
- [ ] Study INTENA/INTREQ handling — verify lxa's `lxa_custom_write()` correctly handles SET (bit 15 = 1) vs CLEAR (bit 15 = 0) semantics
- [ ] Study DMACON register handling — DMAEN, BLTEN, BPLEN, COPEN, SPREN, DSKEN, AUD0-3EN; verify lxa stubs are consistent
- [ ] Study copper list execution (`custom.cpp: copper_run()`) — lxa does not emulate copper; document this as intentional (no Intuition-bypass graphics)
- [ ] Study CIA-A/B timers and TOD clocks (`cia.cpp`) — lxa's timer.device already uses host timers; no direct CIA emulation needed; document the mapping
- [ ] Study POTGO / POTGOR (joystick/mouse fire buttons) if any test programs use it

**Audio** (`others/fs-uae/audio.cpp`):
- [ ] Study FS-UAE's audio DMA model — channel state machine (AUD_NONE→AUD_DMA→AUD_PLAY→AUD_PERIOD), AUDDAT/AUDLEN/AUDPER/AUDVOL registers
- [ ] Compare with lxa's `lxa_dev_audio.c` SDL2 model — identify gaps in channel allocation, period→Hz conversion, volume scaling
- [ ] Study audio interrupt (INTB_AUD0-3) timing — when does lxa fire audio completion interrupts?
- [ ] Study 4-channel mixing and sample format (8-bit signed, Paula sample rate = CPU_CLOCK / period)

**Video / Rendering** (`others/fs-uae/drawing.cpp`, `linetoscr_*.cpp`, `od-fs/fsemu/`):
- [ ] Study FS-UAE's planar→chunky conversion pipeline (`drawing.cpp`) — compare with lxa's SDL2 rendering; lxa uses SDL surfaces directly (no planar bitplane emulation at hardware level)
- [ ] Study fsemu rendering framework (`od-fs/fsemu/`) — frame pacing, vsync, pixel aspect ratio correction; identify improvements for lxa's display.c
- [ ] Study Picasso96 RTG emulation (`picasso96.cpp`) — lxa has no RTG mode; document as future Phase 79+ scope
- [ ] Study sprite overlay rendering for mouse pointer — verify lxa's SDL custom cursor approach is equivalent
- [ ] Study overscan / display window / bitplane DMA fetch timing (not applicable to lxa's layer-based approach, but document boundary)

**Filesystem / Host** (`others/fs-uae/filesys.cpp`, `od-fs/filesys_host.cpp`, `od-fs/fsdb_unix.cpp`):
- [ ] Study FS-UAE's virtual filesystem packet dispatch (`filesys.cpp: action_*()`) — compare with lxa's `lxa_dos.c` DOS packet handling; look for missing ACTION_* codes
- [ ] Study FS-UAE's case-insensitive filename matching (`fsdb_unix.cpp`) — compare with lxa's `vfs_resolve_case_path()` for correctness
- [ ] Study FS-UAE's file comment storage (`.uaem` metadata files) — consider adopting for `SetComment` support
- [ ] Study FS-UAE's file protection bit mapping to Unix permissions — compare with lxa's `_dos_set_protection()`
- [ ] Study FS-UAE's `NotifyRequest` implementation — compare with lxa's `StartNotify`/`EndNotify` stubs

---

#### 78-W: Structural Verification — OS Data Structure Offsets

> Cross-check lxa's in-memory struct layouts against AROS `exec/exec_init.c` and NDK `include/exec/types.i`.
> Use `offsetof()` assertions in a dedicated GTest to catch regressions.

- [ ] Write `tests/struct_offsets_gtest.cpp` — compile-time `static_assert` / runtime `EXPECT_EQ` checks for:
  - `ExecBase`: sizeof, key field offsets (SoftVer, LowMemChkSum, ChkBase, ColdCapture, CoolCapture, WarmCapture, SysStkUpper, SysStkLower, MaxLocMem, DebugEntry, DebugData, AlertData, MaxExtMem, ChkSum, IntVects, ThisTask, IdleCount, DispCount, Quantum, Elapsed, SysFlags, IDNestCnt, TDNestCnt, AttnFlags, AttnResched, ResModules, TaskTrapCode, TaskExceptCode, TaskExitCode, TaskSigAlloc, TaskTrapAlloc, MemList, ResourceList, DeviceList, IntrList, LibList, PortList, TaskReady, TaskWait, SoftInts, LastAlert, SysBase fields up to and including PowerSupplyFrequency)
  - `Task`: sizeof=TC_SIZE, tc_Node, tc_Flags, tc_State, tc_IDNestCnt, tc_TDNestCnt, tc_SigAlloc, tc_SigWait, tc_SigRecvd, tc_SigExcept, tc_TrapAlloc, tc_TrapAble, tc_ExceptData, tc_ExceptCode, tc_TrapData, tc_TrapCode, tc_SPReg, tc_SPLower, tc_SPUpper, tc_Switch, tc_Launch, tc_MemEntry, tc_UserData
  - `MsgPort`: mp_Node(=0), mp_Flags, mp_SigBit, mp_SigTask, mp_MsgList
  - `Message`: mn_Node, mn_ReplyPort, mn_Length
  - `Library`: lib_Node, lib_Flags, lib_pad, lib_NegSize, lib_PosSize, lib_Version, lib_Revision, lib_IdString, lib_Sum, lib_OpenCnt
  - `MemHeader` / `MemChunk`
  - `Interrupt`: is_Node, is_Data, is_Code
  - `IORequest` / `IOStdReq`
  - `FileLock`: fl_Link, fl_Key, fl_Access, fl_Task, fl_Volume
  - `FileInfoBlock`: fib_DiskKey(=0), fib_DirEntryType(=4), fib_FileName(=8), fib_Protection(=116), fib_EntryType(=120), fib_Size(=124), fib_NumBlocks(=128), fib_Date(=132), fib_Comment(=144), fib_OwnerUID(=224), fib_OwnerGID(=226)
  - `DateStamp`: ds_Days, ds_Minute, ds_Tick
  - `Screen`: sizeof, key field offsets
  - `Window`: sizeof, key field offsets
  - `RastPort`: sizeof = RP_SIZE, key field offsets
  - `BitMap`: sizeof, BytesPerRow, Rows, Flags, Depth, pad, Planes
  - `TextFont`: sizeof, key field offsets
  - `ViewPort` / `RasInfo` / `ColorMap`
  - `Gadget` / `StringInfo` / `PropInfo` / `Image` / `Border` / `IntuiText`
  - `MenuItem` / `Menu`
  - `IntuiMessage`

---

#### 78-X: Regression & Integration Testing

- [ ] After each 78-A through 78-W subsystem fix: run full `ctest --test-dir build -j8` and confirm all existing tests still pass
- [ ] Add new GTest assertions for each behavioral deviation found vs AROS
- [ ] Run RKM sample programs after structural changes to catch regressions (gadtoolsgadgets_gtest, simplemenu_gtest, filereq_gtest, fontreq_gtest, easyrequest_gtest)
- [ ] Run application tests after changes to exec/dos (asm_one_gtest, devpac_gtest, kickpascal_gtest, cluster2_gtest, dpaint_gtest)
- [ ] Update version to 0.7.0 when Phase 78 is complete (new MINOR — significant compatibility milestone)

---

## Version History

| Version | Phase | Key Changes |
| :--- | :--- | :--- |
| 0.6.61 | 77 | **Phase 77 Complete — Missing Libraries & Devices!** Implemented `mathieeesingbas.library` (12 IEEE single-precision math functions via EMU_CALL, d2 register clobbering fix). `trackdisk.device` stub (DD 3.5" floppy geometry, TD_MOTOR/TD_CHANGENUM/TD_CHANGESTATE/TD_PROTSTATUS/TD_GETGEOMETRY). Fixed `audio.device` Open() (was ADIOERR_NOALLOCATION). Enhanced `diskfont.library` (disk font loading from FONTS: directory, AFF_DISK in AvailFonts). Enhanced `locale.library` (FormatDate all format codes, FormatString printf-like, ParseDate). Fixed latent d2/d3/d4 register clobbering bug in `mathieeedoubbas.library` (all 11 asm stubs). 4 new m68k test programs (mathieeesingbas 35 tests, trackdisk, audio, locale 31 tests). 38/38 tests passing. |
| 0.6.60 | 76 | **Phase 76 Complete — Intuition & BOOPSI Enhancements!** Full BOOPSI inter-object communication: icclass/modelclass/gadgetclass dispatchers with embedded ICData, `_boopsi_do_notify()` notification pipeline with ICA_TARGET/ICA_MAP tag mapping, loop prevention. propgclass/strgclass rewritten with INST_DATA for PropGData/StrGData, PGA_*/STRINGA_* tag processing, GM_HANDLEINPUT/GM_GOINACTIVE notification. SetGadgetAttrsA/DoGadgetMethodA implemented (were stubs/crashes). ActivateWindow with proper deactivation. ZipWindow with WA_Zoom ZoomData toggle. AutoRequest modal requester. New BOOPSI_IC test (25 assertions) + Talk2Boopsi sample test. 38/38 tests passing. |
| 0.6.59 | 75b | **Hardware Blitter Emulation & CLI Assigns!** Full OCS/ECS blitter emulation (all 256 minterms, barrel shift, fill modes, ascending/descending). Fixed 32-bit custom chip writes. `-a name=path` CLI flag for command-line assigns. New hw_blitter test (7 tests). 38/38 tests passing. |
| 0.6.58 | 75a | **Test Suite Performance Optimization!** Reduced `RunCyclesWithVBlank` and `WaitForEventLoop` cycle budgets across 14 test drivers. `gadtoolsgadgets_gtest` reduced from 181s to ~130s (28% faster, safely under 180s timeout). Key findings: STOP instruction means large cycle budgets don't waste wall-clock time when CPU idle; TypeString needs minimum 8M cycles (40x200000) for string gadget rendering + GADGETUP + printf flush; inject function cycle budgets in `lxa_api.c` must not be reduced (causes event queue overflows). Total suite: ~756s (down from ~800s). 36/38 tests passing (2 pre-existing crashes unchanged). |
| 0.6.57 | 75 | **Phase 75 Complete — Advanced Graphics & Layers!** Scan-line polygon fill in `AreaEnd` (even-odd fill rule, multi-polygon). All four pixel array functions implemented. `ScrollLayer` for non-SuperBitMap layers. 9 layers.library functions. 38/38 tests passing. |
| 0.6.56 | 74 | **Phase 74 Complete — DOS & VFS Hardening!** UnLoadSeg, InternalUnLoadSeg, AllocDosObject/FreeDosObject extensions, errno2Amiga mapping, case-insensitive path resolution. 35/35 tests passing. |
| 0.6.55 | 73 | **Phase 73 Complete — Core Library Resource Management & Stability!** Fixed library reference counting (`lib_OpenCnt++/--`, `LIBF_DELEXP` clearing) in all 6 libraries (graphics, intuition, utility, mathtrans, mathffp, expansion). Implemented `Exception()` (LVO -66) with full tc_ExceptCode signal dispatch loop. Fixed `exceptions.s`: tc_Switch/tc_Launch callbacks, TF_EXCEPT handling in Schedule/Dispatch, Dispatch entry a6 load. Process initialization: pr_SegList set from NP_Seglist, pr_ConsoleTask/pr_FileSystemTask inherited from parent, cli_Prompt/cli_CommandDir inherited from parent CLI. Bootstrap FIXME cleanup. New Library ref counting test. 38/38 tests passing. |
| 0.6.54 | 72 | **Phase 72 Complete — Application Compatibility & Analysis!** Implemented `dopus.library` stub (97 functions). Extended KickPascal2 test to verify window opening (background process + window count polling). Fixed `console.device` CONU_LIBRARY handling (early return for unit -1). Added 68040 MMU registers (TC, ITT0/1, DTT0/1, MMUSR, URP, SRP) to CPU struct with MOVEC read/write handlers. Silenced PFLUSH stderr output. Comprehensive codebase audit: 93 FIXME/TODO markers catalogued across 15 files. Identified top 8 priority areas for future phases. 38/38 tests passing. |
| 0.6.53 | 71 | **Phase 71 Complete — GTest migration finalized!** Removed all 16 legacy C test drivers (`*_test.c`), ported missing coverage to GTest equivalents. 8 GTest drivers enhanced with checks from legacy versions (mousetest: button up/down/close, rawkey: key mapping/qualifiers/close, asm_one/devpac/maxonbasic: screen dims/mouse/cursor, kickpascal: screen dims/mouse, dpaint: full rewrite from no-op, cluster2: screen dims/editor/mouse/cursor/RMB). Cleaned CMakeLists.txt — `add_test_driver()` removed, only `add_gtest_driver()` remains. Test count: 54 → 38 (16 duplicates removed). 38/38 tests passing. |
| 0.6.52 | 71 | **Performance & ClipBlit fix!** `cpu_instr_callback` ~80x speedup via `g_debug_active` fast-path flag (skips breakpoint scanning/tracing when no debugging active; `dpaint_gtest`: 25s → 0.47s). Fixed ClipBlit crash (PC=0 on `rts`): root cause was direct call to `LockLayerRom`/`UnlockLayerRom` clobbering A5 frame pointer (AmigaOS ABI uses A5 for layer parameter, conflicting with GCC frame pointer). Removed no-op direct calls, ClipBlit test re-enabled. 54/54 ctest entries. |
| 0.6.51 | 70a | **Phase 70a Complete!** IDCMP_VANILLAKEY: rawkey-to-ASCII conversion with lookup tables for unshifted/shifted keys, GadToolsGadgets keyboard shortcuts now work. FileReq: window position clamping fix (TopEdge=0 accepted via sentinel -1), widened Drawer gadget label spacing. FontReq: full font requester implementation (window with OK/Cancel buttons, font list display, font selection output via fo_Attr). Fixed LXAFontRequester/LXAFileRequester struct layouts (type field at offset 0 for polymorphic dispatch). 3 new filereq_gtest tests, 5 new fontreq_gtest tests, 3 new vanillakey gadtoolsgadgets_gtest tests. 54/54 ctest entries. |
| 0.6.50 | 70a | Depth gadget AROS-style rendering (two overlapping unfilled rectangles via RectFill edges, shine-filled front rect). Depth gadget click handler: WindowToBack/WindowToFront toggle with Screen->FirstWindow list reordering. TAB/Shift-TAB string gadget cycling in _handle_string_gadget_key: forward/backward scan of GTYP_STRGADGET gadgets with wrap-around, direct gadget activation. 3 new functional tests (DepthGadgetClick, TabCyclesStringGadgets, ShiftTabCyclesBackward), enhanced DepthGadgetRendered pixel test. gadtoolsgadgets_gtest: 20→23 tests. 53/53 tests. |
| 0.6.49 | 70a | GTYP_SIZING resize gadget: system gadget with GFLG_RELRIGHT\|GFLG_RELBOTTOM relative positioning, 3D icon rendering, resize-drag interaction (SELECTDOWN/MOUSEMOVE/SELECTUP), SizeWindow() min/max enforcement. Border enlargement for WFLG_SIZEGADGET (BorderBottom=10, BorderRight=18) per RKRM/NDK. Fixed depth gadget positioning. Fixed IntuitionTest.Validation timing (lxa_flush_display). 2 new pixel tests (SizeGadgetRendered, DepthGadgetRendered). gadtoolsgadgets_gtest: 18→20 tests. 53/53 tests. |
| 0.6.48 | 70a | STRING_KIND double-bevel border: gt_create_ridge_bevel() implements FRAME_RIDGE style groove border (outer recessed: shadow top-left/shine bottom-right, inner raised: shine top-left/shadow bottom-right). 4 Border structs with proper memory management via gt_free_ridge_bevel(). FreeGadgets detects GTYP_STRGADGET for correct free. 1 new pixel test (StringGadgetDoubleBevelRendered). gadtoolsgadgets_gtest: 17→18 tests. 53/53 tests. |
| 0.6.47 | 70a | GT_Underscore keyboard shortcut indicator: gt_strip_underscore() strips prefix character from displayed labels, underline IntuiText linked via NextText chain draws '_' glyph under shortcut character. Fixed _render_gadget() to walk IntuiText NextText chain (was only rendering first IntuiText). gt_create_label() always allocates IText copy, gt_free_label() frees NextText + IText + IntuiText. 1 new pixel test (UnderscoreLabelRendered). gadtoolsgadgets_gtest: 16→17 tests. 53/53 tests. |
| 0.6.46 | 70a | SLIDER_KIND prop gadget: knob rendering (raised bevel, AUTOKNOB|FREEHORIZ|PROPNEWLOOK), mouse click/drag interaction (click-on-knob drag with offset tracking, click-outside-knob jump), IDCMP_MOUSEMOVE/GADGETUP with level as Code, GT_SetGadgetAttrs(GTSL_Level). PropInfo allocation/freeing in CreateGadgetA/FreeGadgets. 5 new slider tests (SliderClick, SliderDrag, ButtonResetsSlider, SliderKnobVisible, SliderBevelBorderRendered). gadtoolsgadgets_gtest: 11→16 tests. 53/53 tests. |
| 0.6.45 | 70a | MenuLayout sample ported from RKM (8 new tests). Fixed gadget position calculations in simplegtgadget_gtest (topborder=20 not 12, added ClickCheckbox test + checkbox bevel pixel test). Fixed gadtoolsgadgets_gtest (corrected button/string positions). INTENA re-enable fix, menu selection encoding fix. Removed all temporary debug logging. 53/53 tests. |
| 0.6.44 | 70a | SimpleMenu: save-behind buffer for menu drop-down cleanup (save/restore screen bitmap region on open/close/switch), IRQ3 debug output silenced. 2 new pixel tests (MenuDropdownClearedAfterSelection, ScreenTitleBarRestoredAfterMenu). simplemenu_gtest: 3→5 tests. 52/52 tests. |
| 0.6.43 | 70a | EasyRequest: full BuildEasyRequestArgs implementation (formatted body/gadget text with %s/%ld/%lu/%lx, 3D bevel buttons, centered layout, SysReqHandler event loop, FreeSysRequest cleanup). 7 new easyrequest_gtest tests (4 behavioral + 3 pixel). 52/52 tests. |
| 0.6.42 | 70a | SimpleImage: rewrote to match RKM (2 images, correct positions, Delay, no extra gadgets). IntuiText: full-screen window, no title bar, OpenWindowTagList ~0 defaults. 7 new simpleimage_gtest tests (4 behavioral + 3 pixel). 51/51 tests. |
| 0.6.41 | 70a | SimpleGad GADGHCOMP button click highlight. FillRectDirect byte-optimized (~8x speedup). _complement_gadget_area() for XOR highlight on SELECTDOWN/SELECTUP. lxa_flush_display() API. 48/49 tests. |
| 0.6.40 | 70 | **Phase 70 Complete!** Test suite hardening & expansion. Rewrote 3 samples to match RKM (UpdateStrGad, GadToolsGadgets, RGBBoxes). Added 3 graphics tests (AllocRaster, LayerClipping, SetRast) + 10 DOS VFS corner-case tests (DeleteRename, DirEnum, ExamineEdge, FileIOAdvanced, FileIOErrors, LockExamine, LockLeak, RenameOpen, SeekConsole, SpecialChars). DOS: 14→24 tests, Graphics: 14→17. 49/49 ctest entries. |
| 0.6.39 | 70 | **GadTools visual rendering!** Bevel borders (raised/recessed) for BUTTON/STRING/INTEGER/SLIDER/CHECKBOX/CYCLE/MX. Text labels with PLACETEXT_IN centering. WA_InnerWidth/WA_InnerHeight. Interactive GadToolsGadgets sample. 13 GadTools tests (8 functional + 5 pixel). 49/49 tests. |
| 0.6.38 | 70 | **String gadget input fix!** Fixed rawkey number row mapping, Text() cp_x layer offset bug, rewrote inject_string with per-char VBlank+cycle budget. Cleaned all debug logging. 49/49 tests. |
| 0.6.37 | 70 | **Palette pipeline fix!** SetRGB4/LoadRGB4/SetRGB32/SetRGB32CM/LoadRGB32 all operational. OpenScreen initial palette propagation. Debug LPRINTF cleanup. Pixel test updated for correct colors. 47/49 tests (2 updatestrgad pre-existing). |
| 0.6.36 | 70 | **49/49 tests passing!** `CreateGadgetA()` STRING_KIND/INTEGER_KIND: allocates StringInfo + buffer from tags. `FreeGadgets()` frees StringInfo memory. New `gadtoolsgadgets_gtest` test driver. |
| 0.6.35 | 70 | **48/48 tests passing!** Implemented ActivateGadget(). String gadget NumChars init in OpenWindow() per RKRM. Fixed simplemenu_test timing. All pre-existing test failures resolved. |
| 0.6.34 | 70 | Pixel verification tests for gadget/window borders. User gadget rendering in `_render_window_frame()`. Fixed `_render_gadget()` for border-based gadgets. Implemented `RefreshGadgets()`. Permanent exception logging improvement. Fixed flaky simplegad_gtest (CPU cycle budget). |
| 0.6.33 | 69 | **48/48 tests passing!** Fixed apps_misc_gtest (DOpus, KP2, SysInfo). Rewrote Delay() with timer.device. Optimized stress tests. Console timing fixes. |
| 0.6.32 | 63 | Phase 63 complete: Transitioned to Google Test & liblxa host-side drivers. Identified failures in samples and apps. |
| 0.6.30 | 63 | Started Phase 63: Transition to Google Test & VS Code C++ TestMate. Roadmap update. |
| 0.6.29 | 63 | Phase 63 (RKM fixes) - fixed SystemTagList, RGBBoxes, menu activation/clearing, window dragging. |
| 0.6.28 | 63 | Phase 63 (RKM fixes) - rewritten simplegad, simplemenu, custompointer, gadtoolsmenu, filereq. |
| 0.6.27 | 61 | Phase 61 complete: Implemented mathieeedoubbas.library (12 functions) and mathieeedoubtrans.library (17 functions) using EMU_CALL pattern. 29 EMU_CALL handlers for IEEE double-precision math. IEEEDPExample test sample. Cluster2 now loads successfully. |
| 0.6.26 | 59-60 | Phases 59-60 complete: Enhanced Devpac and MaxonBASIC test drivers with visual verification, screenshot capture, cursor key testing. Consistent structure with Phase 58 (KickPascal). |
| 0.6.25 | 58 | Phase 58 complete: Fixed critical display sync bug in lxa_api.c (planar-to-chunky conversion in lxa_run_cycles). KickPascal 2 fully working with cursor key support. Enhanced kickpascal_test.c with visual verification. |
| 0.6.24 | 57 | Phase 57 complete: 12 host-side test drivers, extended liblxa API (lxa_inject_rmb_click, lxa_inject_drag, lxa_get_screen_info, lxa_read_pixel, lxa_read_pixel_rgb). |
| 0.6.23 | 57 | Added 11 host-side test drivers: simplegad, mousetest, rawkey, simplegtgadget, updatestrgad, simplemenu, asm_one, devpac, kickpascal, maxonbasic, dpaint. Deep dive app drivers for ASM-One, Devpac, KickPascal 2, MaxonBASIC, DPaint V. |
| 0.6.22 | 56 | Fixed mathtrans.library CORDIC and SPExp bugs, added FFPTrans sample (42 RKM samples, 214 total tests) |
| 0.6.21 | 56 | Added Uptime, GadToolsGadgets samples, enhanced test_runner.sh with uptime/DateStamp normalization (40 RKM samples, 162 total tests) |
| 0.6.20 | 56 | Added FileReq/FontReq/GadToolsMenu samples, fixed exception handler hexdump spam, implemented MakeScreen/RethinkDisplay/RemakeDisplay, fixed DoubleBuffer VBlank reentrancy + RasInfo NULL bugs (38 RKM samples, 160 total tests) |
| 0.6.19 | 56 | Added Talk2Boopsi sample (BOOPSI inter-object communication with propgclass/strgclass, ICA_MAP/ICA_TARGET) (36 RKM samples, 122 total tests) |
| 0.6.18 | 56 | Fixed display_close use-after-free, implemented AvailFonts(), added CloneScreen/PublicScreen/VisibleWindow/WinPubScreen/MeasureText/AvailFonts/CustomPointer samples (34 RKM samples, 156 total tests) |
| 0.6.17 | 56 | Host-side test driver infrastructure (liblxa): simplegad_test, fixed IDCMP timing issues |
| 0.6.14 | 56 | Added Sift (IFF parsing), TaskList, AllocEntry, FindBoards samples; implemented FindConfigDev |
| 0.6.13 | 56 | Fixed mathffp bugs (SPSub, SPCmp, SPTst, SPAbs), added FFPExample and SimpleGTGadget samples |
| 0.6.12 | 56 | Added SimpleImage, ShadowBorder, EasyIntuition, SimpleMenu, EasyRequest, SimpleTimer, Clipping samples |
| 0.6.11 | 56 | Added SimpleGad, UpdateStrGad, IntuiText samples |
| 0.6.10 | 56 | Fixed test hex normalization, added Hooks1 sample |
| 0.6.9 | 56 | Started RKM Samples Integration |
| 0.6.8 | 55 | ASM-One V1.48 now working |
| 0.6.7 | 54 | Trap handler implementation |
| 0.6.6 | 49 | Fixed library init calling convention |
| 0.6.5 | 49+ | Extended memory map (Slow RAM, Ranger RAM, etc.) |
| 0.6.4 | 49 | Fixed Signal/Wait scheduling bug |
| 0.6.3 | 49 | Fixed DetailPen/BlockPen 0xFF handling |
| 0.6.2 | 45 | console.device async |
| 0.6.1 | 45 | timer.device async |
| 0.6.0 | 44 | BOOPSI visual completion |
