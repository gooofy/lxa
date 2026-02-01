# Amiga Application Compatibility Strategy

To transition from a "clean-room" implementation to a "Wine-like" runtime for Amiga software, `lxa` must provide the standard environment Amiga applications expect.

## Core Infrastructure Requirements

### 1. Font System (`diskfont.library`)
Real applications rarely use just the built-in Topaz font.
- **Diskfont**: Implementation of `diskfont.library` to load `.font` and `.glyph` files.
- **Host Mapping**: Ability to map Amiga font requests to host OpenType/TrueType fonts or provide a bundled set of public-domain Amiga fonts (e.g. AROS fonts).
- **Fonts Assign**: Automatic setup of `FONTS:` assign to `~/.lxa/Fonts`.

### 2. Preferences & Environment
Applications often query system settings.
- **ENV: / ENVARC:** Full implementation of persistent environment variables.
- **Prefs Files**: Support for `sys/screenmode.prefs`, `sys/font.prefs`, etc.
- **Locale**: Basic `locale.library` to handle date formats and character sets.

### 3. Standard UI Libraries
Most well-behaved Amiga apps use system-standard GUI toolkits.
- **GadTools**: `gadtools.library` for standard menus, buttons, and layouts.
- **ASL**: `asl.library` for file and font requesters.
- **BOOPSI**: Object-oriented UI system within `intuition.library`.
- **Layers**: `layers.library` for managing overlapping windows and clipping.

### 4. System Devices
- **timer.device**: Essential for animation, timeouts, and event polling.
- **input.device**: Centralized input handling beyond simple IDCMP.
- **clipboard.device**: Inter-process copy/paste support.

## Phase-Based Integration Approach

### Step 1: CLI & Low-Complexity Apps
Target applications that use primarily `dos.library` and `utility.library`.
- **Target**: KickPascal, older compilers, text-based utilities.
- **Goal**: Validate `ReadArgs`, file I/O, and basic memory management.

### Step 2: System UI Apps
Target applications using `gadtools.library` and `asl.library`.
- **Target**: Standard Amiga preferences editors, simple productivity tools.
- **Goal**: Validate UI library implementations and `layers.library` clipping.

### Step 3: Complex Interactive Apps
Target applications with custom UI or high-frequency input.
- **Target**: ProCalc, early word processors, specialized editors.
- **Goal**: Validate `BOOPSI`, `input.device`, and `timer.device`.
