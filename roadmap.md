# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project!

---

## Current Status

**Version: 0.6.21** | **Phase 56 In Progress** | **41 RKM Sample Tests Passing** | **162 Total Tests Passing**

The lxa project has achieved a comprehensive AmigaOS-compatible environment with 95%+ library compatibility across Exec, DOS, Graphics, Intuition, and system libraries.

**Recent Fixes (Phase 56)**:
- **Fixed OpenScreen ColorMap initialization**: Screens now have a properly initialized ColorMap with standard Workbench colors. Also implemented SetRGB4CM() for setting ColorMap entries.
- **Fixed exception handler hexdump spam**: Debug hexdump in exception handler was using LPRINTF instead of DPRINTF, causing massive output when exceptions occurred during normal operation. Now properly wrapped in #ifdef ENABLE_DEBUG.
- **New ASL Library Samples**:
  - `FileReq` - ASL file requester: AllocAslRequest(ASL_FileRequest), FreeAslRequest with tag configuration
  - `FontReq` - ASL font requester: AllocAslRequest(ASL_FontRequest), FreeAslRequest
- **Implemented MakeScreen/RethinkDisplay/RemakeDisplay**: These Intuition functions now work for double-buffering support (returns success, actual refresh via WaitTOF).
- **Fixed display_close() use-after-free bug**: g_active_display wasn't cleared when display was closed, causing intermittent malloc corruption when VBlank tried to refresh a freed display.
- **Implemented AvailFonts()**: diskfont.library AvailFonts() now returns ROM fonts (topaz.font) instead of empty list.
- **New Screen Management Samples**:
  - `CloneScreen` - Screen cloning with LockPubScreen, GetScreenDrawInfo, GetVPModeID, OpenScreenTags
  - `PublicScreen` - Opening screens with inherited pens from public screens
  - `VisibleWindow` - QueryOverscan() for proper full-screen window placement
  - `WinPubScreen` - Opening windows on default public screen with WA_PubScreen
- **New Graphics/Font Samples**:
  - `AvailFonts` - Font enumeration with AvailFonts(), OpenDiskFont()
  - `MeasureText` - Text measurement with TextLength(), TextExtent(), TextFit()
  - `WBClone` - Low-level graphics: LockPubScreen, GetVPModeID, LockIBase, InitBitMap, AllocRaster, InitRastPort, SetRast, GetRGB4
- **New Input/Cursor Samples**:
  - `CustomPointer` - Custom mouse pointer with SetPointer(), ClearPointer(), Request(), EndRequest()
  - `MouseTest` - Mouse input handling (IDCMP_MOUSEMOVE, IDCMP_MOUSEBUTTONS, DoubleClick) - interactive
  - `RawKey` - Raw keyboard input with RawKeyConvert() - interactive
- **Host-side Test Driver Infrastructure (liblxa)**: Added simplegad_test driver that uses the liblxa API to:
  - Start the SimpleGad sample
  - Wait for window to open
  - Click gadgets programmatically via `lxa_inject_mouse_click()`
  - Verify GADGETDOWN/GADGETUP IDCMP messages appear in output
  - Fixed timing issue where task must reach WaitPort() before events are injected
- **Enhanced Interactive Testing Infrastructure**: Converted passive samples to true interactive tests using `test_inject.h`:
  - `SimpleMenu` - Now performs RMB menu selection and verifies MENUPICK messages
  - `UpdateStrGad` - Now types into string gadgets and tests backspace handling
  - `SimpleGTGadget` - Now clicks GadTools BUTTON_KIND and CYCLE_KIND gadgets
- **Note**: `SimpleGad` is tested via host-side driver (`tests/drivers/simplegad_test`) rather than in-ROM test_inject.h
- **New test_inject.h helpers**: `test_mouse_click()`, `test_menu_select()`, `test_region_changed()`, `test_wait_for_region_content()`, `test_gadget_center()`, `TEST_DRAIN_PORT()` macro
- **Fixed critical Amiga2Date bug**: Day-of-month calculation was off by 1, causing incorrect date conversions. Fixed formula to match Hinnant's civil_from_days algorithm.
- **New Utility Library Samples**:
  - `IStr` - String comparison and case conversion: Stricmp, Strnicmp, ToUpper, ToLower
  - `A2D` - Date conversion functions: Amiga2Date, CheckDate, Date2Amiga, GetSysTime
- **Enhanced test_runner.sh**: Added normalization for date/time values (Seconds=, sec/min/hour, mday/month/year) for portable test output
- **New IFF Parsing Samples**:
  - `Sift` - iffparse.library test: AllocIFF, FreeIFF, OpenIFF, CloseIFF, ParseIFF, CurrentChunk, IDtoStr, InitIFFasDOS
- **New Exec Samples**:
  - `TaskList` - ExecBase task list walking: Disable/Enable, TaskWait/TaskReady lists, FindTask
  - `AllocEntry` - Multi-block memory allocation: AllocEntry, FreeEntry with 4 memory blocks
- **New Expansion Samples**:
  - `FindBoards` - expansion.library test: FindConfigDev enumeration
- **Implemented expansion.library FindConfigDev**: Changed from stub to proper implementation (returns NULL for no boards in emulator)
- **Added test_runner_with_args.sh**: For tests that need command-line arguments
- **Fixed mathffp.library bugs**:
  - `SPSub`: Fixed argument handling - was computing x-y instead of y-x
  - `SPCmp`: Fixed infinite loop caused by trying to preserve CCR across a function call (jsr clobbers CCR). Rewrote using Scc instructions.
  - `SPTst`: Same fix as SPCmp - use direct byte test instead of GetCC()
  - `SPAbs`: Implemented (was a stub) - clears sign bit in FFP format
- **New Math Samples**:
  - `FFPExample` - mathffp.library test: SPFlt, SPFix, SPAdd, SPSub, SPMul, SPDiv, SPCmp, SPNeg, SPAbs, SPTst
- **New GadTools Samples**:
  - `SimpleGTGadget` - CreateContext, CreateGadget (BUTTON_KIND, CHECKBOX_KIND, INTEGER_KIND, CYCLE_KIND), GetVisualInfo, GT_RefreshWindow, GT_SetGadgetAttrs
- **Fixed Cause() A5 register bug**: The software interrupt handler in exec.c had an A5 register conflict. The compiler uses A5 as frame pointer but AmigaOS requires A5 to hold is_Code. Fixed with inline assembly that saves/restores A5 around handler invocation.
- **Added TimerSoftInt to test suite**: Now passing after Cause() fix.
- **New Intuition UI Samples**: 
  - `SimpleImage` - DrawImage() with chip memory image data
  - `ShadowBorder` - DrawBorder() with chained borders, GetScreenDrawInfo()
  - `EasyIntuition` - OpenScreenTags(), OpenWindowTags() with WA_CustomScreen
  - `SimpleMenu` - Menu strip creation, SetMenuStrip(), ItemAddress(), submenus
  - `EasyRequest` - EasyRequestArgs() with variable substitution
- **New Device Samples**:
  - `SimpleTimer` - timer.device TR_ADDREQUEST, TR_GETSYSTIME, time delays
- **New Graphics Samples**:
  - `Clipping` - NewRegion(), OrRectRegion(), InstallClipRegion(), clip regions
- **test_runner.sh**: Added timer value normalization for portable test output

**DPaint V Status**: Libraries load successfully, Workbench screen opens, application initializes without crashes. Currently investigating hang during initialization after font loading.

**Previous Fixes**:
- **Trap handler implementation**: Added trap vector handlers (trap #0 - trap #14) in exceptions.s.
- **Library init calling convention fix**: Per RKRM, library init should receive D0=Library, A0=SegList, A6=SysBase. Fixed typedef and all 22 ROM library InitLib functions.
- **MakeFunctions pointer arithmetic**: Word-offset format requires skipping 2 bytes (not 1).
- **Extended memory map support**: Added support for Slow RAM, Ranger RAM, and Extended ROM.
- **Signal/Wait task scheduling bug**: Fixed critical bug in Schedule() and Signal().
- **DetailPen/BlockPen 0xFF handling**: Fixed mapping of 0xFF pen values to screen defaults.

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

---

## Active Phase

### Phase 56: RKM Samples Integration (Current)
**Goal**: Add a `samples/` directory with RKM-based sample programs. Ensure they are auto-installed in `SYS:Samples` and integrated into the test suite.

**CRITICAL PURPOSE**: These samples are **integration tests first, samples second**. Their primary purpose is to:
1. **Identify gaps and bugs** in lxa's libraries and kernel implementation
2. **Stress test** lxa with diverse real-world usage patterns from official RKM examples
3. **Prevent future bugs** - Every issue found and fixed here is one we won't hit in production applications
4. **Build confidence** - When DPaint, Devpac, etc. work, we know these foundational patterns are solid

**IMPORTANT RULES**:
- When you hit stubs or bugs: **FIX THE BUG IN LXA, DO NOT HACK THE SAMPLE**
- Do not hesitate to **extend and enhance** samples to make them more challenging
- Add **additional test cases** that stress edge conditions
- Each sample should verify correctness, not just "run without crashing"

**TODO**:
- [x] Create `samples/` directory structure.
- [x] Implement `SimpleTask` (Exec task creation).
- [x] Implement `Signals` (Exec signals).
- [x] Implement `Semaphore` (Exec semaphores).
- [x] Implement `Tag1` (Utility tags).
- [x] Implement `OpenWindowTags` (Intuition windowing).
- [x] Update `lxa` bootstrap to auto-install samples to `SYS:Samples` in `~/.lxa/System`.
- [x] Add integration tests that compile and run these samples.
- [x] **Exec Samples**:
    - [x] `Port1`/`Port2` (Message ports and IPC) - **Sample exists but not in automated tests (requires concurrent execution)**.
    - [x] `BuildList` (Exec list management).
    - [x] `Allocate`/`AllocEntry` (Exec memory allocation).
    - [x] `DeviceUse` (Exec device I/O basics).
    - [x] `TimerSoftInt` (Exec software interrupts) - **Sample exists but not in automated tests (complex m68k interrupt calling conventions)**.
- [ ] **Intuition UI Samples**:
    - [x] `SimpleGad` (Basic boolean gadgets with GACT_IMMEDIATE/RELVERIFY).
    - [x] `UpdateStrGad` (String gadget updates, RemoveGList/AddGList/RefreshGList).
    - [x] `IntuiText` (Text rendering with PrintIText, GetScreenDrawInfo).
    - [x] `SimpleImage`/`ShadowBorder` (UI rendering primitives).
    - [x] `EasyIntuition` (Custom screens with OpenScreenTags/OpenWindowTags).
    - [x] `SimpleMenu` (Intuition menu systems with submenus).
    - [x] `CustomPointer` (Pointer with SetPointer/ClearPointer/Request/EndRequest).
    - [ ] `MouseTest`/`RawKey` (Interactive input - requires host-side test drivers).
    - [x] `EasyRequest` (Requesters with variable substitution).
    - [x] `CloneScreen`/`PublicScreen` (Screen cloning and public screen pens).
    - [x] `DoubleBuffer` (Double-buffered screens).
    - [x] `VisibleWindow`/`WinPubScreen` (Window placement and public screens).
- [ ] **Graphics Samples**:
    - [x] `RGBBoxes` (Graphics primitives and views).
    - [x] `Clipping` (Layer management and clipping regions).
    - [x] `AvailFonts`/`MeasureText` (Font handling and text measurement).
    - [ ] `WBClone` (Workbench cloning and primitives).
    - [ ] `SSprite`/`VSprite`/`Bob` (Sprite and animation system).
- [ ] **Advanced UI Samples**:
    - [x] `SimpleGTGadget` (GadTools gadget creation and management).
    - [x] `GadToolsMenu` (GadTools menu creation).
    - [x] `GadToolsGadgets` (GadTools library UI - more gadget types).
    - [x] `FileReq`/`FontReq` (ASL standard requesters).
    - [x] `Talk2Boopsi` (BOOPSI inter-object communication with propgclass/strgclass).
- [ ] **Utility & System Samples**:
    - [x] `Hooks1` (Callback hooks).
    - [x] `IStr` (Internal string handling - Stricmp, Strnicmp, ToUpper, ToLower).
    - [x] `Uptime` (System uptime calculation).
    - [x] `Sift` (IFF file structure viewer with iffparse.library).
    - [ ] `ClipFTXT` (Clipboard IFF handling - requires clipboard.device).
    - [ ] `Broker`/`HotKey` (Commodities system).
    - [ ] `IconExample`/`AppIcon`/`AppWindow` (Workbench integration).
    - [x] `FFPExample` (Fast Floating Point math - SPFlt, SPFix, SPAdd, SPSub, SPMul, SPDiv, SPCmp, SPNeg, SPAbs, SPTst).
    - [ ] `SPIEEE`/`DPIEEE` (IEEE floating point math - requires mathieeesingbas.library).
    - [x] `FindBoards` (expansion.library board enumeration).
    - [x] `TaskList` (ExecBase task list enumeration with Disable/Enable).
    - [x] `AllocEntry` (Multi-block memory allocation with AllocEntry/FreeEntry).
- [ ] **Device Samples**:
    - [ ] `Simple_Timer`/`Get_Systime` (Timer device).
    - [ ] `ClipDemo`/`ChangeHook_Test` (Clipboard device).
    - [ ] `AskKeymap` (Console/Keymap interaction).
    - [ ] `V36_Device_Use` (Modern device I/O pattern).
- [ ] Ensure all samples achieve 100% pass rate in CI.

---

## High Priority Phases

### Phase 57: Host-Side Test Driver Migration
**Goal**: Migrate all interactive UI tests and deep dive app tests to use the liblxa host-side driver infrastructure.

**Why This Matters**:
- Host-side drivers provide full control over timing and event injection
- Better debugging capabilities (host-side breakpoints)
- More reliable than in-ROM test_inject.h approach
- Enables automated testing of complex interactive applications

**TODO - Migrate test_inject.h Samples**:
- [ ] `SimpleMenu` → `tests/drivers/simplemenu_test.c`
- [ ] `UpdateStrGad` → `tests/drivers/updatestrgad_test.c`
- [ ] `SimpleGTGadget` → `tests/drivers/simplegtgadget_test.c`

**TODO - Create Deep Dive App Test Drivers**:
- [ ] `tests/drivers/kickpascal_test.c` - KickPascal 2 automated testing
- [ ] `tests/drivers/devpac_test.c` - Devpac/HiSoft assembler testing
- [ ] `tests/drivers/maxonbasic_test.c` - MaxonBASIC IDE testing
- [ ] `tests/drivers/dpaint_test.c` - DPaint V testing
- [ ] `tests/drivers/asm_one_test.c` - ASM-One testing

**TODO - Extend liblxa API**:
- [ ] Add `lxa_inject_rmb_click()` for menu access
- [ ] Add `lxa_inject_drag()` for drag operations
- [ ] Add `lxa_get_screen_info()` for screen queries
- [ ] Add `lxa_read_pixel()` for visual verification

**Completed**:
- [x] `simplegad_test.c` - SimpleGad gadget click testing

### Phase 58: KickPascal 2 Deep Dive
**Goal**: Full KickPascal 2 IDE functionality with automated testing via host-side driver.
**Status**: ⚠️ UI Issues (Screen clearing, repaint speed, cursor keys).

### Phase 59: Devpac (HiSoft) Deep Dive
**Goal**: Verify editor content area and achieve full assembler functionality.
**Status**: ✅ WORKING - Window opens and responds to input.

### Phase 60: MaxonBASIC Deep Dive
**Goal**: Verify and complete full MaxonBASIC IDE functionality.
**Status**: ⚠️ UNKNOWN - Opens Workbench and window.

### Phase 61: Oberon 2 Deep Dive
**Goal**: Fix A5 register corruption and enable Oberon system.
**Status**: ⚠️ PARTIAL - Trap handlers implemented.

### Phase 62: Cluster2 Deep Dive
**Goal**: Investigate and achieve full compatibility for Cluster2.
**Status**: 🆕 PLANNED

---

## Future Phases

### Phase 63: DPaint V Deep Dive
**Goal**: Fix hang during initialization after font loading.
**Status**: ⚠️ PARTIAL - Libraries load, Workbench opens.

### Phase 64: Directory Opus 4 Deep Dive
**Goal**: Full Directory Opus 4 compatibility.
**Status**: ⚠️ PARTIAL - Exit code 1 investigation needed.

### Phase 65: SysInfo Deep Dive
**Goal**: Verify SysInfo displays complete system information.

### Phase 66: EdWord Pro Deep Dive
**Goal**: Verify and complete full word processor functionality.

### Phase 67: BeckerText II Deep Dive
**Goal**: Verify full window content renders and text editing.

---

## Postponed Phases
- **Phase xx: Pixel Array Operations**: Optimized pixel array functions.

---

## Version History

| Version | Phase | Key Changes |
| :--- | :--- | :--- |
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
