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
SDL2 integration, graphics.library with BitMap/RastPort, basic rendering (pixel ops, lines, pens), screen management, comprehensive test suite.

### Phase 14: Intuition Basics (COMPLETE)
Window management (OpenWindow/CloseWindow), IDCMP input handling, mouse tracking, screen linking.

### Phase 15: Rootless Mode & Advanced Graphics (COMPLETE)
Rootless windowing, blitter operations, text rendering (Topaz-8), AutoRequest/EasyRequest.

### Phase 16: Console Device Enhancement (COMPLETE)
Full ANSI/CSI escape sequence support: cursor control, text attributes (SGR), cursor visibility, comprehensive test coverage.

### Phase 17: Additional Commands (COMPLETE)
System commands (AVAIL, RESIDENT), script support (ASK, REQUESTCHOICE), shell enhancements (CD, WHICH, PATH).

### Phase 18: App-DB & KickPascal 2 Testing (COMPLETE)
App manifest system, library loading from disk, RomTag support, test runner infrastructure.

### Phase 19: Graphics & Layers Completion (COMPLETE)
Full layers.library (creation, locking, damage tracking), region functions, window invalidation.

### Phase 20: KickPascal 2 Compatibility (COMPLETE)
Multi-assign support, automatic program-local assigns, WBenchToFront, console device window attachment.

### Phase 21: UI Testing Infrastructure (COMPLETE)
Input injection, screen capture, headless mode, ROM-side test helpers.

### Phase 22: Console Device Completion (COMPLETE)
Authentic AmigaOS console.device behavior with comprehensive CSI support.

### Phase 23: Graphics Library Consolidation (COMPLETE)
Authentic BitMap/RastPort/View/ViewPort structures, correct GfxBase fields.

### Phase 24: Layers Library Consolidation (COMPLETE)
Authentic clipping, locking, and damage semantics matching RKRM.

### Phase 25: Intuition Library Consolidation (COMPLETE)
Authentic windowing, screen, input behavior, IDCMP, system gadgets, menus.

### Phase 26: Real-World Application Testing (COMPLETE)
**Goal**: Validate lxa compatibility with real Amiga applications.
**Achievements**:
- **10+ Apps Tested**: Confirmed working (KickPascal 2, GFA Basic), Partial (EdWordPro, MaxonBASIC, Devpac, ASM-One, SysInfo), and Failing (DPaint, PPaint).
- **EdWordPro Success**: Opens custom screen, editor window, and message window; enters event loop.
- **New Libraries Stubbed**:
  - `gadtools.library` (menus, gadgets, visual info)
  - `workbench.library` (AppWindow/Icon stubs)
  - `asl.library` (request stubs)
- **Intuition/Graphics Enhancements**:
  - `sysiclass` BOOPSI support (dummy images)
  - `InitArea` polygon filling
  - `LockIBase`, `ShowTitle`, `SetWindowTitles`, `SetPointer`
  - Dynamic `DrawInfo` allocation
- **Memory/Hardware Compat**:
  - Pseudo-ROM (0xF80000) handling
  - CIA/Custom chip register read/write stubs (0xBFxxxx, 0xDFFxxx)
- **Documentation**: Comprehensive `COMPATIBILITY.md` created.

---

## Active Phases

## Phase 27: Core System Libraries & Preferences (COMPLETE)
**Goal**: Improve visual fidelity, system configuration, and core library support.
- [x] **diskfont.library** - OpenDiskFont, AvailFonts stub.
- [x] **icon.library** - Full function set implemented.
- [x] **expansion.library** - ConfigChain stubs.
- [x] **translator.library** - Translate() stub.
- [x] **locale.library** - Character classification, OpenLocale.
- [x] **FONTS: Assign** - Automatic provisioning of font directories.
- [x] **Topaz Bundling** - Built-in Topaz-8 support in graphics.library.
- [x] **ENVARC: Persistence** - Map `ENVARC:` to `SYS:Prefs/Env-Archive` (mapped to host).

## Phase 28: Advanced UI Frameworks (COMPLETE)
**Goal**: Implement standard Amiga UI widgets and object-oriented systems.

### Step 28.1: BOOPSI Foundation (COMPLETE)
- [x] **sysiclass/imageclass** - Basic stubs implemented in Phase 26 (returns dummy images).
- [x] **rootclass** - Base class with OM_NEW, OM_DISPOSE, OM_SET, OM_GET.
- [x] **Class Registry** - MakeClass, AddClass, RemoveClass.
- [x] **NewObjectA / DisposeObject** - Full object lifecycle.
- [x] **Gadget Classes** - gadgetclass, propgclass, strgclass, buttongclass implemented with attribute handling.

### Step 28.2: GadTools & ASL Enhancement (COMPLETE)
- [x] **gadtools** - Mostly implemented in Phase 26.
- [x] **Gadget Rendering** - Basic rendering stubs in place (GM_RENDER methods).
- [x] **ASL Requesters** - Stub implementation returns FALSE (full GUI deferred to Phase 36).

**Note**: Full visual rendering for gadgets and ASL GUI requesters deferred to Phase 36.

## Phase 29: Exec Devices (IN PROGRESS - 3/5 COMPLETE)
**Goal**: Implement common Exec devices needed by tested apps.

### Step 29.1: Critical Devices (3/3 COMPLETE)
- [x] **timer.device** - TR_GETSYSTIME, CMD_READ implemented (Phase 29 complete). TR_ADDREQUEST stubs (completes immediately; async deferred to Phase 37).
- [x] **clipboard.device** - CMD_READ, CMD_WRITE, CMD_UPDATE implemented with in-memory storage (up to 256KB).
- [x] **input.device** - Fixed command 9 warning (BeginIO now properly handles all commands silently).

### Step 29.2: Stubbed Devices (0/2 COMPLETE)
- [ ] **audio.device** - Stub open/cmd (AmigaBasic).
- [ ] **gameport.device** - Stub joystick/mouse.

## Phase 30: Third-Party Libraries
**Goal**: Support common libraries used by apps.
- [ ] **reqtools.library** - Needed by ASM-One, EdWordPro.
- [ ] **keymap.library** - Needed by EdWordPro.
- [ ] **iffparse.library** - Needed by DPaint, ASM-One.
- [ ] **dopus.library** - Needed by Directory Opus.

## Phase 31: Extended CPU & Hardware Support
**Goal**: Support apps accessing hardware directly.
- [ ] **68030 Instructions** - MMU stubs (PMOVE) for SysInfo.
- [ ] **Custom Chip Registers** - Logging/no-op for remaining registers (Denise, Agnus).
- [ ] **CIA Resources** - cia.resource for timer access.

## Phase 32: Re-visiting Application Testing
**Goal**: Return to application testing after blocking issues are resolved.
- [ ] **EdWordPro** - Implement input handling to make it interactive.
- [ ] **MaxonBASIC** - Fix divide-by-zero (display calc) issues.
- [ ] **Devpac** - Debug crash after window open.
- [ ] **Full Regression Test** - Retest all 10+ apps with new libraries/devices.

---

## Future Phases

### Phase 33: Quality & Stability Hardening
- Memory tracking, leak detection, stress testing.

### Phase 34: IFF & Datatypes Support (Detailed)
- [ ] **iffparse.library** - Full implementation (moved from Phase 30).
- [ ] **datatypes.library** - Basic framework.

### Phase 35: Advanced Console Features
- CONU_CHARMAP, RawKeyConvert, advanced CSI sequences.

### Phase 36: BOOPSI & GadTools Visual Completion
**Goal**: Complete visual rendering for BOOPSI gadgets and ASL requesters.
- [ ] **GM_RENDER Implementation** - Full rendering for buttongclass, propgclass, strgclass.
  - Bevel boxes, 3D highlighting, text rendering
  - Proportional gadget knobs and containers
  - String gadget cursor and text display
- [ ] **GadTools Rendering** - Implement DrawBevelBoxA and gadget visual feedback.
- [ ] **ASL GUI Requesters** - Full interactive file/font requester implementation.
  - File/directory browser with scrolling
  - Pattern matching and filtering
  - Font selection with preview
- [ ] **Input Handling** - Complete GM_HANDLEINPUT for all gadget types.
  - String editing (insert, delete, cursor movement)
  - Proportional gadget dragging
  - Keyboard shortcuts and tab cycling

### Phase 37: Async I/O & Timer Completion
**Goal**: Implement proper asynchronous I/O for devices.
- [ ] **timer.device Async** - TR_ADDREQUEST with proper delay queue and timeout handling.
- [ ] **console.device Async** - Non-blocking reads with timeout support.
- [ ] **Event Loop Integration** - Coordinate device async I/O with WaitPort/Wait().

---
