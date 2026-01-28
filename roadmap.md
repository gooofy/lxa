# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec**, **DOS**, and a functional **Userland**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

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

## Phase 4: Basic Userland & Metadata
**Goal**: Implement the first set of essential AmigaDOS commands.

### Step 4.1: Essential Userland (C:)
Implement basic commands as separate binaries in `SYS:C`:
- **DIR / LIST**: Directory exploration.
- **TYPE**: File viewing.
- **MAKEDIR / DELETE**: Filesystem manipulation.
- **INFO / DATE**: System information.

### Step 4.2: Metadata & Variables
- **API Implementation**: `Rename()`, `SetProtection()`, `SetComment()`, `SetDate()`.
- **Environment API**: `GetVar()`, `SetVar()`, `FindVar()`, `DeleteVar()` (Local/Global).
- **Tools**: `RENAME`, `PROTECT`, `FILENOTE`, `SET`, `SETENV`, `GETENV`.

---

## Phase 5: The Interactive Shell & Scripting
**Goal**: A fully functional, interactive Amiga Shell experience.

### Step 5.1: Interactive Shell Binary
- **Interactive Mode**: Greet users with the classic `1.SYS:> ` prompt.
- **Command Dispatcher**: Support for "Internal" commands (fast-path for `CD`, `PATH`, `PROMPT`).
- **Input Handling**: Support for command history and basic line editing.

### Step 5.2: Scripting Logic
- **API Implementation**: `Execute()` logic, handling the `s` bit.
- **Control Flow Commands**: `IF`, `ELSE`, `ENDIF`, `SKIP`, `LAB`, `QUIT`, `FAILAT`.
- **Communication**: `ECHO` (NOLINE support), `ASK`.
- **Shell Tools**: `ALIAS`, `WHICH`, `WHY`, `FAULT`.

---

## Phase 6: System Management & Assignments
**Goal**: Advanced system control and logical drive management.

### Step 6.1: Assignment API
- **API Implementation**: `AssignLock()`, `AssignName()`, `AssignPath()`.
- **Search Path**: Proper integration of `SYS:C` and user-defined paths into the Shell loader.

### Step 6.2: System Tools
- **Process Management**: `STATUS`, `BREAK`, `RUN`, `CHANGETASKPRI`.
- **Memory & Stack**: `AVAIL`, `STACK`.
- **Device Control**: `ASSIGN`, `MOUNT`, `RELABEL`, `LOCK`.

---

## Phase 7: Advanced Utilities & Finalization
**Goal**: Reach Milestone 1.0 with a polished toolset.

- **Utilities**: `COPY` (recursive), `JOIN`, `SORT`, `EVAL`, `SEARCH`.
- **Compatibility**: `VERSION`, `WAIT`, `MAKELINK`.
- **Final Polish**: Documentation, `README.md` update, and AROS reference review for edge cases.

---

## Implementation Rules
1. **Clean Room Approach**: AROS/Kickstart source is for architectural reference ONLY.
2. **Host-First**: Prefer host-side (`emucall`) implementations for filesystem tasks.
3. **Internal vs External**: Compiled-in Shell commands for speed, separate `C:` binaries for extensibility.
