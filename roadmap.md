# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

**IMPORTANT**: **Always** adhere to project and coding standards outlined in `AGENTS.md`! Test coverage, roadmap updates and documentation are **mandatory** in this project!

---

## Current Status

**Version: 0.6.7** | **Phase 54 In Progress** | **~96 Integration Tests Passing**

The lxa project has achieved a comprehensive AmigaOS-compatible environment with 95%+ library compatibility across Exec, DOS, Graphics, Intuition, and system libraries.

**Recent Fixes**:
- **Trap handler implementation**: Added trap vector handlers (trap #0 - trap #14) in exceptions.s.
  Trap dispatcher checks tc_TrapCode and calls task's trap handler if set, otherwise calls
  EMU_CALL_EXCEPTION. Trap exceptions are now fatal (stop emulator) since continuing after
  traps like stack overflow causes undefined behavior.
- **Library init calling convention fix**: The `libInitFn_t` typedef had incorrect register 
  assignments (D0 and A6 were swapped). Per RKRM, library init should receive D0=Library, 
  A0=SegList, A6=SysBase. Fixed typedef and all 22 ROM library InitLib functions.
- **MakeFunctions pointer arithmetic**: Word-offset format requires skipping 2 bytes (not 1).
- Extended memory map support for Slow RAM (0x00C00000-0x00D7FFFF), Ranger RAM (0x00E00000-0x00E7FFFF),
  Extended ROM (0x00F00000-0x00F7FFFF), and ROM writes. GFA Basic now runs without memory errors.
- Fixed critical Signal/Wait task scheduling bug: When a single task called Wait() and entered
  the dispatch idle loop, Signal() would wake the task but Schedule() would fail to properly
  switch to it because ThisTask still pointed to the waiting task. Fixed Schedule() to exit
  early when ThisTask is not running, and Signal() to trigger reschedule in idle loop case.
- Fixed signal_pingpong test: Child task logic bug caused orphaned task after main exited.
- Fixed DetailPen/BlockPen 0xFF handling: Devpac now renders correctly.

**Devpac Status**: Window renders AND responds to mouse/keyboard input!
**GFA Basic Status**: Now runs without memory errors (screen/window opens, console initialized)!

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

**Status**: ⚠️ PARTIAL - Emulator exits cleanly (no infinite loop), DOpus behavior needs further analysis

**Completed**:
- [x] Fixed infinite loop after tasks exit - emulator now stops correctly
- [x] Added Signal() validation to prevent signaling freed/invalid tasks
- [x] Fixed RemTask() to clear ThisTask before Dispatch (prevents phantom tasks)
- [x] Added Schedule() NULL check for ThisTask in assembly
- [x] Added EMU_CALL_WAIT stop detection when all tasks have exited
- [x] Fixed EXECBASE_THISTAK typo in lxa.c
- [x] Cleaned up debug output in exec.c

**Investigation Findings**:
- DOpus main process returns exit code 1 by design (launcher pattern)
- Main process spawns `dopus_task` background process then exits
- `dopus_task` is created successfully but also exits immediately
- Neither process opens dopus.library, graphics.library, or any other DOpus-specific libraries
- No OpenScreen/OpenWindow calls are made
- The issue occurs in very early initialization (before any library opens)
- Only dos.library and utility.library are opened (C runtime startup)

**Known Issues**:
- [ ] DOpus/dopus_task exit before opening application libraries
- [ ] Root cause unknown - possibly C runtime startup check or early initialization failure  
- [ ] May require instruction-level tracing to identify exact failure point
- [ ] commodities.library not found (non-fatal, if DOpus gets further)
- [ ] rexxsyslib.library not found (non-fatal)
- [x] signal_pingpong test - Fixed (test logic bug caused orphaned child task)

**Testing Requirements**:
- [ ] Extend test framework to capture multi-process failures
- [ ] Add automated detection of exception loops
- [ ] Verify dopus.library loads and initializes correctly
- [ ] Test main window opens with correct dimensions
- [ ] Verify file list rendering
- [ ] Test directory navigation functionality

**Implementation Tasks**:
- [ ] Implement instruction-level tracing for dopus_task startup
- [ ] Check if DOpus expects specific C runtime behavior (pre-V36 startup code?)
- [ ] Verify CreateProc() properly initializes process context
- [ ] Check for WBStartup vs CLI startup differences
- [ ] Verify stack pointer and registers on process entry
- [ ] Implement commodities.library stub (optional, for hotkey support)

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

**Status**: ⚠️ PARTIAL - Memory errors fixed, launches with window, needs functionality verification

**Issues Fixed**:
- [x] OpenScreen() creates 21-pixel tall screen - Fixed in Phase 40 (expanded to 256)
- [x] Memory writes to Slow RAM (0x00C00000-0x00D7FFFF) - Added memory region handling
- [x] Memory writes to Ranger RAM (0x00E00000-0x00E7FFFF) - Added memory region handling
- [x] Memory writes to Extended ROM (0x00F00000-0x00F7FFFF) - Added memory region handling
- [x] Memory writes to ROM area (0x00F80000+) - Added ROM write-protect handling

**Current Behavior**:
- Opens screen (640x256) with title "GFA-BASIC Editor"
- Opens window (640x256)
- Initializes console device
- Enters event loop waiting for input
- No crashes or memory errors

**Remaining Tasks**:
- [ ] Verify editor window renders with correct dimensions
- [ ] Test text input in editor
- [ ] Verify line numbering display
- [ ] Test BASIC command execution (PRINT, INPUT, etc.)
- [ ] Verify graphics commands (LINE, CIRCLE, etc.)
- [ ] Test program save/load functionality

**Success Criteria**: GFA Basic launches with correct screen size, allows BASIC programming, executes commands

---

### Phase 49: Devpac (HiSoft) Deep Dive
**Goal**: Fix window rendering bug and achieve full assembler IDE functionality.

**Status**: ✅ WORKING - Devpac window opens and responds to input!

**Issues Fixed**:
1. **DetailPen/BlockPen 0xFF Bug** (Commit 73621b2):
   - Devpac passes `DetailPen=255` and `BlockPen=255` (0xFF) in its NewWindow structure
   - Per RKRM, when these values are ~0 (0xFF), the system should use screen defaults
   - Fixed in `_intuition_OpenWindow()` to map 0xFF pen values to screen defaults

2. **Signal/Wait Task Scheduling Bug** (Commit 31f5918):
   - Devpac's window rendered but wouldn't respond to mouse/keyboard input
   - Root cause: When single task calls Wait() and enters dispatch idle loop,
     VBlank interrupt's Signal() would wake the task, but Schedule() would
     try to re-add ThisTask (which was still pointing to the waiting task)
     to TaskReady, corrupting the list or preventing the task from running
   - Fixed by:
     a) Schedule() now exits early if ThisTask->tc_State != TS_RUN (let dispatch loop handle it)
     b) Signal() now triggers reschedule when ThisTask is not running (idle loop case)

3. **Library Init Calling Convention Bug** (Current Session):
   - When Devpac tries to load `amigaguide.library` from disk, the library's init 
     function was called with wrong register convention
   - The `libInitFn_t` typedef had D0 and A6 swapped. Per RKRM, library init should be:
     - D0 = Library pointer
     - A0 = Segment list (BPTR)  
     - A6 = SysBase
   - Fixed typedef and all 22 ROM library InitLib function signatures

4. **MakeFunctions Pointer Arithmetic Bug**:
   - `(CONST_APTR)___funcInit+1` only adds 1 byte, but word-offset format requires
     skipping 2 bytes (the -1 marker)
   - Fixed to `(WORD *)___funcInit+1`

5. **GCC ICE Workaround**:
   - Changing LOG_LEVEL to LOG_INFO triggered a GCC internal compiler error on
     functions with a5/a6 register constraints (LockLayerRom, UnlockLayerRom, 
     AttemptLockLayerRom)
   - Fixed with `__attribute__((optimize("O0")))` on affected functions

**Ghidra decompiled source**: ~/.lxa/System/Apps/Devpac/Devpac.c

**Completed**:
- [x] Fixed DetailPen/BlockPen 0xFF handling in OpenWindow()
- [x] Window title bar now renders correctly
- [x] Window borders render with proper 3D effect
- [x] Fixed Signal/Wait scheduling bug - window now responds to input
- [x] Fixed library init calling convention - disk libraries now load properly
- [x] amigaguide.library now loads (fails to open datatype, but gracefully)
- [x] All integration tests pass

**Remaining Tasks**:
- [ ] Verify editor content area renders properly
- [ ] Verify menu bar displays
- [ ] Test text input in editor
- [ ] Verify syntax highlighting for assembly code
- [ ] Test assembler execution with simple program
- [ ] Verify error messages display

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

**Status**: ✅ WORKING - Window opens, application runs, waiting for user input!

**Ghidra decompiled source**: ~/.lxa/System/Apps/BTII/BT-II.c

**Issues Fixed**:
- [x] **CreateProc/cli_Module bug**: BeckerText reads cli_Module from CLI structure to get its seglist for spawning child processes. Bootstrap wasn't setting pr_SegList or cli_Module before running loaded programs. Fixed in exec.c bootstrap.
- [x] **Assembly bug in exceptions.s**: Line 123 had `move.w 0xffff, IDNestCnt(a6)` (reads from address 0xffff) instead of `move.w #0xffff, IDNestCnt(a6)` (immediate -1).
- [x] **Register clobbering bug in _bootstrap**: GCC caches function pointers (like lprintf) in callee-saved registers A2-A5. External programs like BeckerText don't preserve these registers on return. Added `__asm__ __volatile__` register clobber after childfn() call to force GCC to reload registers.
- [x] **GOT corruption bug in _bootstrap**: GCC uses A5-relative GOT (Global Offset Table) to access function pointers like `emu_stop`. External programs may write to this memory area, corrupting the GOT entries. Fixed by using direct inline assembly for EMU_CALL_STOP instead of calling emu_stop() function.
- [x] **Invalid memory reads**: Changed mread8 at invalid addresses (0xfffffffc etc.) from triggering debugger to just printing warnings. Some programs intentionally read from these addresses (checking for memory expansion, sentinel values, etc.)

**Current Behavior**:
- CreateProc successfully creates child process "BT-II" with seglist
- Main process calls EMU_CALL_STOP which redirects to RemTask(NULL) since child is running
- Child process "BT-II" continues execution
- Opens Workbench screen (640x256)
- Window titled "BECKERtext II" opens at position (170, 97) with size (300x62)
- Window frame renders correctly
- Application enters event loop waiting for user input
- Some harmless invalid memory reads at 0xfffffffc area during initialization (BT-II checking for features)

**Testing Requirements**:
- [ ] Verify full window content renders (not just frame)
- [ ] Test text editing functionality
- [ ] Verify menu structure (if present)
- [ ] Test file save/load operations

**Success Criteria**: BeckerText II launches and displays fully functional text editor window

---

### Phase 54: Oberon 2 Deep Dive
**Goal**: Fix trap handling and enable Oberon system.

**Status**: ⚠️ PARTIAL - Trap handlers now implemented, but Oberon has corrupt A5 register

**Investigation Findings**:
- Oberon runtime uses `trap #2` for stack overflow checking
- Code pattern: `cmpa.l A7, A5; bls skip; trap #2`
- A5 should contain stack limit, but is 0xffffffff (corrupt)
- With A5=0xffffffff, stack check always fails triggering trap #2

**Completed**:
- [x] Implemented trap vector handlers (trap #0 - trap #14) in exceptions.s
- [x] Set up trap vectors in coldstart() at addresses 0x80-0xB8
- [x] Trap dispatcher checks tc_TrapCode and calls task's handler if set
- [x] Default trap handler calls EMU_CALL_EXCEPTION
- [x] Trap exceptions (vectors 32-46) now halt emulator (fatal errors)

**Root Cause Analysis**:
- The Oberon runtime expects A5 to be set to the stack bottom/limit
- A5 is used as a frame pointer or global base pointer
- A5 is corrupt (0xffffffff) at the point of the stack check
- This is likely a startup/initialization issue in how Oberon is launched

**Remaining Tasks**:
- [ ] Investigate why A5 is corrupt in Oberon runtime
- [ ] Check if Oberon uses special WBStartup or tooltype handling
- [ ] Verify process stack setup provides proper values for A5
- [ ] Check if Oberon runtime expects specific register values on entry
- [ ] May need decompilation/disassembly of Oberon to understand startup

**Testing Command**:
```bash
cd ~/.lxa && echo "run SYS:Apps/Oberon/Oberon" | timeout 15 /home/guenter/projects/amiga/lxa/src/lxa/build/host/bin/lxa
```

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
| 0.6.7 | 54 | Trap handler implementation (trap #0-#14), trap dispatch via tc_TrapCode, fatal trap handling |
| 0.6.6 | 49 | Fixed library init calling convention (D0/A0/A6), MakeFunctions pointer fix, disk libraries now load |
| 0.6.5 | 49+ | Extended memory map (Slow RAM, Ranger RAM, Extended ROM, ROM writes), GFA Basic now runs |
| 0.6.4 | 49 | Fixed Signal/Wait scheduling bug (Devpac now responds to input), signal_pingpong test fix |
| 0.6.3 | 49 | Fixed DetailPen/BlockPen 0xFF handling (Devpac window renders correctly) |
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
