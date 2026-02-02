# LXA Application Compatibility Notes

## Tested Applications

### KickPascal 2 - ✅ WORKS
- **Binary**: `APPS:KP2/KP`
- **Status**: Window opens, IDE loads successfully
- **Test**: `tests/apps/kickpascal2`

### GFA Basic 2 - ✅ WORKS
- **Binary**: `APPS:GFABasic/gfabasic`
- **Status**: Editor window opens, accepts input
- **Functions Implemented**:
  - GetScreenData() - returns screen info
  - InitGels() - GEL system initialization (stub)
  - IntuiTextLength() - text width calculation
  - Expanded custom chip register handling (DMACON, colors)
  - Expanded memory region handling (Zorro-II/III, autoconfig)
- **Notes**:
  - Uses input.device (BeginIO command 9 warning, non-fatal)
  - Opens custom screen with editor window

### MaxonBASIC - ⚠️ PARTIAL
- **Binary**: `APPS:MaxonBASIC/MaxonBASIC`
- **Status**: Loads libraries but fails to create menus
- **Error Message**: "Konnte Menü nicht erzeugen" (Could not create menu)
- **Libraries Added** (this session):
  - gadtools.library (stub) - provides basic gadget/menu API
  - workbench.library (stub) - provides Workbench integration API
  - asl.library (stub) - provides file/font requesters
- **Functions Implemented**:
  - GetProgramDir() - returns current process pr_HomeDir
  - AllocRemember() / FreeRemember() - Intuition memory tracking
  - LockPubScreen() / UnlockPubScreen() - returns FirstScreen
- **Fix Required**:
  - CreateMenusA() currently returns NULL - needs real implementation
  - Clipboard.device not found (non-fatal warning)

### SysInfo - ⚠️ PARTIAL
- **Binary**: `APPS:SysInfo/SysInfo`
- **Status**: Window opens briefly, then crashes
- **Issue**: Uses 68030/68040 MMU instructions (PMOVE CRP) for hardware detection
- **Fix Required**: Either:
  - Trap and handle 68030 instructions
  - Run in 68030 emulation mode
  - SysInfo needs a 68000-compatible version
- **Test**: `tests/apps/sysinfo`

### Devpac (HiSoft) - ⚠️ PARTIAL
- **Binary**: `APPS:Devpac/Devpac`
- **Status**: Doesn't open window, accesses hardware directly
- **Issues**:
  1. Uses BOOPSI (NewObjectA) - stubbed but returns NULL
  2. Accesses custom chip registers (0x00DFF*** / 0x00FFF***) directly
- **Fix Required**:
  - Implement BOOPSI object system
  - May need custom chip register emulation
- **Test**: `tests/apps/devpac`

### Directory Opus 4 - ⚠️ PARTIAL
- **Binary**: `APPS:DOPUS/DirectoryOpus`
- **Status**: Loads but fails to open due to missing library
- **Issues**:
  1. Requires dopus.library (included in APPS:DOPUS/libs but not in LIBS:)
  2. Needs proper assign setup (LIBS: should include DOPUS/libs)
  3. Uses extended ROM area (0x00F00000-0x00F7FFFF) - now returns 0
- **Libraries Needed**:
  - dopus.library (included)
  - arp.library (included)
  - May need others
- **Fix Required**:
  - Test infrastructure needs to add app-specific libs to LIBS: assign
  - OR implement library search in application's directory
- **Test**: `tests/apps/dopus`

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
Basic stub implementation for AmigaOS 2.0+ GadTools API:
- CreateGadgetA() - creates basic gadget structures
- FreeGadgets() - frees gadget chains
- CreateMenusA() - **returns NULL** (needs implementation)
- FreeMenus() - stub
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
- Currently stubbed (NewObjectA returns NULL)
- Need full BOOPSI class system implementation

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
