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

## Phase 26: Real-World Application Testing (NEARLY COMPLETE)

**Goal**: Validate lxa compatibility with real Amiga applications beyond just KickPascal 2.

**IMPORTANT**: **all** testing needs to be fully automated, **no** manual testing required.

### Step 26.1: KickPascal 2 Full Compatibility
- [x] **Binary Loading** - KP2 executable loads and validates successfully
- [x] **IDE Launch** - KP2 launches as background process, window opens
- [ ] **File Operations** - Open, Save, New work with KP2 format (deferred - needs ASL)
- [ ] **Text Editing** - All editor features work (cursor, selection, scroll)
- [ ] **Compilation** - KP2 can compile Pascal programs on lxa (deferred)
- [x] **Documentation** - Documented in COMPATIBILITY.md

### Step 26.2: Application Test Suite
- [x] **Create tests/apps/ directory** - Structured testing for real applications
- [x] **Test runner infrastructure** - app_test_runner.sh with APPS: assign setup
- [x] **COMPATIBILITY.md** - Documents all tested apps with status and issues

### Step 26.3: Additional Test Applications

**Working Apps (✅):**
- [x] **KickPascal 2** - IDE opens, accepts input, functional
- [x] **GFA Basic 2** - Editor window opens, accepts input

**Partial Apps (⚠️):**
- [x] **MaxonBASIC** - Opens gadtools, gets screen info, fails with "Konnte keine Bildschirminformation erhalten"
  - Implemented: CreateMenusA (full), GetScreenDrawInfo (dynamic alloc fix), GetVPModeID, QueryOverscan, GetBitMapAttr, ModeNotAvailable
  - Issue: May need dri_CheckMark/dri_AmigaKey Image structures
- [x] **ASM-One v1.48** - Opens Workbench + editor screen, crashes at invalid ROM address (0x00FBEED1)
  - Needs: iffparse.library, reqtools.library, timer.device
- [x] **SysInfo** - Window opens, crashes due to 68030 MMU instructions (PMOVE CRP)
- [x] **Devpac** - Uses BOOPSI (NewObjectA returns NULL), accesses custom chip registers
- [x] **AmigaBasic** - Window opens, crashes (needs audio.device, timer.device)
- [x] **Directory Opus 4** - Needs dopus.library in LIBS: assign

**Failing Apps (❌):**
- [x] **DPaint V** - Exits code 20, needs iffparse.library
- [x] **Personal Paint** - Exits code 1, unknown issue

**Functions Implemented During Phase 26:**
- **gadtools.library** (new):
  - CreateMenusA() - Full implementation creating Menu/MenuItem hierarchies
  - FreeMenus() - Properly frees menu chains with all items/subitems
  - CreateGadgetA(), FreeGadgets(), CreateContext()
  - GetVisualInfoA(), FreeVisualInfo()
  - GT_GetIMsg(), GT_ReplyIMsg(), GT_RefreshWindow()
  - LayoutMenusA(), LayoutMenuItemsA(), DrawBevelBoxA()
- **workbench.library** (new):
  - UpdateWorkbench(), AddAppWindowA/IconA/MenuItemA(), WBInfo()
- **asl.library** (new):
  - AllocAslRequest(), FreeAslRequest(), AslRequest() (returns FALSE - no GUI)
- **Intuition**:
  - GetPrefs() - system preferences with sensible defaults
  - ViewPortAddress() - returns window's screen ViewPort
  - OffMenu()/OnMenu() - menu enable/disable
  - GetScreenData() - copies screen info to buffer
  - IntuiTextLength() - calculates text pixel width
  - GetScreenDrawInfo() - fixed to use dynamic allocation (ROM is read-only!)
  - FreeScreenDrawInfo() - properly frees allocated memory
  - LockPubScreen() - auto-opens Workbench if no screen exists
  - QueryOverscan() - returns standard PAL dimensions
  - AllocRemember()/FreeRemember() - memory tracking
  - GetProgramDir() - returns pr_HomeDir
- **Graphics**:
  - InitGels() - GEL system initialization (stub)
  - GetVPModeID() - returns mode ID from viewport flags
  - ModeNotAvailable() - returns 0 (all modes available)
  - GetBitMapAttr() - returns BMA_HEIGHT/DEPTH/WIDTH/FLAGS
- **Memory Regions**:
  - Extended ROM (0xF00000-0xF7FFFF) - returns 0
  - Zorro-II/III (0x01000000+) - returns 0xFF
  - Autoconfig (0xE80000-0xEFFFFF) - returns 0
  - CIA area (0xBFD000-0xBFFFFF) - returns 0xFF
- **Custom Chips**:
  - DMACON, color register writes now logged/ignored

### Step 26.4: Completion Criteria
- [x] **Track application status** - COMPATIBILITY.md maintained
- [x] **Document failures** - All failures documented with reasons
- [x] **10+ apps tested** - KP2, GFA, MaxonBASIC, ASM-One, SysInfo, Devpac, AmigaBasic, DOpus, DPaint, PPaint

### Step 26.5: Remaining Blockers for Apps (moved to later phases)
The following are needed for more apps but deferred to appropriate phases:
- **timer.device** - See Phase 31 (needed by ASM-One, AmigaBasic)
- **audio.device** - See Phase 31 (needed by AmigaBasic)
- **iffparse.library** - See Phase 34 (needed by DPaint, ASM-One)
- **reqtools.library** - See Phase 32 (needed by ASM-One)
- **BOOPSI** - See Phase 28 (needed by Devpac)
- **68030 instructions** - See Phase 29 (needed by SysInfo)

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

## Phase 28: Advanced UI Frameworks

**Goal**: Implement standard Amiga UI widgets, object-oriented systems, and advanced console features.

### Step 28.1: BOOPSI Foundation (HIGH PRIORITY - needed by Devpac, many apps)
- [ ] **rootclass** - Base class with OM_NEW, OM_DISPOSE, OM_SET, OM_GET, OM_UPDATE
- [ ] **Class Registry** - MakeClass, AddClass, RemoveClass, FindClass, FreeClass
- [ ] **NewObjectA / DisposeObject** - Full object lifecycle management
- [ ] **SetAttrsA / GetAttr** - Attribute get/set with proper method dispatch
- [ ] **DoMethodA / DoSuperMethodA** - Method invocation
- [ ] **CoerceMethodA** - Forced class method dispatch

### Step 28.2: BOOPSI System Classes
- [ ] **imageclass** - Standard image rendering class
- [ ] **frameiclass** - Frame/border rendering
- [ ] **sysiclass** - System imagery (checkmarks, Amiga key, arrows)
- [ ] **gadgetclass** - Base gadget class
- [ ] **propgclass** - Proportional/slider gadget
- [ ] **strgclass** - String gadget class
- [ ] **buttongclass** - Button gadget class
- [ ] **groupgclass** - Gadget grouping

### Step 28.3: GadTools Library Enhancement
Most of gadtools.library is now implemented (Phase 26). Remaining:
- [x] **CreateContext** - Gadget list context management
- [x] **CreateGadgetA** - Create standard gadgets (basic implementation)
- [x] **FreeGadgets** - Free gadget list
- [x] **CreateMenusA** - Create menus from NewMenu array (full implementation)
- [x] **FreeMenus** - Free menu chain (full implementation)
- [x] **LayoutMenusA** - Layout menus (returns TRUE)
- [x] **GT_GetIMsg / GT_ReplyIMsg** - GadTools message handling
- [x] **GT_RefreshWindow** - Refresh gadtools gadgets
- [x] **DrawBevelBoxA** - 3D beveled box rendering (stub)
- [x] **GetVisualInfoA / FreeVisualInfo** - Screen visual information
- [ ] **Full gadget kinds** - BUTTON_KIND, STRING_KIND, INTEGER_KIND, CHECKBOX_KIND, etc.
- [ ] **Gadget rendering** - Actual visual gadget drawing

### Step 28.4: ASL Library Enhancement
Basic asl.library stub exists (Phase 26). For full functionality:
- [x] **AllocAslRequest / FreeAslRequest** - Request structure management
- [x] **AslRequest** - Returns FALSE (no GUI yet)
- [ ] **File Requester GUI** - Bridge to host file dialog or native implementation
- [ ] **Font Requester GUI** - Font selection with preview
- [ ] **Screen Mode Requester** - Display mode selection

### Step 28.5: Advanced Console Features (deferred from Phase 22)
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

## Phase 29: Extended CPU Support

**Goal**: Support applications that use 68020/68030/68040 instructions for hardware detection.

### Step 29.1: 68030 Instruction Emulation
- [ ] **PMOVE** - MMU control register access (PMOVE CRP, PMOVE TC, etc.)
- [ ] **MOVEC** - Control register moves (VBR, CACR, etc.)
- [ ] **Cache Instructions** - CPUSHA, CINV (no-op on emulator)
- [ ] **Trap Handling** - Graceful handling of privileged instructions

### Step 29.2: CPU Detection Compatibility
- [ ] **AttnFlags** - Report appropriate CPU type in ExecBase.AttnFlags
- [ ] **SysInfo Compatibility** - Handle MMU probing without crash
- [ ] **68040.library** - Stub for 68040-specific functions

---

## Phase 30: Hardware Register Emulation

**Goal**: Support applications that access Amiga custom chip registers directly.

### Step 30.1: Custom Chip Register Stubs
- [x] **INTENA** (0xDFF09A) - Interrupt enable (implemented)
- [x] **DMACON** (0xDFF096) - DMA control (no-op, logged)
- [x] **COLOR registers** (0xDFF180+) - Color palette (no-op, logged)
- [ ] **CIA-A/CIA-B** (0xBFE001/0xBFD000) - Timer, keyboard, disk control stubs
- [ ] **Denise** (0xDFF000-0xDFF03F) - Sprite pointers, display control
- [ ] **Agnus** (0xDFF040-0xDFF09F) - Blitter registers (stub or host-side impl)
- [ ] **Paula** (0xDFF0A0-0xDFF0DF) - Audio registers (stub)

### Step 30.2: Safe Hardware Access
- [x] **Memory Map Handler** - Intercept reads/writes to chip space (basic)
- [x] **Extended ROM** (0xF00000-0xF7FFFF) - Returns 0 (no extended Kickstart)
- [x] **Zorro-II/III** (0x01000000+) - Returns 0xFF (no expansion boards)
- [x] **Autoconfig** (0xE80000-0xEFFFFF) - Returns 0 (no boards)
- [ ] **No-op Mode** - Return sensible defaults for all reads
- [ ] **Logging Mode** - Log hardware access for debugging

---

## Phase 31: Exec Devices (HIGH PRIORITY - needed by many apps)

**Goal**: Implement common Exec devices that applications depend on.

### Step 31.1: timer.device
- [ ] **Device Framework** - OpenDevice/CloseDevice/BeginIO/AbortIO
- [ ] **UNIT_MICROHZ / UNIT_VBLANK** - Timer unit types
- [ ] **TR_ADDREQUEST** - Add timed request (integrate with scheduler)
- [ ] **TR_GETSYSTIME** - Get system time
- [ ] **TR_SETSYSTIME** - Set system time (no-op or restricted)
- [ ] **ReadEClock()** - High-resolution timer reading

### Step 31.2: audio.device (Low Priority - stub only)
- [ ] **Device Stubs** - OpenDevice returns error or minimal success
- [ ] **CMD_WRITE** - Accept audio data (discard/no-op)
- [ ] **ADCMD_ALLOCATE/FREE** - Channel allocation (stub)
- [ ] **Note**: Full audio support is out of scope; stub allows apps to continue

### Step 31.3: input.device Enhancement
- [ ] **IND_ADDHANDLER** - Add input handler to chain
- [ ] **IND_REMHANDLER** - Remove input handler
- [ ] **IND_WRITEEVENT** - Inject input events
- [ ] **BeginIO command 9** - Currently warned, needs investigation

### Step 31.4: gameport.device (Low Priority)
- [ ] **Joystick/Mouse Reading** - Basic port reading stubs
- [ ] **Controller Types** - Report no controller connected

---

## Phase 32: Library Path Search & Third-Party Libraries

**Goal**: Support applications that bundle their own libraries and common third-party libraries.

### Step 32.1: Library Search Order
- [ ] **Program Directory** - Search for libs in app's own directory first
- [ ] **PROGDIR:libs/** - Support program-local libs directory
- [ ] **Multi-assign LIBS:** - Add app directories to LIBS: automatically
- [ ] **Config-driven** - Allow apps.json to specify additional lib paths

### Step 32.2: Common Third-Party Libraries
- [ ] **reqtools.library** - Common requester library (needed by ASM-One)
  - rtFileRequest, rtFontRequest, rtScreenModeRequest
  - rtEZRequest, rtGetString, rtGetLong
- [ ] **arp.library** - AmigaRP compatibility library (stub or partial)
- [ ] **xpk libraries** - Compression libraries (stub)
- [ ] **dopus.library** - Directory Opus support library

---

## Phase 33: Quality & Stability Hardening

**Goal**: Ensure production-grade reliability.

### Step 33.1: Memory Debugging
- [ ] Memory tracking, leak detection, double-free detection, buffer overflow guards
- [ ] **Stress Testing** - 24-hour continuous operation, 10,000 file operations.

### Step 33.2: Compatibility Testing
- [ ] Test with common Amiga software, document issues, create compatibility database.

### Step 33.3: Performance Optimization
- [ ] Profile critical paths, optimize VFS, memory allocation, emucall overhead.

---

## Phase 34: IFF & Datatypes Support

**Goal**: Support IFF file format handling for graphics applications.

### Step 34.1: iffparse.library (HIGH PRIORITY - needed by DPaint, ASM-One)
- [ ] **IFF Context Management** - AllocIFF, FreeIFF, OpenIFF, CloseIFF
- [ ] **Chunk Parsing** - ParseIFF, StopChunk, StopOnExit, PropChunk
- [ ] **Chunk Reading** - ReadChunkBytes, ReadChunkRecords
- [ ] **Chunk Writing** - WriteChunkBytes, WriteChunkRecords, PushChunk, PopChunk
- [ ] **Stream Hooks** - InitIFFasDOS, InitIFFasClip
- [ ] **Collection Handling** - CollectionChunk, FindCollection, FindProp
- [ ] **Error Handling** - CurrentChunk, ParentChunk, GoodID, GoodType

### Step 34.2: datatypes.library (Future)
- [ ] **Basic Framework** - ObtainDataTypeA, ReleaseDataType
- [ ] **Picture Datatype** - IFF ILBM loading/display
- [ ] **Text Datatype** - Plain text handling
- [ ] **Note**: Full datatypes is complex, may be deferred

---

## Phase 35: Clipboard Support

**Goal**: Implement clipboard device for copy/paste functionality.

### Step 35.1: clipboard.device
- [ ] **Device Framework** - OpenDevice, CloseDevice, BeginIO
- [ ] **CBD_POST** - Post data to clipboard
- [ ] **CBD_CURRENTREADID / CBD_CURRENTWRITEID** - Clip ID management
- [ ] **CBD_CHANGEHOOK** - Clipboard change notification
- [ ] **Unit 0** - Primary clipboard (bridge to host clipboard)

### Step 35.2: IFF Integration
- [ ] **FTXT clip format** - Text clipboard format
- [ ] **ILBM clip format** - Graphics clipboard format (future)

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
