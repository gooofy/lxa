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

---

## Active Phases

## Phase 22: Console Device Completion (IN PROGRESS)

**Goal**: Achieve authentic AmigaOS console.device behavior through rigorous comparison with AROS implementation and RKRM documentation.

### Mandatory Reference Sources (MUST be consulted for ALL console.device work):
1. **AROS Source**: `~/projects/amiga/lxa/src/lxa/others/AROS-20231016-source` - Reference implementation
2. **RKRM Documentation**: `~/projects/amiga/rkrm/rkrm_console_device.md` - Official behavior specification
3. **RKRM Sample Code**: `~/projects/amiga/sample-code/rkm/console/` - Official example usage patterns

### Step 22.1: Console Device Line Mode Fix (COMPLETE)
- [x] **Created kp2_test** - Mimics KP2's 1-byte-at-a-time read pattern
- [x] **Fixed lxa_dev_console.c** - Only wait if no complete line in buffer
- [x] **Verified fix** - kp2_test passes, reads buffered characters immediately

### Step 22.2: AROS Comparison & Gap Analysis (COMPLETE)
**Compared lxa console.device against AROS implementation and implemented missing features.**

#### 22.2.1: Console Units & Opening
- [ ] **CONU_LIBRARY (-1)** - Library-only mode without window (for RawKeyConvert)
- [x] **CONU_STANDARD (0)** - Standard console with SMART_REFRESH window
- [ ] **CONU_CHARMAP (1)** - Character-mapped console with SIMPLE_REFRESH
- [ ] **CONU_SNIPMAP (3)** - Character-mapped with copy/paste support
- [ ] **CONFLAG_DEFAULT / CONFLAG_NODRAW_ON_NEWSIZE** - Redraw behavior flags

#### 22.2.2: Console Device Commands (RKRM-defined)
- [x] **CMD_READ** - Read from console input stream
- [x] **CMD_WRITE** - Write to console output (with -1 length for null-terminated)
- [x] **CMD_CLEAR** - Clear input buffer
- [x] **CD_ASKKEYMAP** - Get console's current keymap (stub - returns success)
- [x] **CD_SETKEYMAP** - Set console's keymap (stub - accepts and ignores)
- [x] **CD_ASKDEFAULTKEYMAP** - Get system default keymap (stub)
- [x] **CD_SETDEFAULTKEYMAP** - Set system default keymap (stub)

#### 22.2.3: Console Device Functions
- [ ] **RawKeyConvert()** - Convert IECLASS_RAWKEY events to ANSI bytes
- [ ] **CDInputHandler()** - Handle input events for console device

#### 22.2.4: Control Characters (ANSI/ISO standard)
- [x] **BELL (0x07)** - DisplayBeep (stub implemented)
- [x] **BACKSPACE (0x08)** - Move cursor left one column
- [x] **TAB (0x09)** - Move to next tab stop
- [x] **LINEFEED (0x0A)** - Move down (with LNM mode for CR+LF)
- [x] **VERTICAL TAB (0x0B)** - Move up one line
- [x] **FORMFEED (0x0C)** - Clear window
- [x] **CARRIAGE RETURN (0x0D)** - Move to column 1
- [x] **SHIFT OUT (0x0E)** - Set MSB before displaying
- [x] **SHIFT IN (0x0F)** - Undo SHIFT OUT
- [x] **ESC (0x1B)** - Start ESC sequence
- [x] **INDEX (0x84)** - Move active position down
- [x] **NEXT LINE (0x85)** - Begin next line (CR+LF)
- [x] **HORIZONTAL TAB SET (0x88)** - Set tab at cursor
- [x] **REVERSE INDEX (0x8D)** - Move active position up
- [x] **CSI (0x9B)** - Control sequence introducer

#### 22.2.5: CSI Escape Sequences (ANSI Standard)
- [x] **CSI n @ (0x40)** - Insert N characters
- [x] **CSI n A (0x41)** - Cursor up N positions
- [x] **CSI n B (0x42)** - Cursor down N positions
- [x] **CSI n C (0x43)** - Cursor forward N positions
- [x] **CSI n D (0x44)** - Cursor backward N positions
- [x] **CSI n E (0x45)** - Cursor to next line (column 1)
- [x] **CSI n F (0x46)** - Cursor to previous line (column 1)
- [x] **CSI n G** - Cursor horizontal absolute (column N)
- [x] **CSI r;c H (0x48)** - Cursor position (row;column)
- [x] **CSI n I (0x49)** - Cursor horizontal tab (forward N tabs)
- [x] **CSI J (0x4A)** - Erase in display (all 3 modes: 0=to end, 1=to start, 2=all)
- [x] **CSI K (0x4B)** - Erase in line (all 3 modes: 0=to end, 1=to start, 2=all)
- [x] **CSI L (0x4C)** - Insert line
- [x] **CSI M (0x4D)** - Delete line
- [x] **CSI n P (0x50)** - Delete N characters
- [x] **CSI n S (0x53)** - Scroll up N lines
- [x] **CSI n T (0x54)** - Scroll down N lines
- [x] **CSI n W (0x57)** - Cursor tab control (set/clear tabs)
- [x] **CSI n Z (0x5A)** - Cursor backward tab
- [x] **CSI n;n m (0x6D)** - Select graphic rendition (SGR)
- [x] **CSI 6 n (0x6E)** - Device status report (cursor position)
- [x] **CSI 20 h (0x68)** - Set LF mode (LF = CR+LF)
- [x] **CSI 20 l (0x6C)** - Reset LF mode (LF = LF only)
- [x] **CSI n f (0x66)** - Horizontal/vertical position (same as H)

#### 22.2.6: Amiga-Specific CSI Sequences
- [x] **CSI >1 h** - Enable auto-scroll mode
- [x] **CSI >1 l** - Disable auto-scroll mode
- [x] **CSI ?7 h** - Enable auto-wrap mode
- [x] **CSI ?7 l** - Disable auto-wrap mode
- [x] **CSI n t** - Set page length (stub - uses window size)
- [x] **CSI n u** - Set line length (stub - uses window size)
- [x] **CSI n x** - Set left offset (stub - ignored)
- [x] **CSI n y** - Set top offset (stub - ignored)
- [ ] **CSI events {** - Set raw input events
- [ ] **CSI events }** - Reset raw input events
- [x] **CSI n p (0x70)** - Cursor visibility (0=hide, 1=show)
- [x] **CSI 0 q (0x71)** - Window status request (returns bounds)
- [ ] **CSI ... r (0x72)** - Window bounds report (response)
- [ ] **CSI params | (0x7C)** - Input event report

#### 22.2.7: SGR (Select Graphic Rendition) Parameters
- [x] **0** - Reset to normal
- [x] **1** - Bold (brightens color)
- [x] **2** - Faint (dim)
- [x] **3** - Italic (tracked, display as normal)
- [x] **4** - Underline (tracked, display as normal)
- [x] **7** - Inverse (swap fg/bg)
- [x] **8** - Concealed (tracked, display as normal)
- [x] **21** - Not bold (double underline alias)
- [x] **22** - Not bold/faint
- [x] **23** - Not italic
- [x] **24** - Not underlined
- [x] **27** - Not inverse
- [x] **28** - Not concealed
- [x] **30-37** - Foreground colors 0-7
- [x] **39** - Default foreground
- [x] **40-47** - Background colors 0-7
- [x] **49** - Default background
- [ ] **>0-7** - Global background color (V36+)

#### 22.2.8: Raw Input Events
- [ ] **Event type 1** - Raw keyboard input
- [ ] **Event type 2** - Raw mouse input
- [ ] **Event type 4** - Pointer position
- [ ] **Event type 6** - Timer
- [ ] **Event type 7** - Gadget pressed
- [ ] **Event type 8** - Gadget released
- [ ] **Event type 11** - Close gadget
- [ ] **Event type 12** - Window resized
- [ ] **Event type 13** - Window refreshed
- [ ] **Event type 17** - Active window
- [ ] **Event type 18** - Inactive window

#### 22.2.9: ConUnit Structure Fields (from AROS conunit.h)
- [x] **cu_Window** - Associated window pointer
- [x] **cu_XCP/cu_YCP** - Cursor character position
- [x] **cu_XMax/cu_YMax** - Maximum character positions
- [x] **cu_XRSize/cu_YRSize** - Character cell size in pixels
- [x] **cu_XROrigin/cu_YROrigin** - Raster origin
- [x] **cu_XRExtant/cu_YRExtant** - Raster extent
- [ ] **cu_XMinShrink/cu_YMinShrink** - Minimum shrink values
- [x] **cu_XCCP/cu_YCCP** - Current cursor character position
- [ ] **cu_KeyMapStruct** - Console's keymap
- [x] **cu_TabStops[80]** - Tab stop positions
- [x] **cu_FgPen/cu_BgPen** - Foreground/background colors
- [x] **cu_DrawMode** - Drawing mode (JAM1, JAM2, etc.)
- [x] **cu_TxFlags** - Text style flags (bold, italic, underline) - tracked in LxaConUnit
- [x] **cu_Modes** - Mode flags (LNM, ASM, AWM) - tracked in LxaConUnit
- [ ] **cu_RawEvents** - Raw event mask

#### 22.2.10: Graphics Library Updates
- [x] **ScrollRaster()** - Implemented using BltBitMap and RectFill

### Step 22.3: Input Handling Improvements (MOSTLY COMPLETE)
- [ ] **Raw Mode (vs Cooked/Line Mode)** - Character-at-a-time input (deferred to 22.4)
- [x] **Proper backspace handling** - Cannot backspace past input start
- [x] **Delete key handling** - Delete character (generates DEL 0x7F)
- [x] **Arrow key sequences** - Generate proper CSI sequences for arrows (Up/Down/Left/Right with Shift support)
- [x] **Function key sequences** - F1-F10, F11-F12 generate CSI sequences (shifted/unshifted)
- [x] **Special key handling** - Help, Insert, Page Up/Down, Home/End (all with Shift variants)
- [ ] **Qualifier handling** - Ctrl, Alt modifier processing (Shift done, others deferred)

### Step 22.4: CON: Handler Layer (COMPLETE)
- [x] **Evaluate need** - Apps using Open("CON:...") need full CON: handler support
- [x] **CON: path parsing** - Parse CON:x/y/w/h/title/options syntax
- [x] **Window creation** - Create Intuition window on Open("CON:...")
- [x] **Console attachment** - Attach console.device to the window
- [x] **Read/Write routing** - Route DOS Read/Write to console.device
- [x] **Close cleanup** - Close console.device and window on Close()
- [x] **RAW: support** - RAW: windows also handled (raw mode flag tracked)
- [x] **Test coverage** - con_handler test verifies basic functionality

### Step 22.5: Test Coverage (NOT STARTED)
**100% test coverage for all console.device functionality:**
- [ ] **Unit tests** - Test each CSI sequence in isolation
- [ ] **Input tests** - Test keyboard input handling
- [ ] **Output tests** - Test text rendering and cursor movement
- [ ] **Edge case tests** - Cursor at boundaries, buffer overflow, etc.
- [ ] **Integration tests** - Full console session simulation
- [ ] **RKRM sample compatibility** - Console.c example must work perfectly

### Step 22.6: Documentation (NOT STARTED)
- [ ] **Update doc/console_device.md** - Full API documentation
- [ ] **Compatibility notes** - Document known differences from real Amiga

---

## Future Phases

## Phase 23: KickPascal 2 & Real-World Application Testing

**Goal**: Validate lxa compatibility with real Amiga applications, using KickPascal 2 as the primary test case.

### Step 23.1: KickPascal 2 Full Compatibility
- [ ] **Complete IDE Launch** - Editor window opens correctly with menus
- [ ] **File Operations** - Open, Save, New work with KP2 format
- [ ] **Text Editing** - All editor features work (cursor, selection, scroll)
- [ ] **Compilation** - KP2 can compile Pascal programs on lxa
- [ ] **Documentation** - Document any KP2-specific workarounds needed

### Step 23.2: Application Test Suite
- [ ] **Create app-tests/ directory** - Structured testing for real applications
- [ ] **Test runner infrastructure** - Automated launch, screenshot, validation
- [ ] **App manifest format** - Define expected behavior per application

### Step 23.3: Additional Test Applications
- [ ] **Text editors** - Test ED, other common editors
- [ ] **File managers** - Directory Opus (if feasible), DiskMaster
- [ ] **Development tools** - Assemblers, compilers that run on Amiga
- [ ] **Utilities** - Common Amiga utilities and tools

### Step 23.4: Compatibility Database
- [ ] **Track application status** - Working, partial, broken
- [ ] **Document failures** - What breaks and why
- [ ] **Prioritize fixes** - Based on application popularity

---

## Phase 24: Deferred UI Features

**Goal**: Complete deferred UI components from earlier phases.

### Step 24.1: System Gadgets (from Phase 14.3)
- [ ] **Visual Representation** - Close/Depth/Drag gadget rendering
- [ ] **Gadget Interaction** - Hit testing and basic state changes

### Step 24.2: Menu System (from Phase 15.5)
- [ ] **Menu Bar Handling** - Global vs per-window menus
- [ ] **SetMenuStrip / ClearMenuStrip** - Menu management functions
- [ ] **Menu Events** - IDCMP_MENUPICK handling and ItemAddress

### Step 24.3: Window Layer Clipping (from Phase 20.3)
- [ ] **Bounds Checking** - Implement pixel-level clipping in WritePixel, Draw, Text, etc.
- [ ] **Layer ClipRect Usage** - Use layer's ClipRect list for proper clipping (already built by layers.library)
- [ ] **Test** - Verify out-of-bounds drawing is properly clipped

---

## Phase 25: Core System Libraries & Preferences

**Goal**: Improve visual fidelity, system configuration, and core library support.

### Step 25.1: Standard Libraries
- [x] **diskfont.library** - OpenDiskFont (falls back to ROM fonts), AvailFonts stub.
- [x] **icon.library** - Fixed LVO table alignment, full function set implemented.
- [x] **expansion.library** - ConfigChain and board enumeration stubs (existing).
- [x] **translator.library** - Translate() stub (copies input to output).
- [x] **locale.library** - Character classification (IsAlpha, IsDigit, etc.), ConvToLower/ConvToUpper, GetLocaleStr, OpenLocale with default English locale.
- [ ] **FONTS: Assign** - Automatic provisioning of font directories.
- [ ] **Topaz Bundling** - Ensure a high-quality Topaz-8/9 replacement is available.

### Step 25.2: Preferences & Environment
- [ ] **ENVARC: Persistence** - Map `ENVARC:` to `~/.lxa/Prefs/Env` with persistence.

---

## Phase 26: Advanced UI Frameworks

**Goal**: Implement standard Amiga UI widgets and object-oriented systems.

### Step 26.1: GadTools & ASL
- [ ] **gadtools.library** - Standard UI widgets (buttons, sliders, lists).
- [ ] **asl.library** - File and Font requesters (bridged to host or native).

### Step 26.2: BOOPSI Foundation
- [ ] **Intuition Classes** - `imageclass`, `gadgetclass`, `rootclass`.
- [ ] **NewObject / DisposeObject** - Object lifecycle management.

---

## Phase 27: Quality & Stability Hardening

**Goal**: Ensure production-grade reliability.

### Step 27.1: Memory Debugging
- [ ] Memory tracking, leak detection, double-free detection, buffer overflow guards
- [ ] **Stress Testing** - 24-hour continuous operation, 10,000 file operations.

### Step 27.2: Compatibility Testing
- [ ] Test with common Amiga software, document issues, create compatibility database.

### Step 27.3: Performance Optimization
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
