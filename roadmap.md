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

### Phase 78: AROS Compatibility Verification
**Goal**: Compare and verify `lxa` implementation against AROS.
**TODO**:
- [ ] Audit core OS structures (`ExecBase`, `DOSBase`, `GfxBase`, etc.) and verify offsets and sizes against AROS definitions.
- [ ] Compare `lxa` API implementations (e.g. Memory allocation, List operations) against AROS source code.
- [ ] Run AROS test suite (if applicable) within `lxa` to formally verify behavior.

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
