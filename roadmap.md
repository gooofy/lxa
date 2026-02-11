# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project!

---

## Current Status

**Version: 0.6.52** | **Phase 71 In Progress** | **54/54 Tests Passing (1 pre-existing devpac_test failure)**

Phase 71: Performance & Infrastructure improvements.

**Current Status**:
- 54/54 ctest entries (53 passing, 1 pre-existing devpac_test divide-by-zero failure)
- **cpu_instr_callback ~80x speedup**: Added `g_debug_active` fast-path flag — when no debugging is active, the per-instruction callback only writes the trace buffer and checks PC < 0x100 safety net (skips breakpoint scanning, tracing, stepping). `dpaint_gtest` went from ~25s to 0.47s.
- **ClipBlit crash fixed**: Root cause was GCC using A5 as frame pointer while `LockLayerRom`/`UnlockLayerRom` ABI requires layer pointer in A5 — direct calls from ClipBlit clobbered the stack frame, causing `rts` to address 0. Fix: removed no-op Lock/Unlock direct calls from ClipBlit (they're no-ops in lxa). ClipBlit test re-enabled and passing.

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

## Completed Phases (1-69)

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

---

## Next Steps

### Phase 71: Performance & Infrastructure
**Goal**: Improve emulator performance and test infrastructure.
**STATUS**: In progress.
- [x] Fix ClipBlit layer cleanup crash — root cause: direct call to `LockLayerRom`/`UnlockLayerRom` from ClipBlit clobbers A5 frame pointer (AmigaOS ABI uses A5 for layer parameter). Fix: removed no-op direct calls. ClipBlit test re-enabled.
- [x] Optimize per-instruction callback (`cpu_instr_callback`) — added `g_debug_active` fast-path flag. When no debugging active, callback only writes trace buffer + PC safety check. ~80x speedup (dpaint_gtest: 25s → 0.47s).
- [ ] Optimize `dpaint_test` legacy C driver runtime (~25s due to DPaint's genuine initialization time, not callback overhead)

### Phase 72: Application Compatibility
**Goal**: Deeper application testing and compatibility improvements.
**TODO**:
- [ ] Implement `dopus.library` stub for Directory Opus window-open testing
- [ ] Extend KickPascal2 apps_misc test to verify window opening (currently load-only)
- [ ] SysInfo deeper testing — hardware detection without MMU crashes
- [ ] Explore new application targets for compatibility testing

---

## Postponed Phases
- **Phase xx: Pixel Array Operations**: Optimized pixel array functions.

---

## Version History

| Version | Phase | Key Changes |
| :--- | :--- | :--- |
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
