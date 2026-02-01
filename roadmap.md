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
- **Phase 1**: Exec Core & Tasking - Signals, Message Ports, Processes, Stack Management
- **Phase 2**: Configuration & VFS - Host config, case-insensitive filesystem mapping
- **Phase 3**: First-Run & Filesystem API - Auto-provisioning, Locks, Examination API
- **Phase 4**: Basic Userland - ReadArgs, Pattern Matching, initial C: tools
- **Phase 5**: Interactive Shell - Shell with scripts and control flow
- **Phase 6**: CMake Build System - Cross-compilation toolchain
- **Phase 6.5**: Interrupt System - 50Hz timer-driven preemptive scheduler
- **Phase 7**: System Management - STATUS, RUN, BREAK, ASSIGN commands
- **Phase 7.5**: Command Completeness - DIR, TYPE, DELETE wildcards and Ctrl+C

### Phase 8: Test Coverage & Quality Assurance (COMPLETE)
Comprehensive test infrastructure with 100+ integration tests covering:
- **DOS Library**: Lock/Unlock, Examine/ExNext, File I/O, Seek, Delete/Rename, CreateDir, Pattern Matching, ReadArgs, SystemTagList, Execute
- **Exec Library**: Tasks, Signals, Memory, Semaphores, Message Ports, Lists
- **Commands**: DIR, DELETE, TYPE, MAKEDIR, ASSIGN, STATUS, BREAK, RUN
- **Shell**: Control flow, aliases, scripts, variables
- **Host/Emulation**: Memory access, VFS layer, emucalls, configuration
- **Stress Tests**: Memory (1000s of allocations), Tasks (50+), Filesystem (200+ files)

Key bug fixes: ROM static data, SystemTagList use-after-free, ReadArgs stack overflow, Execute() crash

### Phase 9: New Commands & Utilities (COMPLETE)
Full command set implemented:
- **File Commands**: COPY, RENAME, JOIN, PROTECT, FILENOTE, LIST, SEARCH, SORT
- **System Commands**: VERSION, WAIT, INFO, DATE, MAKELINK, EVAL
- **Environment**: SET, SETENV, GETENV, UNSET, UNSETENV
- **DOS Functions**: SetVar, GetVar, DeleteVar, FindVar

### Phase 10: DOS Library Completion (COMPLETE)
~80% AmigaOS DOS compatibility achieved:
- **Steps 10.1-10.4**: Path utilities (FilePart, PathPart, AddPart), Character I/O (FGetC, FPutC, UnGetC, FGets, FPuts, PutStr, WriteChars), Formatted output (VFPrintf, VPrintf), I/O redirection (SelectInput, SelectOutput)
- **Step 10.5**: File handle utilities (DupLockFromFH, ExamineFH, NameFromFH, ParentOfFH, OpenFromLock)
- **Step 10.6**: Break handling (CheckSignal, WaitForChar)
- **Step 10.7**: Pattern matching (MatchFirst/MatchNext/MatchEnd, MatchPatternNoCase, ParsePatternNoCase)
- **Step 10.8**: Date/Time (DateToStr, StrToDate, CompareDates)
- **Step 10.9**: Error handling (Fault, PrintFault with full error message table)
- **Step 10.10**: DosList access (LockDosList, UnLockDosList, FindDosEntry, NextDosEntry, IsFileSystem)
- **Default assigns**: C:, S:, L:, LIBS:, DEVS:, T:, ENV:, ENVARC: auto-created on startup

### Phase 11: Exec Library Completion (COMPLETE)
~95% AmigaOS Exec compatibility achieved:
- **Steps 11.1-11.4**: List ops (RemTail), Memory (TypeOfMem, CopyMemQuick), Pools (CreatePool, DeletePool, AllocPooled, FreePooled), Formatting (RawDoFmt)
- **Steps 11.5-11.8**: Async I/O (SendIO, WaitIO, CheckIO, AbortIO), Software interrupts (Cause), Shared semaphores (ObtainSemaphoreShared, AttemptSemaphoreShared), Named semaphores (AddSemaphore, RemSemaphore, FindSemaphore)

### Phase 12: Utility Library Completion (COMPLETE)
~90% AmigaOS Utility compatibility achieved:
- **Steps 12.1-12.5**: String (Strnicmp), Date (Date2Amiga, CheckDate), TagItem management (AllocateTagItems, FreeTagItems, CloneTagItems, RefreshTagItemClones, FilterTagItems, TagInArray), Hooks (CallHookPkt), 64-bit math (SMult64, UMult64)

---

## Phase 13: Graphics Foundation (COMPLETE)

**Goal**: Establish the core graphics subsystem and host display integration.

### Step 13.1: Host Display Subsystem (COMPLETE)
- [x] **SDL2 Integration** - SDL2 detection in CMake, conditional compilation, display.h/display.c API.
- [x] **Surface Management** - `display_t` abstraction with window/renderer/texture management, palette support.
- [x] **Emucalls** - Graphics emucalls 2000-2999 range (INIT, SHUTDOWN, OPEN_DISPLAY, CLOSE_DISPLAY, REFRESH, SET_COLOR, SET_PALETTE4/32, WRITE_PIXEL, READ_PIXEL, RECT_FILL, DRAW_LINE, UPDATE_PLANAR, UPDATE_CHUNKY, GET_SIZE, AVAILABLE, POLL_EVENTS).

### Step 13.2: Graphics Library Basics (COMPLETE)
- [x] **Library Structure** - `graphics.library` already initialized with `GfxBase`.
- [x] **Data Structures** - `BitMap`, `RastPort` already defined in Amiga headers.
- [x] **Memory** - `AllocRaster`/`FreeRaster` for plane memory, RASSIZE macro formula.

### Step 13.3: Basic Rendering (COMPLETE)
- [x] **Pixel Ops** - `WritePixel`, `ReadPixel` (software implementation with planar format).
- [x] **Drawing** - `Draw` (Bresenham line algorithm), `RectFill` (software rectangle fill).
- [x] **State** - `SetAPen`, `SetBPen`, `SetDrMd`, `GetAPen`, `GetBPen`, `GetDrMd`, `SetABPenDrMd`.
- [x] **Bitmap Init** - `InitBitMap`, `InitRastPort`, `SetRast`, `Move`.
- [x] **Timing** - `WaitTOF`, `WaitBlit` (no-ops, no real hardware).

### Step 13.4: Graphics Test Coverage (COMPLETE)
- [x] **Test Suite** - Create `tests/graphics/` directory with comprehensive tests.
- [x] `init_rastport` - Verify InitRastPort() sets correct defaults.
- [x] `init_bitmap` - Verify InitBitMap() calculates BytesPerRow correctly.
- [x] `alloc_raster` - Test AllocRaster()/FreeRaster() memory handling.
- [x] `pen_state` - Test pen/mode functions (SetAPen, GetAPen, etc.).
- [x] `pixel_ops` - Test WritePixel()/ReadPixel() for various colors.
- [x] `line_draw` - Test Draw() for horizontal, vertical, diagonal lines.
- [x] `rect_fill` - Test RectFill() including COMPLEMENT mode.
- [x] `set_rast` - Test SetRast() clears to correct color.

See `doc/graphics_testing.md` for detailed test specifications.

### Step 13.5: Screen Management (COMPLETE)
- [x] **OpenScreen** - Map `OpenScreen` to Host Window creation.
- [x] **CloseScreen** - Free bitplanes and close Host Window.
- [x] **Display Loop** - Blit Amiga BitMap to Host Window texture.
- [x] **Test** - `tests/intuition/screen_basic` validates screen creation and drawing.

---

## Phase 14: Intuition Basics (COMPLETE)

**Goal**: Implement windowing and user input.

### Step 14.1: Intuition Core (COMPLETE)
- [x] **Library Structure** - `intuition.library` already initialized.
- [x] **OpenWindow** - Allocates Window, links to Screen, sets up borders and RastPort.
- [x] **CloseWindow** - Cleans up IDCMP port, unlinks from Screen, frees Window.

### Step 14.2: Input Handling (COMPLETE)
- [x] **IDCMP Port Setup** - UserPort created/deleted based on IDCMPFlags.
- [x] **ModifyIDCMP** - Dynamically add/remove IDCMP flags.
- [x] **Input Pipeline** - SDL Events -> Host emucalls (3010-3014) -> ROM processing.
- [x] **IDCMP Posting** - Post `IntuiMessage` to Window ports (MOUSEBUTTONS, RAWKEY, CLOSEWINDOW).
- [x] **Mouse Tracking** - Track mouse position, update `Window->MouseX/Y`.
- [x] **Screen Linking** - Screens linked to IntuitionBase->FirstScreen/ActiveScreen.
- [x] **WaitTOF Hook** - Input processing triggered from WaitTOF for all screens.

### Step 14.3: Basic Gadgets (DEFERRED)
- [ ] **System Gadgets** - Visual representation of Close/Depth/Drag gadgets.
- [ ] **Gadget Interaction** - Hit testing and basic state changes.

### Step 14.4: Intuition Test Coverage (COMPLETE)
- [x] **Test Suite** - `tests/intuition/` directory with comprehensive tests.
- [x] `screen_basic` - Verify OpenScreen() creates correct structures.
- [x] `window_basic` - Verify OpenWindow() and window linkage.
- [x] `idcmp` - Test ModifyIDCMP and port creation/deletion.
- [x] `idcmp_input` - Test screen linking, IDCMP setup, WaitTOF input processing hook.

See `doc/graphics_testing.md` for detailed test specifications.

---

## Phase 15: Rootless Mode & Advanced Graphics (COMPLETE)

**Goal**: seamless desktop integration and rich graphics.

### Step 15.1: Rootless Windowing (COMPLETE)
- [x] **Mode Switch** - Configuration `rootless_mode` in `[display]` section.
- [x] **Host Support** - `display_window_t` type with open/close/move/resize functions.
- [x] **Emucalls** - Intuition emucalls 3020-3028 (OPEN_WINDOW, CLOSE_WINDOW, MOVE_WINDOW, SIZE_WINDOW, WINDOW_TOFRONT, WINDOW_TOBACK, SET_TITLE, REFRESH_WINDOW, GET_ROOTLESS).
- [x] **Window Mapping** - `OpenWindow` creates individual SDL windows when rootless mode enabled.
- [x] **Event Routing** - Window handles stored in `Window->UserData` for event routing.

### Step 15.2: Advanced Rendering (COMPLETE)
- [x] **Blitter** - `BltBitMap`, `BltBitMapRastPort`, `BltTemplate` (CPU emulation with minterm logic).
- [x] **Text** - `OpenFont`, `CloseFont`, `SetFont`, `Text`, `TextLength` with built-in Topaz-8 font.
- [x] **Layers** - Basic `LockLayerRom`, `UnlockLayerRom` stubs (no-ops for single-threaded).

### Step 15.3: UI Elements (COMPLETE)
- [x] **Requesters** - `AutoRequest`, `EasyRequestArgs` implemented (console-based, return defaults).

### Step 15.5: Menus (DEFERRED)
- [ ] **Menu System** - Menu bar handling (global vs per-window), SetMenuStrip, ClearMenuStrip.
- [ ] **Menu Events** - IDCMP_MENUPICK handling and ItemAddress.

### Step 15.4: Graphics Test Coverage (COMPLETE)
- [x] `text_render` - Test OpenFont, SetFont, Text, TextLength with built-in Topaz font.
- [x] `blt_bitmap` - Test BltBitMap with minterm copy, NULL handling, edge cases.

---

## Phase 16: Console Device Enhancement (COMPLETE)

**Goal**: Provide full ANSI/CSI escape sequence support for terminal applications.

### Step 16.1: Cursor Control (COMPLETE)
- [x] **Cursor Positioning** - CSI row;col H/f, relative movement (A/B/C/D/E/F/G)
- [x] **Tab Control** - Horizontal tab (I), backward tab (Z)

### Step 16.2: Line/Screen Control (COMPLETE)
- [x] **Erase Functions** - CSI J (display), CSI K (line) with all variants (0/1/2)
- [x] **Insert/Delete** - Lines (L/M), Characters (@/P)
- [x] **Scrolling** - Scroll up (S), scroll down (T)

### Step 16.3: Text Attributes (COMPLETE)
- [x] **SGR Attributes** - Bold (1), faint (2), italic (3), underline (4), inverse (7), concealed (8)
- [x] **Colors** - Foreground (30-37, 39), background (40-47, 49)
- [x] **Reset** - Attribute disable codes (22-28), full reset (0)

### Step 16.4: Window Queries & Raw Mode (COMPLETE)
- [x] **Cursor Visibility** - Hide (CSI 0 p), show (CSI p / CSI 1 p)
- [x] **Mode Control** - Autowrap enable/disable (CSI ?7 h/l)
- [x] **Control Characters** - Form feed (clear), vertical tab (up), BEL, BS, HT, LF, CR
- [x] **C1 Controls** - IND (84), NEL (85), RI (8D)
- [x] **ESC Sequences** - ESC[ as alternative CSI, ESC c reset

### Step 16.5: Console Test Coverage (COMPLETE)
- [x] `csi_cursor` - Comprehensive test for all CSI sequence translations

---

## Phase 17: Additional Commands (COMPLETE)

**Goal**: Expand command set to cover common AmigaOS utilities.

### Step 17.1: System Commands (COMPLETE)
- [x] **AVAIL** - Display memory information (chip/fast/total/largest)
- [x] **RESIDENT** - List libraries and devices (ADD/REMOVE stubbed)

### Step 17.2: Script Support (COMPLETE)
- [x] **ASK** - Interactive yes/no prompt (Shell built-in)
- [x] **REQUESTCHOICE** - Text-based choice dialog with numbered options

### Step 17.3: Shell Enhancements (COMPLETE)
- [x] **CD** with no args - Shows current directory (Shell built-in)
- [x] **WHICH** - Locate command in search path (Shell built-in)
- [x] **PATH** - Manage command search path with ADD/RESET/SHOW (Shell built-in)

### Step 17.4: Test Coverage (COMPLETE)
- [x] `commands/avail` - Tests AvailMem() functionality

---

## Phase 18: App-DB & KickPascal 2 Testing (COMPLETE)

**Goal**: Establish the infrastructure for running real Amiga applications, using KickPascal 2 as the first target.

### Step 18.1: KickPascal 2 Analysis & Environment (COMPLETE)
- [x] **Dependency Analysis** - Identified required libraries (`icon.library`), devices, and assigns for `KP` executable.
- [x] **Assign Setup** - Config `[assigns]` section for app-specific assigns (KP2:, LIBS:, DEVS:, S:, T:, L:).
- [x] **App Manifest** - Created `app.json` format for defining application environments.

### Step 18.2: Initial Execution & Library Loading (COMPLETE)
- [x] **LoadSeg Fix** - Fixed hunk type handling by masking upper bits (memory flags) from hunk types.
- [x] **Library Loading** - OpenLibrary now loads external m68k libraries from `LIBS:` directory.
- [x] **RomTag Support** - Implemented `FindResident` and `InitResident` for library initialization.
- [x] **KickPascal 2** - Successfully loads and exits (GUI requires display support).

### Step 18.3: App-DB Infrastructure (COMPLETE)
- [x] **Test Runner** - Implemented `tests/run_apps.py` to execute applications from `../lxa-apps/`.
- [x] **Manifest Parser** - Support `app.json` for environment setup (assigns, libraries, test config).
- [x] **Config Generation** - Dynamic config file creation with app-specific assigns.

Key implementations:
- `FindResident()` / `InitResident()` in exec.library for RomTag-based library initialization
- `OpenLibrary()` disk loading: scans LIBS: for missing libraries, finds RomTags, initializes
- VFS absolute path passthrough for Linux paths starting with `/`
- Config `[assigns]` section for persistent assign setup

---

## Phase 19: Standard Library Expansion

**Goal**: Implement the "Big Three" libraries required by most Amiga GUI software.

### Step 19.1: Layers & Clipping (IN PROGRESS)
- [x] **layers.library** - Core implementation with Layer_Info, Layer creation/deletion, locking.
  - Implemented: NewLayerInfo, DisposeLayerInfo, InitLayers
  - Implemented: CreateUpfrontLayer, CreateBehindLayer, DeleteLayer
  - Implemented: LockLayer, UnlockLayer, LockLayers, UnlockLayers, LockLayerInfo, UnlockLayerInfo
  - Implemented: BeginUpdate, EndUpdate, WhichLayer, InstallClipRegion
  - Implemented: MoveLayer, SizeLayer, ScrollLayer, MoveSizeLayer
  - Implemented: Hook layer variants, layer visibility (HideLayer, ShowLayer)
- [ ] **Window Invalidation** - Handle damage regions and `BeginRefresh`/`EndRefresh`.
- [ ] **ClipRect Splitting** - Full overlap handling for multiple layers.

### Step 19.2: GadTools & ASL
- [ ] **gadtools.library** - Standard UI widgets (buttons, sliders, lists).
- [ ] **asl.library** - File and Font requesters (bridged to host or native).

### Step 19.3: BOOPSI Foundation
- [ ] **Intuition Classes** - `imageclass`, `gadgetclass`, `rootclass`.
- [ ] **NewObject / DisposeObject** - Object lifecycle management.

---

## Phase 20: Font System & Preferences

**Goal**: Improve visual fidelity and system configuration.

### Step 20.1: Font System
- [ ] **diskfont.library** - Full implementation for font loading.
- [ ] **FONTS: Assign** - Automatic provisioning of font directories.
- [ ] **Topaz Bundling** - Ensure a high-quality Topaz-8/9 replacement is available.

### Step 20.2: Preferences & Environment
- [ ] **ENVARC: Persistence** - Map `ENVARC:` to `~/.lxa/Prefs/Env` with persistence.
- [ ] **Locale Basics** - `locale.library` stubs for system defaults.

---

## Phase 21: Quality & Stability Hardening

**Goal**: Ensure production-grade reliability.

### Step 21.1: Memory Debugging
- [ ] Memory tracking, leak detection, double-free detection, buffer overflow guards
- [ ] **Stress Testing** - 24-hour continuous operation, 10,000 file operations.

### Step 21.2: Compatibility Testing
- [ ] Test with common Amiga software, document issues, create compatibility database.

### Step 21.3: Performance Optimization
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
