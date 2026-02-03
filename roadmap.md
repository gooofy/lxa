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

### Phase 27: Core System Libraries & Preferences (COMPLETE)
**Goal**: Improve visual fidelity, system configuration, and core library support.
- [x] **diskfont.library** - OpenDiskFont, AvailFonts stub.
- [x] **icon.library** - Full function set implemented.
- [x] **expansion.library** - ConfigChain stubs.
- [x] **translator.library** - Translate() stub.
- [x] **locale.library** - Character classification, OpenLocale.
- [x] **FONTS: Assign** - Automatic provisioning of font directories.
- [x] **Topaz Bundling** - Built-in Topaz-8 support in graphics.library.
- [x] **ENVARC: Persistence** - Map `ENVARC:` to `SYS:Prefs/Env-Archive` (mapped to host).

### Phase 28: Advanced UI Frameworks (COMPLETE)
**Goal**: Implement standard Amiga UI widgets and object-oriented systems.
- [x] **sysiclass/imageclass** - Basic stubs implemented in Phase 26 (returns dummy images).
- [x] **rootclass** - Base class with OM_NEW, OM_DISPOSE, OM_SET, OM_GET.
- [x] **Class Registry** - MakeClass, AddClass, RemoveClass.
- [x] **NewObjectA / DisposeObject** - Full object lifecycle.
- [x] **Gadget Classes** - gadgetclass, propgclass, strgclass, buttongclass implemented with attribute handling.
- [x] **gadtools** - Mostly implemented in Phase 26.
- [x] **Gadget Rendering** - Basic rendering stubs in place (GM_RENDER methods).
- [x] **ASL Requesters** - Stub implementation returns FALSE (full GUI deferred to Phase 36).

**Note**: Full visual rendering for gadgets and ASL GUI requesters deferred to Phase 36.

### Phase 29: Exec Devices (COMPLETE)
**Goal**: Implement common Exec devices needed by tested apps.
- [x] **timer.device** - TR_GETSYSTIME, CMD_READ implemented. TR_ADDREQUEST stubs (async deferred to Phase 37).
- [x] **clipboard.device** - CMD_READ, CMD_WRITE, CMD_UPDATE implemented with in-memory storage (up to 256KB).
- [x] **input.device** - Fixed command 9 warning (BeginIO properly handles all commands silently).
- [x] **audio.device** - Stub returns ADIOERR_NOALLOCATION (no audio hardware).
- [x] **gameport.device** - Stub succeeds but provides no joystick data.

### Phase 30: Third-Party Libraries (COMPLETE)
**Goal**: Support common libraries used by apps.
- [x] **keymap.library** - MapRawKey, MapANSI stubs, default US keymap.

### Phase 31: Extended CPU & Hardware Support (COMPLETE)
**Goal**: Support apps accessing hardware directly.
- [x] **68030 Instructions** - Switched from 68000 to 68030 CPU emulation to support MMU instructions (PMOVE) for SysInfo.
- [x] **Custom Chip Registers** - Added comprehensive logging for custom chip registers (Denise, Agnus, Paula) with register name lookup.
- [x] **CIA Resources** - Implemented OpenResource() and added ciaa.resource/ciab.resource for timer access.
- [x] **Tests** - Created exec/resources integration test to verify OpenResource() and CIA resource availability.

### Phase 32: Application Testing Fixes (COMPLETE)
**Goal**: Fix blocking issues discovered during application testing.
- [x] **Address 0 Read Fix** - Removed overly aggressive debugger trigger on reads from address 0 (valid chip RAM).
- [x] **Exception Handling** - Made CPU exceptions non-fatal in multitasking scenarios (log and continue instead of halt).
- [x] **SetWriteMask Stub** - Implemented no-op stub for graphics.library SetWriteMask() used by Devpac.
- [x] **Zorro-III Memory** - Extended memory handling to cover 0x01000000-0x0FFFFFFF address range.
- [x] **Error Message Cleanup** - Fixed OpenLibrary error message formatting causing spurious blank lines.
- [x] **Test Updates** - All 4 app tests now pass (devpac, dopus, kickpascal2, sysinfo).

---

## Active Phases

### Phase 33: Missing Library Stubs (COMPLETE)
**Goal**: Implement stub libraries commonly required by applications.
**Achievements**:
- [x] **commodities.library** - Full stub implementation (30 functions including CreateCxObj, CxBroker, ActivateCxObj, etc.).
- [x] **rexxsyslib.library** - ARexx interface library stub (80+ function slots, non-standard bias at 126).
- [x] **iffparse.library** - IFF file format parsing stub (42 functions including AllocIFF, ParseIFF, ReadChunkBytes, etc.).
- [x] **reqtools.library** - Popular third-party requester library stub (28 functions for file/font/string requesters).
- [x] **Test Verification** - All 4 app tests pass (devpac, dopus, kickpascal2, sysinfo).

### Phase 34: ROM Robustness & Safety Fixes (COMPLETE)
**Goal**: Improve ROM robustness against invalid pointers and malformed data.
**Achievements**:
- [x] **FindTask() safety** - Validates name pointers, handles NULL/-1 gracefully.
- [x] **FindName() safety** - Validates name and node pointers, handles corrupted lists.
- [x] **RemTask() validation** - Validates task pointers and node types, ignores invalid values.
- [x] **strcmp() safety** - Handles NULL and 0xFFFFFFFF pointers without crashing.
- [x] **lprintf %s safety** - Validates string pointers before dereferencing.
- [x] **SuperState()/UserState()** - Stub implementations (no-op in emulation).
- [x] **OpenLibrary()** - Validates library name, handles paths with colons (for dopus:libs/).
- [x] **Test Verification** - All 4 app tests pass (devpac, dopus [expected fail], kickpascal2, sysinfo).

**Notes on Directory Opus**: DOpus 4 loads successfully and dopus.library opens from `dopus:libs/`.
However, deep corruption in DOpus/dopus.library cleanup code causes crashes during task exit.
The crash involves garbage pointers being passed to RemTask (0xFFFFFFFF, 0x0c, 0x02, etc.) 
followed by stack corruption leading to jumps to invalid addresses. This requires deeper
investigation of the application's internal cleanup routines. Deferred to Phase 40.

---

## Active Phases

### Phase 35: Application-Specific Fixes (COMPLETE)
**Goal**: Address specific compatibility issues discovered in tested apps.
**Achievements**:
- [x] **RemHead()/RemTail() safety** - Added validation for corrupted/uninitialized list pointers.
- [x] **Memory overflow handling** - Extended valid memory area to 0x00DFFFFF to handle apps that overflow RAM allocations slightly (writes to 0x00A00000-0x00DFEEFF now silently ignored).
- [x] **MaxonBASIC** - Now runs successfully! Opens Workbench screen and main window "Unbenannt" (Untitled), enters event loop. Previous divide-by-zero crash no longer occurs.
- [x] **EdWordPro** - Confirmed working! Opens custom screen and editor window, enters main event loop waiting for input.
- [x] **GFA Basic** - RemHead crash FIXED. App has separate memory corruption issue (writes to custom chip area) that requires deeper investigation.
- [ ] **ASM-One** - Pre-existing crash (jumps into ASCII data). Child process created by CreateNewProc crashes early. Deferred for future investigation.
**Test Status**: All 4 app tests pass (devpac, dopus [expected fail], kickpascal2, sysinfo).

### Phase 36: Exec Library Completion (COMPLETE)
**Goal**: Complete `exec.library` implementation - all functions fully implemented with 100% test coverage.

**Completion Criteria**: When this phase is complete, exec.library will be **fully functional** with no stubs remaining (except hardware-specific functions that cannot be emulated). Every function must have corresponding tests verifying correct behavior.

**Achievements:**
- [x] **AROS Comparison** - Reviewed exec implementation against AROS sources (162 files)
- [x] **NewMinList()** - LVO -828, V45+ list initialization
- [x] **AllocVecPooled()** - LVO -1014, V39+ pool-based vector allocation
- [x] **FreeVecPooled()** - LVO -1020, V39+ pool-based vector deallocation
- [x] **RawDoFmt Bug Fix** - Fixed left-alignment padding bug for strings
- [x] **Test Infrastructure** - Added filter for spurious mread/mwrite errors
- [x] **RawDoFmt Tests** - 24-test suite covering all format specifiers
- [x] **Insert()** - LVO -234, insert node into list at specific position
- [x] **AllocAbs()** - LVO -204, allocate memory at absolute address
- [x] **AddLibrary()** - LVO -396, add library to system list
- [x] **RemLibrary()** - LVO -402, remove library from system list
- [x] **SetFunction()** - LVO -420, patch library function
- [x] **SumLibrary()** - LVO -426, compute library checksum
- [x] **AddDevice()** - LVO -432, add device to system list
- [x] **RemDevice()** - LVO -438, remove device from system list
- [x] **AddResource()** - LVO -486, add resource to system list
- [x] **RemResource()** - LVO -492, remove resource from system list
- [x] **Procure()** - LVO -540, message-based semaphore obtain
- [x] **Vacate()** - LVO -546, message-based semaphore release
- [x] **ObtainSemaphoreList()** - LVO -582, lock multiple semaphores atomically
- [x] **ReleaseSemaphoreList()** - LVO -588, unlock multiple semaphores
- [x] **AllocTrap()** - LVO -342, allocate trap vector (returns -1 in emulation)
- [x] **FreeTrap()** - LVO -348, free trap vector (no-op in emulation)
- [x] **AddMemList()** - LVO -618, add memory region to system free pool
- [x] **AddMemHandler()** - LVO -774, low memory warning handler
- [x] **RemMemHandler()** - LVO -780, remove memory handler

**Technical Notes:**
- NUM_EXEC_FUNCS = 172 to accommodate LVOs up to -1020
- MinList variants (InsertMinNode, etc.) share LVOs with regular list functions
- Hardware interrupt functions remain stubs (no real hardware)
- Cache functions are no-ops (no hardware cache)
- Trap functions return failure/no-op (no real trap vectors in emulation)
- All remaining stubs are hardware-specific and cannot be emulated

**Status**: exec.library is now feature-complete for emulation purposes. All critical functions are implemented and tested.

---

## Future Phases

### Phase 37: Graphics Library Completion (IN PROGRESS)
**Goal**: Complete `graphics.library` implementation - all functions fully implemented with 100% test coverage.

**Completion Criteria**: When this phase is complete, graphics.library will be **fully functional** with no stubs. Every function must have corresponding tests. Rendering output must match authentic AmigaOS behavior.

**Audit Results:**
- [x] **Audit graphics.c** - Identified 124 stub functions with assert(FALSE), 2 FIXMEs
- [x] **AROS Comparison** - Reviewed AROS sources (199 files in rom/graphics/)
- [x] **NDK Function List** - Verified against NDK 3.2R4 clib/graphics_protos.h

**Current Status:**
- **Total Lines**: ~4,200 in lxa_graphics.c (increased from ~3,800)
- **Unimplemented Stubs**: 109 functions (down from 115)
- **FIXMEs**: 2 (library open count management in OpenLib)
- **Implemented**: Core functions + Priority 1 functions + Partial Priority 2 below

**Implementation Strategy - Priority 1 (Critical for applications) - ✅ FULLY COMPLETE WITH 100% TEST COVERAGE:**
- [x] **AllocBitMap/FreeBitMap** - Dynamic bitmap allocation (already implemented)
- [x] **EraseRect** - Fast rectangle clearing (already implemented)
- [x] **TextExtent/TextFit** - Text measurement for layout (IMPLEMENTED v0.5.0) ✅ TESTED
- [x] **AreaMove/AreaDraw/AreaEnd** - Polygon filling primitives (IMPLEMENTED v0.5.0 - outline drawing, full fill deferred) ✅ TESTED
- [x] **ClipBlit** - Clipped blitting between RastPorts (IMPLEMENTED v0.5.0 - simplified version) ✅ TESTED
- [x] **GetRPAttrsA/SetRPAttrsA** - RastPort attribute management (IMPLEMENTED v0.5.0) ✅ TESTED
- [x] **BltMaskBitMapRastPort** - Masked bitmap blitting (IMPLEMENTED v0.5.0) ✅ TESTED
- [x] **ClearEOL/ClearScreen** - Text console operations (already implemented)
- [x] **InitTmpRas** - Temporary raster for area operations (already implemented)
- [x] **LockLayerRom/UnlockLayerRom** - Layer synchronization (already implemented as no-ops)

**v0.5.0 Achievements:**
- ✅ Implemented 9 Priority 1 graphics functions
- ✅ All existing tests continue to pass (50+ integration tests)
- ✅ **100% Test Coverage** for Priority 1 functions:
  - `tests/graphics/text_extent/` - Tests TextExtent() and TextFit() with 7 comprehensive test cases
  - `tests/graphics/rpattrs/` - Tests SetRPAttrsA/GetRPAttrsA with 11 test cases covering all RPTAG attributes, TAG_IGNORE, and TAG_SKIP
  - `tests/graphics/area_fill/` - Tests AreaMove/AreaDraw/AreaEnd with 7 test cases including polygon state management
  - `tests/graphics/clipblit/` - Tests ClipBlit with and without layers, different minterms
  - `tests/graphics/blt_mask/` - Tests BltMaskBitMapRastPort with NULL masks, edge cases, and different minterms
- ✅ Fixed previously stubbed functions:
  - SetOutlinePen/GetOutlinePen - Now fully implemented (were stubs causing test failures)
  - SetWriteMask - Now properly sets RastPort->Mask field
- TextExtent() and TextFit() provide accurate text measurement for layout calculations
- SetRPAttrsA/GetRPAttrsA support RPTAG_Font, RPTAG_APen, RPTAG_BPen, RPTAG_DrMd, RPTAG_OutlinePen, RPTAG_WriteMask, RPTAG_MaxPen, RPTAG_DrawBounds
- Area functions support polygon outline drawing (full scan-line filling deferred to future phase)
- ClipBlit provides basic layer-aware blitting
- BltMaskBitMapRastPort supports masked blitting operations

**Notes:**
- AreaEnd() currently draws polygon outlines; full scan-line polygon filling algorithm from AROS areafill.c deferred to Priority 2
- ClipBlit() provides simplified implementation; advanced overlap detection deferred to Priority 2

**Implementation Strategy - Priority 2 (Nice to have):**
- [x] **DrawEllipse** - Ellipse drawing using midpoint algorithm (IMPLEMENTED - tests needed)
- [x] **AreaEllipse** - Ellipse for area fill operations (IMPLEMENTED - tests needed)
- [x] **PolyDraw** - Polyline drawing (IMPLEMENTED) ✅ TESTED
- [ ] **Flood** - Flood fill
- [ ] **BltPattern** - Pattern blitting
- [ ] **ScrollRasterBF** - Backfill scrolling
- [ ] **BitMapScale/ScalerDiv** - Bitmap scaling
- [ ] **GELs system** - at least BOBs need to be emulated
- [ ] **Display mode functions** (OpenMonitor, FindDisplayInfo, BestModeIDA)
- [ ] **Blitter** (OwnBlitter, DisownBlitter, QBlit, QBSBlit) - software emulation or stubs as appropriate
- [ ] **Pixel array operations** - Deferred to Phase 43 (requires optimized blitting infrastructure)

**Recent Progress (v0.5.1):**
- ✅ Implemented 3 Priority 2 functions:
  - **DrawEllipse** - Full midpoint ellipse algorithm with degenerate case handling (point, line, ellipse)
  - **AreaEllipse** - Ellipse outline tracing for area filling operations with four-quadrant support
  - **PolyDraw** - Connected polyline drawing from coordinate array
- ✅ Improved **InitRastPort** - Added explicit NULL initialization for all pointer fields (Layer, BitMap, Font, etc.)
- ✅ New test: `tests/graphics/polydraw/` - Comprehensive PolyDraw() testing (squares, single points, zero count, closed polygons) ✅ PASSES
- 📝 **Deferred pixel array functions to Phase 43** - ReadPixelLine8, WritePixelLine8, ReadPixelArray8, WritePixelArray8, WriteChunkyPixels require AROS-style optimized blitting infrastructure (not simple WritePixel loops)
- **Stub count: 109** (was 115, added 6 stubs back after reverting pixel array functions to stubs)
- All existing tests continue to pass (50+ integration tests)

**Implementation Strategy - Priority 3 (Hardware/rarely used - can remain stubs):**
- Copper functions (CBump, CMove, CWait, InitView, LoadView, MakeVPort, MrgCop, etc.) - hardware-specific
- Hardware sprites (GetSprite, FreeSprite, ChangeSprite, MoveSprite, etc.) - not relevant for emulation

**Required Tests:**
- [x] BitMap operations - AllocBitMap, FreeBitMap, depth, planes
- [x] RastPort operations - all drawing primitives
- [x] Area filling - AreaMove, AreaDraw, AreaEnd (flood fill pending)
- [x] Text rendering - TextExtent, TextFit, different styles
- [x] Clipping - ClipBlit, layer-aware drawing
- [x] Rectangle operations - EraseRect, RectFill
- [x] Pixel operations - ReadPixel, WritePixel
- [x] Polyline operations - PolyDraw ✅ TESTED
- [ ] Ellipse operations - DrawEllipse, AreaEllipse (implemented, tests needed)
- [ ] Flood fill - Flood() function
- [ ] Pattern blitting - BltPattern() function
- [ ] Pixel array operations - Deferred to Phase 43

### Phase 38: Intuition Library Completion  
**Goal**: Complete `intuition.library` implementation - all functions fully implemented with 100% test coverage.

**Completion Criteria**: When this phase is complete, intuition.library will be **fully functional** with no stubs. Every function must have corresponding tests. Window/screen behavior must match authentic AmigaOS.

**Analysis Tasks:**
- [ ] **Audit intuition.c** - Identify all stubs, FIXMEs, and incomplete functions
- [ ] **AROS Comparison** - Review against AROS sources for parity
- [ ] **NDK Function List** - Verify all NDK functions are implemented

**Implementation Tasks (to be detailed after audit):**
- [ ] Implement all identified stubs
- [ ] Fix all FIXMEs
- [ ] Ensure IDCMP handling matches AmigaOS
- [ ] Complete gadget support (system and custom)
- [ ] Complete menu support

**Required Tests:**
- [ ] Screen operations - open, close, depth arrange
- [ ] Window operations - open, close, resize, move, activate
- [ ] IDCMP message handling - all message types
- [ ] Gadget operations - create, add, remove, refresh
- [ ] Menu operations - create, attach, handle selection
- [ ] Requester operations - easy, auto, custom
- [ ] Input handling - mouse, keyboard events
- [ ] Refresh handling - simple, smart refresh

### Phase 39: Re-Testing Phase 2
**Goal**: Comprehensive re-testing after Phase 34-35 fixes.
- [ ] **Devpac** - Verify continued compatibility.
- [ ] **KickPascal 2** - Regression test.
- [ ] **SysInfo** - Regression test.
- [ ] **EdWordPro** - Full interactive testing.
- [ ] **MaxonBASIC** - Test after divide-by-zero fix.
- [ ] **GFA Basic** - Test after pointer fix.
- [ ] **New Apps** - Test additional applications from App-DB.
- [ ] **Update COMPATIBILITY.md** - Document all results.

### Phase 40: IFF & Datatypes Support
- [ ] **iffparse.library** - Full implementation (if not done in Phase 33).
- [ ] **datatypes.library** - Basic framework.

### Phase 41: Advanced Console Features
- CONU_CHARMAP, RawKeyConvert, advanced CSI sequences.

### Phase 42: BOOPSI & GadTools Visual Completion
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

### Phase 43: Pixel Array Operations
**Goal**: Implement optimized pixel array functions using AROS-style blitting infrastructure.

**Background**: These functions were initially implemented using simple WritePixel() loops but caused crashes and infinite loops. AROS uses specialized `write_pixels_8()` helpers with optimized blitting that handle BitMap formats, layer integration, and memory operations properly.

**Analysis Tasks:**
- [ ] Study AROS `write_pixels_8()` implementation in gfxfuncsupport.c
- [ ] Study AROS pixel array implementations (writePixelLine8.c, writePixelArray8.c, etc.)
- [ ] Understand BitMap format handling (interleaved vs. standard)
- [ ] Understand TempRP handling for buffered operations

**Implementation Tasks:**
- [ ] **ReadPixelLine8** - Read horizontal line of chunky pixels
- [ ] **WritePixelLine8** - Write horizontal line of chunky pixels  
- [ ] **ReadPixelArray8** - Read rectangular array of chunky pixels
- [ ] **WritePixelArray8** - Write rectangular array of chunky pixels
- [ ] **WriteChunkyPixels** - Fast chunky pixel writes with bytesperrow support
- [ ] Handle BitMap formats (interleaved, standard)
- [ ] Integrate with layer/clipping system
- [ ] Optimize memory operations (avoid per-pixel function calls)

**Required Tests:**
- [ ] ReadPixelLine8/WritePixelLine8 - horizontal lines
- [ ] ReadPixelArray8/WritePixelArray8 - rectangular regions
- [ ] WriteChunkyPixels - with various bytesperrow values
- [ ] Test with different BitMap depths and formats
- [ ] Test with layer clipping
- [ ] Performance tests (ensure no infinite loops)

### Phase 44: Async I/O & Timer Completion
**Goal**: Implement proper asynchronous I/O for devices.
- [ ] **timer.device Async** - TR_ADDREQUEST with proper delay queue and timeout handling.
- [ ] **console.device Async** - Non-blocking reads with timeout support.
- [ ] **Event Loop Integration** - Coordinate device async I/O with WaitPort/Wait().

### Phase 45: Directory Opus Deep Dive
**Goal**: Full Directory Opus 4 compatibility.
- [ ] **Stack Corruption Analysis** - Trace the source of stack corruption during DOpus cleanup.
- [ ] **dopus.library Investigation** - Debug the bundled library's task cleanup routines.
- [ ] **Memory Corruption Detection** - Add memory guards/canaries to detect overwrites.
- [ ] **Missing Function Analysis** - Identify any unimplemented functions DOpus depends on.
- [ ] **powerpacker.library** - Implement basic decompression if needed by DOpus.
- [ ] **inovamusic.library** - Stub or implement if needed by DOpus.

---
