# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project!

---

## Current Status

**Version: 0.6.32** | **Phase 63 Complete** | **Google Test Transition**

The lxa project has successfully transitioned to a unified testing infrastructure based on Google Test and liblxa host-side drivers.

**Phase 63 Summary (Complete)**:
- Successfully transitioned the core test suite (Exec, DOS, Intuition, Graphics, etc.) to Google Test.
- Integrated host-side drivers for major applications including ASM-One, Devpac, KickPascal, MaxonBASIC, Cluster2, and DPaint V.
- Replaced legacy `test_inject.h` approach with robust, automated verification using `liblxa`.
- Standardized the development workflow around GTest and automated integration testing.

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

## Completed Phases (1-63)

### Foundation & Transitions (Phases 1-63)
- ✅ **Phases 1-62**: Core Exec, DOS, Graphics, Intuition, and Application support implementation.
- ✅ **Phase 63**: **Google Test & liblxa Transition** - Successfully migrated the core test suite to Google Test and integrated host-side drivers for major applications (ASM-One, Devpac, KickPascal, MaxonBASIC, Cluster2, DPaint V). Replaced legacy `test_inject.h` with robust, automated verification using `liblxa`.

---

## Active Goal: 100% Passing Test Suite

The current test suite identifies several regressions and environment issues that must be resolved to achieve a stable 0.7.0 release. **We shall not hesitate to extend the test suite to identify more bugs on the way.**

### Phase 64: Fix Failing Samples (Standalone Drivers)
**Goal**: Align standalone test drivers with restored RKM sample code.
**Status**: 🔴 FAILING
**TODO**:
- [ ] Update `tests/drivers/updatestrgad_test.c` to use interactive input/output matching (current: timeout waiting for debug strings).
- [ ] Update `tests/drivers/simplegtgadget_test.c` to use interactive input/output matching (current: timeout waiting for debug strings).
- [ ] Verify both standalone drivers pass.

### Phase 65: Fix RawKey & Input Handling
**Goal**: Investigate and fix why keyboard/mouse events are not consistently processed in tests.
**Status**: 🔴 FAILING (`rawkey_gtest`)
**TODO**:
- [ ] Debug `samples/intuition/RawKey.c` execution in GTest environment.
- [ ] Verify IDCMP RAWKEY event delivery and conversion (Current: "Key Down"/"Key Up" not found in output).
- [ ] Ensure shifted key and space key events are correctly captured.

### Phase 66: Fix Intuition & Console Regressions
**Goal**: Resolve functional failures in Intuition and console.device.
**Status**: 🔴 FAILING (`intuition_gtest`, `console_gtest`)
**TODO**:
- [ ] Resolve `IntuitionTest.Validation` failure (window/screen info mismatches).
- [ ] Fix `ConsoleTest.CSICursor` and `ConsoleTest.ConHandler` failures.
- [ ] Fix `ConsoleTest.InputConsole` interaction issues.
- [ ] Verify BitMap content verification (`lxa_get_content_pixels()`) across all tests.

### Phase 67: Fix Test Environment & App Paths
**Goal**: Ensure all "Deep Dive" app tests can find their binaries and data.
**Status**: 🔴 FAILING (Binary not found errors in `asm_one_gtest`, `devpac_gtest`, etc.)
**TODO**:
- [ ] Update `LxaTest::FindAppsPath()` in `lxa_test.h` to include `src/lxa-apps` in search locations.
- [ ] Fix hardcoded absolute paths in `asm_one_gtest.cpp`, `devpac_gtest.cpp`, etc.
- [ ] Verify all app tests can successfully load their binaries in the build environment.

### Phase 68: Extend Samples Tests
- [ ] RGBBoxesTest: measure runtime: the test application has a 5 second delay, currently it exits nearly immediately. Extend the test so it measures the time the app took, if less then 5 seconds: fail, debug, iterate
- [ ] Gadget/Intuition related tests: take screenshots of the application and analyze them: are window, gadget and menu borders where you expect them to be? Sample pixels at various places: do they have the color they should have?

### Phase 69: Application Deep Dive Fixes
**Goal**: Resolve functional failures in real-world applications.
**Status**: 🔴 FAILING (`apps_misc_gtest`, etc.)
**TODO**:
- [ ] Fix Directory Opus 4 "Cannot load DirectoryOpus" error (Post-path fix).
- [ ] Fix KickPascal 2 automated test failures in `apps_misc_gtest`.
- [ ] Resolve any initialization hangs or rendering bugs identified by the app test suite.
- [ ] Ensure ASM-One, Devpac, and Cluster2 are fully responsive to automated input.

### Phase 70: Stress Tests & Misc Libraries
**Goal**: Ensure system stability under load.
**Status**: 🔴 FAILING (`misc_gtest`)
**TODO**:
- [ ] Resolve `misc_gtest` failures in Icon, IffParse, and DataTypes tests.
- [ ] Tune stress test parameters (Filesystem, Memory, Tasks) for reliable execution in CI.

### Phase 71: Test Suite Expansion
**Goal**: Proactively identify more bugs by extending coverage.
**TODO**:
- [ ] Add more complex Graphics/Layers clipping tests.
- [ ] Extend DOS tests with more VFS corner cases (locking, seeking, large files).
- [ ] Implement TDD: add failing tests for every new bug report before fixing.

---

## Postponed Phases
- **Phase xx: Pixel Array Operations**: Optimized pixel array functions.

---

## Version History

| Version | Phase | Key Changes |
| :--- | :--- | :--- |
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
