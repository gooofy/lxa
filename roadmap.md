# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project!

---

## Current Status

**Version: 0.6.68** | **Phase 78-A Complete** | **38/38 Tests Passing (GTest-only)**

Phase 78-W: Structural Verification — OS Data Structure Offsets — complete.
Phase 78-A-1: Exec Library AROS Verification — 10 bug fixes complete (v0.6.63).
Phase 78-A-2: Exec `Wait()` wait-mask fix complete (v0.6.64).
Phase 78-A-3: Exec `Wait(SIGBREAKF_CTRL_C)` verification complete (v0.6.65).
Phase 78-A-4: Exec signals and semaphores verification complete (v0.6.66).
Phase 78-A-5: Exec interrupts, nesting counters, library management, and `SumKickData()` verification complete (v0.6.68).

**Current Status**:
- 38/38 ctest entries (all GTest), including new ExecVerify test
- Phase 78-A AROS comparison completed: 27 issues identified in exec.c (10 bugs fixed, 10 behavioral differences noted, 1 missing feature, 6 correct)
- All 10 bugs fixed and verified with m68k tests
- Fixed `exec.library/Wait()` to clear `tc_SigWait` on return, matching AROS semantics and preventing stale wait masks from affecting later signal delivery
- Verified `exec.library/Wait(SIGBREAKF_CTRL_C)` already matches AROS semantics and added regression coverage to lock in Ctrl-C break handling
- Restored AROS-compatible user signal allocation defaults (`TaskSigAlloc = 0xFFFF`) so `AllocSignal(-1)` and `CreateMsgPort()` use user-allocatable signals instead of reserved low bits
- Verified and fixed Exec semaphore semantics: `AddSemaphore()` now initializes semaphores, shared semaphore APIs preserve `ss_Owner == NULL`, and waiter handoff is covered by m68k regression tests
- Verified Exec interrupt server chaining and software interrupt dispatch, plus nested `Disable()`/`Enable()` and `Forbid()`/`Permit()` counter semantics with dedicated regression coverage
- Verified Exec library management helpers against AROS semantics: `OpenLibrary()` now forwards requested versions to library open vectors and rejects too-old disk-loaded libraries; `MakeLibrary()`, `SetFunction()`, `SumLibrary()`, `AddLibrary()`, and `RemLibrary()` are locked in by custom-library tests
- Implemented and verified `SumKickData()` against AROS checksum rules for `KickTagPtr` and `KickMemPtr`
- Fixed DOS CLI BSTR handling for program metadata so `GetProgramName()`/`PROGDIR:` users (including DPaint V) stop constructing broken paths during startup
- Regression sweep complete: `exec_gtest`, `shell_gtest`, `rgbboxes_gtest`, and `dpaint_gtest` are green, and full `ctest --test-dir build --output-on-failure -j8` is green again
- Fixed test/runtime regressions in synchronous timer I/O setup, `SystemTagList()` wait-loop polling, shell variable coverage, and multitask/rgbboxes assertions

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

## Completed Phases (1-77) & Past Milestones

All foundational work, test suite transitions, performance tuning, and implementation of core libraries (Exec, DOS, Graphics, Intuition, GadTools, ASL, BOOPSI, utility, locale, math, trackdisk, etc.) have been completed. 

- **Phases 1-72**: Foundation, Test Suite Stabilization (Google Test & liblxa), Performance & Infrastructure.
- **Phase 72.5**: Codebase Audit (93 markers catalogued and resolved).
- **Phases 73-77**: Core Library Resource Management, DOS/VFS Hardening, Advanced Graphics & Layers, Intuition/BOOPSI enhancements, Missing Libraries & Devices.
- **Phase 78-A-1**: Exec Library AROS Verification — 10 bug fixes complete.
- **Phase 78-A-2**: Exec `Wait()` wait-mask clearing aligned with AROS, with signal regression coverage.
- **Phase 78-A-3**: Exec `Wait(SIGBREAKF_CTRL_C)` behavior verified against AROS, with explicit break-signal regression coverage.
- **Phase 78-A-5**: Exec interrupts, nesting counters, library management, and `SumKickData()` verified against AROS behavior.
- **Phase 78-W**: Structural Verification — OS Data Structure Offsets — 460 assertions passing.

---

## Current & Future Plans

### Phase 78: AROS Compatibility Verification & FS-UAE Analysis

**Goal**: Systematically compare `lxa` against AROS (reference source in `others/AROS-20231016-source/`) and
analyse FS-UAE (`others/fs-uae/`) for algorithmic insights applicable to lxa's WINE-like approach.

Extend the test suite with targeted tests where feasible, extending the test suite coverage.

> **Note**: AROS source lives under `others/AROS-20231016-source/rom/`.  
> Key sub-dirs: `exec/`, `dos/`, `graphics/`, `hyperlayers/`, `intuition/`, `utility/`, `timer/`, `keymap/`,
> `expansion/`, `task/`, `devs/console/`, `devs/input/`, `devs/trackdisk/`, `devs/gameport/`, `devs/keyboard/`,
> `compiler/alib/` (GadTools, ASL alib wrappers).

---

#### 78-A: Exec Library Full Verification Checklist (`src/rom/exec.c` vs `others/AROS-20231016-source/rom/exec/`)

Status: complete in 0.6.68.

**Data Structures** (verify offsets/sizes against `exec/exec_init.c`, `exec/exec_util.c`, NDK `exec/types.i`):
- [x] `ExecBase` — verify all public field offsets (VBlankFrequency, PowerSupplyFrequency, KickMemPtr, etc.)
- [x] `Task` — tc_Node, tc_Flags, tc_State, tc_IDNestCnt, tc_TDNestCnt, tc_SigAlloc, tc_SigWait, tc_SigRecvd, tc_SigExcept, tc_TrapAlloc, tc_TrapAble, tc_ExceptData, tc_ExceptCode, tc_TrapData, tc_TrapCode, tc_SPReg, tc_SPLower, tc_SPUpper, tc_Switch, tc_Launch, tc_MemEntry, tc_UserData
- [x] `MsgPort` — mp_Node, mp_Flags, mp_SigBit, mp_SigTask, mp_MsgList
- [x] `Message` — mn_Node, mn_ReplyPort, mn_Length
- [x] `IORequest` / `IOStdReq` — io_Message, io_Device, io_Unit, io_Command, io_Flags, io_Error, io_Actual, io_Length, io_Data, io_Offset
- [x] `Library` — lib_Node, lib_Flags, lib_pad, lib_NegSize, lib_PosSize, lib_Version, lib_Revision, lib_IdString, lib_Sum, lib_OpenCnt
- [x] `Device` / `Unit` structure offsets
- [x] `Interrupt` — is_Node, is_Data, is_Code
- [x] `IntVector` (ExecBase.IntVects array)
- [x] `MemChunk` / `MemHeader` — mh_Node, mh_Attributes, mh_First, mh_Lower, mh_Upper, mh_Free
- [x] `SemaphoreRequest` / `SignalSemaphore` — ss_Link, ss_NestCount, ss_Owner, ss_QueueCount, ss_WaitQueue
- [x] `List` / `MinList` / `Node` / `MinNode` offsets

**Memory Management** (vs `exec/memory.c`, `exec/mementry.c`):
- [x] `AllocMem` — MEMF_CLEAR, MEMF_CHIP, MEMF_FAST, MEMF_ANY, MEMF_LARGEST, alignment guarantees
- [x] `FreeMem` — size parameter must match allocated size; verify behaviour on NULL pointer
- [x] `AllocVec` / `FreeVec` — hidden size word prepended; MEMF_CLEAR
- [x] `AllocEntry` / `FreeEntry` — MemList allocation loop, partial failure rollback
- [x] `AvailMem` — MEMF_TOTAL, MEMF_LARGEST
- [x] `TypeOfMem` — returns attribute flags for address; returns 0 for unknown addresses
- [x] `AddMemList` / `AddMemHandler` / `RemMemHandler`
- [x] `CopyMem` / `CopyMemQuick` — alignment optimizations; behaviour on overlap

**List Operations** (vs `exec/lists.c`):
- [x] `AddHead` / `AddTail` / `Remove` / `RemHead` / `RemTail` — correct sentinel node handling
- [x] `Insert` — position node, predecessor check
- [x] `FindName` — case-sensitive string compare, returns first match
- [x] `Enqueue` — priority-based insertion (ln_Pri, higher=closer to head)

**Task Management** (vs `exec/tasks.c`, `exec/task/`, `exec/schedule.c`):
- [x] `AddTask` — NP_* tags, stack allocation, initial PC/stack setup
- [x] `RemTask` — can remove self (via Wait loop exit) or other task; cleanups
- [x] `FindTask` — NULL returns current task; string search
- [x] `SetTaskPri` — returns old priority; reschedule if needed
- [x] `GetCC` / `SetSR`
- [x] Task state machine: TS_RUN, TS_READY, TS_WAIT, TS_EXCEPT, TS_INVALID

**Signals & Semaphores** (vs `exec/semaphores.c`):
- [x] `AllocSignal` / `FreeSignal` — signal bit allocation from tc_SigAlloc
- [x] `Signal` — set bits in tc_SigRecvd; wake from wait if bits match tc_SigWait
- [x] `Wait` — clear tc_SigWait on return
- [x] `Wait` — SIGBREAKF_CTRL_C handling
- [x] `SetSignal` — set/clear signal bits without scheduling
- [x] `InitSemaphore` / `ObtainSemaphore` / `ReleaseSemaphore` — nest count, queue
- [x] `ObtainSemaphoreShared` / `AttemptSemaphore` / `AttemptSemaphoreShared`
- [x] `AddSemaphore` / `FindSemaphore` / `RemSemaphore`

**I/O & Message Passing** (vs `exec/io.c`, `exec/ports.c`):
- [x] `CreateMsgPort` / `DeleteMsgPort` — signal bit allocation, mp_SigBit, mp_SigTask
- [x] `CreateIORequest` / `DeleteIORequest`
- [x] `OpenDevice` / `CloseDevice` — device open/close counting
- [x] `DoIO` / `SendIO` / `AbortIO` / `WaitIO` / `CheckIO` — sync vs async I/O
- [x] `GetMsg` / `PutMsg` / `ReplyMsg` / `WaitPort`

**Interrupts** (vs `exec/interrupts.c`):
- [x] `AddIntServer` / `RemIntServer` — INTB_* interrupt vectors; priority insertion
- [x] `Cause` — software interrupt dispatch
- [x] `Disable` / `Enable` — nesting via IDNestCnt
- [x] `Forbid` / `Permit` — nesting via TDNestCnt

**Library Management** (vs `exec/libraries.c`, `exec/resident.c`):
- [x] `OpenLibrary` / `CloseLibrary` — version check, lib_OpenCnt, expunge on close
- [x] `MakeLibrary` / `MakeFunctions` / `SumLibrary`
- [x] `AddLibrary` / `RemLibrary` / `FindName` in SysBase->LibList
- [x] `SetFunction` — replace LVO function, return old pointer
- [x] `SumKickData`

**Miscellaneous Exec**:
- [ ] `RawDoFmt` — format string parsing (`%s`, `%d`, `%ld`, `%x`, `%lx`, `%b`, `%c`), PutChProc callback; verify against AROS `exec/rawfmt.c`
- [ ] `NewRawDoFmt` — extended version
- [x] `Debug` — serial output stub
- [ ] `Alert` — deadlock/crash alert display
- [ ] `Supervisor` — enter supervisor mode
- [x] `UserState` — return to user mode
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

#### 78-X: Regression & Integration Testing

- [ ] After each 78-A through 78-W subsystem fix: run full `ctest --test-dir build -j8` and confirm all existing tests still pass
- [x] Regression maintenance: restore `exec_gtest`, `shell_gtest`, and `rgbboxes_gtest` to green after timer/SystemTagList/test-harness regressions; verified with full `ctest --test-dir build --output-on-failure -j8`
- [ ] Add new GTest assertions for each behavioral deviation found vs AROS
- [ ] Run RKM sample programs after structural changes to catch regressions (gadtoolsgadgets_gtest, simplemenu_gtest, filereq_gtest, fontreq_gtest, easyrequest_gtest)
- [ ] Run application tests after changes to exec/dos (asm_one_gtest, devpac_gtest, kickpascal_gtest, cluster2_gtest, dpaint_gtest)
- [ ] Update version to 0.7.0 when Phase 78 is complete (new MINOR — significant compatibility milestone)

---

## Version History

*Massively compacted. For full historical details, refer to Git history.*

| Version | Phase | Key Changes |
| :--- | :--- | :--- |
| **0.6.68** | 78-A Complete | Completed Exec Phase 78-A verification by covering interrupt nesting, library management helpers, and `SumKickData()`, with the full 38-test suite green. |
| **0.6.67** | 78-A-4 | Full regression suite green again after fixing DOS CLI BSTR program-name handling, which restored DPaint V startup. |
| **0.6.66** | 78-A-4 | Phase 78-A-4 Complete — verified Exec signal allocation and semaphore semantics, including shared locks and waiter handoff. |
| **0.6.65** | 78-A-3 | Phase 78-A-3 Complete — verified `Wait(SIGBREAKF_CTRL_C)` behavior and added Ctrl-C regression coverage. |
| **0.6.64** | 78-A-2 | Phase 78-A-2 Complete — fixed `Wait()` to clear `tc_SigWait` on return and added signal regression coverage. |
| **0.6.63** | 78-A-1 | Phase 78-A-1 Complete — Exec AROS Bug Fixes! Fixed 10 bugs in `exec.c`. |
| **0.6.0 - 0.6.62** | 44 - 78-W | Core system implementation, test suite stabilization, OS data structure offset verification, missing libraries implementation. |
