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

- [x] Implement missing functions and stubs as far as possible — `NextObject()`, `WBenchToFront()`, and `WBenchToBack()` now follow the public NDK/AROS semantics with direct BOOPSI/screen-order regression coverage (v0.6.113); follow-up Intuition compatibility work now covers `ClearDMRequest()`, `SetDMRequest()`, `CloseWorkBench()`, `GetDefPrefs()`, `SetPrefs()`, `NewModifyProp()`, `MoveWindowInFrontOf()`, `SetEditHook()`, `LendMenus()`, `SetWindowPointerA()`, `TimedDisplayAlert()`, and `HelpControl()` with direct regression coverage in the requester, pointer, screen, and window test programs (v0.6.118)
- [x] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [x] Performance review: identify performance improvement opportunities, add them to phase 79

---

#### 78-F: GadTools Library (`src/rom/lxa_gadtools.c` vs `others/AROS-20231016-source/rom/compiler/alib/`)

- [x] `CreateContext` / context-list teardown — public NDK surface verified as `CreateContext()` + `FreeGadgets()` (no public `FreeContext()` entry point); direct regression coverage now locks the invisible sentinel gadget semantics, head/tail linkage, and safe teardown across all currently supported GadTools kinds (v0.6.114)
- [x] `CreateGadgetA` — all current lxa-supported kinds (`BUTTON_KIND`, `CHECKBOX_KIND`, `CYCLE_KIND`, `INTEGER_KIND`, `LISTVIEW_KIND`, `MX_KIND`, `NUMBER_KIND`, `PALETTE_KIND`, `SCROLLER_KIND`, `SLIDER_KIND`, `STRING_KIND`, `TEXT_KIND`) now have direct regression coverage for creation/linkage and kind-specific gadget-class assignment (v0.6.114)
- [x] `FreeGadgets` — teardown coverage now walks full mixed-kind gadget lists; no public NDK `FreeGadget()` symbol was found in the bundled AmigaOS 3.2 surface, so the roadmap tracks the exported `FreeGadgets()` API instead (v0.6.114)
- [x] `GT_SetGadgetAttrsA` / `GT_GetGadgetAttrs` — checkbox/cycle/string/integer/slider state round-trips and updates verified with direct regression coverage (v0.6.110)
- [x] `GT_ReplyIMsg` / `GT_GetIMsg` — thin wrappers around Intuition IDCMP now have direct regression coverage via `Tests/GadTools/MessageFilters` and interactive event-loop coverage in the GadTools samples (v0.6.115)
- [x] `GT_RefreshWindow` — window gadget-list refresh path is locked down by `Tests/GadTools/ContextRefresh` and exercised by the RKM-style GadTools samples (v0.6.115)
- [x] `GT_BeginRefresh` / `GT_EndRefresh` — refresh wrapper semantics now have direct regression coverage via `Tests/GadTools/ContextRefresh` (v0.6.115)
- [x] `GT_PostFilterIMsg` / `GT_FilterIMsg` — current compatibility passthrough semantics are verified with direct message-wrapper regression coverage (v0.6.115)
- [x] `CreateMenusA` / `FreeMenus` / `LayoutMenuItemsA` / `LayoutMenusA` — NewMenu validation, user-data storage, menu/item flag propagation, and menu-strip/item layout now have direct regression coverage via `Tests/GadTools/MenuAPI` plus interactive sample coverage in `gadtoolsmenu_gtest` (v0.6.115)
- [x] `DrawBevelBoxA` — raised/recessed/ridge bevel rendering now runs in the automated GadTools regression driver via `Tests/GadTools/DrawBevelBox` (v0.6.114)

##### 78-F-2: Review

- [x] Implement missing functions and stubs as far as possible — message wrappers, refresh wrappers, and menu creation/layout helpers now have hosted implementations with direct regression coverage (v0.6.115)
- [x] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [x] Performance review: identify performance improvement opportunities, add them to phase 79



---

#### 78-G: ASL Library (`src/rom/lxa_asl.c` vs `others/AROS-20231016-source/rom/compiler/alib/`)

Status: core requester allocation and modal execution now cover file, font, and screen mode requesters in 0.6.119, including direct GTest/sample regression coverage for `fr_ArgList`, `fo_TAttr`, basic screen-mode selection, and the public `AllocAslRequestTags()` / `AslRequestTags()` varargs helpers.

- [x] `AllocAslRequest` / `FreeAslRequest` — ASL_FileRequest, ASL_FontRequest, ASL_ScreenModeRequest
- [x] `AslRequestTags` / `AslRequest` — run modal requester, return TRUE/FALSE; direct file/font/screen-mode regressions now verify the public varargs helper surface against the bundled NDK alib definitions (v0.6.119)
- [x] FileRequester tags: ASLFR_Window, ASLFR_TitleText, ASLFR_InitialFile, ASLFR_InitialDrawer, ASLFR_InitialPattern, ASLFR_Flags1 (FRF_DOSAVEMODE, FRF_DOMULTISELECT, FRF_DOPATTERNS)
- [x] FontRequester tags: ASLFO_Window, ASLFO_TitleText, ASLFO_InitialName, ASLFO_InitialSize, ASLFO_InitialStyle, ASLFO_Flags (FOF_DOFIXEDWIDTHONLY, FOF_DOSTYLEBUTTONS)
- [x] ScreenMode requester tags — initial mode/size/depth filters, title/buttons, and modal selection/cancel flow
- [x] `fr_File` / `fr_Drawer` / `fr_NumArgs` / `fr_ArgList` output fields
- [x] `fo_Attr` / `fo_TAttr` output fields ✅ (Phase 70a)

##### 78-G-2: Review

- [x] Implement missing functions and stubs as far as possible — remaining follow-up is broader ScreenMode requester option coverage beyond the current modal/file/font/screen-mode helper surface
- [x] Architecture review: requester varargs entry points intentionally reuse the core taglist implementation paths (`AllocAslRequest` / `AslRequest`) via the public pragma/inline ABI, so no extra hosted dispatch layer is needed; broader ASL/private requester architecture follow-ups remain out of scope until more tags and hooks land, with no new Phase 79 item required in the meantime (v0.6.119)
- [x] Performance review: the public varargs helper path is compiler-side stack/tag marshalling into the existing taglist entry points, so there is no distinct runtime hot path to optimize beyond the current requester implementations; no new Phase 79 item is required for this verification-only pass (v0.6.119)

---

#### 78-H: Utility Library (`src/rom/lxa_utility.c` vs `others/AROS-20231016-source/rom/utility/`)

Status: complete in 0.6.124.

- [x] `FindTagItem` — scan TagList for tag; handle TAG_MORE, TAG_SKIP, TAG_IGNORE, TAG_END
- [x] `GetTagData` — return ti_Data or default
- [x] `TagInArray` — check if tag present in array
- [x] `FilterTagItems` — remove non-listed tags
- [x] `MapTags` — remap tag values
- [x] `AllocateTagItems` / `FreeTagItems` / `CloneTagItems`
- [x] `RefreshTagItemClones`
- [x] `NextTagItem` — iterate with state pointer
- [x] `CallHookPkt` — invoke Hook with A0=hook, A2=object, A1=message
- [x] `HookEntry` — verified against NDK `amiga.lib` semantics via direct `utility/tagitems` regression coverage; `CallHookPkt()` now explicitly exercises the public `HookEntry`/`h_SubEntry` path instead of a test-local shim (v0.6.123)
- [x] `CheckDate` / `Amiga2Date` / `Date2Amiga` — struct ClockData ↔ seconds-since-epoch
- [x] `SMult32` / `UMult32` / `SDivMod32` / `UDivMod32` — 32-bit multiply/divide
- [x] `PackStructureTags` / `UnpackStructureTags` — TagItem ↔ struct mapping via PackTable
- [x] `NamedObjectA` / `AllocNamedObjectA` / `FreeNamedObject` / `AddNamedObject` / `RemNamedObject` / `FindNamedObject` — root and nested namespace allocation, duplicate-name rejection, case-insensitive lookup, removal/reply semantics, and user-space allocation now have direct regression coverage (v0.6.122)
- [x] `GetUniqueID`
- [x] `MakeDosPatternA` / `MatchDosPatternA` — verified out of scope for `utility.library` after checking the bundled NDK 3.2 public utility surface (`Autodocs/utility.md`, `FD/utility_lib.fd`, `Include_H/clib/utility_protos.h`) and the bundled AROS utility sources; no exported `utility.library` API with these names exists, and DOS pattern support remains tracked under `dos.library` `ParsePattern*()` / `MatchPattern*()` coverage (v0.6.124)

##### 78-H-2: Review

- [x] Implement missing functions and stubs as far as possible — `HookEntry` is now verified as an `amiga.lib` companion surface used by utility hooks, and the remaining DOS-pattern follow-up was closed by confirming `MakeDosPatternA()` / `MatchDosPatternA()` are not part of the public `utility.library` surface tracked by the bundled NDK/AROS references (v0.6.124)
- [x] Architecture review: utility named objects still duplicate private namespace/list plumbing inside `lxa_utility.c`; Phase 79 now tracks factoring that internal bookkeeping behind shared helpers before `HookEntry`/DOS-pattern follow-up work extends the library further (v0.6.122)
- [x] Performance review: utility namespace lookup now has coverage, and Phase 79 now tracks adding optional name-lookup acceleration so larger shared namespaces do not stay on linear scans forever (v0.6.122)

---

#### 78-I: Locale Library (`src/rom/lxa_locale.c` vs NDK `libraries/locale.h`)

- [x] `OpenLocale` / `CloseLocale` — NULL = default locale; direct named fallback for built-in English default
- [x] `OpenCatalog` / `CloseCatalog` — `.catalog` loading from `PROGDIR:Catalogs/` and `LOCALE:Catalogs/`, with built-in-language and exact-version fallback behavior covered by exec regressions (v0.7.0)
- [x] `GetCatalogStr` / `GetLocaleStr`
- [x] `IsAlpha` / `IsDigit` / `IsSpace` / `IsUpper` / `IsLower` / `IsAlNum` / `IsGraph` / `IsCntrl` / `IsPrint` / `IsPunct` / `IsXDigit`
- [x] `ToUpper` / `ToLower` — verified against the bundled NDK as `utility.library` entry points patched through locale support rather than public `locale.library` APIs; locale-side case conversion remains `ConvToUpper()` / `ConvToLower()` (v0.7.0)
- [x] `StrnCmp` — built-in ASCII/collation strength handling for `SC_ASCII`, `SC_COLLATE1`, and `SC_COLLATE2`
- [x] `ConvToUpper` / `ConvToLower`
- [x] `FormatDate` ✅ (Phase 77) — all % codes; verified against AROS `locale/formatdate.c`
- [x] `FormatString` ✅ (Phase 77) — `%s`/`%d`/`%ld`/`%u`/`%lu`/`%x`/`%lx` with PutChProc callback
- [x] `ParseDate` ✅ (Phase 77)

##### 78-I-2: Review

- [x] Implement missing functions and stubs as far as possible — locale.library now covers the remaining public V38 surface tracked here, including catalog loading/parsing, catalog/default string fallback, character classification/case conversion, and `StrConvert()` / `StrnCmp()` regression coverage alongside the existing date/string formatting paths (v0.7.0)
- [x] Architecture review: locale formatting, catalog lookup, and collation still embed one built-in English/ASCII data source directly in `lxa_locale.c`; Phase 79 now tracks splitting locale-private data/provider state behind a shared companion so future real locale files/language drivers do not couple every API to one monolithic translation unit (v0.7.0)
- [x] Performance review: catalog string lookup and built-in collation transforms remain linear scans/rebuilds per call; Phase 79 now tracks indexing catalog IDs and caching transformed collation keys so repeated `GetCatalogStr()` / `StrnCmp()` / `StrConvert()` workloads avoid repeated full scans and folding work (v0.7.0)



---

#### 78-J: IFFParse Library (`src/rom/lxa_iffparse.c` vs NDK `libraries/iffparse.h`)

- [x] `AllocIFF` / `FreeIFF`
- [x] `InitIFFasDOS` / `InitIFFasClip`
- [x] `OpenIFF` / `CloseIFF` — IFFF_READ, IFFF_WRITE
- [x] `ParseIFF` — IFFPARSE_RAWSTEP, IFFPARSE_SCAN, IFFPARSE_STEP; return codes
- [x] `StopChunk` / `StopOnExit` / `EntryHandler` / `ExitHandler`
- [x] `PushChunk` / `PopChunk` — write support
- [x] `ReadChunkBytes` / `ReadChunkRecords` / `WriteChunkBytes` / `WriteChunkRecords`
- [x] `CurrentChunk` / `ParentChunk` / `FindProp` / `FindCollection` / `FindPropContext`
- [x] `CollectionChunk` / `PropChunk`
- [x] `OpenClipboard` / `CloseClipboard` — clipboard-backed IFF streams now work for the covered hosted path; direct `ReadClipboard` / `WriteClipboard` entry points remain outside the public NDK iffparse surface tracked here

##### 78-J-2: Review

- [x] Validation complete: full ROM rebuild, direct iffparse regression coverage, and full `ctest --test-dir build --output-on-failure -j16` are green
- [x] Implement missing functions and stubs as far as possible
- [x] Architecture review: identify architecture improvement opportunities, add them to phase 79
- [x] Performance review: identify performance improvement opportunities, add them to phase 79



---

#### 78-K: Keymap Library (`src/rom/lxa_keymap.c` vs `others/AROS-20231016-source/rom/keymap/`)

- [x] `SetKeyMapDefault` / `AskKeyMapDefault` — default-pointer update semantics now match the public RKRM/AROS contract, including explicit NULL installs for callers that temporarily clear the default (v0.7.1)
- [x] `MapANSI` — convert ANSI string to rawkey+qualifier sequence, including string-descriptor lookup, control translation, overflow reporting, and dead-key prefix emission (v0.7.1)
- [x] `MapRawKey` — rawkey+qualifier+keymap → character(s); qualifier processing (shift, alt, ctrl), string descriptors, key-up filtering, and dead/deadable translation now have direct regression coverage (v0.7.1)
- [x] `KeyMap` structure — km_LoKeyMap/km_LoKeyMapTypes/km_HiKeyMap/km_HiKeyMapTypes plus the remaining public pointer fields are now verified by direct struct-offset coverage (v0.7.1)
- [x] Dead-key sequences (diaeresis, accent, tilde) — hosted keymap translation now implements the public `DPF_DEAD` / `DPF_MOD` path, including prior-key state for deadable output (v0.7.1)

##### 78-K-2: Review

- [x] Implement missing functions and stubs as far as possible — `MapANSI()` is no longer a stub, `MapRawKey()` now covers the documented dead-key path, and direct exec regressions lock the public keymap surface against the bundled RKRM/AROS behavior (v0.7.1)
- [x] Architecture review: keymap translation still duplicates AROS-style descriptor scanning and dead-key bookkeeping inline inside `lxa_keymap.c`; Phase 79 now tracks factoring descriptor lookup and dead-key metadata discovery behind shared private helpers before alternate keymaps or console-side overrides expand further (v0.7.1)
- [x] Performance review: `MapANSI()` still rescans both keymap halves to rediscover dead-key metadata and candidate matches on every call; Phase 79 now tracks caching descriptor/dead-key indexes per installed keymap so repeated conversions avoid full linear scans (v0.7.1)



---

#### 78-L: Timer Device (`src/rom/lxa_dev_timer.c` vs `others/AROS-20231016-source/rom/timer/`)

- [x] `TR_ADDREQUEST` — UNIT_MICROHZ, UNIT_VBLANK (VBL aligned), UNIT_ECLOCK, UNIT_WAITUNTIL, and UNIT_WAITECLOCK now convert/round delays per the bundled timer autodocs and complete immediate past-deadline waits without hanging the caller (v0.7.2)
- [x] `TR_GETSYSTIME` / `TR_SETSYSTIME` — system time now follows the device-set offset instead of raw host time, and direct device regressions verify the zeroed-request semantics plus subsequent `GetSysTime()` / `TR_GETSYSTIME` reads (v0.7.2)
- [x] `AbortIO` for timer requests — pending timer requests now return 0 on successful abort, set `IOERR_ABORTED`, and zero the request time in the hosted queue path (v0.7.2)
- [x] `GetSysTime` / `AddTime` / `SubTime` / `CmpTime` — `struct timeval` normalization, carry/borrow handling, and classic timer-device comparison ordering are locked down by direct device regression coverage (v0.7.2)
- [x] EClockVal reading — `ReadEClock()` now returns a stable frequency plus advancing 64-bit `EClockVal` values without relying on 64-bit ROM helper libgcc symbols (v0.7.2)

##### 78-L-2: Review

- [x] Implement missing functions and stubs as far as possible — the hosted timer surface tracked for this phase now covers request scheduling across the documented units, system-time set/get behavior, abort semantics, library helper calls, and direct `EClockVal` reads with regression coverage in `Tests/Devices/Timer` / `Tests/Devices/TimerAsync` plus `devices_gtest` and `exec_gtest` validation (v0.7.2)
- [x] Architecture review: timer timekeeping still mixes raw-host time reads, system-time offsets, and per-unit conversion rules inline inside `lxa_dev_timer.c`; Phase 79 now tracks factoring that state and conversion logic behind a small private timer-time companion so request scheduling and direct library calls cannot drift semantically (v0.7.2)
- [x] Performance review: timer requests currently recompute unit conversions and scan the fixed host queue linearly for every add/remove/expiry check; Phase 79 now tracks consolidating conversion helpers and moving the hosted queue toward wake-time-ordered bookkeeping so longer-lived timer workloads avoid repeated full scans (v0.7.2)



---

#### 78-M: Console Device (`src/rom/lxa_dev_console.c` vs `others/AROS-20231016-source/rom/devs/console/`)

- [x] `CMD_WRITE` — ANSI cursor movement, erase, cursor-visibility, and SGR color/attribute handling are locked down by the direct `Tests/Console/csi_unit` and `Tests/Console/sgr_unit` regressions plus `console_gtest` validation (v0.7.5)
- [x] `CMD_READ` — basic character/line reads, async completion, keymap-backed translation, and the documented raw-input-event/report stream now have direct regression coverage for legacy special-key reads plus `aSRE` / `aRRE` raw-key, mouse-button, gadget, and resize reports in `Tests/Console/raw_events_unit` under `console_gtest`; remaining follow-up is broader event-class breadth and any documented read-mode flags once a verified public reference is identified (v0.7.6)
- [x] `CD_ASKKEYMAP` / `CD_SETKEYMAP` — default-keymap query/set behavior and the documented `CONU_LIBRARY` library-only path are covered directly by the RKM-style `AskKeymap` sample, while unit-local keymap install/query plus `CMD_READ` rawkey translation stay covered by the dedicated console keymap regression; standard console opens no longer pre-copy the default keymap unless callers explicitly install one (v0.7.5)
- [x] `CONU_LIBRARY` unit (-1) handling — library-only open path and `RawKeyConvert()` access are now locked by direct regression coverage alongside the keymap command tests (v0.7.3)
- [x] Window resize events (IDCMP_NEWSIZE) → console resize — hosted console units now recompute bounds from `IDCMP_NEWSIZE`, and `Tests/Console/csi_unit` verifies that window growth updates the reported console rows/columns (v0.7.5)
- [x] Scroll-back buffer management — `CD_SETUPSCROLLBACK` / `CD_SETSCROLLBACKPOSITION` now have hosted compatibility coverage via `Tests/Console/scrollback_unit`, with text-mode history storage and repositioning wired for standard console units (v0.7.6)
- [x] Verify ANSI CSI sequences: `\e[H` (home), `\e[J` (erase display), `\e[K` (erase line), `\e[?25l/h` (cursor hide/show) — direct pixel/bounds regressions in `Tests/Console/csi_unit` and attribute-state regressions in `Tests/Console/sgr_unit` now cover the tracked write-side behavior (v0.7.5)

##### 78-M-2: Review

- [x] Implement missing functions and stubs as far as possible — keymap command handling, standard-unit default-keymap fallback, library-only open semantics, write-side ANSI/resize coverage, raw-input/report `CMD_READ` compatibility, and initial scroll-back command support are now covered; remaining follow-up is broader event-class breadth and documented read-mode flags (v0.7.6)
- [x] Architecture review: console-device follow-up should split private console-unit state and centralize input-event/report translation; added to phase 79 (v0.7.5)
- [x] Performance review: console-device follow-up should avoid per-keystroke keymap setup churn and redundant cursor refresh waits; added to phase 79 (v0.7.5)



---

#### 78-N: Input Device (`src/rom/lxa_dev_input.c` vs `others/AROS-20231016-source/rom/devs/input/`)

- [x] `IND_ADDHANDLER` / `IND_REMHANDLER` — input handler chain; priority ordering (v0.7.7)
- [x] `IND_SETTHRESH` / `IND_SETPERIOD` — key repeat configuration storage and direct request coverage (v0.7.7)
- [x] `IND_SETMPORT` / `IND_SETMTYPE` (v0.7.7)
- [x] `InputEvent` chain processing; current hosted coverage now dispatches `IECLASS_RAWKEY`, `IECLASS_RAWMOUSE`, and `IECLASS_POINTERPOS` through `input.device`, with direct `IND_WRITEEVENT` / `IND_ADDEVENT` regression coverage and Intuition-side host input routing preserved (v0.7.7)
- [x] Qualifier accumulation: `PeekQualifier()` now tracks the held-state keyboard and mouse-button bits (`IEQUALIFIER_LSHIFT`, `RSHIFT`, `CAPSLOCK`, `CONTROL`, `LALT`, `RALT`, `LCOMMAND`, `RCOMMAND`, `NUMERICPAD`, `MIDBUTTON`, `RBUTTON`, `LEFTBUTTON`) while dispatched `InputEvent`s preserve transient per-event bits such as `REPEAT`, `INTERRUPT`, and `MULTIBROADCAST`; direct `Tests/Devices/Input` regressions cover key/button press and release snapshots plus transient-bit delivery (v0.7.8)
- [x] `FreeInputHandlerList` — verified out of scope after checking the bundled NDK 3.2 and AROS public `input.device` surfaces; no exported API with this name is present, so Phase 78-N closes on the documented `PeekQualifier()` + command set instead (v0.7.8)

##### 78-N-2: Review

- [x] Implement missing functions and stubs as far as possible — `input.device` now provides handler installation/removal, held qualifier snapshots via `PeekQualifier()`, transient qualifier preservation on dispatched events, repeat timing setters, mouse-port/type setters, and direct event dispatch for the currently hosted rawkey/rawmouse/pointer event path with dedicated `Tests/Devices/Input` coverage; the old `FreeInputHandlerList` follow-up is retired after verifying it is not part of the bundled public `input.device` ABI (v0.7.8)
- [x] Architecture review: input-event synthesis still lives partly in `lxa_intuition.c` and partly in `lxa_dev_input.c`; Phase 79 now tracks centralizing host-event to `InputEvent` translation behind a shared private input companion so Intuition, console, and future commodities consumers cannot drift semantically (v0.7.7)
- [x] Performance review: the new hosted handler path still rebuilds and walks the full handler chain for every single event without batching or cached qualifier/event templates; Phase 79 now tracks lightweight reusable event-translation helpers plus optional handler fast-path bookkeeping so bursty keyboard/mouse workloads avoid redundant per-event setup work (v0.7.7)



---

#### 78-O: Audio Device (`src/rom/lxa_dev_audio.c`)

- [x] `ADCMD_ALLOCATE` / `ADCMD_FREE` — channel allocation bitmask (channels 0-3)
- [x] `ADCMD_SETPREC` — allocation precedence
- [x] `CMD_WRITE` — DMA audio playback: period/volume/data/length
- [x] `ADCMD_PERVOL` — change period and volume of running sound
- [x] `ADCMD_FINISH` / `ADCMD_WAITCYCLE`
- [x] Audio interrupt (end-of-sample notification)
- [x] SDL2 audio stream mapping to Amiga channel model

##### 78-O-2: Review

- [x] Implement missing functions and stubs as far as possible — `audio.device` now covers the tracked hosted surface for allocation/free, precedence updates, queued single-channel `CMD_WRITE`, live `ADCMD_PERVOL` / `ADCMD_FINISH` / `ADCMD_WAITCYCLE`, and end-of-cycle `INTB_AUD0-3` interrupt delivery with direct regression coverage in `Tests/Devices/Audio` plus `devices_gtest` / `exec_gtest` validation (v0.7.9)
- [x] Architecture review: audio playback timing, request queues, and IRQ dispatch currently live together inside `lxa_dev_audio.c`; Phase 79 now tracks splitting channel-state/timing bookkeeping from interrupt/request management so future lock/steal semantics and broader Paula compatibility can grow without one monolithic device file (v0.7.9)
- [x] Performance review: hosted audio playback currently re-queues one fragment per write/cycle and linearly scans misc/pending request lists on every VBlank; Phase 79 now tracks wake-time-ordered audio events plus per-channel waiter buckets so longer queued playback avoids redundant full-list walks and fragment churn (v0.7.9)



---

#### 78-P: Trackdisk Device (`src/rom/lxa_dev_trackdisk.c` vs `others/AROS-20231016-source/rom/devs/trackdisk/`)

- [x] `TD_READ` / `TD_WRITE` / `TD_FORMAT` / `TD_SEEK` — hosted trackdisk units now map to `.adf` images exposed either directly or via the first `.adf` file inside the mapped `DFx:` directory, with direct regression coverage for sector reads/writes, track formatting, seek validation, missing-media handling, and write-protect behavior in `Tests/Devices/Trackdisk` (v0.7.11)
- [x] `TD_MOTOR` ✅ (Phase 77)
- [x] `TD_CHANGENUM` / `TD_CHANGESTATE` / `TD_PROTSTATUS` / `TD_GETGEOMETRY` ✅ (Phase 77)
- [x] `TD_ADDCHANGEINT` / `TD_REMCHANGEINT` — hosted compatibility now holds the change-notify request pending until `TD_REMCHANGEINT`, matching the public NDK lifetime semantics with direct regression coverage in `Tests/Devices/Trackdisk` (v0.7.10)
- [x] `ETD_READ` / `ETD_WRITE` — extended count checking now rejects stale media counters and sector-label requests on the hosted `.adf` path, with direct regression coverage in `Tests/Devices/Trackdisk` (v0.7.11)
- [x] Error codes: TDERR_NotSpecified, TDERR_NoSecHdr, TDERR_BadSecPreamble, TDERR_TooFewSecs, TDERR_NoDisk

##### 78-P-2: Review

- [x] Implement missing functions and stubs as far as possible — hosted `trackdisk.device` now covers the planned `.adf`-backed read/write/format/seek surface, extended read/write count validation, write-protect/missing-media behavior, and direct regression coverage in `Tests/Devices/Trackdisk`; validation is green via targeted `devices_gtest` and full `ctest --test-dir build --output-on-failure -j16` (v0.7.11)
- [x] Architecture review: hosted trackdisk `.adf` discovery and request validation currently split between ROM-side command dispatch and host-side image lookup/open code; Phase 79 now tracks factoring disk-image selection, media-state refresh, and command validation behind one private trackdisk companion so direct and extended command paths cannot drift semantically (v0.7.11)
- [x] Performance review: hosted trackdisk I/O currently re-opens and rescans the mapped `DFx:` path for every request, with byte-at-a-time host transfers on each read/write; Phase 79 now tracks caching resolved image handles/metadata and batching sector copies so repeated floppy traffic avoids redundant directory scans and syscall churn (v0.7.11)



---

#### 78-Q: Expansion Library (`src/rom/lxa_expansion.c` vs `others/AROS-20231016-source/rom/expansion/`)

- [x] `FindConfigDev` ✅ — verify ConfigDev chain search
- [x] `AddConfigDev` / `RemConfigDev`
- [x] `AllocBoardMem` / `FreeBoardMem`
- [x] `AllocExpansionMem` / `FreeExpansionMem`
- [x] `ConfigBoard` / `ConfigChain`
- [x] `MakeDosNode` / `AddDosNode` — bootnode creation
- [x] `ObtainConfigBinding` / `ReleaseConfigBinding`
- [x] `SetCurrentBinding` / `GetCurrentBinding`

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
- [x] Regression maintenance: restore `simplegtgadget_gtest` and all `gadtoolsgadgets_*_gtest` shards to green after hosted-output timing drift and underline-rendering regressions; verified with targeted reruns and full `ctest --test-dir build --output-on-failure -j16` (v0.6.116)
- [x] Regression maintenance: restore `exec_gtest`, `shell_gtest`, and `rgbboxes_gtest` to green after timer/SystemTagList/test-harness regressions; verified with full `ctest --test-dir build --output-on-failure -j16`
- [x] Rebalance long-running GTest drivers for parallel CTest execution by splitting oversized suites (`gadtoolsgadgets`, `simplegad`, `simplemenu`, `menulayout`, `cluster2`) into smaller shards; verified with full `ctest --test-dir build --output-on-failure -j16`
- [x] Regression maintenance: restore `graphics_gtest` (`ColorsPens`) and `dos_gtest` (`AssignNotify`) after palette-pen matching and assign-backed `DeviceProc()` regressions; verified with focused reruns and full `ctest --test-dir build --output-on-failure -j16`
- [ ] Add new GTest assertions for each behavioral deviation found vs AROS
- [x] Run RKM sample programs after structural changes to catch regressions (gadtoolsgadgets_gtest, simplemenu_gtest, filereq_gtest, fontreq_gtest, easyrequest_gtest) — ASL requester regression sweep now covers `filereq_gtest`, `fontreq_gtest`, `screenmodereq_gtest`, and `misc_gtest` targeted reruns in 0.6.117
- [ ] Run application tests after changes to exec/dos (asm_one_gtest, devpac_gtest, kickpascal_gtest, cluster2_gtest, dpaint_gtest)
- [ ] Update version when Phase 78 is complete (next MINOR — significant compatibility milestone)

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
- [ ] GadTools architecture: factor shared gadget/menu label creation, underscore handling, and pen/font resolution behind reusable helpers so gadget, menu, and bevel rendering paths cannot drift semantically
- [ ] GadTools architecture: introduce compact private companions for GadTools-owned menu/item metadata instead of overloading public structs with ad-hoc hosted allocations when more V39+ features land
- [ ] Utility architecture: factor named-object/root-namespace bookkeeping behind shared private helpers or companions instead of open-coding list/semaphore ownership inside `lxa_utility.c` before remaining utility APIs expand further
- [ ] Locale architecture: split locale-private built-in strings, catalog state, and future language-driver/file-backed providers behind a shared companion layer so `lxa_locale.c` does not keep one monolithic English-only data source wired directly into every API
- [ ] IFFParse architecture: factor declaration matching, property/collection storage, and clipboard stream state into smaller helpers/companions so `lxa_iffparse.c` does not keep growing one monolithic parser state machine
- [ ] Graphics performance: avoid repeated palette list walks in `ObtainBestPenA()`/`FindColor()` by caching exact-match or nearest-pen hints inside palette-extra state
- [ ] Graphics performance: skip redundant viewport/display updates when `ScrollVPort()` or `ChangeVPBitMap()` are called with unchanged state
- [ ] Intuition performance: cache the current Workbench screen pointer or tail/front bookkeeping so `OpenWorkBench()`, `WBenchToFront()`, and `WBenchToBack()` avoid repeated full screen-list scans
- [ ] Layers performance: avoid full `RebuildAllClipRects()` passes for simple z-order/visibility changes by recomputing only the affected layer span
- [ ] Layers performance: replace coarse `DamageExposedAreas()` whole-intersection refreshes with exact exposed-rectangle splitting to reduce redundant refresh damage on move/size/delete paths
- [ ] GadTools performance: cache per-menu/item text extents and derived layout widths so repeated `LayoutMenusA()`/`LayoutMenuItemsA()` calls avoid re-measuring every label and submenu chain
- [ ] GadTools performance: skip redundant `GT_RefreshWindow()` full-list redraws when attribute updates can identify a single affected gadget or requester subtree
- [ ] Utility performance: add optional accelerated name lookup for large shared namespaces so repeated `FindNamedObject()` scans do not remain purely linear as more hosted subsystems start sharing named objects
- [ ] Locale performance: add indexed catalog-string lookup and reusable collation transform caches so repeated `GetCatalogStr()` / `StrnCmp()` / `StrConvert()` calls avoid linear entry scans and repeated ASCII folding work
- [ ] Keymap architecture: factor descriptor lookup, qualifier decoding, and dead-key bookkeeping behind shared private helpers so `MapRawKey()` and `MapANSI()` cannot drift as alternate keymaps or console-side overrides grow
- [ ] Keymap performance: cache per-keymap dead-key indexes and descriptor search metadata so repeated `MapANSI()` conversions avoid rescanning both keymap halves on every call
- [ ] Timer architecture: factor raw-host time reads, system-time offsets, and per-unit delay/absolute-time conversion rules behind a private timer-time helper so `TR_*` requests and direct timer.library entry points cannot drift semantically
- [ ] Timer performance: replace the fixed unsorted hosted timer queue plus repeated per-request conversion work with wake-time-ordered bookkeeping so add/remove/expiry checks avoid full linear scans on heavier timer workloads
- [ ] Console architecture: separate private hosted console-unit state (CSI parser, pending reads, raw-event masks, future scroll-back storage) from the public `struct ConUnit` surface so `CMD_WRITE`, `CMD_READ`, and resize/refresh handling share one compatibility model
- [ ] Console architecture: centralize console input-event translation/report generation so keymap conversion, special-key CSI output, and future `aSRE` / `aRRE` / `aIER` / `aSKR` support cannot drift across the IDCMP, `CMD_READ`, and library-entry paths
- [ ] Console performance: cache reusable keymap/default-keymap state for active console units so repeated `CMD_READ` key conversions avoid reopening or rebuilding translation state for every keystroke
- [ ] Console performance: batch cursor redraw/refresh decisions during bursty writes, input processing, and resize handling so console output avoids redundant `WaitTOF()` and full cursor repaint churn
- [ ] Input architecture: centralize host-event to `InputEvent` synthesis behind shared private helpers so `lxa_intuition.c`, `lxa_dev_input.c`, console raw-event generation, and future commodities consumers all reuse one compatibility path
- [ ] Input performance: cache reusable qualifier/event-template state and optional handler fast-path metadata so bursty keyboard/mouse workloads avoid rebuilding equivalent `InputEvent` state and fully generic handler walks on every host event
- [ ] Audio architecture: split private audio channel timing/state, request-queue ownership, and IRQ dispatch helpers behind a compact companion so allocation/steal rules, playback control, and end-of-sample interrupt delivery cannot drift as more Paula-compatible behavior lands
- [ ] Audio performance: replace VBlank-wide misc/pending-request scans and per-write fragment requeueing with per-channel wake buckets / lightweight host buffering so longer playback sequences avoid redundant list walks and queue churn
- [ ] Trackdisk architecture: factor hosted `.adf` discovery, media-state refresh, and direct/extended command validation behind a private trackdisk companion so ROM-side command dispatch and host-side image handling cannot drift semantically
- [ ] Trackdisk performance: cache resolved `DFx:` image metadata/handles and batch sector transfers so repeated floppy I/O avoids per-request directory rescans, reopen churn, and byte-at-a-time host copies
- [ ] IFFParse performance: avoid repeated full context-stack LCI scans during `ParseIFF()` by separating declaration indexes from stored items and caching active stop/property/collection matches per context


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
- **Phase 78-F Review**: GadTools review pass completed; menu creation/layout helpers plus message/refresh wrappers now have hosted compatibility implementations with direct regression coverage, and the remaining GadTools architecture/performance follow-ups are tracked in Phase 79.
- **Phase 78-G Review**: ASL review advanced; the public `AllocAslRequestTags()` / `AslRequestTags()` varargs surface is now verified across file, font, and screen mode requester flows, and the remaining ASL follow-up is broader ScreenMode option coverage (v0.6.119).
- **Phase 78-H**: utility.library is now fully closed for this roadmap phase: `HookEntry`-driven hook dispatch is verified against the NDK `amiga.lib` contract, the named-object namespace regressions lock duplicate handling, enumeration, removal holds, and reply semantics, and the old DOS-pattern follow-up is retired after confirming it is not part of the public `utility.library` surface in the bundled NDK/AROS references (v0.6.124).
- **Phase 78-I**: locale.library now closes the remaining public V38 surface tracked for this phase: named/default locale opens, on-disk `.catalog` loading from `PROGDIR:` / `LOCALE:`, catalog/default string lookup, character classification and case conversion, and `StrConvert()` / `StrnCmp()` collation coverage are all exercised by direct exec regressions alongside the existing `FormatDate()` / `FormatString()` / `ParseDate()` tests (v0.7.0).
- **Phase 78-J**: iffparse.library now covers the tracked V36 parser surface for hosted use: chunk allocation/open/close, RAWSTEP/STEP/SCAN traversal, stop conditions, entry/exit handlers, property/collection storage, write-mode chunk construction, record/byte I/O helpers, context queries, and clipboard-backed IFF streams are exercised by the direct iffparse regression plus the full green suite; the remaining Phase 79 work is structural/performance cleanup inside `lxa_iffparse.c` rather than missing public APIs.
- **Phase 78-K**: keymap.library now closes the tracked public keymap surface for this phase: default-keymap query/update semantics, `MapRawKey()` qualifier/string/dead-key translation, `MapANSI()` rawkey encoding, and `KeyMap` structure layout all have direct regression coverage via `Tests/Exec/Keymap` and `Tests/Exec/StructOffsets` (v0.7.1).
- **Phase 78-L**: timer.device now closes the tracked hosted timer surface for this phase: `TR_ADDREQUEST` covers the documented relative/absolute timer units (`UNIT_MICROHZ`, `UNIT_VBLANK`, `UNIT_ECLOCK`, `UNIT_WAITUNTIL`, `UNIT_WAITECLOCK`), `TR_GETSYSTIME` / `TR_SETSYSTIME` share the same system-time offset semantics as `GetSysTime()`, `AbortIO()` returns classic timer abort results, `AddTime()` / `SubTime()` / `CmpTime()` normalize `timeval`s with direct regression coverage, and `ReadEClock()` returns advancing 64-bit values without ROM-side libgcc helpers; validation is green via `devices_gtest` and `exec_gtest` (v0.7.2).
- **Phase 78-M**: console.device compatibility advanced again: direct console regressions now lock ANSI cursor movement/erase/cursor-visibility handling, SGR color and attribute state, `IDCMP_NEWSIZE`-driven bounds updates, keymap/library-mode behavior, legacy special-key reads, broadened raw-input/report coverage (`aSRE` / `aRRE` for raw-key, mouse-button, gadget, and resize reports), and initial scroll-back commands together under `console_gtest`; the remaining follow-up is broader event-class breadth and documented read-mode flags (v0.7.6).
- **Phase 78-N**: input.device verification is now closed for the tracked public surface: handler-chain ordering (`IND_ADDHANDLER` / `IND_REMHANDLER`), repeat timing setters, mouse-port/type setters, direct `IND_WRITEEVENT` / `IND_ADDEVENT` dispatch, held-state `PeekQualifier()` snapshots, and transient per-event qualifier delivery are all covered by `Tests/Devices/Input`; the older `FreeInputHandlerList()` follow-up is retired after confirming it is not part of the bundled NDK/AROS public ABI (v0.7.8).
- **Phase 78-O**: audio.device now closes the tracked hosted audio surface for this phase: channel allocation/free, precedence changes, queued `CMD_WRITE` playback timing, live `ADCMD_PERVOL` / `ADCMD_FINISH` / `ADCMD_WAITCYCLE`, SDL-backed fragment playback, and end-of-sample `INTB_AUD0-3` interrupt delivery are exercised by `Tests/Devices/Audio` with validating `devices_gtest` / `exec_gtest` runs (v0.7.9).
- **Phase 78-P**: `trackdisk.device` now covers the tracked hosted surface for this phase: `.adf`-backed `TD_READ` / `TD_WRITE` / `TD_FORMAT` / `TD_SEEK`, extended `ETD_READ` / `ETD_WRITE` counter validation, classic geometry/status and write-protect/missing-media errors, plus the earlier change-interrupt lifetime semantics are all exercised by `Tests/Devices/Trackdisk`; validation is green via targeted `devices_gtest` and full `ctest --test-dir build --output-on-failure -j16` (v0.7.11).
- **Phase 78-W**: Structural Verification — OS Data Structure Offsets — 633 assertions passing, now including `KeyMap` layout coverage.
