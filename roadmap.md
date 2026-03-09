# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project! Keep the roadmap structured, compact and focused on future phases and milestones. Compact/summarize past phases and milestones whenever possible.

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

**Review**:
- [x] Implement missing functions and stubs as far as possible — `InitCode()`, `SetIntVector()`, `CachePreDMA()`, `CachePostDMA()`, and hosted `ColdReboot()` semantics now match the current lxa/AROS-compatible surface closely enough for direct regression coverage
- [x] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [x] Performance review: identify performance improvement opportunities, add them to phase 79

---

#### 78-B: DOS Library (`src/rom/lxa_dos.c` vs `others/AROS-20231016-source/rom/dos/`)

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

##### 78-B-8: Review

- [x] Implement missing functions and stubs as far as possible — `DeviceProc()`, DOS packet compatibility helpers (`DoPkt`/`SendPkt`/`WaitPkt`/`ReplyPkt`/`AbortPkt`), and `SetProgramDir()` now have hosted implementations with direct regression coverage
- [x] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [x] Performance review: identify performance improvement opportunities, add them to phase 79

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
- [x] `InitView` / `InitVPort`
- [x] `LoadView` — switch to new View (Intuition-level only in lxa)
- [x] `MakeVPort` / `MrgCop` / `FreeVPortCopLists` / `FreeCopList`
- [x] `VideoControl` — VTAG_* tags; verify tag list handling
- [x] `GetDisplayInfoData` — DisplayInfo, DimensionInfo, MonitorInfo, NameInfo
- [x] `FindDisplayInfo` / `NextDisplayInfo`

**Sprites & GELs** (vs `graphics/sprite.c`, `graphics/gel.c`):
- [x] `GetSprite` / `FreeSprite` / `ChangeSprite` / `MoveSprite`
- [x] `InitGels` / `AddBob` / `AddVSprite` / `RemBob` / `RemVSprite` / `DrawGList` / `SortGList`

**Pens & Colors** (vs `graphics/color.c`):
- [x] `SetRGB4` / `SetRGB32` / `SetRGB4CM` / `SetRGB32CM` / `GetRGB32`
- [x] `LoadRGB4` / `LoadRGB32`
- [x] `ObtainPen` / `ReleasePen` / `FindColor`

**BitMap Utilities**:
- [x] `ScalerDiv` — fast integer scale helper
- [x] `BitMapScale` — hardware-accelerated bitmap scale via blitter
- [x] `ExtendFont` / `StripFont` — tf_Extension for proportional/kerning
- [x] `AddFont` / `RemFont`
- [x] `GfxFree` / `GfxNew` / `GfxAssociate` / `GfxLookUp`

##### 78-C-2: Review

- [x] Implement missing functions and stubs as far as possible — `WaitBOVP()`, `ScrollVPort()`, `UCopperListInit()`, `ObtainBestPenA()`, `CoerceMode()`, `ChangeVPBitMap()`, `AllocDBufInfo()`, `FreeDBufInfo()`, and `SetMaxPen()` now have hosted compatibility implementations with direct regression coverage
- [x] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [x] Performance review: identify performance improvement opportunities, add them to phase 79


---

#### 78-D: Layers Library (`src/rom/lxa_layers.c` vs `others/AROS-20231016-source/rom/hyperlayers/`)

- [x] `InitLayers` — initialise LayerInfo struct, including default bounds and compatibility flags (v0.6.94)
- [x] `NewLayerInfo` / `DisposeLayerInfo` — allocation, initialization, and pooled `ClipRect` cleanup verified (v0.6.94)
- [x] `CreateUpfrontLayer` / `CreateBehindLayer` / `CreateUpfrontHookLayer` / `CreateBehindHookLayer` — with and without SuperBitMap (v0.6.94)
- [x] `DeleteLayer` — releases ClipRects, rebuilds exposure state, and preserves damage tracking for revealed layers (v0.6.94)
- [x] `MoveLayer` — move by delta; update ClipRect chain (v0.6.94)
- [x] `SizeLayer` — resize layer; rebuild ClipRects (v0.6.94)
- [x] `BehindLayer` / `UpfrontLayer` — z-order without moving (v0.6.94)
- [x] `MoveLayerInFrontOf` ✅ (Phase 75)
- [x] `ScrollLayer` ✅ (Phase 75)
- [x] `BeginUpdate` / `EndUpdate` ✅ (Phase 75)
- [x] `SwapBitsRastPortClipRect` ✅ (Phase 75)
- [x] `InstallClipRegion` ✅ (Phase 75)
- [x] `SortLayerCR` ✅ (Phase 75)
- [x] `DoHookClipRects` ✅ (Phase 75)
- [x] `ShowLayer` / `HideLayer` / `LayerOccluded` / `SetLayerInfoBounds` — visibility, z-order restore, and `Layer_Info` clipping bounds verified (v0.6.92)
- [x] `LockLayer` / `UnlockLayer` / `LockLayers` / `UnlockLayers` / `LockLayerRom` / `UnlockLayerRom` — current lxa compatibility semantics verified for layer and graphics locking entry points (v0.6.94)
- [x] `WhichLayer` — hit-test point to top-most visible layer/ClipRect, including hidden-layer and bounds-clipped coverage (v0.6.92)
- [x] `FattenLayerInfo` / `ThinLayerInfo` — `NEWLAYERINFO_CALLED` compatibility semantics verified (v0.6.92)
- [x] `InstallLayerHook` / `InstallLayerInfoHook` — previous-hook return and assignment semantics verified (v0.6.92)
- [x] `AddLayerInfoTag` / `CreateLayerTagList` — current hook-layer tag semantics verified for `LA_BackfillHook`, `LA_SuperBitMap`, `LA_WindowPtr`, `LA_Hidden`, `LA_InFrontOf`, and `LA_Behind` (v0.6.93)

##### 78-D-2: Review

- [x] Implement missing functions and stubs as far as possible — review locked down backdrop-layer ordering and immobility semantics (`CreateBehindLayer`, `CreateUpfrontLayer`, `UpfrontLayer`, `BehindLayer`, `MoveLayerInFrontOf`, `MoveLayer`, `SizeLayer`, `MoveSizeLayer`) with direct regression coverage in `Tests/Layers/CoreOps` (v0.6.111)
- [x] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [x] Performance review: identify performance improvement opportunities, add them to phase 79

---

#### 78-E: Intuition Library (`src/rom/lxa_intuition.c` vs `others/AROS-20231016-source/rom/intuition/`)

**Data Structures** (vs `intuition/iobsolete.h`, NDK `intuition/intuition.h`):
- [x] `IntuitionBase` — FirstScreen, ActiveScreen, ActiveWindow, MouseX/Y, Seconds/Micros verified against the current public NDK layout; `ActiveGadget`/`MenuVerifyTimeout` do not appear in the published AmigaOS 3.2 public structure and remain out of scope pending a verified public reference
- [x] `Screen` — NextScreen, FirstWindow, LeftEdge/TopEdge/Width/Height, MouseY/X, Flags, Title, DefaultTitle, BarHeight, BarVBorder, BarHBorder, MenuVBorder, MenuHBorder, WBorTop/Left/Right/Bottom, Font, ViewPort, RastPort, BitMap, LayerInfo, FirstGadget, DetailPen, BlockPen, SaveColor0, BarLayer, ExtData, UserData
- [x] `Window` — NextWindow, LeftEdge/TopEdge/Width/Height, MouseX/Y, MinWidth/MinHeight/MaxWidth/MaxHeight, Flags, Title, Screen, RPort, BorderLeft/Top/Right/Bottom, FirstGadget, Parent/Descendant, Pointer/PtrHeight/PtrWidth/XOffset/YOffset, IDCMPFlags, UserPort, WindowPort, MessageKey, DetailPen, BlockPen, CheckMark, ScreenTitle, GZZWidth, GZZHeight, ExtData, UserData, WLayer, IFont, MoreFlags
- [x] `IntuiMessage` — ExecMessage, Class, Code, Qualifier, IAddress, MouseX/Y, Seconds/Micros, IDCMPWindow, SpecialLink
- [x] `Gadget` — NextGadget, LeftEdge/TopEdge/Width/Height, Flags, Activation, GadgetType, GadgetRender/SelectRender, GadgetText, MutualExclude, SpecialInfo, GadgetID, UserData
- [x] `StringInfo` — Buffer, UndoBuffer, BufferPos, MaxChars, DispPos, UndoPos, NumChars, DispCount, CLeft, CTop, LayerPtr, LongInt, AltKeyMap
- [x] `PropInfo` — Flags, HorizPot/VertPot, HorizBody/VertBody, CWidth/CHeight, HPotRes/VPotRes, LeftBorder/TopBorder
- [x] `Image` — LeftEdge, TopEdge, Width, Height, Depth, ImageData, PlanePick, PlaneOnOff, NextImage
- [x] `Border` — LeftEdge, TopEdge, FrontPen, BackPen, DrawMode, Count, XY, NextBorder
- [x] `IntuiText` — FrontPen, BackPen, DrawMode, LeftEdge, TopEdge, ITextFont, IText, NextText
- [x] `MenuItem` — NextItem, LeftEdge/TopEdge/Width/Height, Flags, MutualExclude, ItemFill, SelectFill, Command, SubItem, NextSelect
- [x] `Menu` — NextMenu, LeftEdge/TopEdge/Width/Height, Flags, MenuName, FirstItem
- [x] `Requester` — OlderRequest, LeftEdge/TopEdge/Width/Height, RelLeft/RelTop, ReqGadget, ReqBorder, ReqText, Flags, BackFill, ReqLayer, ReqPad1, ImageBMap, RWindow, ReqImage, ReqPad2

**Screens** (vs `intuition/screens.c`):
- [x] `OpenScreen` / `CloseScreen` / `OpenScreenTagList` — direct regression coverage now locks default opens, tag overrides/clears, `OpenScreenTagList(NULL, tags)`, ViewModes-based small-height expansion, and V36-style `CloseScreen()` refusal while windows remain open
- [x] `LockPubScreen` / `UnlockPubScreen` / `LockPubScreenList` / `UnlockPubScreenList`
- [x] `GetScreenData` — fill ScreenBuffer from screen
- [x] `MoveScreen` — panning
- [x] `ScreenToFront` / `ScreenToBack`
- [x] `ShowTitle` — SA_ShowTitle toggle
- [x] `PubScreenStatus`

**Windows** (vs `intuition/windows.c`):
- [x] `OpenWindow` / `CloseWindow` / `OpenWindowTagList`
- [x] `MoveWindow` / `SizeWindow` / `ChangeWindowBox`
- [x] `WindowToFront` / `WindowToBack`
- [x] `ActivateWindow` ✅ (Phase 76)
- [x] `ZipWindow` ✅ (Phase 76)
- [x] `SetWindowTitles` — current lxa semantics locked for update, no-change (`(UBYTE *)-1`), and blank-title cases
- [x] `RefreshWindowFrame`
- [x] `WindowLimits` — min/max width/height
- [x] `BeginRefresh` / `EndRefresh` — WFlg damage tracking
- [x] `ModifyIDCMP`
- [x] `GetDefaultPubScreen`

**Gadgets** (vs `intuition/gadgets.c`, `intuition/boopsi.c`):
- [x] `AddGadget` / `RemoveGadget` / `AddGList` / `RemoveGList`
- [x] `RefreshGadgets` / `RefreshGList`
- [x] `ActivateGadget`
- [x] `SetGadgetAttrsA` ✅ (Phase 78-E menu/BOOPSI expansion, v0.6.103)
- [x] `DoGadgetMethodA` ✅ (Phase 78-E menu/BOOPSI expansion, v0.6.103)
- [x] `OnGadget` / `OffGadget` — GADGDISABLED flag; re-render
- [x] `SizeWindow` interaction with GFLG_RELRIGHT/RELBOTTOM relative gadgets — shared geometry handling now covers hit-testing/rendering/highlight paths, and resize redraws clear stale pixels before relative gadgets are repainted (v0.6.104)

**BOOPSI / OOP** (vs `intuition/boopsi.c`, `intuition/classes.c`):
- [x] `NewObjectA` / `DisposeObject` — class dispatch, tag list (v0.6.105)
- [x] `GetAttr` / `SetAttrsA` / `CoerceMethodA` (v0.6.105)
- [x] `DoMethodA` — OM_NEW, OM_SET, OM_GET, OM_DISPOSE, OM_ADDMEMBER, OM_REMMEMBER, OM_NOTIFY, OM_UPDATE (v0.6.105)
- [x] `MakeClass` / `FreeClass` / `AddClass` / `RemoveClass` (v0.6.105)
- [x] `ICA_TARGET / ICA_MAP` notification pipeline ✅ (Phase 76)
- [x] Standard classes: rootclass, imageclass, frameiclass, sysiclass, fillrectclass, gadgetclass, propgclass, strgclass, buttongclass, frbuttonclass, groupgclass, icclass, modelclass ✅ (Phase 76)
- [x] `GetScreenDepth` / `GetScreenRect` — verified out of scope after checking bundled NDK 3.2 and AROS public Intuition surfaces; no exported API with these names is present, so `ScreenDepth`/`GetScreenData` remain the public coverage points for the current screen helpers (v0.6.109)

**Menus** (vs `intuition/menus.c`):
- [x] `SetMenuStrip` / `ClearMenuStrip` / `ResetMenuStrip` (v0.6.105)
- [x] `OnMenu` / `OffMenu` — packed menu/item/sub-item enable-state semantics verified (v0.6.103)
- [x] `ItemAddress` — menu number decode (MENUNUM/ITEMNUM/SUBNUM macros) (v0.6.105)

**Requesters** (vs `intuition/requesters.c`):
- [x] `Request` / `EndRequest` (v0.6.106)
- [x] `BuildSysRequest` / `FreeSysRequest` / `SysReqHandler` (v0.6.106)
- [x] `AutoRequest` ✅ (Phase 76)
- [x] `EasyRequestArgs` / `BuildEasyRequestArgs` ✅ (Phase 70a)

**Pointers & Input** (vs `intuition/pointer.c`):
- [x] `SetPointer` / `ClearPointer` — sprite data, xoffset/yoffset (v0.6.107)
- [x] `SetMouseQueue` — limit pending MOUSEMOVE messages (v0.6.107)
- [x] `CurrentTime` — fill seconds/micros from timer (v0.6.107)
- [x] `DisplayAlert` / `DisplayBeep` (v0.6.107)

**Drawing in Intuition**:
- [x] `DrawBorder` — linked Border chain rendering (v0.6.108)
- [x] `DrawImage` / `DrawImageState` — `DrawImage()` now follows the public `DrawImageState(IDS_NORMAL)` path, direct regressions cover linked classic image chains plus `PlanePick==0` fill behavior, and `DrawImageState()` is locked against AROS-style `IDS_SELECTED` semantics for standard images; no bundled public NDK/AROS `IMGS_*` Intuition image states were found beyond the `IDS_*` surface (v0.6.109)
- [x] `PrintIText` / `IntuiTextLength` (v0.6.108)
- [x] `EraseImage` (v0.6.108)
- [x] `DrawBoopsiObject` — verified out of scope after checking bundled NDK 3.2 and AROS public Intuition surfaces; no exported API with this name is present, and BOOPSI image drawing remains covered through `DrawImageState()`/`IM_DRAW` on image objects (v0.6.109)

##### 78-E-2: Review

- [x] Implement missing functions and stubs as far as possible — `NextObject()`, `WBenchToFront()`, and `WBenchToBack()` now follow the public NDK/AROS semantics with direct BOOPSI/screen-order regression coverage (v0.6.113)
- [x] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [x] Performance review: identify performance improvement opportunities, add them to phase 79

---

#### 78-F: GadTools Library (`src/rom/lxa_gadtools.c` vs `others/AROS-20231016-source/rom/compiler/alib/`)

- [x] `CreateContext` / context-list teardown — public NDK surface verified as `CreateContext()` + `FreeGadgets()` (no public `FreeContext()` entry point); direct regression coverage now locks the invisible sentinel gadget semantics, head/tail linkage, and safe teardown across all currently supported GadTools kinds (v0.6.114)
- [x] `CreateGadgetA` — all current lxa-supported kinds (`BUTTON_KIND`, `CHECKBOX_KIND`, `CYCLE_KIND`, `INTEGER_KIND`, `LISTVIEW_KIND`, `MX_KIND`, `NUMBER_KIND`, `PALETTE_KIND`, `SCROLLER_KIND`, `SLIDER_KIND`, `STRING_KIND`, `TEXT_KIND`) now have direct regression coverage for creation/linkage and kind-specific gadget-class assignment (v0.6.114)
- [x] `FreeGadgets` — teardown coverage now walks full mixed-kind gadget lists; no public NDK `FreeGadget()` symbol was found in the bundled AmigaOS 3.2 surface, so the roadmap tracks the exported `FreeGadgets()` API instead (v0.6.114)
- [x] `GT_SetGadgetAttrsA` / `GT_GetGadgetAttrs` — checkbox/cycle/string/integer/slider state round-trips and updates verified with direct regression coverage (v0.6.110)
- [ ] `GT_ReplyIMsg` / `GT_GetIMsg` — thin wrappers around Intuition IDCMP
- [ ] `GT_RefreshWindow` — paint gadget list
- [ ] `GT_BeginRefresh` / `GT_EndRefresh`
- [ ] `GT_PostFilterIMsg` / `GT_FilterIMsg`
- [ ] `CreateMenusA` / `FreeMenus` / `LayoutMenuItemsA` / `LayoutMenusA`
- [x] `DrawBevelBoxA` — raised/recessed/ridge bevel rendering now runs in the automated GadTools regression driver via `Tests/GadTools/DrawBevelBox` (v0.6.114)

##### 78-F-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



---

#### 78-G: ASL Library (`src/rom/lxa_asl.c` vs `others/AROS-20231016-source/rom/compiler/alib/`)

- [ ] `AllocAslRequest` / `FreeAslRequest` — ASL_FileRequest, ASL_FontRequest, ASL_ScreenModeRequest
- [ ] `AslRequestTags` / `AslRequest` — run modal requester, return TRUE/FALSE
- [ ] FileRequester tags: ASLFR_Window, ASLFR_TitleText, ASLFR_InitialFile, ASLFR_InitialDrawer, ASLFR_InitialPattern, ASLFR_Flags1 (FRF_DOSAVEMODE, FRF_DOMULTISELECT, FRF_DOPATTERNS)
- [ ] FontRequester tags: ASLFO_Window, ASLFO_TitleText, ASLFO_InitialName, ASLFO_InitialSize, ASLFO_InitialStyle, ASLFO_Flags (FOF_DOFIXEDWIDTHONLY, FOF_DOSTYLEBUTTONS)
- [ ] ScreenMode requester tags
- [ ] `fr_File` / `fr_Drawer` / `fr_NumArgs` / `fr_ArgList` output fields
- [ ] `fo_Attr` / `fo_TAttr` output fields ✅ (Phase 70a)

##### 78-G-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79

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

##### 78-H-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79

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

##### 78-I-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



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

##### 78-J-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



---

#### 78-K: Keymap Library (`src/rom/lxa_keymap.c` vs `others/AROS-20231016-source/rom/keymap/`)

- [ ] `SetKeyMap` / `AskKeyMapDefault`
- [ ] `MapANSI` — convert ANSI string to rawkey+qualifier sequence
- [ ] `MapRawKey` — rawkey+qualifier+keymap → character(s); qualifier processing (shift, alt, ctrl)
- [ ] `KeyMap` structure — km_LoKeyMap/km_LoKeyMapTypes/km_HiKeyMap/km_HiKeyMapTypes; KCF_* type flags
- [ ] Dead-key sequences (diaeresis, accent, tilde)

##### 78-K-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



---

#### 78-L: Timer Device (`src/rom/lxa_dev_timer.c` vs `others/AROS-20231016-source/rom/timer/`)

- [ ] `TR_ADDREQUEST` — UNIT_MICROHZ (≥50 ms), UNIT_VBLANK (VBL aligned), UNIT_ECLOCK, UNIT_WAITUNTIL
- [ ] `TR_GETSYSTIME` / `TR_SETSYSTIME`
- [ ] `AbortIO` for timer requests
- [ ] `GetSysTime` / `AddTime` / `SubTime` / `CmpTime` — `struct timeval`
- [ ] EClockVal reading

##### 78-L-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



---

#### 78-M: Console Device (`src/rom/lxa_dev_console.c` vs `others/AROS-20231016-source/rom/devs/console/`)

- [ ] `CMD_WRITE` — ANSI escape sequences: cursor movement, SGR attributes (bold/reverse/underline), erase, color (foreground/background 30-37/40-47)
- [ ] `CMD_READ` — read character/line; CRAF_* flags
- [ ] `CD_ASKKEYMAP` / `CD_SETKEYMAP`
- [ ] `CONU_LIBRARY` unit (-1) handling ✅ (Phase 72)
- [ ] Window resize events (IDCMP_NEWSIZE) → console resize
- [ ] Scroll-back buffer management
- [ ] Verify ANSI CSI sequences: `\e[H` (home), `\e[J` (erase display), `\e[K` (erase line), `\e[?25l/h` (cursor hide/show)

##### 78-M-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



---

#### 78-N: Input Device (`src/rom/lxa_dev_input.c` vs `others/AROS-20231016-source/rom/devs/input/`)

- [ ] `IND_ADDHANDLER` / `IND_REMHANDLER` — input handler chain; priority ordering
- [ ] `IND_SETTHRESH` / `IND_SETPERIOD` — key repeat
- [ ] `IND_SETMPORT` / `IND_SETMTYPE`
- [ ] `InputEvent` chain processing; IE_RAWKEY, IE_BUTTON, IE_POINTER, IE_TIMER, IE_NEWACTIVE, IE_NEWSIZE, IE_REFRESH
- [ ] Qualifier accumulation: IEQUALIFIER_LSHIFT, RSHIFT, CAPSLOCK, CONTROL, LALT, RALT, LCOMMAND, RCOMMAND, NUMERICPAD, REPEAT, INTERRUPT, MULTIBROADCAST, MIDBUTTON, RBUTTON, LEFTBUTTON
- [ ] `FreeInputHandlerList`

##### 78-N-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



---

#### 78-O: Audio Device (`src/rom/lxa_dev_audio.c`)

- [ ] `ADCMD_ALLOCATE` / `ADCMD_FREE` — channel allocation bitmask (channels 0-3)
- [ ] `ADCMD_SETPREC` — allocation precedence
- [ ] `CMD_WRITE` — DMA audio playback: period/volume/data/length
- [ ] `ADCMD_PERVOL` — change period and volume of running sound
- [ ] `ADCMD_FINISH` / `ADCMD_WAITCYCLE`
- [ ] Audio interrupt (end-of-sample notification)
- [ ] SDL2 audio stream mapping to Amiga channel model

##### 78-O-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



---

#### 78-P: Trackdisk Device (`src/rom/lxa_dev_trackdisk.c` vs `others/AROS-20231016-source/rom/devs/trackdisk/`)

- [ ] `TD_READ` / `TD_WRITE` / `TD_FORMAT` / `TD_SEEK` — map to host file I/O on `.adf` image
- [ ] `TD_MOTOR` ✅ (Phase 77)
- [ ] `TD_CHANGENUM` / `TD_CHANGESTATE` / `TD_PROTSTATUS` / `TD_GETGEOMETRY` ✅ (Phase 77)
- [ ] `TD_ADDCHANGEINT` / `TD_REMCHANGEINT` — disk change notification via interrupt
- [ ] `ETD_READ` / `ETD_WRITE` — extended commands with `TDU_PUBFLAGS`
- [ ] Error codes: TDERR_NotSpecified, TDERR_NoSecHdr, TDERR_BadSecPreamble, TDERR_TooFewSecs, TDERR_NoDisk

##### 78-P-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



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

##### 78-Q-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



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

##### 78-R-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



---

#### 78-S: Diskfont Library (`src/rom/lxa_diskfont.c`)

- [ ] `OpenDiskFont` ✅ (Phase 77) — verify against AROS disk font format: contents file, `.font` binary format (header, chardata, charloc, charspace, charkern)
- [ ] `AvailFonts` ✅ (Phase 77) — AFF_MEMORY, AFF_DISK; verify AvailFontsHeader + AvailFonts array packing
- [ ] `DisposeFontContents` — free AvailFonts result
- [ ] `NewFontContents` — build AF list for single drawer
- [ ] Proportional font support (TF_PROPORTIONAL, tf_CharSpace, tf_CharKern arrays)
- [ ] Multi-plane fonts (depth > 1)

##### 78-S-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



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

##### 78-T-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



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

##### 78-V-2: Review

- [ ] Implement missing functions and stubs as far as possible
- [ ] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [ ] Performance review: identify performance improvement opportunities, add them to phase 79



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

- [ ] After each 78-A through 78-W subsystem fix: run full `ctest --test-dir build -j16` and confirm all existing tests still pass
- [x] Regression maintenance: restore `exec_gtest`, `shell_gtest`, and `rgbboxes_gtest` to green after timer/SystemTagList/test-harness regressions; verified with full `ctest --test-dir build --output-on-failure -j16`
- [x] Rebalance long-running GTest drivers for parallel CTest execution by splitting oversized suites (`gadtoolsgadgets`, `simplegad`, `simplemenu`, `menulayout`, `cluster2`) into smaller shards; verified with full `ctest --test-dir build --output-on-failure -j16`
- [x] Regression maintenance: restore `graphics_gtest` (`ColorsPens`) and `dos_gtest` (`AssignNotify`) after palette-pen matching and assign-backed `DeviceProc()` regressions; verified with focused reruns and full `ctest --test-dir build --output-on-failure -j16`
- [ ] Add new GTest assertions for each behavioral deviation found vs AROS
- [ ] Run RKM sample programs after structural changes to catch regressions (gadtoolsgadgets_gtest, simplemenu_gtest, filereq_gtest, fontreq_gtest, easyrequest_gtest)
- [ ] Run application tests after changes to exec/dos (asm_one_gtest, devpac_gtest, kickpascal_gtest, cluster2_gtest, dpaint_gtest)
- [ ] Update version to 0.7.0 when Phase 78 is complete (new MINOR — significant compatibility milestone)

### Phase 79: Architecture and performance improvements

- [ ] Exec architecture: replace the manual built-in resident table population in `coldstart()` with a single shared registration source so `ResModules`, built-in library/device registration, and resident verification stay in sync
- [ ] Exec architecture: factor interrupt vector state management (`SetIntVector`, server chains, sentinel defaults) behind shared helpers so direct vectors and queued interrupt servers cannot drift semantically
- [ ] Exec performance: pre-bucket residents by startup class/version during coldstart so `InitCode()` does not need to linearly rescan every resident on each replay
- [ ] Exec performance: cache built-in resident/library name lookups used during init and replay to avoid repeated list walks and string compares during startup-sensitive paths
- [ ] DOS architecture: consolidate packet helper behavior (`DoPkt`, `SendPkt`, `WaitPkt`, `ReplyPkt`) behind one internal packet transport path so synchronous and asynchronous DOS messaging cannot diverge
- [ ] DOS architecture: make process-only DOS helpers (`SetProgramDir`, packet waits, `DeviceProc`) share one validated current-process accessor to centralize task-vs-process edge handling
- [ ] DOS performance: reduce repeated `FindTask(NULL)`/port lookups in packet-heavy paths by caching current process context for a single DOS call chain
- [ ] DOS performance: reuse lightweight DOS packet/standard-packet allocations for synchronous `DoPkt()` traffic to avoid per-call alloc/free churn on handler-heavy workloads
- [ ] Graphics architecture: consolidate hosted viewport state (`ColorMap`, `ViewPortExtra`, `DBufInfo`, scroll origin, display handle) behind one internal viewport companion structure instead of spreading compatibility state across public structs
- [ ] Graphics architecture: route copper-list and double-buffer helpers through shared placeholder alloc/free helpers so `MakeVPort`, `UCopperListInit`, and `ChangeVPBitMap` cannot drift in ownership semantics
- [ ] Layers architecture: centralize layer-list insertion/removal and backdrop-order invariants behind one internal z-order helper path so create/reorder/show operations cannot drift semantically
- [ ] Layers architecture: separate future private layers state (shape hooks, nested-family bookkeeping, visibility-only metadata) from public `struct Layer` fields so unsupported V50+ semantics can grow without coupling to public layout hacks
- [ ] Intuition architecture: centralize screen-type classification and Workbench-screen lookup behind shared helpers so Workbench/public-screen paths cannot drift on mixed screen lists
- [ ] Intuition architecture: separate private screen-order bookkeeping from public `struct Screen` list links so future Workbench/public-screen behavior can grow without overloading public flags semantics
- [ ] Graphics performance: avoid repeated palette list walks in `ObtainBestPenA()`/`FindColor()` by caching exact-match or nearest-pen hints inside palette-extra state
- [ ] Graphics performance: skip redundant viewport/display updates when `ScrollVPort()` or `ChangeVPBitMap()` are called with unchanged state
- [ ] Intuition performance: cache the current Workbench screen pointer or tail/front bookkeeping so `OpenWorkBench()`, `WBenchToFront()`, and `WBenchToBack()` avoid repeated full screen-list scans
- [ ] Layers performance: avoid full `RebuildAllClipRects()` passes for simple z-order/visibility changes by recomputing only the affected layer span
- [ ] Layers performance: replace coarse `DamageExposedAreas()` whole-intersection refreshes with exact exposed-rectangle splitting to reduce redundant refresh damage on move/size/delete paths


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
- **Phase 78-A Review**: Exec review pass completed; remaining stubbed `InitCode`, `SetIntVector`, `CachePreDMA`, `CachePostDMA`, and hosted `ColdReboot` behavior now have implementation coverage, with direct exec regressions and full-suite validation.
- **Phase 78-B**: DOS library verification completed, including loader/runtime coverage, assign/notify support, buffered I/O, and the final pattern/regression sweep.
- **Phase 78-B Review**: DOS review pass completed; `DeviceProc`, DOS packet compatibility helpers, and `SetProgramDir` now have hosted implementations with direct DOS regression coverage and full-suite validation.
- **Phase 78-C Review**: Graphics review pass completed; viewport/copper/palette/double-buffer compatibility helpers now have hosted implementations with direct graphics regression coverage and full-suite validation.
- **Phase 78-D Review**: Layers review pass completed; backdrop-layer z-order/immutability semantics now match the documented RKRM surface with direct layers regression coverage, and remaining architecture/performance follow-ups are tracked in Phase 79.
- **Phase 78-E Review**: Intuition review pass advanced; `NextObject()`, `WBenchToFront()`, and `WBenchToBack()` now match the documented public behavior with BOOPSI/screen-order regression coverage, and the remaining intuition architecture/performance follow-ups are tracked in Phase 79.
- **Phase 78-W**: Structural Verification — OS Data Structure Offsets — 460 assertions passing.
