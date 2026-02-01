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

## Phase 19: Graphics & Layers Completion (COMPLETE)

**Goal**: Implement the remaining core graphics components.

### Step 19.1: Layers & Clipping (COMPLETE)
- [x] **layers.library** - Core implementation with Layer_Info, Layer creation/deletion, locking.
  - Implemented: NewLayerInfo, DisposeLayerInfo, InitLayers
  - Implemented: CreateUpfrontLayer, CreateBehindLayer, DeleteLayer
  - Implemented: LockLayer, UnlockLayer, LockLayers, UnlockLayers, LockLayerInfo, UnlockLayerInfo
  - Implemented: BeginUpdate, EndUpdate, WhichLayer, InstallClipRegion
  - Implemented: MoveLayer, SizeLayer, ScrollLayer, MoveSizeLayer
  - Implemented: Hook layer variants, layer visibility (HideLayer, ShowLayer)
- [x] **Window Invalidation** - BeginRefresh/EndRefresh in intuition.library, damage tracking in layers.library.
- [x] **ClipRect Splitting** - Full overlap handling with SplitRectAroundObscurer(), proper RebuildClipRects().

### Step 19.2: Region Functions (COMPLETE)
- [x] **graphics.library regions** - Full region support for clipping.
  - Implemented: NewRegion, DisposeRegion, ClearRegion
  - Implemented: OrRectRegion, AndRectRegion, ClearRectRegion, XorRectRegion
  - Implemented: OrRegionRegion, AndRegionRegion, XorRegionRegion
  - Helper functions: AllocRegionRectangle, FreeRegionRectangle, UpdateRegionBounds
- [x] **Damage Tracking** - AddDamageToLayer, DamageExposedAreas for layer operations.

---

## Phase 20: KickPascal 2 Compatibility - Critical Fixes (IN PROGRESS)

**Goal**: Address blocking issues preventing KickPascal 2 from running correctly.

### Step 20.1: Window Size & Screen Resolution (COMPLETE)
**Problem**: KickPascal 2 opened a 252x350 window instead of filling the 640x256 Workbench screen.

**Root Causes Found**:
1. **Config file not found**: KP2 tried to open `S:Pascal.config` but S: pointed to system directory, not KP2's local `s/` directory.
2. **WBenchToFront didn't open Workbench**: KP2 calls `WBenchToFront` (LVO -342) expecting it to open Workbench if not already open, but our implementation was a no-op stub. This caused `IntuitionBase->FirstScreen` to be NULL when KP2 tried to read screen dimensions.

**Fixes Implemented**:
- [x] **Multi-assign support in VFS** - `vfs_assign_prepend_path()` adds paths to beginning of assigns, `vfs_resolve_path()` tries all paths until finding existing file.
- [x] **Automatic program-local assigns** - On program load, add program's `s/`, `libs/`, `c/`, `devs/`, `l/` subdirectories to standard assigns (prepended for priority).
- [x] **Set current directory on load** - Set `pr_CurrentDir` and `pr_HomeDir` to program's directory in `_bootstrap()`.
- [x] **Fixed WBenchToFront** - Now opens Workbench screen if not already open (matches AmigaOS behavior).

**Result**: KP2 now opens with correct 640x244 window dimensions and loads config properly.

### Step 20.2: Console Device Window Attachment (COMPLETE)
**Problem**: KickPascal 2's console output (text, prompts) is not visible in the window. Console device currently only logs to debug output, doesn't render to Intuition windows.

**Output Fixes Implemented**:
- [x] **LxaConUnit Structure** - Extended ConUnit with cursor position, character cell dimensions, raster area bounds, pen colors, and CSI parsing state.
- [x] **Console Open** - Extract Window pointer from `io_Data` field, create ConUnit with `console_create_unit()`, calculate raster area from window borders.
- [x] **Text Rendering** - CMD_WRITE processes characters via `console_process_char()`, renders using `Text()` at calculated pixel positions.
- [x] **CSI Sequence Parsing** - Full support for Amiga's 0x9B and ANSI ESC[ sequences:
  - H/f: Cursor Position (CUP/HVP)
  - A/B/C/D: Cursor movement
  - J: Erase in Display, K: Erase in Line
  - m: Select Graphic Rendition (colors)
  - p: Cursor visibility (Amiga-specific)
  - {: Scroll region (Amiga-specific)
- [x] **Control Characters** - LF, CR, TAB, BS, BEL, FF handling.
- [x] **Scrolling** - `console_scroll_up()` using `ScrollRaster()`.
- [x] **Display Refresh** - `WaitTOF()` called after writes to trigger screen update.

**Input Fixes Implemented**:
- [x] **SDL Event Polling Fix** - `EMU_CALL_INT_POLL_INPUT` now calls `display_poll_events()` before `display_get_event()` to ensure SDL events are dequeued.
- [x] **Rawkey-to-ASCII Conversion** - Lookup tables for unshifted/shifted keys, support for Shift, Caps Lock, and Control modifiers.
- [x] **Input Ring Buffer** - 256-byte circular buffer for console input with line-mode support.
- [x] **IDCMP_RAWKEY Subscription** - Console Open() adds IDCMP_RAWKEY to window via ModifyIDCMP().
- [x] **CMD_READ from Window** - Reads IDCMP_RAWKEY messages from window's UserPort instead of host stdin.
- [x] **Character Echo** - Typed characters echoed to console output with backspace handling.
- [x] **Cursor Display** - XOR block cursor shown during input, CSI 'p' controls visibility.

**Result**: KP2's text output and keyboard input work in the window. Line editing has known issues (see Phase 22).

**Known Issues**:
- Line editing (backspace over program-output text) doesn't match real Amiga behavior
- KP2's workspace size input field doesn't work correctly (works on real Amiga)
- Needs proper testing infrastructure to debug (see Phase 21)

### Step 20.3: Window Layer Clipping (DEFERRED)
**Problem**: Drawing outside window bounds may render on screen instead of being clipped.

- [ ] **Bounds Checking** - Implement pixel-level clipping in WritePixel, Draw, Text, etc.
- [ ] **Layer ClipRect** - Use layer's ClipRect list for proper clipping (already built by layers.library).
- [ ] **Test** - Verify out-of-bounds drawing is properly clipped.

**Current State**: Layer coordinate translation works, but no enforcement of window boundaries.

---

## Phase 21: UI Testing Infrastructure (IN PROGRESS)

**Goal**: Enable automated testing of graphical applications with input simulation and screen verification. This infrastructure is essential for debugging console.device and other UI issues.

See `doc/ui_testing.md` for detailed specifications.

### Step 21.1: Host-Side Infrastructure (COMPLETE)
**Emucalls for input injection and screen capture implemented in host.**

**Implemented** (in `src/lxa/lxa.c` and `src/lxa/display.c`):
- [x] **EMU_CALL_TEST_INJECT_KEY (4100)** - Inject rawkey + qualifier events into event queue
- [x] **EMU_CALL_TEST_INJECT_STRING (4101)** - Inject string as sequence of key events
- [x] **EMU_CALL_TEST_INJECT_MOUSE (4102)** - Inject mouse position + button events
- [x] **EMU_CALL_TEST_SET_HEADLESS (4120)** - Enable/disable headless mode
- [x] **EMU_CALL_TEST_GET_HEADLESS (4121)** - Query headless mode state
- [x] **EMU_CALL_TEST_WAIT_IDLE (4122)** - Wait for event queue to drain
- [x] **EMU_CALL_TEST_CAPTURE_SCREEN (4110)** - Capture screen to PPM file
- [x] **EMU_CALL_TEST_CAPTURE_WINDOW (4111)** - Capture window to PPM file
- [x] **EMU_CALL_TEST_COMPARE_SCREEN (4112)** - Stub (returns -1, not yet implemented)

**Key functions in display.c**:
- `display_inject_key()` - Queue synthetic keyboard events
- `display_inject_string()` - Convert ASCII string to rawkey sequence
- `display_inject_mouse()` - Queue synthetic mouse events
- `display_set_headless()` / `display_get_headless()` - Headless mode control
- `display_event_queue_empty()` - Check if events are pending
- `display_capture_screen()` / `display_capture_window()` - Save to PPM format

### Step 21.2: ROM-Side Test Helpers (COMPLETE)
**Header-only implementation for m68k test programs.**

**Implemented** (in `tests/common/test_inject.h`):
- [x] **test_inject_key()** - Inject single rawkey event (down or up)
- [x] **test_inject_string()** - Inject string as key sequence via emucall
- [x] **test_inject_mouse()** - Inject mouse event (move or button)
- [x] **test_inject_keypress()** - Helper for key down+up pair
- [x] **test_inject_return()** - Inject Return key press
- [x] **test_inject_backspace()** - Inject Backspace key press
- [x] **test_capture_screen()** - Capture screen to PPM file
- [x] **test_set_headless()** / **test_get_headless()** - Headless mode control
- [x] **test_wait_idle()** - Wait for event queue to drain
- [x] **Rawkey definitions** - RAWKEY_* constants for all keys
- [x] **Qualifier definitions** - IEQUALIFIER_* for modifiers

### Step 21.3: Console Device Unit Tests (COMPLETE)
**Test suite for console input/output with injection.**

**Implemented**:
- [x] `tests/console/input_inject/` - Verifies IDCMP injection infrastructure works
  - Opens screen and window with IDCMP_RAWKEY
  - Injects "AB" string via test_inject_string()
  - Receives 4 RAWKEY events (A down, A up, B down, B up)
  - Injects Return key, verifies down/up events received
- [x] `tests/console/input_console/` - Verifies console device input path
  - Opens window with console.device attached
  - Writes prompt "Enter value: " via CMD_WRITE
  - Injects "100" + Return via test injection
  - Reads via CMD_READ, receives "100\n" (4 bytes)
  - Demonstrates full input path works correctly

**Test Infrastructure Improvements**:
- [x] **test_runner.sh** - Updated to filter LXA debug output for clean comparisons
- [x] **Debug filtering** - Filters coldstart:, _exec:, _dos:, _intuition:, display:, etc.
- [x] **Exit code handling** - Tests pass based on output comparison, not exit code

### Step 21.4: KickPascal 2 Investigation (COMPLETE)
**Tasks**:
- [x] **Verified input path works** - input_console test proves console.device input functions correctly
- [x] **Reproduced issue** - Created kp2_test that mimics KP2's 1-byte-at-a-time read pattern
- [x] **Identified root cause** - Line mode was re-waiting for newline on every read, even with buffered data
- [x] **Fixed console.device** - Only wait for newline if buffer doesn't already contain one
- [x] **Test validates fix** - kp2_test passes: reads '1', '0', '0', '\n' correctly

**The Bug**: KP2 reads console input 1 byte at a time. Our console.device was waiting for a complete line on EVERY read, causing the second read to hang forever even though data was already buffered.

**The Fix**: In `lxa_dev_console.c`, changed line mode wait from `if (unit->line_mode)` to `if (unit->line_mode && !input_has_line(unit))` - only enter wait loop if no complete line is already buffered.

---

## Phase 22: Console Device Completion & KP2 Compatibility (PARTIALLY COMPLETE)

**Goal**: Fix console.device to match real Amiga behavior, using the testing infrastructure from Phase 21.

### Step 22.1: Console Device Line Mode Fix (COMPLETE)
**Problem**: KickPascal 2's workspace size input hangs after first character read.

**Root Cause Found**: Line mode was re-waiting for newline on every CMD_READ, even when data was already buffered from a previous line.

**Tasks**:
- [x] **Created kp2_test** - Mimics KP2's 1-byte-at-a-time read pattern
- [x] **Fixed lxa_dev_console.c** - Only wait if no complete line in buffer
- [x] **Verified fix** - kp2_test passes, reads buffered characters immediately

### Step 22.2: Additional Console Device Features (NOT STARTED)
**Potential Future Work**:
- [ ] **CON: Handler vs console.device** - May need CON: handler layer for some apps
- [ ] **Raw Mode** - Character-at-a-time input mode (ACTION_SCREEN_MODE)
- [ ] **Input Buffer Semantics** - Exact handling of backspace/delete in edge cases

### Step 22.3: KickPascal 2 Validation (NEEDS MANUAL TESTING)
**Tasks**:
- [x] **Automated unit test** - kp2_test validates the fix works
- [ ] **Manual KP2 test** - Launch real KP2, test workspace input interactively
- [ ] **Basic Editing Test** - Open editor, type text, verify display
- [ ] **Reference Screenshots** - Compare against real Amiga captures

**Note**: Full KP2 validation requires manual testing since we can't inject keys into SDL windows programmatically without xdotool or similar tools.

---

## Phase 23: Core System Libraries & Preferences

**Goal**: Improve visual fidelity, system configuration, and core library support.

### Step 23.1: Standard Libraries (PARTIALLY COMPLETE)
- [x] **diskfont.library** - OpenDiskFont (falls back to ROM fonts), AvailFonts stub.
- [x] **icon.library** - Fixed LVO table alignment, full function set implemented.
- [x] **expansion.library** - ConfigChain and board enumeration stubs (existing).
- [x] **translator.library** - Translate() stub (copies input to output).
- [x] **locale.library** - Character classification (IsAlpha, IsDigit, etc.), ConvToLower/ConvToUpper, GetLocaleStr, OpenLocale with default English locale.
- [ ] **FONTS: Assign** - Automatic provisioning of font directories.
- [ ] **Topaz Bundling** - Ensure a high-quality Topaz-8/9 replacement is available.

### Step 23.2: Preferences & Environment
- [ ] **ENVARC: Persistence** - Map `ENVARC:` to `~/.lxa/Prefs/Env` with persistence.

---

## Phase 24: Advanced UI Frameworks

**Goal**: Implement standard Amiga UI widgets and object-oriented systems.

### Step 24.1: GadTools & ASL
- [ ] **gadtools.library** - Standard UI widgets (buttons, sliders, lists).
- [ ] **asl.library** - File and Font requesters (bridged to host or native).

### Step 24.2: BOOPSI Foundation
- [ ] **Intuition Classes** - `imageclass`, `gadgetclass`, `rootclass`.
- [ ] **NewObject / DisposeObject** - Object lifecycle management.

---

## Phase 25: Quality & Stability Hardening

**Goal**: Ensure production-grade reliability.

### Step 25.1: Memory Debugging
- [ ] Memory tracking, leak detection, double-free detection, buffer overflow guards
- [ ] **Stress Testing** - 24-hour continuous operation, 10,000 file operations.

### Step 25.2: Compatibility Testing
- [ ] Test with common Amiga software, document issues, create compatibility database.

### Step 25.3: Performance Optimization
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
