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

## Phase 1: Exec Core & Tasking Infrastructure ✓ COMPLETE
**Goal**: Establish a stable, multitasking foundation.

- ✓ Signal & Message completion (`Wait`, `PutMsg`, etc.)
- ✓ Process & Stack Management (`CreateNewProc`, `Exit`)
- ✓ Task numbering using `RootNode->rn_TaskArray`

---

## Phase 2: Configuration & VFS Layer ✓ COMPLETE
**Goal**: Support the `~/.lxa` structure and flexible drive mapping.

- ✓ Host Configuration System (config.toml)
- ✓ Case-Insensitive Path Resolution
- ✓ Basic File I/O Bridge (`Open`, `Read`, `Write`, `Close`, `Seek`)

---

## Phase 3: First-Run Experience & Filesystem API ✓ COMPLETE
**Goal**: Create a "just works" experience for new users and complete the core DOS API.

### Step 3.1: Automatic Environment Setup (The ".lxa" Prefix)
- ✓ **Bootstrap Logic**: If `~/.lxa` is missing, create it automatically.
- ✓ **Skeletal Structure**: Create `SYS:C`, `SYS:S`, `SYS:Libs`, `SYS:Devs`, `SYS:T`.
- ✓ **Default Config**: Generate a sensible `config.ini` mapping `SYS:` to the new `~/.lxa/System`.
- ✓ **Startup-Sequence**: Provide a default `S:Startup-Sequence` that sets up the path and assigns.

### Step 3.2: Locks & Examination API
- ✓ **API Implementation**: `Lock()`, `UnLock()`, `DupLock()`, `SameLock()`, `Examine()`, `ExNext()`, `Info()`, `ParentDir()`, `CurrentDir()`, `CreateDir()`, `DeleteFile()`, `Rename()`, `NameFromLock()`.
- ✓ **Host Bridge**: Map to `stat`, `opendir`, `readdir`, `mkdir`, `unlink`, `rename` on Linux.

---

## Phase 4: Basic Userland & Metadata ✓ COMPLETE
**Goal**: Implement the first set of essential AmigaDOS commands with AmigaDOS-compatible template parsing.

**Status**: ✅ COMPLETE - Core infrastructure and command integration finished

### Completed Core Infrastructure:
- ✅ **Semaphore Support**: `InitSemaphore()`, `ObtainSemaphore()`, `ReleaseSemaphore()` - C runtime compatibility
- ✅ **ReadArgs()**: Template-based command parsing with /A, /K, /S, /N, /M, /F modifiers
- ✅ **SetProtection()**: File permission mapping to Linux
- ✅ **SetComment()**: File comments via xattr/sidecar files

### Step 4.1: Essential Userland (C:) - Commands ✓ DONE
- ✅ **SYS:C structure**: Directory created with build system
- ✅ **Command binaries**: All commands using AmigaDOS template syntax:
  - `DIR` - `DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S`
  - `DELETE` - `FILE/M/A,ALL/S,QUIET/S,FORCE/S`
  - `TYPE` - `FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S`
  - `MAKEDIR` - `NAME/M/A`

### Step 4.1a: Amiga-Compatible Command Parser Framework ✓ DONE
**Blockers Resolved**: Commands now use AmigaDOS template system exclusively

1. **✓ ReadArgs() Implementation** (dos.library) - `src/rom/lxa_dos.c:2310-2515`
   - ✓ Parse command templates (e.g., `DIR,OPT/K,ALL/S,DIRS/S`)
   - ✓ Support template modifiers: /A (required), /M (multiple), /S (switch), /K (keyword), /N (numeric), /F (final)
   - ✓ Handle keyword arguments with/without equals sign
   - ✓ Support /? for template help
   
2. **✓ Semaphore Support** (exec.library) - `src/rom/exec.c:1862-1939`
   - ✓ `InitSemaphore()`, `ObtainSemaphore()`, `ReleaseSemaphore()`
   - ✓ `AttemptSemaphore()` for non-blocking acquisition
   - Required for C runtime compatibility

### Step 4.1b: Command Behavioral Alignment ✓ DONE

**DIR Command** (`sys/C/dir.c`)
- ✅ Template: `DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S`
- ✅ Pattern matching support (`#?` wildcards) for filtering
- ✅ ALL switch to show hidden files
- ✅ DIRS/FILES switches for filtering
- ✅ Detailed listing with OPT=L
- ⚠️ Interactive mode (INTER) stubbed but not fully implemented

**DELETE Command** (`sys/C/delete.c`)
- ✅ Template: `FILE/M/A,ALL/S,QUIET/S,FORCE/S`
- ✅ Multiple file support (/M modifier)
- ✅ ALL switch for recursive directory deletion
- ✅ QUIET switch to suppress output
- ✅ FORCE switch for error suppression

**TYPE Command** (`sys/C/type.c`)
- ✅ Template: `FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S`
- ✅ Multiple file support (/M modifier)
- ✅ TO keyword for output redirection
- ✅ HEX switch for hex dump with ASCII column
- ✅ NUMBER switch for line numbering

**MAKEDIR Command** (`sys/C/makedir.c`)
- ✅ Template: `NAME/M/A`
- ✅ Multiple directory creation (/M modifier)

### Step 4.2: Pattern Matching ✓ DONE

**Completed**:
- ✅ **ParsePattern()**: Converts user patterns to internal form - `src/rom/lxa_dos.c:2629-2763`
- ✅ **MatchPattern()**: Matches patterns against strings - `src/rom/lxa_dos.c:2765-2780`
- ✅ **Wildcard Support**: `#?` (any string), `?` (single char), `#` (repeat), `%` (escape), `*` (convenience)

### Step 4.3: Metadata & Variables 🚧 PARTIAL

**Completed**:
- ✅ **SetProtection()**: Maps Amiga protection bits to Linux permissions (`src/lxa/lxa.c:1350`)
- ✅ **SetComment()**: Stores comments via xattr or sidecar files (`src/lxa/lxa.c:1395`)
- ✅ **ReadArgs()**: Template-based command argument parsing (`src/rom/lxa_dos.c:2310`)

**Pending (Phase 4b)**:
- **SetDate()**: Set file modification times (maps to `utimes()`)
- **GetVar()/SetVar()/DeleteVar()/FindVar()**: Environment variables
- **Tools**: RENAME, PROTECT, FILENOTE, SET, SETENV, GETENV

---

## Phase 5: The Interactive Shell & Scripting ✓ COMPLETE
**Goal**: A fully functional, interactive Amiga Shell experience.

**Status**: ✅ COMPLETE - Shell with interactive mode, scripting, and argument passing

### Completed Core Infrastructure:
- ✅ **Interactive Shell Binary** (`sys/System/Shell.c`)
  - AmigaDOS I/O (no stdio dependency)
  - Classic `1.SYS:> ` prompt with customizable prompt string
  - Line-by-line input handling (works with pipes and interactive mode)
  - Internal commands: CD, PATH, PROMPT, ECHO, ALIAS, WHICH, WHY, FAULT
  - Control flow: IF/ELSE/ENDIF, SKIP/LAB
  - FAILAT support for error tolerance

- ✅ **Execute() API** (dos.library)
  - Script execution with automatic Shell fallback
  - Input/output redirection support

- ✅ **Command-Line Arguments**
  - Programs can now receive arguments: `lxa <program> <args...>`
  - Shell can run scripts: `lxa Shell script.txt`
  - **Files**: `src/lxa/lxa.c`, `src/rom/exec.c`, `src/include/emucalls.h`

- ✅ **Infrastructure Fixes**
  - Piped input handling: `_dos_stdinout_fh()` uses STDIN_FILENO for Input()
  - SYS: defaults to current directory (config template updated)
  - Missing utility functions: UDivMod32, Stricmp, SDivMod32, UMult32

### Phase 5 Testing:
- ✅ All existing tests pass (helloworld, lock_examine, signal_pingpong)
- ✅ Shell verified working with interactive and scripted input
- ✅ Command-line argument passing verified

---

## Phase 6: Build System Migration (CMake) ✅ COMPLETE
**Goal**: Modernize the build system, support proper installation, and automate environment setup.

### Step 6.1: CMake Infrastructure
- ✅ **Cross-Compilation Support**: Implemented `cmake/m68k-amigaos.cmake` toolchain file for `m68k-amigaos-gcc`.
- ✅ **Unified Build System**: Created root `CMakeLists.txt` and component `CMakeLists.txt` for `lxa` (host), `lxa.rom` (target), and `sys/` (target).
- ✅ **Build Optimization**: CMake provides dependency tracking and parallel build support.

### Step 6.2: Installation & Layout
- ✅ **Standardized Paths**: Implemented `install` targets following FHS-like structure.
  - Binaries: `${CMAKE_INSTALL_PREFIX}/bin/`
  - Data files: `${CMAKE_INSTALL_PREFIX}/share/lxa/`
- ✅ **Install Prefix**: Default prefix set to `~/projects/amiga/lxa/usr` for testing.
- ✅ **System Template**: Created `share/lxa/System/` as a master template for user environments.
  - Includes: `C/`, `S/`, `Libs/`, `Devs/`, `T/`, `L/` directories
  - Default `config.ini` and `S/Startup-Sequence` templates

### Step 6.3: First-Run Automation
- ✅ **LXA_PREFIX Support**: Added support for `LXA_PREFIX` environment variable (defaulting to `~/.lxa`).
  - `vfs_init()` checks for `LXA_PREFIX` env var
  - Falls back to `~/.lxa` if not set
- ✅ **Auto-Initialization**: On startup, if `LXA_PREFIX` is missing, `lxa` automatically:
  - Creates the directory structure (SYS:, S:, C:, Libs:, Devs:, T:)
  - Copies system files from the installation's `share/lxa/System` (via `copy_system_template()`)
  - Deploys a default `config.ini`
- ✅ **ROM Discovery**: `auto_detect_rom_path()` now searches in:
  - Current directory
  - `src/rom/` (relative paths for development)
  - `LXA_PREFIX/` directory
  - `~/.lxa/` directory
  - `/usr/local/share/lxa/` (system location)
  - `/usr/share/lxa/` (system location)

**Lessons Learned**:
- CMake's `ExternalProject` was not needed; using `add_subdirectory` with compiler checks works well for mixed host/target builds.
- File copying during installation requires CMake's `install(CODE ...)` for post-build artifacts.
- The toolchain file approach makes cross-compilation configuration clean and reusable.

**Documentation Updated**: ✅ README.md and roadmap.md updated with new build instructions.

---

## Phase 7: System Management & Assignments
**Goal**: Advanced system control and logical drive management.

### Step 6.1: Assignment API
- **API Implementation**: `AssignLock()`, `AssignName()`, `AssignPath()`.
- **Search Path**: Proper integration of `SYS:C` and user-defined paths into the Shell loader.

### Step 6.2: System Tools
- **Process Management**: `STATUS`, `BREAK`, `RUN`, `CHANGETASKPRI`.
- **Memory & Stack**: `AVAIL`, `STACK`.
- **Device Control**: `ASSIGN`, `MOUNT`, `RELABEL`, `LOCK`.

---

## Phase 8: Advanced Utilities & Finalization
**Goal**: Reach Milestone 1.0 with a polished toolset.

- **Utilities**: `COPY` (recursive), `JOIN`, `SORT`, `EVAL`, `SEARCH`.
- **Compatibility**: `VERSION`, `WAIT`, `MAKELINK`.
- **Final Polish**: Documentation, `README.md` update, and AROS reference review for edge cases.

---

## Implementation Rules
1. **Clean Room Approach**: AROS/Kickstart source is for architectural reference ONLY.
2. **Host-First**: Prefer host-side (`emucall`) implementations for filesystem tasks.
3. **Internal vs External**: Compiled-in Shell commands for speed, separate `C:` binaries for extensibility.
4. **Amiga Authenticity**: Commands must use AmigaDOS argument syntax (templates, keywords, switches) not Unix-style flags.
   - Use `ReadArgs()` template parsing in all C: commands
   - Templates: `DIR,OPT/K,ALL/S,DIRS/S` not `dir -la`
   - Keywords: `ALL`, `DIRS`, `TO` not `-a`, `-d`, `-o`

## Next Steps: Phase 7 - System Management & Assignments

### Immediate (Next Session):
1. **Assignment API**
   - Implement `AssignLock()`, `AssignName()`, `AssignPath()` APIs.
   - Add assign support to the VFS layer.
2. **Shell Integration**
   - Update Shell to use assigns for command lookup.
   - Implement ASSIGN command for runtime assign management.
3. **ASSIGN Command**
   - Create `sys/C/assign.c` with proper AmigaDOS template.
   - Support: `ASSIGN <name> <path>`, `ASSIGN <name> SHOW`, `ASSIGN <name> REMOVE`

### Medium Term:
4. **System Management Tools**
   - STATUS - Show running processes
   - AVAIL - Show memory availability
   - STACK - Show/check stack usage
5. **Process Control**
   - BREAK - Send Ctrl-C to a process
   - RUN - Background process execution
   - CHANGETASKPRI - Modify task priority

---

## Status Summary

### ✅ Phase 1-6 Complete

All foundational phases are complete:
- Phase 1: Exec multitasking (signals, messages, semaphores, processes)
- Phase 2: Configuration & VFS (drive mapping, case-insensitive paths)
- Phase 3: Filesystem API (locks, examine, directories, file operations)
- Phase 4: Userland commands (DIR, TYPE, DELETE, MAKEDIR with templates)
- Phase 5: Interactive Shell (scripting, control flow, argument passing)
- Phase 6: Build System Migration (CMake, installation, LXA_PREFIX)

### ✅ Phase 6 Complete: Build System Migration (CMake)

All Phase 6 tasks completed:
- CMake-based build system with m68k-amigaos toolchain support
- FHS-compliant installation paths (bin/, share/lxa/)
- LXA_PREFIX environment variable for custom environments
- Automatic system template copying on first run
- ROM discovery in multiple locations

### 📋 Ready for Phase 7: System Management & Assignments

See Phase 7 section below for upcoming features.

