# lxa Roadmap: Towards a Fuller AmigaOS Implementation

This document outlines the strategic plan for expanding `lxa` into a more complete AmigaOS-compatible environment, focusing on **Exec** and **DOS**. We follow a "WINE-like" approach for DOS, where filesystem operations are efficiently bridged to the host Linux system.

---

## Core Philosophy & Boundaries

### WINE-like DOS Approach
Instead of emulating hardware-level disk controllers (like the trackdisk.device) and running Amiga-native filesystems (like OFS/FFS) on top, `lxa` maps Amiga logical drives directly to Linux directories.

- **Drives as Directories**: `DH0:`, `DF0:`, etc., are backed by standard Linux directories.
- **Host Interception**: DOS calls (`Open`, `Read`, `Lock`, etc.) are intercepted via `emucall` and handled by the host `lxa` process using Linux system calls.
- **Case Insensitivity**: The host layer handles Amiga's case-insensitive path resolution over the case-sensitive Linux filesystem.

### Support Boundaries

| Feature | Status/Goal | Note |
| :--- | :--- | :--- |
| **Exec Multitasking** | Full Support | Signal-based multitasking, Message Ports, Semaphores. |
| **DOS Filesystem** | Hosted | Mapped to Linux via VFS layer. |
| **Device Handlers** | Partial | Built-in handlers for `SYS:`, `CON:`, `NIL:`. No external `L:` handlers initially. |
| **Assigns** | Full Support | Logical assigns, multi-directory assigns (Paths). |
| **Floppy Emulation** | Directory-based | Floppies appear as directories. ADF support is out of scope for now. |
| **Low-level Disk I/O** | Unsupported | No direct track access or custom filesystems. |

---

## Configuration & Drive Concept

The emulated environment is configured via a host-side configuration file, typically located at `~/.lxa/config.toml` (or specified via CLI).

### Configuration File Structure (Concept)
```toml
# lxa configuration file

[system]
# General system settings
ram_size = 10485760  # 10 MB
rom_path = "../rom/lxa.rom"

[drives]
# Amiga Drive = Linux Path
# The first drive defined is typically the boot drive (SYS:)
DH0 = "~/.lxa/drive_hd0"
DH1 = "~/amiga_projects"
Work = "/mnt/data/amiga/work"

[floppies]
# Floppies are also mapped to directories for a WINE-style experience
DF0 = "~/.lxa/floppy0"
DF1 = "~/downloads/new_game_dir"

[assigns]
# Logical assigns defined at startup
S = "SYS:S"
Libs = "SYS:Libs"
Devs = "SYS:Devs"
C = "SYS:C"
```

### AmigaDOS Concepts & Handlers

In this WINE-style approach, we balance between host-side efficiency and AmigaOS architectural compatibility.

- **Internal Handlers**: Core devices like `CON:`, `RAW:`, `NIL:`, and `RAM:` are implemented as internal "pseudo-handlers" in the host `lxa` process. They are reached via `emucall` when `Open()` is called on these names.
- **Filesystem Handlers**: Instead of a separate Amiga process for each drive (like `L:FileSystem`), all filesystem operations for `DHx:` and `DFx:` are routed to the host's VFS layer.
- **Expansion Space**: For future expansion, `lxa_dos` will support the `DosPacket` interface. This allows Amiga-native handlers to be implemented if needed (e.g. for specialized network protocols), but the default path is always host-bridged.

## Boundaries: What is NOT supported
- **Custom Filesystems**: You cannot use `L:FastFileSystem` or similar on emulated drives. The host's native filesystem is used.
- **Direct Block Access**: `CMD_READ`/`CMD_WRITE` to trackdisk-like devices will not be supported for these mapped drives.
- **Physical Media**: No direct support for physical floppy drives or CD-ROMs via Amiga-native drivers.

---

## Phase 1: Exec Core & Tasking Infrastructure ✓ COMPLETE
**Goal**: Establish a stable, multitasking foundation for the OS.

### Step 1.1: Signal & Message Completion ✓ COMPLETE
- **Status**: All functions implemented and tested
- **Tasks**:
    - ✓ Implement `Wait()`, `Signal()`, `SetSignal()`, `SetExcept()` in `exec.c`
    - ✓ Implement `PutMsg()`, `GetMsg()`, `ReplyMsg()`, `WaitPort()` in `exec.c`
    - ✓ Implement `AddPort()`, `RemPort()`, `FindPort()` in `exec.c`
    - ✓ Implement `CreateMsgPort()`, `DeleteMsgPort()` in `exec.c`
    - ✓ Implement `CreateIORequest()`, `DeleteIORequest()` in `exec.c`
    - ✓ Initialize `SysBase->PortList` in coldstart
- **Testing**: Ping-pong test (`tests/exec/signal_pingpong/`) validates two tasks exchanging messages.

### Step 1.2: Process & Stack Management ✓ COMPLETE
- **Status**: All functions implemented and tested
- **Tasks**:
    - ✓ Finalize `CreateNewProc()` in `lxa_dos.c` with proper error handling
    - ✓ Add minimum stack size validation (4096 bytes)
    - ✓ Add stack alignment to 4-byte boundary
    - ✓ Implement `Exit()` with resource cleanup (CLI structure, callbacks)
    - ✓ Implement task number assignment (static counter, see known issues)
- **Testing**: Helloworld test and ping-pong test validate process creation and termination.

### Known Issues (Phase 1) - ALL RESOLVED

- **Child process exit crash**: ✓ FIXED
  - When the main process returned from main(), a corrupted return address could cause the emulator to crash with "undefined EMU_CALL #255".
  - **Solution**: Modified the emulator to handle undefined EMU_CALL gracefully by setting `g_running = FALSE` instead of crashing. This allows the emulator to exit cleanly when the main program has finished.
  - **Files changed**: `src/lxa/lxa.c` (default case in `op_illg()` switch)

- **Task numbering**: ✓ FIXED
  - Previously used a static counter instead of proper `RootNode->rn_TaskArray` for CLI process numbering.
  - **Solution**: Implemented proper AmigaOS-compatible task numbering using `RootNode->rn_TaskArray`:
    - Added `allocTaskNum()` to find/allocate free CLI slots with dynamic array expansion
    - Added `freeTaskNum()` to release slots for reuse when processes exit
    - Integrated with `CreateNewProc()` and `Exit()` for automatic allocation/deallocation
  - **Files changed**: `src/rom/lxa_dos.c` (added RootNode initialization and task array management)

---

## Phase 2: Configuration & VFS Layer
**Goal**: Support the `~/.lxa` structure and flexible drive mapping.

### Step 2.1: Host Configuration System
- **Tasks**:
    - Implement a config parser (e.g., `.lxa/config.ini`) in the host `lxa` code.
    - Default path: `~/.lxa/drive_hd0` maps to `SYS:`.
    - Allow mapping: `[drives] DH1=/home/user/data`, `DF0=/tmp/floppy`.
- **Testing Strategy**: Run `lxa` and verify it can list files from different Linux directories via Amiga device names.

### Step 2.2: Case-Insensitive Path Resolution
- **Tasks**:
    - Implement a robust path resolver in `lxa.c` that handles `Foo/Bar` matching `foo/bar` on Linux.
    - Support Amiga path separators (`/`, `:`, `.`).
- **Testing Strategy**: Create files with mixed casing in Linux and attempt to `Open()` them from Amiga code with different casing.

---

## Phase 3: AmigaDOS File Management
**Goal**: Complete the core DOS API for file and directory manipulation.

### Step 3.1: Locks & Examination
- **Tasks**:
    - Implement `Lock()`, `UnLock()`, `DupLock()`, `SameLock()`.
    - Implement `Examine()`, `ExNext()`, `ExAll()`.
    - Bridge these to `stat`, `opendir`, `readdir` on the host.
- **Testing Strategy**: Run a `dir` or `ls` command in the emulated environment.
- **Documentation**: Document the mapping of Amiga `FileInfoBlock` attributes to Linux permissions.

### Step 3.2: Assigns & Path Management
- **Tasks**:
    - Implement `AssignLock()`, `AssignName()`.
    - Support logical assigns (e.g., `LIBS:`, `DEVS:`).
- **Testing Strategy**: Assign a logical name to a subdirectory and verify `LoadSeg()` can find libraries there.

---

## Phase 4: Device Handlers & Console Integration
**Goal**: Integrate I/O devices into the DOS object model.

### Step 4.1: Console Handler Improvements
- **Tasks**:
    - Expand `lxa_dev_console.c` to support standard ANSI sequences.
    - Integrate `CON:` handler into the `Open()` logic.
- **Testing Strategy**: Run a text-based tool that uses cursor positioning.

### Step 4.2: NIL: and Internal Handlers
- **Tasks**:
    - Implement the `NIL:` device handler (host-side `/dev/null`).
    - Map `ERROR_` codes from Linux `errno` to AmigaDOS error codes.
- **Testing Strategy**: Redirect output to `NIL:` and verify it behaves correctly.

---

## Phase 5: Finalization & Reference Integration
**Goal**: Reach Milestone 1.0 of the expanded implementation.

### Step 5.1: AROS Reference Review
- **Tasks**:
    - Review AROS `rom/dos` and `rom/exec` for missing edge cases in logic (without copying code).
    - Ensure structure compatibility with common Amiga binaries.
    - AROS source code can be found under `others/AROS-20231016-source`

### Step 5.2: Documentation Update
- **Tasks**:
    - Finalize `README.md` with setup instructions for the WINE-style drives.
    - Create a user guide for the configuration file.

---

## Implementation Rules
1. **No Code Copying**: AROS source is for architectural reference ONLY. All code in `lxa` must be original or appropriately licensed.
2. **Clean Room Approach**: When referencing AROS, focus on the *what* (function signatures, data structures) rather than the *how* (implementation details).
3. **Host-First**: Always prefer a host-side (`emucall`) implementation for filesystem tasks for better performance and host integration.
