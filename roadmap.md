# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project!

---

## Current Status

**Version: 0.6.2** | **Phase 45 Complete** | **36 Integration Tests Passing**

The lxa project has achieved a comprehensive AmigaOS-compatible environment with 95%+ library compatibility across Exec, DOS, Graphics, Intuition, and system libraries.

---

## Core Philosophy & Boundaries

### WINE-like DOS Approach
Instead of emulating hardware-level disk controllers and running Amiga-native filesystems (like OFS/FFS) on top, `lxa` maps Amiga logical drives directly to Linux directories.

- **Drives as Directories**: `DH0:`, `DF0:`, etc., are backed by standard Linux directories.
- **Host Interception**: DOS calls (`Open`, `Read`, `Lock`, etc.) are handled by the host `lxa` process using Linux system calls.
- **Auto-Provisioning**: On first run, `lxa` creates a default `~/.lxa` structure with `SYS:`, `S:`, `C:`, etc., and populates them with a basic `Startup-Sequence` and essential tools.

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
- ✅ Rootless windowing, mouse tracking, keyboard input

### Application Support (Phases 26-35)
- ✅ Real-world app testing: KickPascal 2, GFA Basic, Devpac, MaxonBASIC, EdWordPro, SysInfo
- ✅ Library stubs: gadtools, workbench, asl, commodities, rexxsyslib, iffparse, reqtools, diskfont, icon, expansion, translator, locale, keymap
- ✅ 68030 CPU support, custom chip registers, CIA resources
- ✅ ROM robustness: pointer validation, crash recovery

### Library Completion (Phases 36-38)
- ✅ **exec.library** - 100% complete (172 functions)
- ✅ **graphics.library** - 90%+ complete (text, area fills, blitting, display modes)
- ✅ **intuition.library** - 95%+ complete (windows, screens, gadgets, BOOPSI, IDCMP)

### Testing & Fixes (Phases 39-40)
- ✅ **Phase 39**: Re-testing identified rendering issues in applications
- ✅ **Phase 39b**: Enhanced test infrastructure (bitmap capture, validation API, screenshot comparison)
- ✅ **Phase 40**: Fixed GFA Basic screen height bug (21px→256px), verified Devpac rendering pipeline

### IFF, Datatypes & Async I/O (Phases 41-45)
- ✅ **Phase 41**: iffparse.library full implementation
  - AllocIFF, FreeIFF, InitIFFasDOS
  - OpenIFF, CloseIFF with read/write modes
  - PushChunk, PopChunk with IFFSIZE_UNKNOWN support
  - ReadChunkBytes, WriteChunkBytes
  - ParseIFF with IFFPARSE_SCAN and IFFPARSE_RAWSTEP
  - GoodID, GoodType, IDtoStr validation functions
  - CurrentChunk, ParentChunk navigation
- ✅ **Phase 42**: datatypes.library (Basic Framework)
  - NewDTObjectA, DisposeDTObject - create and destroy datatype objects
  - GetDTAttrsA, SetDTAttrsA - get and set object attributes
  - ObtainDataTypeA, ReleaseDataType - datatype descriptor management
  - Basic datatypesclass BOOPSI class with OM_NEW, OM_DISPOSE, OM_SET, OM_GET
  - Support for DTA_TopVert, DTA_TotalVert and other scroll attributes
  - GetDTString for localized error messages
  - AddDTObject, RemoveDTObject for window integration
  - RefreshDTObjectA, DoAsyncLayout, DoDTMethodA for rendering
  - GetDTMethods, GetDTTriggerMethods for method queries
- ✅ **Phase 43**: console.device Advanced Features (Complete)
  - CONU_CHARMAP and CONU_SNIPMAP unit modes for character map binding
  - RawKeyConvert() library function (wrapper around MapRawKey)
  - CSI 'r' (DECSTBM) - Set Top and Bottom Margins for scrolling regions
  - CSI '{' (Amiga-specific) - Set scrolling region from top
  - Scrolling functions respect scroll_top and scroll_bottom boundaries
- ✅ **Phase 44**: BOOPSI & GadTools Visual Completion (Complete)
  - ROM size increased to 512KB to accommodate new functionality
  - propgclass GM_HANDLEINPUT - proportional gadget dragging with mouse
  - strgclass GM_RENDER - cursor display at BufferPos
  - strgclass GM_HANDLEINPUT - full keyboard input (Return, Escape, Backspace, Delete, arrows, character input)
  - DrawBevelBoxA - actual bevel box rendering with shine/shadow pens
  - GTBB_Recessed, GT_VisualInfo, GTBB_FrameType tag support
  - Frame types: BBFT_BUTTON, BBFT_RIDGE, BBFT_DISPLAY
  - ASL FileRequester - full GUI implementation with window, gadgets, directory listing
  - AllocAslRequest, FreeAslRequest, AslRequest functions
  - ASLFR_* tag parsing for all requester options
- ✅ **Phase 45**: Async I/O & Timer Completion (Complete)
  - timer.device async with TR_ADDREQUEST delay queue and VBlank-driven expiration
  - Host-side timer queue (64 pending requests)
  - EMU_CALL_TIMER_ADD/REMOVE/CHECK/GET_EXPIRED emucalls
  - VBlank hook integration for timer checking
  - Proper ReplyMsg() from m68k side to wake waiting tasks
  - AbortIO support for timer cancellation
  - console.device async with non-blocking CMD_READ
  - ConsoleDevBase structure for tracking open console units
  - Pending read queue with VBlank hook processing
  - AbortIO properly cancels pending reads with IOERR_ABORTED
  - Fixed exec.c AbortIO register calling convention
  - Integration tests: devices/timer_async, devices/console_async

---

## Active Phase

### Phase 46: Directory Opus 4 Deep Dive (In Progress)
**Goal**: Full Directory Opus 4 compatibility with automated testing.

See detailed phase description below in "Application Deep Dive Phases".

---

## Application Deep Dive Phases (46-58)

**Core Mandate**: All testing must be **fully automated**. If the current testing framework is insufficient, it must be extended. The goal for each application is to achieve **full functionality** with comprehensive automated verification.

**Testing Requirements for ALL Apps**:
- ✅ Bitmap capture and comparison (Phase 39b)
- ✅ Screen/window dimension validation (Phase 39b)
- ✅ Test validation API (Phase 39b)
- [ ] Title text rendering verification (extend as needed)
- [ ] Menu structure verification (extend as needed)
- [ ] Gadget rendering verification (extend as needed)
- [ ] User interaction simulation (keyboard/mouse events)
- [ ] Functional outcome verification (file operations, compilation results, etc.)

**Process for Each App**:
1. Identify all known issues from COMPATIBILITY.md
2. Create detailed automated test plan
3. Extend test framework as needed for app-specific validation
4. Implement fixes for identified issues
5. Verify full functionality with automated tests
6. Update COMPATIBILITY.md with results

---

## Future Phases

### Phase 46: Directory Opus 4 Deep Dive
**Goal**: Full Directory Opus 4 compatibility with automated testing.

**Status**: ⚠️ PARTIAL - Main process exits immediately (error code 1), background process has exception loop

**Known Issues**:
- [ ] Main process exits with error code 1 immediately after launch
- [ ] Background process loads dopus.library but enters exception loop
- [ ] powerpacker.library causes exception loop
- [ ] commodities.library not found (non-fatal)
- [ ] rexxsyslib.library not found (non-fatal)

**Testing Requirements**:
- [ ] Extend test framework to capture multi-process failures
- [ ] Add automated detection of exception loops
- [ ] Verify dopus.library loads and initializes correctly
- [ ] Test main window opens with correct dimensions
- [ ] Verify file list rendering
- [ ] Test directory navigation functionality

**Implementation Tasks**:
- [ ] Debug main process immediate exit - trace all library opens and initialization
- [ ] Fix powerpacker.library exception - implement basic decompression stubs
- [ ] Implement commodities.library stub (optional, for hotkey support)
- [ ] Stack corruption analysis during cleanup
- [ ] Memory corruption detection (guards/canaries)
- [ ] Verify all window gadgets render correctly
- [ ] Test file operations (copy, move, delete)

**Success Criteria**: Directory Opus launches, displays file lists, allows basic file operations

---

### Phase 47: KickPascal 2 Deep Dive
**Goal**: Full KickPascal 2 IDE functionality with automated testing.

**Status**: ⚠️ UNKNOWN - No crash detected but functionality unverified

**Known Issues**:
- [ ] IDE interface rendering unverified
- [ ] Menu visibility and functionality unknown
- [ ] User interaction capabilities unknown
- [ ] Window content display unknown

**Testing Requirements**:
- [ ] Implement automated screen/window dimension validation
- [ ] Add menu structure verification (menu bar, menu items)
- [ ] Verify editor window content renders
- [ ] Test text input in editor window
- [ ] Verify syntax highlighting (if applicable)
- [ ] Test compilation functionality with simple program
- [ ] Verify error messages display correctly

**Implementation Tasks**:
- [ ] Deep trace of all OpenWindow/OpenScreen calls with dimension validation
- [ ] Verify all menus are created and positioned correctly
- [ ] Test RastPort rendering for text display
- [ ] Ensure editor window accepts keyboard input
- [ ] Trace DOS calls for source file loading
- [ ] Verify compiler execution path
- [ ] Test error reporting mechanism

**Success Criteria**: IDE launches, displays menus, allows text editing, compiles simple programs

---

### Phase 48: GFA Basic 2 Deep Dive
**Goal**: Fix screen dimension bug and achieve full GFA Basic functionality.

**Status**: ❌ BROKEN - Opens screen with wrong dimensions (640x21 instead of 640x200+)

**Known Issues**:
- [x] OpenScreen() creates 21-pixel tall screen (CRITICAL BUG) - Fixed in Phase 40
- [ ] Screen should be 200+ pixels for usable editor
- [ ] GetScreenData() returns screen info - verify correctness
- [ ] InitGels() stub may need full implementation
- [ ] IntuiTextLength() - verify text width calculations
- [ ] audio.device warning (BeginIO command 9) - non-fatal but investigate

**Testing Requirements**:
- [x] Automated screen dimension validation (640x256 minimum) - Added in Phase 39b
- [ ] Verify editor window renders with correct dimensions
- [ ] Test text input in editor
- [ ] Verify line numbering display
- [ ] Test BASIC command execution (PRINT, INPUT, etc.)
- [ ] Verify graphics commands (LINE, CIRCLE, etc.)
- [ ] Test program save/load functionality

**Implementation Tasks**:
- [x] Fix OpenScreen() height calculation bug - Fixed in Phase 40
- [ ] Verify NewScreen structure parsing for all fields
- [ ] Test display mode handling for custom screens
- [ ] Implement full InitGels() if required for sprites
- [ ] Verify custom chip register handling (DMACON, colors)
- [ ] Test memory region handling (Zorro-II/III, autoconfig)
- [ ] Trace input.device command 9 warning source
- [ ] Verify editor text rendering and cursor positioning

**Success Criteria**: GFA Basic launches with correct screen size, allows BASIC programming, executes commands

---

### Phase 49: Devpac (HiSoft) Deep Dive
**Goal**: Fix window rendering bug and achieve full assembler IDE functionality.

**Status**: ❌ BROKEN - Window opens but completely empty (no title bar, no content)

**Known Issues**:
- [ ] Window opens with correct dimensions (640x245) but is blank
- [ ] Window title "Untitled" set but NOT RENDERED
- [ ] No visible title bar, borders, or content
- [ ] SDL window shows but is empty

**Testing Requirements**:
- [ ] Automated window title bar rendering verification
- [ ] Verify window border rendering
- [ ] Test editor content area renders
- [ ] Verify menu bar displays
- [ ] Test text input in editor
- [ ] Verify syntax highlighting for assembly code
- [ ] Test assembler execution with simple program
- [ ] Verify error messages display

**Implementation Tasks**:
- [ ] Investigate window title bar rendering in display.c
- [ ] Verify Text() calls for title are executed
- [ ] Check RastPort initialization for windows
- [ ] Trace all rendering operations during window open
- [ ] Verify window border rendering code path
- [ ] Test SDL rendering updates for window decorations
- [ ] Ensure RefreshWindowFrame() is called
- [ ] Verify IntuitText rendering for title
- [ ] Test editor RastPort setup and text rendering

**Success Criteria**: Devpac launches with visible window, title bar, menus, allows assembly editing and compilation

---

### Phase 50: MaxonBASIC Deep Dive
**Goal**: Verify and complete full MaxonBASIC IDE functionality.

**Status**: ⚠️ UNKNOWN - Opens Workbench and window but rendering/functionality unverified

**Known Issues**:
- [ ] Menus visibility and functionality unverified
- [ ] Window content rendering unknown
- [ ] Gadget visibility unknown
- [ ] User interaction capabilities unknown
- [ ] clipboard.device not found (non-fatal warning)

**Testing Requirements**:
- [ ] Verify menu bar renders with all menus (File, Edit, etc.)
- [ ] Test menu item selection and actions
- [ ] Verify editor window content renders
- [ ] Test gadget rendering (buttons, string gadgets, etc.)
- [ ] Verify text input in editor
- [ ] Test BASIC command execution
- [ ] Verify program save/load functionality
- [ ] Test debugging features (if present)

**Implementation Tasks**:
- [ ] Deep trace of CreateMenusA() to verify menu structure
- [ ] Verify LayoutMenusA() positioning
- [ ] Test SetMenuStrip() attachment to window
- [ ] Verify all gadtools.library gadget creation
- [ ] Test DrawInfo pen usage for visual elements
- [ ] Verify QueryOverscan() dimensions affect layout
- [ ] Implement clipboard.device stub for copy/paste support
- [ ] Test BOOPSI system images (sysiclass) rendering
- [ ] Verify GetProgramDir() for file operations

**Success Criteria**: MaxonBASIC displays full IDE with menus, allows BASIC programming, supports file operations

---

### Phase 51: SysInfo Deep Dive
**Goal**: Verify SysInfo displays complete system information.

**Status**: ⚠️ UNKNOWN - No crash but actual functionality unverified

**Known Issues**:
- [ ] Window rendering unverified
- [ ] System information display unknown
- [ ] Gadget/button functionality unknown
- [ ] Information accuracy unknown

**Testing Requirements**:
- [ ] Verify main window renders with correct dimensions
- [ ] Test all information fields display (CPU, RAM, OS version, etc.)
- [ ] Verify gadget rendering (tabs, buttons, list views)
- [ ] Test information accuracy against known system state
- [ ] Verify scrolling functionality (if present)
- [ ] Test refresh/update functionality

**Implementation Tasks**:
- [ ] Deep trace of window creation and layout
- [ ] Verify all Text() calls for information display
- [ ] Test gadget creation and positioning
- [ ] Verify AvailMem() returns correct values
- [ ] Test FindResident() for ROM module detection
- [ ] Verify library version detection
- [ ] Test CPU detection routines
- [ ] Verify screen mode information display

**Success Criteria**: SysInfo displays accurate system information in formatted window

---

### Phase 52: EdWord Pro Deep Dive
**Goal**: Verify and complete full word processor functionality.

**Status**: ⚠️ UNKNOWN - Opens screens and windows but functionality unverified

**Known Issues**:
- [ ] Text entry capabilities unknown
- [ ] Menu functionality unverified
- [ ] Editing features unknown
- [ ] Multiple window rendering unverified
- [ ] keymap.library not found (affects keyboard layout)
- [ ] reqtools.library not found (affects file dialogs)
- [ ] rexxsyslib.library not found (affects scripting)
- [ ] clipboard.device not found (affects copy/paste)

**Testing Requirements**:
- [ ] Verify custom screen "EdWord Pro V6.0 - M.Reddy 1997" renders correctly
- [ ] Test main editing window (640x243) content area
- [ ] Verify message window "EdWord Pro Message" displays messages
- [ ] Test text input and editing
- [ ] Verify font rendering and styles (bold, italic, underline)
- [ ] Test menu functionality (File, Edit, Format, etc.)
- [ ] Verify document save/load functionality
- [ ] Test formatting features (alignment, spacing, etc.)

**Implementation Tasks**:
- [ ] Verify LockIBase/UnlockIBase don't cause deadlocks
- [ ] Test ShowTitle() screen title visibility
- [ ] Verify SetWindowTitles() updates correctly
- [ ] Test SetPointer/ClearPointer custom pointer display
- [ ] Verify InitArea() for area fill operations
- [ ] Test AskSoftStyle/SetSoftStyle for text styling
- [ ] Verify SetRGB4() color palette handling
- [ ] Implement keymap.library stub for proper keyboard handling
- [ ] Implement reqtools.library stub for file dialogs
- [ ] Implement clipboard.device for copy/paste support

**Success Criteria**: EdWord Pro allows text editing, formatting, document save/load with full functionality

---

### Phase 53: BeckerText II Deep Dive
**Goal**: Determine why application exits immediately and enable functionality.

**Status**: ⚠️ PARTIAL - Exits cleanly (code 0) but no window opens

**Known Issues**:
- [ ] Repeatedly opens/closes intuition.library
- [ ] All libraries load successfully but exits immediately
- [ ] No windows open
- [ ] May require Workbench environment
- [ ] May need command-line arguments
- [ ] May require configuration files

**Testing Requirements**:
- [ ] Trace all library open/close sequences
- [ ] Verify Workbench environment requirements
- [ ] Test with various command-line arguments
- [ ] Check for required configuration files
- [ ] Test with different screen modes
- [ ] Verify tooltypes/icon settings (if present)

**Implementation Tasks**:
- [ ] Deep trace of initialization sequence
- [ ] Identify why application decides to exit
- [ ] Check for missing environment variables
- [ ] Verify WBStartup vs CLI startup paths
- [ ] Test with default Workbench screen open
- [ ] Check for required assigns or paths
- [ ] Verify icon.library integration
- [ ] Test with various startup configurations

**Success Criteria**: BeckerText II launches and displays text editor window

---

### Phase 54: Oberon 2 Deep Dive
**Goal**: Fix NULL pointer crash and enable Oberon system.

**Status**: ❌ FAILS - NULL pointer crash (PC=0x00000000) at startup

**Known Issues**:
- [ ] All libraries open successfully
- [ ] Crashes at PC=0x00000000 during initialization
- [ ] NULL function pointer or corrupted return address
- [ ] Likely missing or stubbed function returning NULL

**Testing Requirements**:
- [ ] Identify which library function returns NULL
- [ ] Verify all function pointers before call
- [ ] Test with detailed instruction trace around crash
- [ ] Verify stack state at crash point
- [ ] Test initialization sequence step-by-step

**Implementation Tasks**:
- [ ] Enable detailed trace logging for all library calls
- [ ] Identify last successful library call before crash
- [ ] Check for missing utility.library functions
- [ ] Verify dos.library function implementations
- [ ] Check graphics.library initialization functions
- [ ] Verify intuition.library setup functions
- [ ] Test workbench.library integration
- [ ] Check icon.library function pointers
- [ ] Add NULL pointer guards to all library function returns
- [ ] Verify Oberon-specific requirements (if documented)

**Success Criteria**: Oberon 2 launches without crashes and displays Oberon environment

---

### Phase 55: ASM-One v1.48 Deep Dive
**Goal**: Fix ROM address crash and achieve full assembler functionality.

**Status**: ⚠️ PARTIAL - Opens screens then crashes at ROM address 0x00FBEED1

**Known Issues**:
- [ ] Opens Workbench screen (640x256) successfully
- [ ] Opens editor screen (800x600) successfully
- [ ] Crashes accessing invalid ROM address (0x00FBEED1)
- [ ] iffparse.library not found (non-fatal)
- [ ] reqtools.library not found (non-fatal)
- [ ] timer.device not found
- [ ] Likely unimplemented ROM function

**Testing Requirements**:
- [ ] Identify ROM function at 0x00FBE* range
- [ ] Verify all ROM function table entries
- [ ] Test screen opening sequence with validation
- [ ] Verify editor screen rendering
- [ ] Test assembler functionality after fix

**Implementation Tasks**:
- [ ] Trace exact instruction at crash point
- [ ] Identify ROM function being called
- [ ] Implement missing ROM function (likely graphics/intuition)
- [ ] Verify all ROM function pointers are valid
- [ ] Test iffparse.library requirement (may need implementation)
- [ ] Test reqtools.library requirement (may need implementation)
- [ ] Implement timer.device if required
- [ ] Verify editor screen rendering after fix
- [ ] Test assembler compilation functionality

**Success Criteria**: ASM-One launches, displays editor, allows assembly programming and compilation

---

### Phase 56: DPaint V Deep Dive
**Goal**: Add iffparse.library and enable full paint functionality.

**Status**: ❌ FAILS - Exits immediately with error code 20

**Known Issues**:
- [x] Requires iffparse.library (not in LIBS:) - Implemented in Phase 41
- [ ] May require additional libraries
- [ ] Functionality after library fix unknown

**Testing Requirements**:
- [ ] Verify iffparse.library loads correctly
- [ ] Test IFF file loading (ILBM format)
- [ ] Verify paint screen opens
- [ ] Test drawing tools (brush, line, circle, fill)
- [ ] Verify palette/color selection
- [ ] Test IFF file saving
- [ ] Verify animation features (if present)

**Implementation Tasks**:
- [x] Implement iffparse.library - Completed in Phase 41
- [ ] Deep trace of initialization after library loads
- [ ] Verify ILBM file parsing with test images
- [ ] Test custom screen opening for paint area
- [ ] Verify bitmap allocation for paint buffer
- [ ] Test drawing primitive rendering
- [ ] Verify color palette handling
- [ ] Test file save/load with IFF format
- [ ] Verify undo/redo functionality

**Success Criteria**: DPaint V launches, allows painting with full tool functionality, saves/loads IFF files

---

### Phase 57: Personal Paint Deep Dive
**Goal**: Identify and fix initialization failure.

**Status**: ❌ FAILS - Exits immediately with error code 1

**Known Issues**:
- [ ] Unknown initialization failure
- [ ] No diagnostic output
- [ ] Requires debugging to identify issue

**Testing Requirements**:
- [ ] Enable full trace logging
- [ ] Identify failure point in initialization
- [ ] Test required libraries and devices
- [ ] Verify screen opening sequence
- [ ] Test paint functionality after fix

**Implementation Tasks**:
- [ ] Deep trace of all startup operations
- [ ] Check for required libraries
- [ ] Verify configuration file requirements
- [ ] Test different startup modes
- [ ] Identify exact failure point
- [ ] Implement missing functionality
- [ ] Verify screen and window creation
- [ ] Test paint tools and features

**Success Criteria**: Personal Paint launches and provides full paint functionality

---

### Phase 58: AmigaBasic Deep Dive
**Goal**: Fix background task crashes and enable full BASIC functionality.

**Status**: ⚠️ PARTIAL - Window opens then background tasks crash

**Known Issues**:
- [ ] Requires audio.device (not implemented) - for SOUND command
- [ ] Requires timer.device for background task timing
- [ ] Background tasks crash when devices fail
- [ ] GetPrefs() returns defaults - verify correctness
- [ ] ViewPortAddress() - verify correct viewport
- [ ] OffMenu/OnMenu - verify menu state handling

**Testing Requirements**:
- [ ] Verify main window opens correctly
- [ ] Test BASIC command entry
- [ ] Verify program execution (non-audio commands)
- [ ] Test background task stability after device stubs
- [ ] Verify menu functionality
- [ ] Test program save/load
- [ ] Verify graphics commands (LINE, CIRCLE, etc.)

**Implementation Tasks**:
- [ ] Implement audio.device stub (return success, no audio)
- [x] Implement timer.device - Completed in Phase 45
- [ ] Make device open failures non-fatal for background tasks
- [ ] Verify GetPrefs() returns sensible values
- [ ] Test ViewPortAddress() returns correct viewport pointer
- [ ] Verify menu enable/disable functionality
- [ ] Test BASIC interpreter execution
- [ ] Verify graphics command rendering
- [ ] Test file I/O operations

**Success Criteria**: AmigaBasic launches, allows BASIC programming, handles device failures gracefully

---

## Postponed Phases (later)

### Phase xx: Pixel Array Operations
**Goal**: Implement optimized pixel array functions using AROS-style blitting.

**Background**: Simple WritePixel() loops caused crashes. AROS uses specialized `write_pixels_8()` helpers.

- [ ] ReadPixelLine8, WritePixelLine8 - horizontal lines
- [ ] ReadPixelArray8, WritePixelArray8 - rectangular regions
- [ ] WriteChunkyPixels - fast chunky writes with bytesperrow
- [ ] Handle BitMap formats (interleaved, standard)
- [ ] Layer/clipping integration

---

## Version History

| Version | Phase | Key Changes |
| :--- | :--- | :--- |
| 0.6.2 | 45 | console.device async (CMD_READ pending queue, VBlank hook, AbortIO), exec.c AbortIO fix |
| 0.6.1 | 45 | timer.device async (TR_ADDREQUEST delay queue, VBlank hook, AbortIO) |
| 0.6.0 | 44 | BOOPSI visual completion (propgclass, strgclass input), GadTools DrawBevelBoxA, ASL FileRequester GUI, ROM size 512KB |
| 0.5.9 | 43 | console.device advanced features (CONU_CHARMAP, RawKeyConvert, scrolling regions) |
| 0.5.8 | 42 | datatypes.library basic framework |
| 0.5.7 | 41 | iffparse.library full implementation |
| 0.5.6 | 40 | GFA Basic height fix, Devpac rendering verified |
| 0.5.5 | 39b | Test validation infrastructure |
| 0.5.4 | 38 | Intuition library completion |
| 0.5.3 | 37 | Graphics library completion |
| 0.5.2 | 36 | Exec library completion |

---
