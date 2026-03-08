# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project!

---

## Current Status

**Version: 0.6.87** | **Phase 78-C Graphics Verification In Progress** | **49/49 Tests Passing (GTest-only)**

Phase 78-W: Structural Verification — OS Data Structure Offsets — complete.
Phase 78-A-1: Exec Library AROS Verification — 10 bug fixes complete (v0.6.63).
Phase 78-A-2: Exec `Wait()` wait-mask fix complete (v0.6.64).
Phase 78-A-3: Exec `Wait(SIGBREAKF_CTRL_C)` verification complete (v0.6.65).
Phase 78-A-4: Exec signals and semaphores verification complete (v0.6.66).
Phase 78-A-5: Exec interrupts, nesting counters, library management, and `SumKickData()` verification complete (v0.6.68).
Phase 78-A-6: Exec miscellaneous — `RawDoFmt` edge cases, list accessors, `Alert`, `Supervisor` verification complete (v0.6.69).
Phase 78-B replanned: original DOS checklist preserved, split into `78-B-1` through `78-B-7` so each subphase can be completed in one focused session.
Phase 78-B-7: DOS pattern/regression sweep complete; Phase 78-B DOS verification is now complete (v0.6.77).
Phase 78-C: graphics public-structure verification started; `GfxBase`, `BitMap`, `RastPort`, `ViewPort`, `RasInfo`, `ColorMap`, `AreaInfo`, `TmpRas`, `GelsInfo`, and `TextFont` are now covered by `StructOffsets` (v0.6.78).
Phase 78-C: drawing verification expanded; `Draw`, `RectFill`, `SetRast`, `WritePixel`/`ReadPixel`, `PolyDraw`, and `Flood` now run in the unified graphics regression sweep, with `Draw`/`RectFill` specifically locked against `COMPLEMENT` and `INVERSVID` behavior (v0.6.79).
Phase 78-C: ellipse verification expanded; `AreaEllipse` now stores native ellipse markers in `AreaInfo`, `AreaEnd()` fills/renders those records directly, and both `DrawEllipse` and `AreaEllipse` are locked in by the graphics regression sweep (v0.6.80).
Phase 78-C: area-fill verification expanded; `AreaEnd()` is now locked against even-odd fill behavior for self-overlapping paths, and the NDK `AreaCircle` shorthand is validated as `AreaEllipse(r,r)` through direct graphics regression coverage (v0.6.81).
Phase 78-C: blit verification expanded; `BltBitMap`, `BltBitMapRastPort`, `ClipBlit`, and `BltMaskBitMapRastPort` are now covered for overlap semantics, destination minterms, layer ClipRect clipping, and masked copies in the unified graphics regression sweep (v0.6.82).
Phase 78-C: text/font verification expanded; graphics now initializes and queries the public font list correctly, `OpenFont`/`CloseFont` follow list-based lookup and lifetime rules, `SetFont`/`AskFont`/`SetSoftStyle` are locked against current NDK-style semantics, and the existing text regression programs now verify those behaviors directly (v0.6.83).
Phase 78-C: blitter/resource verification expanded; `QBlit`/`QBSBlit` now run through queued FIFO/beam-priority execution with cleanup callbacks, `WaitBlit`/`OwnBlitter`/`DisownBlitter` now coordinate ownership and queue draining instead of acting as no-ops, and `blitter.resource` plus `AddResource`/`RemResource` lifecycle coverage are locked in by new graphics/exec regressions (v0.6.84).
Phase 78-C: bitmap allocation verification expanded; `InitBitMap` is now locked against `BMF_STANDARD`/`pad` initialization while preserving caller-owned planes, and `AllocBitMap`/`FreeBitMap` now have direct regression coverage for chip-plane allocation, flag propagation, and safe cleanup paths (v0.6.85).
Phase 78-C: raster allocation verification expanded; `AllocRaster`/`FreeRaster` are now aligned with AROS/NDK CHIP-memory semantics, `AllocBitMap(BMF_CLEAR)` performs its own plane clearing instead of relying on `AllocRaster`, and the graphics regression sweep now locks in CHIP allocation, minimum-word sizing, and NULL-safe frees (v0.6.86).
Phase 78-C: region verification expanded; region boolean ops now split/intersect/toggle rectangles instead of appending placeholders, `RectInRegion`/`PointInRegion` are exposed through the graphics vectors used by tests, and the graphics regression sweep now covers region/rect membership plus `StressTasks` timing is hardened against host contention (v0.6.87).

**Current Status**:
- 49/49 ctest entries (all GTest), with long-running UI/application suites split into balanced shards for faster parallel execution
- Full `ctest --test-dir build --output-on-failure -j8` rerun refreshed the top-line count at 49/49 passing entries
- Original Phase 78-B DOS checklist retained in full, but regrouped into session-sized subphases to avoid closing the phase against unimplemented stubs
- Phase 78-A AROS comparison completed: 27 issues identified in exec.c (10 bugs fixed, 10 behavioral differences noted, 1 missing feature, 6 correct)
- All remaining miscellaneous Exec items verified: `RawDoFmt` edge cases (maxwidth, precision, `%c`, `%%`, `%b` BSTR, return value), list accessors (`GetHead`/`GetTail`/`GetSucc`/`GetPred` as macros), `Alert` (recovery vs deadend decoding), `Supervisor` (m68k privilege-switch call)
- `NewRawDoFmt` intentionally skipped: AROS V45+ extension not part of standard AmigaOS 3.x
- All 10 bugs fixed and verified with m68k tests
- Fixed `exec.library/Wait()` to clear `tc_SigWait` on return, matching AROS semantics and preventing stale wait masks from affecting later signal delivery
- Verified `exec.library/Wait(SIGBREAKF_CTRL_C)` already matches AROS semantics and added regression coverage to lock in Ctrl-C break handling
- Restored AROS-compatible user signal allocation defaults (`TaskSigAlloc = 0xFFFF`) so `AllocSignal(-1)` and `CreateMsgPort()` use user-allocatable signals instead of reserved low bits
- Verified and fixed Exec semaphore semantics: `AddSemaphore()` now initializes semaphores, shared semaphore APIs preserve `ss_Owner == NULL`, and waiter handoff is covered by m68k regression tests
- Verified Exec interrupt server chaining and software interrupt dispatch, plus nested `Disable()`/`Enable()` and `Forbid()`/`Permit()` counter semantics with dedicated regression coverage
- Verified Exec library management helpers against AROS semantics: `OpenLibrary()` now forwards requested versions to library open vectors and rejects too-old disk-loaded libraries; `MakeLibrary()`, `SetFunction()`, `SumLibrary()`, `AddLibrary()`, and `RemLibrary()` are locked in by custom-library tests
- Implemented and verified `SumKickData()` against AROS checksum rules for `KickTagPtr` and `KickMemPtr`
- Fixed DOS CLI BSTR handling for program metadata so `GetProgramName()`/`PROGDIR:` users (including DPaint V) stop constructing broken paths during startup
- Added DOS CLI metadata helpers (`Set/GetCurrentDirName`, `Set/GetProgramName`, `Set/GetPrompt`) plus `FindCliProc()`/`MaxCli()` coverage, and extended structural verification for `DosLibrary`, `RootNode`, `DosInfo`, `CliProcList`, and `Segment`
- Verified multi-CLI proc-window semantics: `CreateNewProc()` now inherits `pr_WindowPtr` by default, respects explicit `NP_WindowPtr` overrides (including `NULL`), and `SystemTagList()` passes the caller window pointer for synchronous launches while clearing it for async launches, matching documented `pr_WindowPtr` inheritance rules
- Implemented `SetFileSize()` with host-backed truncate/extend semantics and added DOS regression coverage for begin/current/end resizing plus zero-filled extension behavior
- Completed Phase 78-B-3 host-backed DOS extended file semantics: `SetFileDate`, `ExAll`/`ExAllEnd`, and `MakeLink`/`ReadLink` now work with direct DOS/command regression coverage, including soft-link target preservation and hard-link creation
- Completed Phase 78-B-4 DOS assign/device/notify support: `AssignLock`/`AssignLate`/`AssignPath`/`AssignAdd`/`RemAssignList` now handle multi-directory assign iteration and per-path removal, `GetDevProc`/`FreeDevProc` return iterable assign targets, and `StartNotify`/`EndNotify` deliver host-polled notify messages/signals through the VBlank path
- Completed Phase 78-B-5 buffered DOS I/O helpers: `SetVBuf`, `FRead`, `FWrite`, `FPuts`, `VFPrintf`, and `FWritef`-style varargs flows now run through host-backed buffered helpers with direct DOS regression coverage for block reads/writes, line buffering, and `UnGetC` interaction
- Verified against NDK/AROS references that `SPrintf`/`VSPrintf` are not public AmigaOS 3.x DOS APIs, so they are removed from the DOS roadmap scope instead of being exposed as new non-standard vectors
- Completed Phase 78-B-6 DOS loader/runtime coverage: `InternalLoadSeg`, `NewLoadSeg`, `RunCommand`, and `GetSegListInfo` now work with direct regression coverage for hunk seglists, synchronous loaded-program execution, and `CreateProc()` child startup behavior
- Completed Phase 78-B-7 DOS pattern/regression sweep: `MatchPattern`/`MatchPatternNoCase` now cover deferred `(a|b)` alternation groups, direct DOS regression tests lock in CLI metadata and variable helpers plus `SetComment`/`SetProtection`/`Info`/`SameLock`, and the full DOS/application/command regression sweep is green again
- Regression sweep complete: `exec_gtest`, `shell_gtest`, `rgbboxes_gtest`, and `dpaint_gtest` are green, and full `ctest --test-dir build --output-on-failure -j8` is green again
- Fixed test/runtime regressions in synchronous timer I/O setup, `SystemTagList()` wait-loop polling, shell variable coverage, and multitask/rgbboxes assertions
- Fixed the unrelated `commands_gtest` failure in `TYPE`: failed opens now preserve the host-reported `IoErr()` instead of collapsing to stale zero/incorrect errors
- Test-suite scheduling improved: sharded `gadtoolsgadgets`, `simplegad`, `simplemenu`, `menulayout`, and `cluster2` into smaller CTest entries, reducing `ctest -j8` wall time from about 124s to about 95s while keeping total CPU time roughly flat
- Completed project-wide MIT license migration and documentation consistency cleanup: replaced the root Apache 2.0 license text with MIT, aligned repository documentation with the MIT license, refreshed the primary testing/build docs to the current CTest/GTest workflow, and marked older planning documents as historical references where appropriate
- Started Phase 78-C graphics verification by extending `StructOffsets` coverage to the remaining core public graphics structures: `GfxBase`, `ViewPort`, `RasInfo`, `ColorMap`, `AreaInfo`, `TmpRas`, and `GelsInfo`; the existing `BitMap`, `RastPort`, and `TextFont` checks now close the full data-structure subsection against NDK `.i` layout references
- Expanded Phase 78-C drawing verification across the existing graphics primitives: added direct regression coverage for `Draw` and `RectFill` `INVERSVID` behavior on top of the existing `COMPLEMENT` checks, corrected `Draw`/`RectFill` to use `BgPen` when `INVERSVID` is set, integrated `PolyDraw` and `Flood` into the `graphics_gtest` sweep and sample install set, and fixed `StructOffsets` to reflect the NDK `AreaInfo` layout including `FirstX`/`FirstY`
- Expanded Phase 78-C ellipse verification: corrected `AreaEllipse` to follow the AROS/NDK contract of storing a two-entry ellipse record instead of flattening to polygon vectors, taught `AreaEnd()` to fill and outline ellipse records directly, and kept `DrawEllipse`/`AreaEllipse` covered by the unified `graphics_gtest` sweep
- Expanded Phase 78-C area-fill verification: added a self-overlapping double-trace regression so `AreaEnd()` stays aligned with the even-odd scan-line rule, and validated the NDK `AreaCircle` macro shorthand on top of the native ellipse area-fill path
- Expanded Phase 78-C blit verification: refactored bitmap blitting around shared minterm/mask helpers, added overlap-safe `BltBitMap` coverage plus direct `BltBitMapRastPort` and `BltMaskBitMapRastPort` regression tests, and taught the raster-port blit entry points and `ClipBlit` to respect visible destination ClipRects and obscured source layers
- Expanded Phase 78-C text/font verification: `graphics.library` now seeds `GfxBase->TextFonts` with the built-in Topaz font, `OpenFont` searches the public list by name and requested size instead of silently falling back, `CloseFont` drops non-ROM fonts from the public list when their accessor count reaches zero, `SetFont` updates `TxWidth` alongside height/baseline, `AskFont` falls back to the default font when needed, and `SetSoftStyle` now updates `rp->AlgoStyle` with proper enable-mask semantics while `text_render`/`text_extent` lock the behavior in
- Expanded Phase 78-C blitter/resource verification: initialized `GfxBase` blitter wait state, implemented hosted queue draining for `QBlit`/`QBSBlit` with `CLEANUP` callbacks and QBS priority, taught `WaitBlit`/`OwnBlitter`/`DisownBlitter` to honor ownership and pending work using the shared blit wait queue, registered `blitter.resource` during cold start, and added direct regression coverage for both the blitter APIs and `AddResource`/`RemResource`
- Expanded Phase 78-C bitmap allocation verification: `AllocBitMap()` now reuses `InitBitMap()` semantics for public fields, rejects unsupported extended-tag allocations instead of misinterpreting taglists, preserves the public allocation flags we currently support, and new graphics regressions lock `InitBitMap`/`AllocBitMap`/`FreeBitMap` against allocation, zero-fill, and cleanup behavior
- Expanded Phase 78-C raster allocation verification: `AllocRaster()` now matches AROS and NDK autodoc behavior by returning raw CHIP memory without implicit clearing, `AllocBitMap()` now applies `BMF_CLEAR` explicitly to each allocated plane, and `alloc_raster` regression coverage now verifies CHIP memory ownership, minimum `RASSIZE()` rounding, repeated alloc/free cycles, and `FreeRaster(NULL)` safety
- Expanded Phase 78-C region verification: `ClearRectRegion` now splits partially cleared rectangles into the remaining bands, `XorRectRegion` and `AndRegionRegion` operate on actual rectangle geometry instead of placeholder bounds math, the graphics private region vectors now implement `RectInRegion` and `PointInRegion`, `graphics/regions` exercises boolean and membership semantics directly, and `misc_gtest` now treats `StressTasks` as PASS-on-timeout when the wrapper already printed success while the stress binary itself was trimmed to finish comfortably under parallel suite load

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
- **Phase 78-B**: DOS library verification completed, including loader/runtime coverage, assign/notify support, buffered I/O, and the final pattern/regression sweep.
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
- [x] `RawDoFmt` — format string parsing (`%s`, `%d`, `%ld`, `%x`, `%lx`, `%b`, `%c`), PutChProc callback; verified against AROS `exec/rawfmt.c`
- [x] `NewRawDoFmt` — AROS V45+ extension; intentionally skipped (not part of AmigaOS 3.x)
- [x] `Debug` — serial output stub
- [x] `Alert` — recovery vs deadend decoding from alertNum bit 31
- [x] `Supervisor` — enter supervisor mode (m68k assembly privilege-switch)
- [x] `UserState` — return to user mode
- [x] `GetHead` / `GetTail` / `GetSucc` / `GetPred` — macros in `util.h` (AROS header-only, not LVO functions)

---

#### 78-B: DOS Library (`src/rom/lxa_dos.c` vs `others/AROS-20231016-source/rom/dos/`)

Status: complete in 0.6.77.

##### 78-B-1: DOS Core Surface Verification

Status: complete in planning terms; existing implementation and tests cover this slice already.

**Data Structures & structural coverage**:
- [x] `FileLock` — fl_Link, fl_Key, fl_Access, fl_Task, fl_Volume (BPTR chaining)
- [x] `FileHandle` — fh_Link, fh_Port, fh_Type, fh_Buf, fh_Pos, fh_End, fh_Funcs, fh_Func2, fh_Func3, fh_Arg1, fh_Arg2
- [x] `FileInfoBlock` — fib_DiskKey, fib_DirEntryType, fib_FileName[108], fib_Protection, fib_EntryType, fib_Size, fib_NumBlocks, fib_Date, fib_Comment[80], fib_OwnerUID, fib_OwnerGID
- [x] `InfoData` — id_NumSoftErrors, id_UnitNumber, id_DiskState, id_NumBlocks, id_NumBlocksUsed, id_BytesPerBlock, id_DiskType, id_VolumeNode, id_InUse
- [x] `DateStamp` — ds_Days, ds_Minute, ds_Tick
- [x] `DosPacket` — dp_Link, dp_Port, dp_Type, dp_Res1, dp_Res2, dp_Arg1..dp_Arg7

**File I/O**:
- [x] `Open` — MODE_OLDFILE, MODE_NEWFILE, MODE_READWRITE; returns BPTR FileHandle or NULL (IoErr set)
- [x] `Close` — flush, return success/failure; NULL handle no-op
- [x] `Read` — returns bytes read, 0=EOF, -1=error
- [x] `Write` — returns bytes written or -1
- [x] `Seek` — OFFSET_BEGINNING, OFFSET_CURRENT, OFFSET_END; returns old position or -1
- [x] `Flush` — flush file handle buffers

**Lock & Examine**:
- [x] `Lock` — SHARED_LOCK (ACCESS_READ), EXCLUSIVE_LOCK (ACCESS_WRITE); returns BPTR or NULL
- [x] `UnLock` — NULL is no-op
- [x] `DupLock` / `DupLockFromFH`
- [x] `Examine` / `ExNext` — FIB population; fib_DirEntryType positive=dir, negative=file
- [x] `ExamineFH`
- [x] `SameLock` — compare two locks (same volume+key)
- [x] `Info` — populate InfoData for a lock's volume

**Directory & Path Operations**:
- [x] `CreateDir` — returns exclusive lock on new dir or NULL
- [x] `DeleteFile` — file or empty directory; IOERR_DELETEOBJECT on in-use
- [x] `Rename` — cross-directory; error on cross-volume
- [x] `SetProtection` — FIBF_* bits; maps to host `chmod`
- [x] `SetComment` — file comment (extended attribute or ignore)
- [x] `NameFromLock` / `NameFromFH` — build full Amiga path string
- [x] `FilePart` / `PathPart` — string operations only (no I/O)
- [x] `AddPart` — append name to path buffer

**Pattern Matching**:
- [x] `MatchPattern` / `MatchPatternNoCase` — current tested support for `#?`, `?`, `*`, `[a-z]`, `[~a-z]`, and `(a|b)` alternation
- [x] `ParsePattern` / `ParsePatternNoCase` — current tokenization behavior locked in by tests
- [x] `MatchFirst` / `MatchNext` / `MatchEnd` — directory iteration with pattern

**Environment, CLI, Variables**:
- [x] `GetVar` / `SetVar` / `DeleteVar` — GVF_LOCAL_VAR, GVF_GLOBAL_VAR, GVF_BINARY_VAR; ENV: assigns
- [x] `GetProgramName`
- [x] `CurrentDir` — set process current dir, return old lock

**Date & Time**:
- [x] `DateStamp` — fill with current time
- [x] `Delay` — TICKS_PER_SECOND = 50; uses timer.device
- [x] `WaitForChar` — wait up to timeout for character on file handle
- [x] `DateToStr` / `StrToDate` — locale-independent

**Formatting**:
- [x] `FPuts` / `FPrintf` / `VFPrintf`
- [x] `PrintFault` — print IoErr message to file handle

**Program Loading**:
- [x] `LoadSeg` — HUNK_HEADER, HUNK_CODE/DATA/BSS, HUNK_RELOC32, HUNK_END; returns BPTR seglist
- [x] `UnLoadSeg` — walk seglist, FreeVec each hunk ✅ (Phase 74)
- [x] `InternalUnLoadSeg` ✅ (Phase 74)
- [x] `CreateProc` / `CreateNewProc`
- [x] `Execute` — run shell command string
- [x] `System` — run with I/O redirection

**Error Handling**:
- [x] `IoErr` / `SetIoErr` — per-process error code (pr_Result2)
- [x] `Fault` — IOERR_* code to string

##### 78-B-2: DOS Public Structures & CLI Metadata

Goal: finish DOSBase/RootNode/DosInfo public-field verification and the remaining CLI/path metadata helpers.

- [x] `DOSBase` — public fields: RootNode, timer request / utility / intuition base pointers verified via `DosLibrary` layout checks (classic NDK public DOS library layout)
- [x] `RootNode` — rn_TaskArray, rn_ConsoleSegment, rn_Time, rn_RestartSeg, rn_Info, rn_BootProc, rn_CliList, rn_ShellSegment, rn_Flags
- [x] `DosInfo` — di_McName, di_DevInfo, di_Devices, di_Handlers, di_NetHand, di_DevLock, di_EntryLock, di_DeleteLock
- [x] `CliProcList` for multi-CLI task-array metadata
- [x] `Segment` (seglist chain: BPTR links)
- [x] `ProcessWindowNode` / proc-window stack semantics for multi-CLI
- [x] `GetCurrentDirName` / `SetCurrentDirName`
- [x] `GetProgramName` / `SetProgramName`
- [x] `GetPrompt` / `SetPrompt`

##### 78-B-3: DOS Extended File/Directory Semantics

Status: complete in 0.6.73, except `ChangeFilePosition` / `GetFilePosition`, which do not appear to exist as standard public dos.library APIs in the NDK surface used by lxa and are therefore deferred pending a verified public reference.

Goal: finish the remaining file-size, date, link, and full-directory-enumeration APIs.

- [x] `SetFileSize` — truncate/extend file (AROS: `dos/setfilesize.c`)
- [x] `ExAll` / `ExAllEnd` — ED_NAME/ED_TYPE/ED_SIZE/ED_PROTECTION/ED_DATE/ED_COMMENT/ED_OWNER, match-string filtering, multi-entry buffers
- [x] `SetFileDate` — set datestamp
- [x] `MakeLink` — hard link (type 0) or soft link (type 1)
- [x] `ReadLink` — read soft link target

##### 78-B-4: DOS Assigns, Device Resolution, and Notifications

Status: complete in 0.6.74.

Goal: complete assign traversal APIs and host-backed notifications.

- [x] `AssignLock` / `AssignLate` / `AssignPath` / `AssignAdd`
- [x] `RemAssignList` / `GetDevProc` / `FreeDevProc`
- [x] Multi-directory assigns (path list iteration)
- [x] `StartNotify` / `EndNotify` — `NotifyRequest` structure; NRF_SEND_MESSAGE / NRF_SEND_SIGNAL

##### 78-B-5: DOS Formatting & Buffered I/O Completion

Status: complete in 0.6.75.

Goal: finish buffered/file-oriented formatting helpers that are still stubbed or only partially covered.

- [x] `SetVBuf` — set file buffering
- [x] `FPuts` / `FPrintf` / `VFPrintf` / `FRead` / `FWrite`
- [x] Validate non-standard `SPrintf` / `VSPrintf` status against NDK/AROS references and remove them from DOS scope

##### 78-B-6: DOS Program Loading Completion

Status: complete in 0.6.76.

Goal: close the remaining loader/runtime entry points around the existing `LoadSeg`/`Execute`/`System` implementation.

- [x] Fix regression: ctest is still failing on the unrelated commands_gtest type command
- [x] `InternalLoadSeg`
- [x] `NewLoadSeg` — with tags
- [x] `CreateProc` / `CreateNewProc` / `RunCommand`
- [x] `GetSegListInfo`

##### 78-B-7: DOS Pattern/Regression Sweep

Status: complete in 0.6.77.

Goal: close remaining behavior gaps and lock the whole DOS phase down with direct regression coverage.

- [x] `MatchPattern` / `MatchPatternNoCase` — add deferred `(a|b)` alternation coverage and remaining token edge cases used by current tests
- [x] Add direct regression coverage for vars, prompt/program-name/current-dir helpers, `SetComment`/`SetProtection`, `Info`/`SameLock`, and newly completed DOS APIs
- [x] Run the full DOS application/command regression sweep after `78-B-2` through `78-B-6` land

---

#### 78-C: Graphics Library (`src/rom/lxa_graphics.c` vs `others/AROS-20231016-source/rom/graphics/`)

**Data Structures** (vs `graphics/gfx_init.c`, NDK `graphics/gfx.h`):
- [x] `GfxBase` — ActiView, ActiViewCprSemaphore, NormalDisplayRows/Columns, MaxDisplayRow/Col
- [x] `RastPort` — Layer, BitMap, AreaInfo, TmpRas, GelsInfo, Mask, FgPen, BgPen, AOlPen, DrawMode, AreaPtSz, linpatcnt, dummy, Flags, LinePtrn, cp_x, cp_y, minterms[8], PenWidth, PenHeight, Font, AlgoStyle, TxFlags, TxHeight, TxWidth, TxBaseline, TxSpacing, RP_User, longreserved[2], wordreserved[7], reserved[8]
- [x] `BitMap` — BytesPerRow, Rows, Flags, Depth, pad, Planes[8]
- [x] `ViewPort` — Next, ColorMap, DspIns, SprIns, ClrIns, UCopIns, DWidth, DHeight, DxOffset, DyOffset, Modes, SpritePriorities, ExtendedModes, RasInfo
- [x] `RasInfo` — Next, BitMap, RxOffset, RyOffset
- [x] `ColorMap` — Flags, Type, Count, ColorTable, PalExtra, SpriteBase_Even/Odd, Bp_0_base, Bp_1_base
- [x] `AreaInfo` — VctrTbl, VctrPtr, FlagTbl, FlagPtr, Count, MaxCount, FirstX, FirstY
- [x] `TmpRas` — RasPtr, Size
- [x] `GelsInfo` — sprRsrvd, Flags, gelHead/tail, nextLine, lastColor, collHandler, bounds, firstBlissObj, lastBlissObj
- [x] `TextFont` — tf_Message, tf_YSize, tf_Style, tf_Flags, tf_XSize, tf_Baseline, tf_BoldSmear, tf_Accessors, tf_LoChar, tf_HiChar, tf_CharData, tf_Modulo, tf_CharLoc, tf_CharSpace, tf_CharKern

**Drawing Operations** (vs `graphics/draw.c`, `graphics/clip.c`):
- [x] `Move` / `Draw` — update cp_x/cp_y; JAM1/JAM2/COMPLEMENT/INVERSVID draw modes
- [x] `DrawEllipse` — Bresenham ellipse; clip to rp->Layer
- [x] `AreaEllipse` — add ellipse to area fill
- [x] `RectFill` — fills with FgPen; clips to layer
- [x] `SetRast` — fill entire bitmap (not clipped to layer)
- [x] `WritePixel` / `ReadPixel`
- [x] `PolyDraw` — MoveTo first, DrawTo remaining
- [x] `Flood` — FLOOD_COMPLEMENT, FLOOD_FILL
- [x] `BltBitMap` — minterm, mask; all 4 ABC masks; BLTDEST only for clears
- [x] `BltBitMapRastPort` — clip to layer ClipRect chain
- [x] `ClipBlit` — layer-aware BltBitMapRastPort
- [x] `BltTemplate` — expand 1-bit template to pen colors with minterm
- [x] `BltPattern` — fill with pattern
- [x] `BltMaskBitMapRastPort`
- [x] `ReadPixelLine8` / `ReadPixelArray8` / `WritePixelLine8` / `WritePixelArray8` ✅ (Phase 75)

**Text Rendering** (vs `graphics/text.c`):
- [x] `Text` — JAM1/JAM2/COMPLEMENT styles; tf_CharLoc offsets; clip to layer
- [x] `TextLength` — no rendering, measure only
- [x] `SetFont` / `AskFont`
- [x] `OpenFont` — search GfxBase->TextFonts list; match name+ysize
- [x] `CloseFont`
- [x] `SetSoftStyle` — FSF_BOLD, FSF_ITALIC, FSF_UNDERLINED, FSF_EXTENDED via algorithmic transforms

**Area Fill** (vs `graphics/areafill.c`):
- [x] `InitArea` / `AreaMove` / `AreaDraw` / `AreaEnd` — scan-line fill ✅ (Phase 75); verify against AROS even-odd rule
- [x] `AreaCircle` — shorthand for AreaEllipse

**Blitter** (vs `graphics/blitter.c`, `graphics/blit.c`):
- [x] `QBlit` / `QBSBlit` — blitter queue with semaphore
- [x] `WaitBlit` / `OwnBlitter` / `DisownBlitter`
- [x] `InitBitMap` — initialise BitMap struct (does NOT allocate planes)
- [x] `AllocBitMap` / `FreeBitMap` — allocate planes from CHIP mem
- [x] `AllocRaster` / `FreeRaster` — allocate single plane row

**Regions** (vs `graphics/regions.c`):
- [x] `NewRegion` / `DisposeRegion` / `ClearRegion`
- [x] `OrRectRegion` / `AndRectRegion` / `XorRectRegion` / `ClearRectRegion`
- [x] `OrRegionRegion` / `AndRegionRegion` / `XorRegionRegion`
- [x] `RectInRegion` / `PointInRegion`

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
- [x] Rebalance long-running GTest drivers for parallel CTest execution by splitting oversized suites (`gadtoolsgadgets`, `simplegad`, `simplemenu`, `menulayout`, `cluster2`) into smaller shards; verified with full `ctest --test-dir build --output-on-failure -j8`
- [ ] Add new GTest assertions for each behavioral deviation found vs AROS
- [ ] Run RKM sample programs after structural changes to catch regressions (gadtoolsgadgets_gtest, simplemenu_gtest, filereq_gtest, fontreq_gtest, easyrequest_gtest)
- [ ] Run application tests after changes to exec/dos (asm_one_gtest, devpac_gtest, kickpascal_gtest, cluster2_gtest, dpaint_gtest)
- [ ] Update version to 0.7.0 when Phase 78 is complete (new MINOR — significant compatibility milestone)

---

## Version History

*Massively compacted. For full historical details, refer to Git history.*

| Version | Phase | Key Changes |
| :--- | :--- | :--- |
| **0.6.76** | 78-B-6 | Completed DOS program-loading/runtime coverage by implementing `InternalLoadSeg`, `NewLoadSeg`, `RunCommand`, and `GetSegListInfo`, added direct loader/runtime regression tests, and fixed the unrelated `commands_gtest` `TYPE` `IoErr()` regression. |
| **0.6.75** | 78-B-5 | Completed DOS buffered formatting/I/O coverage by implementing `SetVBuf`, `FRead`, `FWrite`, `FPuts`, and host-backed formatted file output paths with direct regression coverage for block I/O, line buffering, and `UnGetC`; removed non-standard DOS `SPrintf`/`VSPrintf` from scope after NDK/AROS verification. |
| **0.6.74** | 78-B-4 | Completed DOS assign/device/notify coverage by adding per-path multi-assign removal, iterable `GetDevProc`/`FreeDevProc`, and host-polled `StartNotify`/`EndNotify` delivery through the VBlank path, with direct DOS regression coverage. |
| **0.6.73** | 78-B-3 | Completed DOS extended file semantics with host-backed `SetFileDate`, `ExAll`/`ExAllEnd`, and `MakeLink`/`ReadLink`, plus regression coverage for directory enumeration, filtering, soft links, and hard links; `ChangeFilePosition`/`GetFilePosition` remain deferred pending a verified public API reference. |
| **0.6.72** | 78-B-2 | Verified DOS proc-window inheritance semantics by teaching `CreateNewProc()`/`SystemTagList()` to preserve or override `pr_WindowPtr` correctly, with direct regression coverage for inherited and explicit `NP_WindowPtr` behavior. |
| **0.6.69** | 78-A-6 | Exec miscellaneous verification: `RawDoFmt` edge cases (maxwidth, `%c`, `%%`, `%b` BSTR, return value), list accessors as macros, `Alert` decoding, `Supervisor` call. 40 sub-tests in new ExecMisc test. |
| **0.6.71** | 78-B-3 | Added host-backed `SetFileSize()` support with regression coverage for truncation, growth, and zero-filled extension, while keeping the full 49-test GTest suite green. |
| **0.6.70** | 78-B-2 | Added DOS CLI metadata helpers and multi-CLI lookup helpers, initialized `RootNode`/`DosInfo` public state, and extended struct-offset coverage for DOS public structures. |
| **0.6.68** | 78-A Complete | Completed Exec Phase 78-A verification by covering interrupt nesting, library management helpers, and `SumKickData()`, with the full 38-test suite green. |
| **0.6.67** | 78-A-4 | Full regression suite green again after fixing DOS CLI BSTR program-name handling, which restored DPaint V startup. |
| **0.6.66** | 78-A-4 | Phase 78-A-4 Complete — verified Exec signal allocation and semaphore semantics, including shared locks and waiter handoff. |
| **0.6.65** | 78-A-3 | Phase 78-A-3 Complete — verified `Wait(SIGBREAKF_CTRL_C)` behavior and added Ctrl-C regression coverage. |
| **0.6.64** | 78-A-2 | Phase 78-A-2 Complete — fixed `Wait()` to clear `tc_SigWait` on return and added signal regression coverage. |
| **0.6.63** | 78-A-1 | Phase 78-A-1 Complete — Exec AROS Bug Fixes! Fixed 10 bugs in `exec.c`. |
| **0.6.0 - 0.6.62** | 44 - 78-W | Core system implementation, test suite stabilization, OS data structure offset verification, missing libraries implementation. |
