# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project!

---

## Current Status

**Version: 0.6.23** | **Phase 57 In Progress** | **42 RKM Sample Tests Passing** | **9 Host-Side Test Drivers**

The lxa project has achieved a comprehensive AmigaOS-compatible environment with 95%+ library compatibility across Exec, DOS, Graphics, Intuition, and system libraries.

**Phase 56 Summary (Complete)**:
- 42 RKM sample programs implemented (Exec, Intuition, Graphics, GadTools, BOOPSI, Math, Devices)
- Host-side test driver infrastructure (liblxa) for automated UI testing
- Fixed mathtrans.library (CORDIC, SPExp), mathffp.library (SPSub, SPCmp, SPTst, SPAbs)
- Fixed Amiga2Date, Cause() A5 register, display_close() use-after-free, ColorMap init
- Implemented AvailFonts(), FindConfigDev(), MakeScreen/RethinkDisplay/RemakeDisplay
- 214 total tests passing

**DPaint V Status**: Libraries load successfully, Workbench screen opens, application initializes without crashes. Currently investigating hang during initialization after font loading.

**ASM-One Status**: ✅ WORKING - Opens screens, window, and editor! Ready for assembly coding!
**Devpac Status**: Window renders AND responds to mouse/keyboard input!

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

## Completed Phases (1-41)

### Foundation (Phases 1-12)
- ✅ Exec multitasking with preemptive scheduling
- ✅ DOS filesystem with case-insensitive VFS
- ✅ Interactive Shell with scripting support
- ✅ 100+ built-in commands (DIR, TYPE, COPY, etc.)
- ✅ utility.library, mathffp.library, mathtrans.library

### Graphics & UI (Phases 13-25)
- ✅ SDL2 integration for display output
- ✅ graphics.library - drawing primitives, blitting, text
- ✅ intuition.library - screens, windows, gadgets, IDCMP
- ✅ layers.library - clipping, layer management
- ✅ console.device - full ANSI/CSI escape sequences

### Application Support (Phases 26-35)
- ✅ Real-world app testing: KickPascal 2, Devpac, MaxonBASIC, EdWordPro, SysInfo
- ✅ Library stubs: gadtools, workbench, asl, commodities, rexxsyslib, iffparse, reqtools, diskfont, icon, expansion, translator, locale, keymap
- ✅ 68030 CPU support, custom chip registers, CIA resources
- ✅ ROM robustness: pointer validation, crash recovery

### Library Completion (Phases 36-38)
- ✅ **exec.library** - 100% complete (172 functions)
- ✅ **graphics.library** - 90%+ complete
- ✅ **intuition.library** - 95%+ complete

### Testing & Fixes (Phases 39-40)
- ✅ **Phase 39**: Re-testing identified rendering issues in applications
- ✅ **Phase 39b**: Enhanced test infrastructure
- ✅ **Phase 40**: Fixed GFA Basic screen height bug

### IFF, Datatypes & Async I/O (Phases 41-45)
- ✅ **Phase 41**: iffparse.library full implementation
- ✅ **Phase 42**: datatypes.library (Basic Framework)
- ✅ **Phase 43**: console.device Advanced Features
- ✅ **Phase 44**: BOOPSI & GadTools Visual Completion
- ✅ **Phase 45**: Async I/O & Timer Completion

### RKM Samples & Test Infrastructure (Phases 46-56)
- ✅ **Phase 56**: RKM Samples Integration - 42 sample programs covering Exec, Intuition, Graphics, GadTools, BOOPSI, Math, Devices. Fixed mathtrans.library CORDIC/SPExp bugs, mathffp.library bugs (SPSub, SPCmp, SPTst, SPAbs), Amiga2Date bug. Host-side test driver infrastructure (liblxa). 214 total tests passing.

---

## Active Phase

### Phase 57: Host-Side Test Driver Migration (Current)
**Goal**: Migrate all interactive UI tests and deep dive app tests to use the liblxa host-side driver infrastructure.

**Why This Matters**:
- Host-side drivers provide full control over timing and event injection
- Better debugging capabilities (host-side breakpoints)
- More reliable than in-ROM test_inject.h approach
- Enables automated testing of complex interactive applications

**TODO - Migrate test_inject.h Samples**:
- [x] `SimpleMenu` → `tests/drivers/simplemenu_test.c`
- [x] `UpdateStrGad` → `tests/drivers/updatestrgad_test.c`
- [x] `SimpleGTGadget` → `tests/drivers/simplegtgadget_test.c`

**TODO - Create Deep Dive App Test Drivers**:
- [x] `tests/drivers/kickpascal_test.c` - KickPascal 2 automated testing
- [x] `tests/drivers/devpac_test.c` - Devpac/HiSoft assembler testing
- [ ] `tests/drivers/maxonbasic_test.c` - MaxonBASIC IDE testing
- [ ] `tests/drivers/dpaint_test.c` - DPaint V testing
- [x] `tests/drivers/asm_one_test.c` - ASM-One testing

**TODO - Extend liblxa API**:
- [ ] Add `lxa_inject_rmb_click()` for menu access
- [ ] Add `lxa_inject_drag()` for drag operations
- [ ] Add `lxa_get_screen_info()` for screen queries
- [ ] Add `lxa_read_pixel()` for visual verification

**Completed**:
- [x] `simplegad_test.c` - SimpleGad gadget click testing
- [x] `mousetest_test.c` - MouseTest mouse input testing
- [x] `rawkey_test.c` - RawKey keyboard input testing

---

## High Priority Phases

### Phase 58: KickPascal 2 Deep Dive
**Goal**: Full KickPascal 2 IDE functionality with automated testing via host-side driver.
**Status**: ⚠️ UI Issues (Screen clearing, repaint speed, cursor keys).
**Driver**: ✅ `kickpascal_test.c` created and passing

### Phase 59: Devpac (HiSoft) Deep Dive
**Goal**: Verify editor content area and achieve full assembler functionality.
**Status**: ✅ WORKING - Window opens and responds to input.
**Driver**: ✅ `devpac_test.c` created and passing

### Phase 60: MaxonBASIC Deep Dive
**Goal**: Verify and complete full MaxonBASIC IDE functionality.
**Status**: ⚠️ UNKNOWN - Opens Workbench and window.
**TODO**: migrate to host-side driver, run deeper tests

### Phase 61: Oberon 2 Deep Dive
**Goal**: Fix A5 register corruption and enable Oberon system.
**Status**: ⚠️ PARTIAL - Trap handlers implemented.
**TODO**: migrate to host-side driver, run deeper tests

### Phase 62: Cluster2 Deep Dive
**Goal**: Investigate and achieve full compatibility for Cluster2.
**Status**: 🆕 PLANNED
**TODO**: migrate to host-side driver, run deeper tests

---

## Future Phases

### Phase 63: DPaint V Deep Dive
**Goal**: Fix hang during initialization after font loading.
**Status**: ⚠️ PARTIAL - Libraries load, Workbench opens.
**TODO**: migrate to host-side driver, run deeper tests

### Phase 64: Directory Opus 4 Deep Dive
**Goal**: Full Directory Opus 4 compatibility.
**Status**: ⚠️ PARTIAL - Exit code 1 investigation needed.
**TODO**: migrate to host-side driver, run deeper tests

### Phase 65: SysInfo Deep Dive
**Goal**: Verify SysInfo displays complete system information.
**TODO**: migrate to host-side driver, run deeper tests

### Phase 66: EdWord Pro Deep Dive
**Goal**: Verify and complete full word processor functionality.
**TODO**: migrate to host-side driver, run deeper tests

### Phase 67: BeckerText II Deep Dive
**Goal**: Verify full window content renders and text editing.
**TODO**: migrate to host-side driver, run deeper tests

### Phase 68: Remaining RKM Samples
**Goal**: Complete the remaining RKM samples that weren't finished in Phase 56.

**TODO - Graphics Samples**:
- [ ] `SSprite`/`VSprite`/`Bob` (Sprite and animation system).

**TODO - Utility & System Samples**:
- [ ] `ClipFTXT` (Clipboard IFF handling - requires clipboard.device).
- [ ] `Broker`/`HotKey` (Commodities system).
- [ ] `IconExample`/`AppIcon`/`AppWindow` (Workbench integration).
- [ ] `SPIEEE`/`DPIEEE` (IEEE floating point math - requires mathieeesingbas.library).

**TODO - Device Samples**:
- [ ] `ClipDemo`/`ChangeHook_Test` (Clipboard device).
- [ ] `V36_Device_Use` (Modern device I/O pattern).

- [ ] Ensure all samples achieve 100% pass rate in CI.

---

## Postponed Phases
- **Phase xx: Pixel Array Operations**: Optimized pixel array functions.

---

## Version History

| Version | Phase | Key Changes |
| :--- | :--- | :--- |
| 0.6.23 | 57 | Added 9 host-side test drivers: simplegad, mousetest, rawkey, simplegtgadget, updatestrgad, simplemenu, asm_one, devpac, kickpascal. Deep dive app drivers for ASM-One, Devpac, KickPascal 2. |
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
