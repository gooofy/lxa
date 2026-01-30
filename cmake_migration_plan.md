# CMake Migration Plan for `lxa`

## Executive Summary
This document outlines the migration of the `lxa` project from a manual Makefile-based build system to CMake. The goal is to provide a modern, maintainable build system that handles both the host (Linux) and target (m68k-amigaos) components, supports proper installation, and enables a "just works" first-run experience.

## 1. Project Components

### 1.1 Host Component (Linux)
- **Executable**: `lxa`
- **Source**: `src/lxa/` (plus Musashi 680x0 emulator)
- **Dependencies**: Standard C library, math library.

### 1.2 Target Components (m68k-amigaos)
- **ROM**: `lxa.rom`
  - **Source**: `src/rom/`
  - **Compiler**: `m68k-amigaos-gcc`
- **System Tools**: `C:Dir`, `C:Delete`, etc.
  - **Source**: `sys/C/`, `sys/System/`
  - **Compiler**: `m68k-amigaos-gcc` (libnix)

## 2. CMake Structure

### 2.1 Root `CMakeLists.txt`
- Project definition.
- Common options.
- Detection of the `m68k-amigaos` toolchain.
- Inclusion of subdirectories.

### 2.2 Host Build (`src/lxa/CMakeLists.txt`)
- Standard CMake target for `lxa`.
- Includes Musashi sources.
- Links against `m` (math library).
- Defines `LXA_DATADIR` for installation path resolution.

### 2.3 Target Build (Cross-Compilation)
Since CMake supports cross-compilation via toolchain files, we will use two approaches:
1.  **Unified Build**: Use `ExternalProject` or multiple `project()` calls if possible, but simpler is:
2.  **Side-by-Side Build**: The root CMake handles the host. A separate CMake configuration (or a custom command) handles the m68k part if unified cross-compilation in one pass is too complex.
    - **Recommended**: A dedicated CMake toolchain file `cmake/m68k-amigaos.cmake`.

### 2.4 Installation Layout
The installation prefix will be set to `~/projects/amiga/lxa/usr` (or `/usr/local` by default).

```
$PREFIX/
├── bin/
│   └── lxa                       # Host emulator
├── share/lxa/
│   ├── lxa.rom                   # ROM image
│   ├── config.ini.template       # Default configuration
│   └── System/                   # Read-only system template
│       ├── C/                    # Amiga commands (m68k)
│       ├── S/                    # Startup scripts
│       ├── Libs/
│       ├── Devs/
│       └── System/               # Other system tools (m68k)
```

## 3. Runtime Initialization Strategy

### 3.1 Prefix Detection
- `LXA_PREFIX`: Environment variable (default `~/.lxa`).
- `LXA_DIR`: Environment variable for the installation directory (fallback to hardcoded `CMAKE_INSTALL_FULL_DATADIR/lxa`).

### 3.2 First-Run Logic
1.  If `~/.lxa` (or `$LXA_PREFIX`) does not exist:
    - Create `~/.lxa/`.
    - Copy structure and files from `$PREFIX/share/lxa/System/` to `~/.lxa/System/`.
    - Copy `$PREFIX/share/lxa/config.ini.template` to `~/.lxa/config.ini`.
2.  Load configuration from `~/.lxa/config.ini`.
3.  Mount `SYS:` pointing to `~/.lxa/System`.

## 4. Implementation Phases

### Phase 6.1: CMake Infrastructure
- [ ] Create `cmake/m68k-amigaos.cmake` toolchain file.
- [ ] Create root `CMakeLists.txt`.
- [ ] Configure `src/lxa` build.
- [ ] Configure `src/rom` build (cross-compiled).
- [ ] Configure `sys/` build (cross-compiled).

### Phase 6.2: Installation & Packaging
- [ ] Define `install` targets for all components.
- [ ] Organize `share/lxa` structure in the source tree or during build.
- [ ] Implement `make install`.

### Phase 6.3: Host Integration
- [ ] Update `src/lxa/lxa.c` to use compile-time `LXA_DATADIR`.
- [ ] Implement the copy-on-first-run logic.
- [ ] Support `LXA_PREFIX` environment variable.

### Phase 6.4: Verification
- [ ] Verify build from scratch using CMake.
- [ ] Verify `make install` to `~/projects/amiga/lxa/usr`.
- [ ] Verify automatic initialization of `~/.lxa` on first run.

## 5. Build Commands (Target State)

```bash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=~/projects/amiga/lxa/usr
make
make install

# Run
~/projects/amiga/lxa/usr/bin/lxa
```
