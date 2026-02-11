# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project!

---

## Current Status

**Version: 0.6.50** | **Phase 70a In Progress** | **53/53 Tests Passing**

Phase 70a: fixing issues identified by comparing RKM sample programs to running originals on a real Amiga. Test-driven approach: reproduce each issue as a test first, then fix.

**Current Status**:
- 53/53 ctest entries passing (23 gadtoolsgadgets_gtest tests: 12 functional + 11 pixel)
- GTYP_SIZING resize gadget: Created sizing system gadget with GFLG_RELRIGHT|GFLG_RELBOTTOM positioning, 3D icon rendering (center box + diagonal indicator), resize-drag logic (SELECTDOWN start, MOUSEMOVE delta, SELECTUP stop), SizeWindow() with min/max enforcement. Border enlargement for WFLG_SIZEGADGET (BorderBottom=10, BorderRight=18) per RKRM/NDK, respecting WFLG_SIZEBBOTTOM/WFLG_SIZEBRIGHT flags. Fixed depth gadget positioning to use window edge (not border edge). 2 new pixel tests (SizeGadgetRendered, DepthGadgetRendered). Fixed IntuitionTest.Validation timing (wait for content rendered before pixel check).
- STRING_KIND double-bevel border: Implemented `gt_create_ridge_bevel()` for FRAME_RIDGE style groove border (outer recessed + inner raised, 4 Border structs). `gt_free_ridge_bevel()` for proper memory cleanup. 1 new pixel test (StringGadgetDoubleBevelRendered).
- GT_Underscore: Keyboard shortcut indicator rendering — underscore prefix stripped from labels, underline drawn under shortcut character via NextText IntuiText chain. _render_gadget() fixed to walk IntuiText NextText chain.
- SLIDER_KIND: Full prop gadget implementation — knob rendering (raised bevel), mouse click/drag interaction, IDCMP_MOUSEMOVE/GADGETUP with level as Code, GT_SetGadgetAttrs(GTSL_Level)
- MenuLayout: NEW sample ported from RKM (menulayout_gtest: 8 tests — 6 functional + 2 pixel)
- EasyRequest: FIXED (full BuildEasyRequestArgs implementation — formatted body/gadget text, 3D bevel buttons, centered layout, SysReqHandler event loop, FreeSysRequest cleanup; 7 new tests: 4 behavioral + 3 pixel)
- SimpleImage: FIXED (rewrote sample to match RKM — 2 images at correct positions, Delay before exit, no extra gadgets/title)
- IntuiText: FIXED (full-screen window, no title bar, correct text, OpenWindowTagList defaults)
- SimpleGad GADGHCOMP button click highlight: FIXED (complement XOR on mouse-down, restore on mouse-up)
- SimpleGTGadget: FIXED (ClickButton/ClickCheckbox/ClickCycle — corrected gadget positions from TopEdge=32 to 40, added ClickCheckbox test, checkbox bevel border pixel test)
- GadToolsGadgets: FIXED (ButtonClick/ButtonBevelBorder/StringGadgetBevelBorder — corrected topborder from 12 to 20, all 16 tests passing)
- SimpleMenu: FIXED (save-behind buffer for menu drop-down cleanup, IRQ3 debug output silenced; 2 new pixel tests: MenuDropdownClearedAfterSelection, ScreenTitleBarRestoredAfterMenu; simplemenu_gtest: 3→5 tests)
- FillRectDirect optimized: byte-at-a-time instead of pixel-at-a-time (~8x speedup)
- INTENA re-enable fix: when INTENA master/VBlank enabled via set operation, arm pending VBlank IRQ
- Menu selection encoding: _encode_menu_selection always encodes subNum bits 11-15
- Known bug: ClipBlit crashes (PC=0x00000000) during layer cleanup — test disabled, filed for Phase 71

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

### Test Suite Stabilization & Hardening (Phases 64-70)
- ✅ **Phase 64**: Fixed standalone test drivers (updatestrgad_test, simplegtgadget_test) with interactive input/output matching.
- ✅ **Phase 65**: Fixed RawKey & input handling — IDCMP RAWKEY events now correctly delivered in GTest environment.
- ✅ **Phase 66**: Fixed Intuition & Console regressions — console.device timing, InputConsole/InputInject wait-for-marker logic, ConsoleAdvanced output.
- ✅ **Phase 67**: Fixed test environment & app paths — all app tests find their binaries correctly.
- ✅ **Phase 68**: Rewrote `Delay()` to use `timer.device` (UNIT_MICROHZ) instead of busy-yield loop. Optimized stress test parameters for reliable CI execution. Fixed ExecTest.Multitask timing.
- ✅ **Phase 69**: Fixed apps_misc_gtest — restructured DirectoryOpus test to handle missing dopus.library gracefully, converted KickPascal2 to load-only test (interactive testing via kickpascal_gtest), fixed SysInfo background-process timeout handling. All 3 app tests now pass.
- ✅ **Phase 70**: **Test Suite Hardening & Expansion** — GadTools visual rendering (bevel borders, text labels, 13 pixel/functional tests), palette pipeline fix (SetRGB4/LoadRGB4/SetRGB32/SetRGB32CM/LoadRGB32), string gadget rawkey/input fixes, rewrote 3 RKM samples, added 3 graphics clipping tests + 10 DOS VFS corner-case tests. DOS: 14→24, Graphics: 14→17. 49/49 ctest entries.

---

## Next Steps

### Phase 70a: Fix Issues Identified by Manual Tests

Issues identified by comparing some of the sample programs to running the original RKM samples on a real Amiga.

**IMPORTANT**: Test-driven approach is **mandatory** when fixing these:
  * Make sure to reproduce each one of them first as part of the integration test.
  * Once the issue is reproducible, fix it and verify the fix by re-running the test.

* GadToolsGadgets
  * ~~`_` keyboard shortcut indicator not rendered correctly~~ **DONE** (GT_Underscore tag parsed, gt_strip_underscore() strips prefix, underline IntuiText via NextText chain, _render_gadget() walks NextText; 1 new pixel test)
  * ~~Volume slider knob not visible after startup~~ **DONE** (PropInfo with AUTOKNOB|FREEHORIZ|PROPNEWLOOK, knob rendering with raised bevel)
  * ~~Initial volume value (5) not visible~~ **DONE** (HorizPot computed from GTSL_Level, knob positioned correctly)
  * ~~Text entry widgets have wrong border style~~ **DONE** (gt_create_ridge_bevel() implements FRAME_RIDGE style double-bevel: outer recessed + inner raised, matching real Amiga; 1 new pixel test)
  * ~~Window lacks resize gadget~~ **DONE** (GTYP_SIZING system gadget: GFLG_RELRIGHT|GFLG_RELBOTTOM, 3D icon, resize-drag, border enlargement per RKRM/NDK; 2 new pixel tests)
  * ~~Window is not draggable~~ **DONE** (was already working via WA_DragBar)
  * ~~Window has distorted depth gadget~~ **DONE** (AROS-style depth gadget rendering: two overlapping unfilled rectangles with RectFill edges, shine-filled front rect; click handler toggles WindowToBack/WindowToFront with proper Screen->FirstWindow list reordering; 1 new functional test DepthGadgetClick, enhanced DepthGadgetRendered pixel test)
  * ~~Window lacks minimize gadget~~ **N/A** (RKM sample does not use WA_Zoom — confirmed against original source)
  * ~~TAB/Shift-TAB doesn't do anything~~ **DONE** (TAB key handler in _handle_string_gadget_key: scans window gadget list for GTYP_STRGADGET, forward/backward cycling with wrap-around, direct gadget activation with cursor-at-end positioning; 2 new functional tests TabCyclesStringGadgets, ShiftTabCyclesBackward)
  * ~~Volume slider does not react to mouse clicks~~ **DONE** (full prop gadget mouse interaction: click-on-knob drag, click-outside-knob jump, MOUSEMOVE with level, GADGETUP with level, GT_SetGadgetAttrs GTSL_Level)
  * ~~Window close gadget does not work~~ **DONE** (was already working via IDCMP_CLOSEWINDOW)
* SimpleGTGadget
  * ~~Button does not react to mouse clicks~~ **DONE** (test timing/coordinates fixed)
  * ~~Checkbox invisible~~ **DONE** (added bevel border for CHECKBOX_KIND, pixel test)
  * ~~Gadget labels extend beyond left window border~~ **DONE** (increased ng_LeftEdge for PLACETEXT_LEFT gadgets)
  * ~~Close gadget does not work~~ **DONE** (was already working, verified by test)
* SimpleGad
  * ~~Button does not react to mouse clicks~~ **DONE** (GADGHCOMP complement highlight, FillRectDirect optimization)
* UpdateStrGad
  * ~~Application does not react to mouse clicks~~ **DONE** (click-to-activate string gadget working, TypeIntoGadget test passes)
* IntuiText
  * ~~Window is not full-screen~~ **DONE** (OpenWindowTagList defaults to full-screen when no WA_Width/WA_Height)
  * ~~Window has wrong broder type~~ **DONE** (removed WA_Title/WA_Flags, thin border BorderTop=2)
  * ~~`Hello World ;)` text collides with window border~~ **DONE** (text at correct position, no collision)
* SimpleImage
  * ~~Shows 3 images instead of 2~~ **DONE** (removed third DrawImage, corrected positions to (10,10)/(100,10))
  * ~~Closes way too quickly~~ **DONE** (added Delay(100) before CloseWindow)
* EasyRequest
  * ~~No requester is displayed, application outputs a bunch of text on the console and then simply exits~~ **DONE** (full BuildEasyRequestArgs/SysReqHandler/FreeSysRequest, formatted text, 3D buttons, 7 tests)
* simplemenu
  * ~~Lots of `_handleIRQ3() called` debug output obscures view of actual application output~~ **DONE** (DPUTS macros commented out in exceptions.s)
  * ~~Menus stay on screen even after a menu item is selected~~ **DONE** (save-behind buffer: save screen bitmap before menu drop-down, restore on close/switch)
* menulayout
  * ~~Sample is missing: important sample, please port 1:1 from RKM!~~ **DONE** (ported from RKM, menulayout_gtest: 8 tests)
* FileReq
  * Window placed too low - bottom off screen
  * extra `Drawer libs:` label
* FontReq   
  * No requester appears, application simply exits

### Phase 71: Performance & Infrastructure
**Goal**: Improve emulator performance and test infrastructure.
**TODO**:
- [ ] Fix ClipBlit layer cleanup crash (PC=0x00000000 in DeleteLayer/SetRast after ClipBlit with layer-backed RastPort) — test disabled in graphics_gtest
- [ ] Investigate per-instruction callback overhead (`M68K_INSTRUCTION_CALLBACK` in `m68kconf.h`) — causes 5-10x slowdown for CPU-intensive tests
- [ ] Consider conditional callback enabling (only when debugger/breakpoints active)
- [ ] Optimize `dpaint_test` runtime (~25s is the slowest test)

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
