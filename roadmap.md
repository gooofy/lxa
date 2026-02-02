# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md` ! Test coverage, roadmap updates and documentations are **mandatory** in this project!

---

## Core Philosophy & Boundaries

### WINE-like DOS Approach
Instead of emulating hardware-level disk controllers and running Amiga-native filesystems (like OFS/FFS) on top, `lxa` maps Amiga logical drives directly to Linux directories.

- **Drives as Directories**: `DH0:`, `DF0:`, etc., are backed by standard Linux directories.
- **Host Interception**: DOS calls (`Open`, `Read`, `Lock`, etc.) are handled by the host `lxa` process using Linux system calls.
- **Auto-Provisioning**: On first run, `lxa` creates a default `~/.lxa` structure with `SYS:`, `S:`, `C:`, etc., and populates them with a basic `Startup-Sequence` and essential tools.

### Support Boundaries

| Feature | Status/Goal | Note |
| :--- | :--- | :--- |
| **Exec Multitasking** | Full Support | Signal-based multitasking, Message Ports, Semaphores. |
| **DOS Filesystem** | Hosted | Mapped to Linux via VFS layer. |
| **Interactive Shell** | Full Support | Amiga-compatible Shell with prompt, history, and scripting. |
| **Assigns** | Full Support | Logical assigns, multi-directory assigns (Paths). |
| **Floppy Emulation** | Directory-based | Floppies appear as directories. |

---

## Completed Phases

### Phase 1-7: Foundation (COMPLETE)
Core system established: Exec multitasking (signals, ports, processes), VFS with case-insensitive mapping, filesystem API (Lock/Examine), ReadArgs parsing, interactive Shell with scripting, CMake build system, 50Hz preemptive scheduler, system commands (STATUS, RUN, BREAK, ASSIGN), file commands with wildcards and Ctrl+C support.

### Phase 8: Test Coverage & Quality Assurance (COMPLETE)
100+ integration tests across DOS/Exec libraries, commands, shell, and host emulation. Stress tests for memory, tasks, and filesystem operations. Fixed critical bugs: ROM static data, use-after-free, stack overflow, Execute() crash.

### Phase 9: New Commands & Utilities (COMPLETE)
File commands (COPY, RENAME, JOIN, PROTECT, FILENOTE, LIST, SEARCH, SORT), system commands (VERSION, WAIT, INFO, DATE, MAKELINK, EVAL), environment management (SET/GET/UNSET ENV/ENVARC), DOS variable functions.

### Phase 10: DOS Library Completion (COMPLETE)
~80% compatibility: Path utilities, character I/O, formatted output, I/O redirection, file handle utilities, break handling, pattern matching, date/time, error handling (Fault/PrintFault), DosList access. Default assigns auto-created.

### Phase 11: Exec Library Completion (COMPLETE)
~95% compatibility: List operations, memory functions, memory pools, RawDoFmt, async I/O (SendIO/WaitIO/CheckIO/AbortIO), software interrupts, shared/named semaphores.

### Phase 12: Utility Library Completion (COMPLETE)
~90% compatibility: String comparison, date conversion, TagItem management, hooks, 64-bit math.

### Phase 13: Graphics Foundation (COMPLETE)
SDL2 integration, graphics.library with BitMap/RastPort, basic rendering (pixel ops, lines, rectangles, pens), screen management (OpenScreen/CloseScreen), comprehensive test suite. See `doc/graphics_testing.md`.

### Phase 14: Intuition Basics (COMPLETE)
Window management (OpenWindow/CloseWindow), IDCMP input handling, mouse tracking, screen linking. **Deferred**: System gadgets (Close/Depth/Drag).

### Phase 15: Rootless Mode & Advanced Graphics (COMPLETE)
Rootless windowing (each Window as separate SDL window), blitter operations (BltBitMap with minterm logic), text rendering (OpenFont/Text with Topaz-8), AutoRequest/EasyRequest. **Deferred**: Menu system (SetMenuStrip, IDCMP_MENUPICK).

### Phase 16: Console Device Enhancement (COMPLETE)
Full ANSI/CSI escape sequence support: cursor control, line/screen erase, text attributes (SGR, colors), cursor visibility, control characters, comprehensive test coverage.

### Phase 17: Additional Commands (COMPLETE)
System commands (AVAIL, RESIDENT), script support (ASK, REQUESTCHOICE), shell enhancements (CD, WHICH, PATH).

### Phase 18: App-DB & KickPascal 2 Testing (COMPLETE)
App manifest system, library loading from disk (OpenLibrary scans LIBS:), RomTag support (FindResident/InitResident), test runner infrastructure, app.json format.

### Phase 19: Graphics & Layers Completion (COMPLETE)
Full layers.library implementation (Layer creation/deletion, locking, clipping, damage tracking, ClipRect splitting), region functions (OrRectRegion, AndRegionRegion, etc.), window invalidation (BeginRefresh/EndRefresh).

### Phase 20: KickPascal 2 Compatibility - Critical Fixes (COMPLETE)
Multi-assign support in VFS, automatic program-local assigns, WBenchToFront opens Workbench, console device window attachment (text rendering, CSI parsing, scrolling), console input (rawkey-to-ASCII, ring buffer, IDCMP integration, character echo, cursor display). **Deferred**: Window layer clipping enforcement.

### Phase 21: UI Testing Infrastructure (COMPLETE)
Input injection emucalls (INJECT_KEY, INJECT_STRING, INJECT_MOUSE), screen capture (CAPTURE_SCREEN, CAPTURE_WINDOW), headless mode, ROM-side test helpers (`test_inject.h`), console device unit tests, KickPascal 2 investigation and fix (line mode buffer handling).

### Phase 22: Console Device Completion (COMPLETE)
**Goal**: Achieve authentic AmigaOS console.device behavior through rigorous comparison with AROS implementation and RKRM documentation.
**Summary**: Fully functional console.device with comprehensive CSI escape sequence support, SGR text attributes, input handling, and CON:/RAW: DOS handlers.

### Phase 23: Graphics Library Consolidation (COMPLETE)
Elevated `graphics.library` to authentic AmigaOS standards: BitMap/RastPort/View/ViewPort structures aligned with NDK, LVO function semantics verified, GfxBase fields populated correctly (DefaultFont), InitRastPort/InitBitMap fixes per AROS.

### Phase 24: Layers Library Consolidation (COMPLETE)
Accurate AmigaOS clipping, locking, and damage semantics: LockLayer/LockLayerInfo/LockLayers match RKRM, region algebra (OrRectRegion, AndRectRegion, ClearRegion), ClipRect rebuilding for layer create/move/size operations.

### Phase 25: Intuition Library Consolidation (COMPLETE)
Authentic AmigaOS windowing, screen, and input behavior: Window/Screen/Gadget structures aligned with NDK, IDCMP message generation with real timestamps (CurrentTime), OpenScreenTagList/OpenWindowTagList with full tag parsing, system gadgets (Close/Depth) with hit testing and IDCMP messages, menu system (SetMenuStrip, menu bar rendering, IDCMP_MENUPICK with FULLMENUNUM encoding), window layer clipping (pixel-level bounds checking, ClipRect iteration).

---

## Active Phases

## Phase 26: KickPascal 2 & Real-World Application Testing (was Phase 23)

**Goal**: Validate lxa compatibility with real Amiga applications, using KickPascal 2 as the first test case.

**IMPORTANT**: **all** testing needs to be fully automated, **no** manual testing required. If neccessary, expand the emulator capabilities so automated tests can generate UI events and observe application behavior (system calls made, file accesses, observe UI output, etc.)

### Step 26.1: KickPascal 2 Full Compatibility
- [x] **Binary Loading** - KP2 executable loads and validates successfully
- [x] **IDE Launch** - KP2 launches as background process, window opens
- [ ] **File Operations** - Open, Save, New work with KP2 format
- [ ] **Text Editing** - All editor features work (cursor, selection, scroll)
- [ ] **Compilation** - KP2 can compile Pascal programs on lxa
- [ ] **Documentation** - Document any KP2-specific workarounds needed

### Step 26.2: Application Test Suite
- [x] **Create tests/apps/ directory** - Structured testing for real applications
- [x] **Test runner infrastructure** - app_test_runner.sh with APPS: assign setup
- [ ] **App manifest format** - Define expected behavior per application

### Step 26.3: Additional Test Applications
- [ ] **Text editors** - Test ED, other common editors
- [ ] **File managers** - Directory Opus (if feasible), DiskMaster
- [ ] **Development tools** - Assemblers, compilers that run on Amiga
- [ ] **Utilities** - Common Amiga utilities and tools

### Step 26.4: Compatibility Database
- [ ] **Track application status** - Working, partial, broken
- [ ] **Document failures** - What breaks and why
- [ ] **Prioritize fixes** - Based on application popularity

### Step 26.5: Multitasking Improvements (Blocker for Full App Testing)
- [x] **Preemptive scheduling** - VBlank-driven task switching works correctly
- [ ] **Non-blocking Delay()** - Current Delay() works but is not time-accurate
- [ ] **Background task execution** - RUN command works, but main task must not exit early
- [ ] **Test harness task communication** - Allow test code to interact with running applications

**Note**: Preemptive multitasking is working (verified with exec/multitask test). The remaining issue is that when the main process exits, background tasks are not properly terminated, causing lxa to hang.

---

## Phase 27: Core System Libraries & Preferences (was Phase 25)

**Goal**: Improve visual fidelity, system configuration, and core library support.

### Step 27.1: Standard Libraries
- [x] **diskfont.library** - OpenDiskFont (falls back to ROM fonts), AvailFonts stub.
- [x] **icon.library** - Fixed LVO table alignment, full function set implemented.
- [x] **expansion.library** - ConfigChain and board enumeration stubs (existing).
- [x] **translator.library** - Translate() stub (copies input to output).
- [x] **locale.library** - Character classification (IsAlpha, IsDigit, etc.), ConvToLower/ConvToUpper, GetLocaleStr, OpenLocale with default English locale.
- [ ] **FONTS: Assign** - Automatic provisioning of font directories.
- [ ] **Topaz Bundling** - Ensure a high-quality Topaz-8/9 replacement is available.

### Step 27.2: Preferences & Environment
- [ ] **ENVARC: Persistence** - Map `ENVARC:` to `~/.lxa/Prefs/Env` with persistence.

---

## Phase 28: Advanced UI Frameworks (was Phase 26)

**Goal**: Implement standard Amiga UI widgets, object-oriented systems, and advanced console features.

### Step 28.1: GadTools & ASL
- [ ] **gadtools.library** - Standard UI widgets (buttons, sliders, lists).
- [ ] **asl.library** - File and Font requesters (bridged to host or native).

### Step 28.2: BOOPSI Foundation
- [ ] **Intuition Classes** - `imageclass`, `gadgetclass`, `rootclass`.
- [ ] **NewObject / DisposeObject** - Object lifecycle management.

### Step 28.3: Advanced Console Features (deferred from Phase 22)
- [ ] **CONU_CHARMAP/CONU_SNIPMAP** - Character-mapped console modes with copy/paste
- [ ] **RawKeyConvert()** - Convert IECLASS_RAWKEY events to ANSI bytes
- [ ] **CDInputHandler()** - Console device input event handler
- [ ] **Raw Input Events** - CSI { } sequences for raw keyboard/mouse events
- [ ] **Global Background Color** - SGR >0-7 parameter (V36+)
- [ ] **Ctrl/Alt Qualifiers** - Full qualifier processing for special key combos
- [ ] **Advanced ConUnit Fields** - cu_KeyMapStruct, cu_RawEvents, cu_XMinShrink/cu_YMinShrink
- [ ] **Integration Tests** - Full console session simulation
- [ ] **RKRM Sample Compatibility** - Console.c example automated testing

---

## Phase 29: Quality & Stability Hardening (was Phase 27)

**Goal**: Ensure production-grade reliability.

### Step 29.1: Memory Debugging
- [ ] Memory tracking, leak detection, double-free detection, buffer overflow guards
- [ ] **Stress Testing** - 24-hour continuous operation, 10,000 file operations.

### Step 29.2: Compatibility Testing
- [ ] Test with common Amiga software, document issues, create compatibility database.

### Step 29.3: Performance Optimization
- [ ] Profile critical paths, optimize VFS, memory allocation, emucall overhead.

## Known Limitations & Future Work

### Shell Features Not Implemented
- I/O redirection (>, >>, <) - commands use internal TO/FROM arguments
- Pipe support (`cmd1 | cmd2`)
- Variable substitution in commands
- SKIP/LAB (goto labels), WHILE loops

### DOS Functions Stubbed
- GetDeviceProc/FreeDeviceProc - handler-based architecture not implemented
- MakeLink - filesystem links not supported

### Notes
- Stack size hardcoded to 4096 in SystemTagList (NP_StackSize tag supported)
- Environment variable inheritance not implemented for child processes
- DosList functions simplified - assigns enumerated via host VFS

---

## Implementation Rules
1. **Clean Room Approach**: AROS/Kickstart source is for architectural reference ONLY.
2. **Host-First**: Prefer host-side (`emucall`) implementations for filesystem tasks.
3. **Amiga Authenticity**: Commands must use AmigaDOS argument syntax (templates, keywords, switches).
4. **Tooling**: Use `ReadArgs()` template parsing in all `C:` commands.
5. **Test Everything**: Every function, every command, every edge case. 100% coverage is mandatory.
