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

## Phase 14: Intuition Basics

**Goal**: Implement windowing and user input.

### Step 14.1: Intuition Core
- [ ] **Library Structure** - `intuition.library` initialization.
- [ ] **Screen/Window** - `OpenWindow`, `CloseWindow` (Integrated Mode: rendering into Screen BitMap).
- [ ] **Refresh** - Handle `BeginRefresh`, `EndRefresh`.

### Step 14.2: Input Handling
- [ ] **Input Pipeline** - SDL Events -> Host Input Handler -> `input.device`.
- [ ] **IDCMP** - Post `IntuiMessage` to Window ports (MOUSEBUTTONS, RAWKEY).
- [ ] **Mouse** - Track mouse position, update `Window->MouseX/Y`.

### Step 14.3: Basic Gadgets
- [ ] **System Gadgets** - Visual representation of Close/Depth/Drag gadgets.
- [ ] **Gadget Interaction** - Hit testing and basic state changes.

### Step 14.4: Intuition Test Coverage (REQUIRED)
- [ ] **Test Suite** - Create `tests/intuition/` directory with comprehensive tests.
- [ ] `screen_basic` - Verify OpenScreen() creates correct structures.
- [ ] `window_basic` - Verify OpenWindow() and IDCMP setup.
- [ ] `idcmp` - Test IDCMP message delivery and ModifyIDCMP.

See `doc/graphics_testing.md` for detailed test specifications.

---

## Phase 15: Rootless Mode & Advanced Graphics

**Goal**: seamless desktop integration and rich graphics.

### Step 15.1: Rootless Windowing
- [ ] **Mode Switch** - Configuration for Rootless vs Hosted.
- [ ] **Window Mapping** - `OpenWindow` creates individual SDL windows in Rootless mode.
- [ ] **Event Routing** - Route events to specific windows.

### Step 15.2: Advanced Rendering
- [ ] **Blitter** - `BltBitMap`, `BltTemplate` (CPU emulation).
- [ ] **Text** - `OpenFont`, `Text`, `SetFont` (Built-in Topaz-8 font).
- [ ] **Layers** - Basic `layers.library` support for clipping.

### Step 15.3: UI Elements
- [ ] **Menus** - Menu bar handling (global vs per-window).
- [ ] **Requesters** - `AutoRequest`, `EasyRequest` as native or modal windows.

---

## Phase 16: Console Device Enhancement

**Goal**: Provide full ANSI/CSI escape sequence support for terminal applications.

### Step 16.1: Cursor Control
- [ ] **Cursor Positioning** - CSI row;col H, relative movement (A/B/C/D), save/restore

### Step 16.2: Line/Screen Control
- [ ] **Clear Line/Screen** - CSI K and CSI J variants
- [ ] **Insert/Delete** - Characters and lines with scrolling

### Step 16.3: Text Attributes
- [ ] **SGR Attributes** - Bold, underline, reverse, colors (30-47)

### Step 16.4: Window Queries & Raw Mode
- [ ] **Window Bounds** - Report dimensions
- [ ] **SetMode** - Raw/cooked mode switching

---

## Phase 17: Additional Commands

**Goal**: Expand command set to cover common AmigaOS utilities.

### Step 17.1: File Management
- [ ] **AVAIL** - Display memory information
- [ ] **RESIDENT** - Manage resident commands

### Step 17.2: Script Support
- [ ] **ASK** - Interactive yes/no prompt
- [ ] **REQUESTCHOICE** - Text-based choice dialog

### Step 17.3: Shell Enhancements
- [ ] **CD** with no args - Show current directory
- [ ] **WHICH** - Locate command in search path
- [ ] **PATH** - Manage command search path

---

## Phase 18: Quality & Stability Hardening

**Goal**: Ensure production-grade reliability.

### Step 18.1: Memory Debugging
- [ ] Memory tracking, leak detection, double-free detection, buffer overflow guards

### Step 18.2: Stress Testing
- [ ] 24-hour continuous operation, 10,000 file operations, 100 concurrent processes

### Step 18.3: Compatibility Testing
- [ ] Test with common Amiga software, document issues, create compatibility database

### Step 18.4: Performance Optimization
- [ ] Profile critical paths, optimize VFS, memory allocation, emucall overhead

---

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
