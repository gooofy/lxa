# LXA Application Compatibility Notes

**Last Updated**: Phase 39 (February 2026) - Comprehensive re-testing after Phase 34-38 fixes

**⚠️ CRITICAL**: Phase 39 testing was **superficial and inadequate**. Tests only checked for crashes, not actual functionality. Manual testing revealed serious rendering issues that automated tests didn't catch.

## Phase 39 Test Results Summary - REVISED

**Initial automated assessment was WRONG.** Tests only verified "doesn't crash" which is insufficient.

**Manual Testing Reveals Serious Issues**:

**❌ BROKEN (2 apps with confirmed rendering bugs):**
- GFA Basic - Opens screen but only **21 pixels tall** (640x21 instead of 640x200+)
- Devpac - Window opens but **completely empty** (no title bar, no content)

**⚠️ UNKNOWN (4 apps need deeper validation):**
- KickPascal 2 - No crash but actual functionality unverified
- MaxonBASIC - Opens Workbench but window rendering unverified
- EdWordPro - Opens windows but actual functionality unverified  
- SysInfo - No crash but actual functionality unverified

**⚠️ PARTIAL (1 app):**
- BeckerText II - Exits cleanly but no window opens

**❌ FAILING (1 app):**
- Oberon 2 - NULL pointer crash at startup

**Accurate Status**: Only 2/8 apps confirmed broken, 4/8 status genuinely unknown, 1/8 partial, 1/8 crash.

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

### GFA Basic 2 - ❌ BROKEN (Phase 39 - Rendering Bug Confirmed)
- **Binary**: `APPS:GFABasic/gfabasic`
- **Status**: Opens screen but **WRONG DIMENSIONS** - only 21 pixels tall
- **Critical Bug**: OpenScreen() creates 640x21 screen instead of proper editor height (640x200+)
- **What Happens**:
  - Application launches without crash
  - Opens custom screen via OpenScreen()
  - Screen is only **21 pixels tall** (unusable)
  - Should be 200+ pixels for editor
- **Why Tests Didn't Catch It**: Tests only check for crashes, not screen dimensions
- **Functions Implemented**:
  - GetScreenData() - returns screen info
  - InitGels() - GEL system initialization (stub)
  - IntuiTextLength() - text width calculation
  - Expanded custom chip register handling (DMACON, colors)
  - Expanded memory region handling (Zorro-II/III, autoconfig)
- **Notes**:
  - Uses input.device (BeginIO command 9 warning, non-fatal)
  - Some custom chip register writes to 0x00FFFF* addresses (non-fatal)
- **Fix Required** (Phase 40):
  - Investigate OpenScreen() height calculation
  - Check NewScreen structure parsing
  - Verify display mode handling for custom screens
  - Fix screen dimension calculation bug

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

### Devpac (HiSoft) - ❌ BROKEN (Phase 39 - Rendering Bug Confirmed)
- **Binary**: `APPS:Devpac/Devpac`
- **Source**: `~/.lxa/System/Apps/Devpac/Devpac.c` (ghidra decompile)
- **Status**: Opens window but **COMPLETELY EMPTY** - no title bar, no content
- **Critical Bugs**:
  - Window opens with correct dimensions (640x245)
  - Window title "Untitled" is set but **NOT RENDERED**
  - Window is **completely empty** - no visible title bar, borders, or content
  - SDL window shows but is blank
- **What Happens**:
  - Opens Workbench screen (640x256) ✓
  - Calls OpenWindow() with title="Untitled" ✓
  - Window structure created correctly ✓
  - But window appears empty on screen ✗
- **Why Tests Didn't Catch It**: Tests only check for crashes, not window rendering
- **Test**: `tests/apps/devpac`
- **Fix Required** (Phase 40):
  - Investigate window title bar rendering in display.c
  - Check if Text() calls for title are executed
  - Verify RastPort initialization for windows
  - Check window border rendering
  - Verify SDL rendering updates for window decorations
- **Phase 39 Results**: Opens without crash but window is non-functional (empty)

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
