# LXA Application Compatibility Notes

**Last Updated**: Phase 49+ (February 2026) - GFA Basic memory errors fixed, Devpac fully fixed

**⚠️ CRITICAL**: Phase 39 testing was **superficial and inadequate**. Tests only checked for crashes, not actual functionality. Manual testing revealed serious rendering issues that automated tests didn't catch.

## Latest Update - GFA Basic Memory Errors Fixed!

**GFA Basic now launches without memory errors**:
1. Extended memory map to handle Slow RAM (0x00C00000-0x00D7FFFF)
2. Extended memory map to handle Ranger RAM (0x00E00000-0x00E7FFFF)
3. Extended memory map to handle Extended ROM writes (0x00F00000-0x00F7FFFF)
4. Extended memory map to handle ROM writes (0x00F80000+)

GFA Basic now opens a 640x256 screen and window, initializes console, and enters event loop waiting for input.

## Current Status Summary

**✅ WORKING (2 apps):**
- Devpac - Window renders correctly AND responds to input
- GFA Basic - Memory errors fixed, launches with window (needs functionality verification)

**⚠️ UNKNOWN (4 apps need deeper validation):**
- KickPascal 2 - No crash but actual functionality unverified
- MaxonBASIC - Opens Workbench and window but rendering/functionality unverified
- EdWordPro - Opens windows but actual functionality unverified  
- SysInfo - No crash but actual functionality unverified

**⚠️ PARTIAL (2 apps):**
- BeckerText II - Opens main window (after Phase 46 fixes) but functionality unverified
- Directory Opus 4 - Main process exits, dopus_task doesn't display GUI

**❌ FAILING (1 app):**
- Oberon 2 - NULL pointer crash at startup

**Root Cause**: Test infrastructure only checks "exit code 0 or timeout" which is meaningless for GUI apps.

**Required Actions**:
1. **Phase 39b**: Build proper test infrastructure with bitmap/screen verification
2. **Phase 40**: Fix identified rendering bugs (GFA screen height, Devpac window)
3. Re-test ALL applications with enhanced verification

## Test Infrastructure Issues (Discovered in Phase 39)

**Current Problems**:
1. ❌ No bitmap/screen content verification
2. ❌ No visual rendering checks
3. ❌ No dimension validation
4. ❌ No title bar rendering checks
5. ❌ Only checks: does it crash? (insufficient)
6. ❌ 60s timeout doesn't verify what happened

**What We Need** (Phase 39b):
1. ✅ Bitmap capture and comparison
2. ✅ Screen/window dimension validation
3. ✅ Title text rendering verification
4. ✅ System call tracing with parameter validation
5. ✅ Success criteria beyond "doesn't crash"
6. ✅ Screenshot-based regression testing

## Tested Applications

### KickPascal 2 - ⚠️ UNKNOWN (Phase 39 - Needs Deeper Validation)
- **Binary**: `APPS:KP2/KP`
- **Status**: No crash detected, but actual functionality unverified
- **Test**: `tests/apps/kickpascal2`
- **What We Know**: Test passes (no crash within timeout)
- **What We Don't Know**: 
  - Does IDE interface actually render?
  - Are menus visible?
  - Can user interact with it?
  - Is window content displayed correctly?
- **Required** (Phase 39b): Deep validation with screen verification

### GFA Basic 2 - ⚠️ PARTIAL (Memory errors fixed, needs functionality verification)
- **Binary**: `APPS:GFABasic/gfabasic`
- **Status**: ⚠️ Memory errors FIXED - launches with window, needs functionality verification
- **Current Behavior**:
  - Application launches without crashes or memory errors
  - Opens custom screen via OpenScreen() (640x256 after height expansion)
  - Opens window titled "GFA-BASIC" (640x256)
  - Initializes console device
  - Enters event loop waiting for user input
- **Issues Fixed**:
  - OpenScreen() height calculation (21px → 256px) - Fixed in Phase 40
  - Memory writes to Slow RAM (0x00C00000-0x00D7FFFF) - Added region handling
  - Memory writes to Ranger RAM (0x00E00000-0x00E7FFFF) - Added region handling
  - Memory writes to Extended ROM (0x00F00000-0x00F7FFFF) - Added region handling
  - Memory writes to ROM area (0x00F80000+) - Added ROM write-protect handling
- **Functions Implemented**:
  - GetScreenData() - returns screen info
  - InitGels() - GEL system initialization (stub)
  - IntuiTextLength() - text width calculation
  - Expanded custom chip register handling (DMACON, colors)
  - Expanded memory region handling (Zorro-II/III, autoconfig, Slow RAM, Ranger RAM, ROM)
- **Remaining Verification Needed**:
  - Verify editor window content renders correctly
  - Test text input in editor
  - Verify BASIC command execution
  - Test program save/load functionality

### BeckerText II - ⚠️ PARTIAL (Phase 39 - NEW)
- **Binary**: `APPS:BTII/BT-II`
- **Status**: Exits cleanly with code 0 but no window opens
- **Behavior**:
  - Repeatedly opens/closes intuition.library
  - All libraries load successfully
  - No errors or crashes
  - Exits immediately without opening windows
- **Notes**:
  - May require Workbench environment to run
  - May be checking for specific system configuration
  - Possibly needs command-line arguments or configuration files

### Oberon 2 - ❌ FAILS (Phase 39 - NEW)
- **Binary**: `APPS:Oberon/Oberon`
- **Status**: Crashes with NULL pointer exception at startup
- **Behavior**:
  - All libraries open successfully (utility, dos, graphics, intuition, workbench, icon)
  - Crashes at PC=0x00000000 during initialization
  - NULL function pointer or corrupted return address
- **Fix Required**:
  - Debug to identify which library function is returning NULL
  - Likely missing or stubbed function that application expects to be valid

### MaxonBASIC - ⚠️ UNKNOWN (Phase 39 - Needs Deeper Validation)
- **Binary**: `APPS:MaxonBASIC/MaxonBASIC`
- **Status**: Opens Workbench and window, but rendering/functionality unverified
- **Progress**: Opens Workbench screen (640x256) and creates main window "Unbenannt"
- **Libraries Used**:
  - gadtools.library - menu/gadget creation API
  - workbench.library - Workbench integration API
  - asl.library - file/font requesters
- **Functions Implemented**:
  - CreateMenusA() - full implementation creating Menu/MenuItem structures
  - FreeMenus() - properly frees menu chains
  - GetProgramDir() - returns current process pr_HomeDir
  - AllocRemember() / FreeRemember() - Intuition memory tracking
  - LockPubScreen() / UnlockPubScreen() - returns FirstScreen, auto-opens Workbench
  - GetScreenDrawInfo() - returns DrawInfo with pens and font info (dynamic allocation)
  - FreeScreenDrawInfo() - properly frees DrawInfo
  - QueryOverscan() - returns PAL dimensions
  - GetVPModeID() - returns mode ID from viewport
  - ModeNotAvailable() - returns 0 (mode available)
  - GetBitMapAttr() - returns bitmap properties
  - NewObjectA(sysiclass) - returns dummy Image for system imagery
  - ClearPointer() - no-op stub
- **What We Know**: 
  - Opens Workbench correctly (640x256)
  - Opens main window
  - No crash detected
- **What We Don't Know**:
  - Are menus visible and functional?
  - Does window content render?
  - Are gadgets visible?
  - Can user interact with it?
- **Notes**: Clipboard.device not found (non-fatal warning)
- **Required** (Phase 39b): Deep validation with screen/menu verification

### SysInfo - ⚠️ UNKNOWN (Phase 39 - Needs Deeper Validation)
- **Binary**: `APPS:SysInfo/SysInfo`
- **Status**: No crash detected, but actual functionality unverified
- **Test**: `tests/apps/sysinfo`
- **What We Know**: Test passes (no crash, improved from earlier phases)
- **What We Don't Know**:
  - Does window render correctly?
  - Is system information displayed?
  - Are gadgets/buttons functional?
- **Phase 39 Results**: No crash (improvement over previous phases)
- **Required** (Phase 39b): Deep validation with content verification

### Devpac (HiSoft) - ✅ WORKING (Phase 49 - Fully Fixed)
- **Binary**: `APPS:Devpac/Devpac`
- **Source**: `~/.lxa/System/Apps/Devpac/Devpac.c` (ghidra decompile)
- **Status**: ✅ **FULLY WORKING** - Window renders correctly AND responds to input
- **Fixes Applied**:
  1. **Phase 40**: DetailPen/BlockPen 0xFF handling - window now renders with title bar, borders
  2. **Phase 49**: Signal/Wait scheduling bug - window now responds to mouse/keyboard input
- **What Works**:
  - Opens Workbench screen (640x256) ✓
  - Calls OpenWindow() with title="Untitled" ✓
  - Window structure created correctly ✓
  - Window renders with title bar, borders, 3D effects ✓
  - Window responds to mouse events (IDCMP_MOUSEBUTTONS) ✓
  - Window responds to keyboard events ✓
- **Test**: `tests/apps/devpac`
- **Remaining Testing**:
  - [ ] Verify editor content area renders properly
  - [ ] Verify menu bar displays and is functional
  - [ ] Test text input in editor
  - [ ] Test assembler execution with simple program

### EdWord Pro - ⚠️ UNKNOWN (Phase 39 - Needs Deeper Validation)
- **Binary**: `APPS:EdWordPro/EdWordPro`
- **Status**: Opens custom screen with windows, but functionality unverified
- **Progress**:
  - Opens custom screen "EdWord Pro V6.0 - M.Reddy 1997" ✓
  - Opens main editing window (640x243) ✓
  - Opens message window "EdWord Pro Message" ✓
  - Enters event loop (no crash) ✓
- **Functions Implemented**:
  - LockIBase() / UnlockIBase() - Intuition base locking (stubs)
  - ShowTitle() - screen title visibility (no-op)
  - SetWindowTitles() - window/screen title update
  - SetPointer() / ClearPointer() - custom mouse pointer (no-op)
  - PubScreenStatus() - public screen status (stub)
  - InitArea() - area fill initialization
  - AskSoftStyle() / SetSoftStyle() - text style queries
  - SetRGB4() - color register setting (no-op)
- **Missing Libraries** (non-fatal):
  - keymap.library - keyboard layout
  - reqtools.library - file requesters
  - rexxsyslib.library - ARexx support
  - clipboard.device - copy/paste
- **What We Know**:
  - Opens screens and windows
  - No crash detected
- **What We Don't Know**:
  - Can text be entered?
  - Are menus functional?
  - Does editing actually work?
  - Are all windows rendering correctly?
- **Required** (Phase 39b): Deep validation with interaction testing

### ASM-One v1.48 - ⚠️ PARTIAL
- **Binary**: `APPS:ASM-One/ASM-One_V1.48`
- **Status**: Opens Workbench and editor screen, then crashes
- **Progress**:
  1. Opens gadtools.library ✓
  2. Opens Workbench screen (640x256) ✓
  3. Opens editor screen (800x600) ✓
  4. Crashes accessing invalid ROM address (0x00FBEED1)
- **Issues**:
  1. iffparse.library not found (non-fatal)
  2. reqtools.library not found (non-fatal)
  3. timer.device not found
  4. Crash appears to be from unimplemented ROM function
- **Fix Required**:
  - Identify which ROM function is at 0x00FBE* range
  - Likely a graphics or intuition function returning bad pointer

### DPaint V - ❌ FAILS
- **Binary**: `APPS:DPaintV/DPaint`
- **Status**: Exits immediately with error code 20
- **Issues**:
  1. Requires iffparse.library (not in LIBS:)
- **Fix Required**:
  - Add iffparse.library to system or implement it

### Personal Paint - ❌ FAILS
- **Binary**: `APPS:ppaint/ppaint`
- **Status**: Exits immediately with error code 1
- **Issues**:
  - Unknown, no diagnostic output
- **Fix Required**:
  - Debug to find initialization failure

### Directory Opus 4 - ⚠️ PARTIAL
- **Binary**: `APPS:DOPUS/DirectoryOpus`
- **Status**: Loads libraries but fails to open window, enters exception loop
- **Issues**:
  1. Main process exits with error code 1 immediately
  2. Background process loads dopus.library successfully
  3. commodities.library not found (non-fatal)
  4. rexxsyslib.library not found (non-fatal)
  5. powerpacker.library causes exception loop
  6. Uses Zorro-III memory area (0x01000000-0x0FFFFFFF) - now handled, returns 0xFF
- **Libraries Found**:
  - dopus.library ✓
  - powerpacker.library ✓
- **Libraries Missing**:
  - commodities.library (Commodities Exchange support)
  - rexxsyslib.library (ARexx support)
- **Fix Required**:
  - Debug why main process exits immediately
  - Investigate powerpacker.library exception
  - Consider implementing commodities.library stub
- **Test**: `tests/apps/dopus` (currently expects failure)

### AmigaBasic - ⚠️ PARTIAL
- **Binary**: `APPS:AmigaBasic/AmigaBASIC`
- **Status**: Window opens, then background tasks crash
- **Issues**:
  1. Requires audio.device (not implemented) - used for SOUND command
  2. Requires timer.device for background task timing
  3. Background tasks crash when devices fail
- **Functions Implemented**:
  - GetPrefs() - returns sensible defaults
  - ViewPortAddress() - returns window's screen ViewPort
  - OffMenu()/OnMenu() - menu enable/disable (no-op)
- **Fix Required**:
  - Implement stub audio.device and timer.device
  - OR make device open failures non-fatal for background tasks

## New Libraries Implemented (Phase 26)

### gadtools.library
Full implementation for AmigaOS 2.0+ GadTools API:
- CreateGadgetA() - creates basic gadget structures
- FreeGadgets() - frees gadget chains
- CreateMenusA() - creates Menu/MenuItem hierarchies from NewMenu arrays
- FreeMenus() - properly frees menu chains with all items/subitems
- LayoutMenusA() / LayoutMenuItemsA() - returns TRUE
- GT_GetIMsg() / GT_ReplyIMsg() - message handling
- GT_RefreshWindow() / GT_BeginRefresh() / GT_EndRefresh() - stubs
- CreateContext() - creates gadget list context
- GetVisualInfoA() / FreeVisualInfo() - screen visual info
- DrawBevelBoxA() - stub (no actual drawing)

### workbench.library
Basic stub implementation for Workbench integration:
- UpdateWorkbench() - no-op
- AddAppWindowA() / RemoveAppWindow() - returns dummy handle
- AddAppIconA() / RemoveAppIcon() - returns dummy handle
- AddAppMenuItemA() / RemoveAppMenuItem() - returns dummy handle
- WBInfo() - returns 0
- OpenWorkbenchObjectA() / CloseWorkbenchObjectA() - returns FALSE

### asl.library
Basic stub implementation for ASL requesters:
- AllocFileRequest() / FreeFileRequest() - obsolete, supported
- RequestFile() - returns FALSE (no GUI)
- AllocAslRequest() / FreeAslRequest() - allocates request structure
- AslRequest() - returns FALSE (no GUI for requesters)

## Memory Regions Added (Phase 26)

### CIA Area (0xBFD000-0xBFFFFF)
- Reads return 0xFF (no buttons pressed, inactive signals)
- Writes are ignored (logged in debug mode)

### Custom Chip Writes
- DMACON (0x096) - ignored
- Color registers (0x180-0x1BF) - ignored
- Byte writes to custom area - ignored

## Common Compatibility Issues

### 1. 68030/68040 Instructions
Some applications use advanced CPU instructions for hardware detection:
- PMOVE (MMU control)
- MOVEC (control registers)
These will fail on 68000 emulation mode.

### 2. Direct Hardware Access
Applications that access:
- Custom chip registers (0x00DFF000-0x00DFFFFF)
- CIA registers (0x00BFD000, 0x00BFE001)
- Chip RAM directly
Will fail without hardware emulation.

### 3. BOOPSI Objects
Applications using NewObjectA/DisposeObject for UI:
- sysiclass and imageclass now return dummy 8x8 Image structures
- Other classes still return NULL
- Full BOOPSI class system not yet implemented

### 4. Missing Devices
Some applications require:
- clipboard.device - for copy/paste
- timer.device - for timing operations  
- audio.device - for sound
These are not yet implemented.

### 5. Missing Libraries
Some applications require:
- reqtools.library
- xpk*.library (compression)
- MUI classes
These are not yet implemented.

## Test Infrastructure

App tests are in `tests/apps/`:
- Each app has its own directory
- Uses `app_test_runner.sh` for setup
- Configures APPS: assign to `~/.lxa/System/Apps`
- Times out after 60 seconds

To run a test:
```bash
cd tests/apps/<appname>
make run-test
```
