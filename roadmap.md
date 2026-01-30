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

### Step 4.1b: Command Behavioral Alignment ✅ COMPLETE (Fixed in Phase 7.5)

**Note**: All commands now have full AmigaDOS-compatible behavior. See **Phase 7.5** for implementation details.

**DIR Command** (`sys/C/dir.c` v2.0)
- ✅ Template: `DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S`
- ✅ Pattern matching support (`#?` wildcards) for filtering
- ✅ ALL switch does recursive traversal with indentation
- ✅ DIRS/FILES switches for filtering
- ✅ Detailed listing with OPT=L
- ✅ Interactive mode (INTER) implemented
- ✅ Entry sorting (dirs first, then files, alphabetically)
- ✅ Ctrl+C break handling

**DELETE Command** (`sys/C/delete.c` v2.0)
- ✅ Template: `FILE/M/A,ALL/S,QUIET/S,FORCE/S`
- ✅ Multiple file support (/M modifier)
- ✅ ALL switch for recursive directory deletion
- ✅ QUIET switch to suppress output
- ✅ FORCE switch suppresses errors
- ✅ Pattern matching in FILE arguments (`DELETE #?.bak`)
- ✅ Ctrl+C break handling

**TYPE Command** (`sys/C/type.c` v2.0)
- ✅ Template: `FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S`
- ✅ Multiple file support (/M modifier)
- ✅ TO keyword for output redirection
- ✅ HEX switch for hex dump with ASCII column
- ✅ NUMBER switch for line numbering
- ✅ Single-file header suppression
- ✅ Ctrl+C break handling
- ❌ Pause/resume on keypress (deferred - requires raw console mode)

**MAKEDIR Command** (`sys/C/makedir.c`)
- ✅ Template: `NAME/M/A`
- ✅ Multiple directory creation (/M modifier)
- ✅ **COMPLETE** - matches AmigaDOS behavior

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

**Pending (Phase 8)**:
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

## Phase 6.5: Interrupt System & Preemptive Multitasking ✅ COMPLETE
**Goal**: Implement proper timer-driven preemptive multitasking to support professional Amiga software.

### Motivation

Professional Amiga applications (PageStream, FinalWriter, MaxonPascal, Reflections, etc.) were
written assuming the full AmigaOS preemptive multitasking environment:

- **Timer-driven scheduler**: CIA-B timer fires at 50/60Hz, triggering exec's scheduler
- **Interrupt-driven I/O**: Devices use interrupts; apps `Wait()` on signals from interrupt handlers
- **Real-time response**: timer.device for animations, cursor blink, auto-save
- **Background processing**: Subtasks for printing, spell-checking run concurrently with UI

The current implementation lacks interrupt support, causing tasks to only switch on explicit
`Wait()` calls. This breaks most real-world software.

### Current Problem

When `DIR` is executed via `System()`:
1. Child process is created and added to `TaskReady`
2. Parent polls with `Delay()` which uses `EMU_CALL_WAIT` (just `usleep()`)
3. No m68k task switch occurs - child never runs until parent exits
4. Output appears delayed or after subsequent commands

### Solution: Timer Interrupt Emulation

Implement proper 68000 interrupt handling with a host-side timer driving the scheduler.

**Architecture Overview**:
```
┌─────────────────────────────────────────────────────────┐
│                    Host (Linux)                         │
│  ┌─────────────┐    ┌──────────────┐                   │
│  │ setitimer   │───▶│ SIGALRM      │                   │
│  │ (50Hz)      │    │ handler      │                   │
│  └─────────────┘    └──────┬───────┘                   │
│                            │ sets g_pending_irq = 3    │
│  ┌─────────────────────────▼────────────────────────┐  │
│  │              m68k_execute() loop                  │  │
│  │  if (g_pending_irq && can_interrupt(SR)) {       │  │
│  │      push_exception_frame(PC, SR);               │  │
│  │      PC = read_vector(0x6C);  // Level 3         │  │
│  │  }                                               │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                 m68k Memory (Amiga)                     │
│  Vector 0x6C ──▶ exec Level 3 interrupt handler        │
│                    │                                    │
│                    ▼                                    │
│  ┌──────────────────────────────────────┐              │
│  │ Exec interrupt dispatcher            │              │
│  │   - Walk IntVects[INTB_VERTB] chain  │              │
│  │   - VBlank server checks quantum     │              │
│  │   - Calls Schedule() if expired      │              │
│  └──────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────┘
```

### Implementation Plan

#### Step 6.5.1: Basic Interrupt Infrastructure ✅ COMPLETE
**Goal**: Add interrupt pending/dispatch mechanism to emulator

- [x] Add `g_pending_irq` flags (one per level 1-7) to emulator state
- [x] Modify `m68k_execute()` loop to check for pending interrupts between instructions
- [x] Implement interrupt acknowledge: compare pending level vs INTENA mask
- [x] Musashi handles exception frame (PC, SR) and vector loading via `m68k_set_irq()`
- [x] Test with timer interrupt injection

**Files**: `src/lxa/lxa.c`

#### Step 6.5.2: Timer-Driven Scheduler (50Hz) ✅ COMPLETE
**Goal**: Host timer triggers Level 3 interrupt for VBlank/scheduler

- [x] Add host-side timer using `setitimer(ITIMER_REAL, ...)` at 50Hz
- [x] SIGALRM handler sets `g_pending_irq |= (1 << 3)` for Level 3
- [x] ROM Level 3 vector handler (`_handleIRQ3`) was already implemented
- [x] Handler counts quantum and calls `_exec_Schedule()` when expired
- [ ] Make timer frequency configurable (50Hz PAL / 60Hz NTSC) - future enhancement

**Files**: `src/lxa/lxa.c`, `src/rom/exceptions.s`

#### Step 6.5.3: Exec Interrupt Server Chains (Future Enhancement)
**Goal**: Proper AddIntServer/RemIntServer for extensibility

**Note**: The scheduler quantum logic is implemented directly in the Level 3 interrupt handler
(`exceptions.s`). The APIs below are for future extensibility, not required for basic operation.

- [ ] Implement `IntVects[]` array in ExecBase for interrupt server chains
- [ ] Implement `AddIntServer()` - add handler to chain for given interrupt
- [ ] Implement `RemIntServer()` - remove handler from chain
- [ ] Create VBlank server that decrements task quantum and requests reschedule
- [ ] Move scheduler quantum logic from ad-hoc to proper VBlank server

**Files**: `src/rom/exec.c`

#### Step 6.5.4: Interrupt Levels & CIA Emulation (Future Enhancement)
**Goal**: Support standard Amiga interrupt levels

| Level | Vector | Use |
|-------|--------|-----|
| 1 | 0x64 | TBE (serial transmit buffer empty) |
| 2 | 0x68 | CIA-A (keyboard, parallel, timer) |
| 3 | 0x6C | CIA-B (VBlank, timer) - **scheduler lives here** |
| 4 | 0x70 | Audio |
| 5 | 0x74 | Disk |
| 6 | 0x78 | External/CIA-A |
| 7 | 0x7C | NMI (unused) |

- [ ] Set up autovector table at 0x64-0x7C during ROM init
- [ ] Implement basic CIA-B timer A for scheduler tick
- [ ] Stub other levels to return (RTE) for now

**Files**: `src/rom/exec.c`, `src/rom/romhdr.s`

#### Step 6.5.5: timer.device (Future Enhancement)
**Goal**: Apps can request timer events

**Note**: Currently `Delay()` uses EMU_CALL_WAIT with scheduler yielding, which works
for basic use cases. Full timer.device support would enable precise timing.

- [ ] Create timer.device skeleton in ROM
- [ ] Implement `TR_ADDREQUEST` for one-shot timer events
- [ ] Implement `TR_GETSYSTIME` for current system time
- [ ] Timer requests use CIA timer and signal requesting task when complete
- [ ] Test with `Delay()` implemented via timer.device

**Files**: `src/rom/lxa_timer.c` (new), `src/rom/exec.c`

### Testing Strategy

1. **Unit test**: Manual interrupt injection triggers handler
2. **Integration test**: signal_pingpong works without explicit `Wait()` hacks
3. **Shell test**: `DIR` output appears immediately before next prompt
4. **Stress test**: Rapid command execution doesn't deadlock or race

### Acceptance Criteria ✅ ALL MET

- [x] 50Hz timer interrupt fires and triggers exec scheduler
- [x] Tasks preempt automatically without explicit `Wait()` calls
- [x] `DIR` and other commands output appears in correct order
- [x] All existing tests pass (6/6)
- [x] No measurable performance regression
- [ ] `timer.device` basic requests work (Future Enhancement: Step 6.5.5)

### Critical Bug Fixes Applied (2024)

1. **SFF_QuantumOver bit mismatch**: The C code was setting bit 6 of the SysFlags WORD,
   but the assembly code tests bit 6 of the HIGH BYTE (which is bit 14 of the word on
   big-endian m68k). Fixed by using `(1 << 14)` in C.
   - Files: `src/rom/lxa_dos.c`, `src/rom/exec.c`

2. **Process exit cleanup**: `_defaultTaskExit` was calling `RemTask()` directly, which
   doesn't clean up CLI TaskArray entries. Fixed to detect NT_PROCESS tasks and call
   `Exit()` instead, which properly calls `freeTaskNum()`.
   - Files: `src/rom/exec.c`

3. **Delay() scheduler integration**: Implemented proper scheduler yielding in `Delay()`
   by setting SFF_QuantumOver and calling Schedule() via Supervisor() when TaskReady
   list is non-empty.
   - Files: `src/rom/lxa_dos.c`

### Future Extensions (Not in 6.5)

- Full CIA chip emulation (ICR registers, both timers)
- Audio interrupts for sampled sound
- Disk interrupts for trackdisk.device
- Serial/parallel interrupts
- 68020+ interrupt stack frame differences

---

## Phase 7: System Management & Assignments ✅ COMPLETE
**Goal**: Advanced system control and logical drive management.

### Step 7.1: Assignment API ✅ COMPLETE
- ✅ **API Implementation**: `AssignLock()`, `AssignPath()`, `AssignLate()`, `AssignAdd()`, `RemAssignList()` - `src/rom/lxa_dos.c`
- ✅ **VFS Layer**: Assign storage and lookup integrated into VFS - `src/lxa/vfs.c`
- ✅ **EMU_CALL Bridge**: `EMU_CALL_DOS_ASSIGN_ADD`, `EMU_CALL_DOS_ASSIGN_REMOVE`, `EMU_CALL_DOS_ASSIGN_LIST` - `src/lxa/lxa.c`
- ✅ **ASSIGN Command**: `sys/C/assign.c` with AmigaDOS template `NAME,TARGET,LIST/S,EXISTS/S,DISMOUNT/S,DEFER/S,PATH/S,ADD/S,REMOVE/S`
  - Create assigns: `ASSIGN TEST: SYS:C`
  - Remove assigns: `ASSIGN TEST: REMOVE`
  - List assigns: `ASSIGN LIST`
  - Check existence: `ASSIGN SYS: EXISTS`
  - Deferred assigns: `ASSIGN WORK: DH0:Work DEFER`
  - Path assigns: `ASSIGN LIBS: SYS:Libs PATH`

**Implementation Notes**:
- `NameFromLock()` currently returns Linux paths; `_dos_assign_add()` detects and handles both Linux and Amiga paths
- Assigns are session-specific (not persisted between lxa invocations)
- System assigns (C:, S:, LIBS:, etc.) are configured via VFS drive mappings in config.ini

### Step 7.2: System Tools ✅ COMPLETE
**Process Management**:
- ✅ **STATUS** (`sys/C/status.c`): List running processes with CLI number, priority, state, stack size, and name
  - Template: `FULL/S,TCB/S,CLI=ALL/S,COM=COMMAND/K`
  - Shows processes from TaskReady, TaskWait lists and current task
- ✅ **BREAK** (`sys/C/break.c`): Send break signals to a process
  - Template: `PROCESS/A/N,ALL/S,C/S,D/S,E/S,F/S`
  - Supports Ctrl-C, Ctrl-D, Ctrl-E, Ctrl-F signals
- ✅ **RUN** (`sys/C/run.c`): Run a command in the background
  - Template: `COMMAND/F`
  - Loads and executes program without waiting for completion
- ✅ **CHANGETASKPRI** (`sys/C/changetaskpri.c`): Change task priority
  - Template: `PRI=PRIORITY/A/N,PROCESS/K/N`
  - Implemented `_exec_SetTaskPri()` properly with priority reordering

**Memory & Stack**:
- ✅ **AVAIL** (`sys/C/avail.c`): Show memory availability
  - Template: `CHIP/S,FAST/S,TOTAL/S,FLUSH/S`
  - Implemented `_exec_AvailMem()` with MEMF_LARGEST support
- ✅ **STACK** (`sys/C/stack.c`): Show/set stack size
  - Template: `SIZE/N`
  - Shows stack size, usage, and free space

**Not Implemented (Future Phase)**:
- Device Control: `MOUNT`, `RELABEL`, `LOCK` - deferred to Phase 8+

---

## Phase 7.5: Command Feature Completeness ✅ COMPLETE
**Goal**: Complete the existing AmigaDOS commands to match their authentic behavior as specified in `AMIGA_COMMAND_GUIDE.md`.

**Status**: ✅ COMPLETE - All core command features implemented (January 2026)

### Implementation Summary

All three commands (DIR, TYPE, DELETE) have been updated with:
- **Ctrl+C break handling** via `SetSignal(SIGBREAKF_CTRL_C)` pattern
- **Pattern matching support** for wildcards (`#?`, `?`, etc.)
- **Sorted output** where applicable
- **AmigaDOS-authentic behavior**

### Step 7.5.1: DIR Command ✅ COMPLETE

**Template**: `DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S`

**File**: `sys/C/dir.c` (v2.0)

| Feature | Status | Notes |
|---------|--------|-------|
| ALL/S | ✅ FIXED | Recursive directory traversal with indentation |
| INTER/S | ✅ IMPLEMENTED | Interactive mode with ?, Return, E, B, T, Q commands |
| Sorting | ✅ IMPLEMENTED | Directories first (alphabetically), then files (alphabetically) |
| Pattern matching | ✅ IMPLEMENTED | Full AmigaDOS wildcard support |
| Ctrl+C handling | ✅ IMPLEMENTED | Outputs `***BREAK` and exits cleanly |
| DIRS/FILES | ✅ WORKING | Filter by type |
| OPT L | ✅ WORKING | Detailed listing with protection bits and sizes |

### Step 7.5.2: TYPE Command ✅ COMPLETE

**Template**: `FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S`

**File**: `sys/C/type.c` (v2.0)

| Feature | Status | Notes |
|---------|--------|-------|
| FROM/M | ✅ DONE | Multiple files supported |
| TO/K | ✅ DONE | Output redirection works |
| HEX/S | ✅ DONE | Hex dump with ASCII column |
| NUMBER/S | ✅ DONE | Line numbering works |
| Ctrl+C break | ✅ IMPLEMENTED | Outputs `***BREAK` and exits cleanly |
| Single-file header | ✅ FIXED | Header only shown for multiple files |
| Pause/Resume | ❌ DEFERRED | Requires raw console mode (Phase 8+) |

### Step 7.5.3: DELETE Command ✅ COMPLETE

**Template**: `FILE/M/A,ALL/S,QUIET/S,FORCE/S`

**File**: `sys/C/delete.c` (v2.0)

| Feature | Status | Notes |
|---------|--------|-------|
| FILE/M/A | ✅ DONE | Multiple files supported |
| ALL/S | ✅ DONE | Recursive delete implemented |
| QUIET/S | ✅ DONE | Suppresses output |
| FORCE/S | ✅ DONE | Suppresses errors |
| Pattern matching | ✅ IMPLEMENTED | Supports `DELETE #?.bak` etc. |
| Ctrl+C handling | ✅ IMPLEMENTED | Aborts but keeps already-deleted files |

### Step 7.5.4: MAKEDIR Command ✅ COMPLETE (No Changes)

**Template**: `NAME/M/A`

Already complete and matches AmigaDOS behavior.

### Step 7.5.5: Common Infrastructure ✅ COMPLETE

- ✅ **Ctrl+C Signal Handling**: All commands use `SetSignal(0L, SIGBREAKF_CTRL_C)` pattern
- ✅ **Pattern Matching**: `ParsePattern()`/`MatchPattern()` used in DIR and DELETE
- ❌ **Console Raw Mode**: Deferred to Phase 8+ (needed for pause/resume, full INTER mode)

### Tests Added

- ✅ `tests/dos/dir_command/` - Tests pattern matching functionality (ParsePattern/MatchPattern)

---

## Phase 8: New Commands & Advanced Utilities
**Goal**: Reach Milestone 1.0 with a polished toolset.

### Step 8.1: File Manipulation Commands
- [ ] **COPY** - Copy files/directories with recursive support
  - Template: `FROM/M/A,TO/A,ALL/S,CLONE/S,DATES/S,NOPRO/S,COM/S,QUIET/S`
- [ ] **RENAME** - Rename/move files
  - Template: `FROM/A,TO/A,QUIET/S`
- [ ] **JOIN** - Concatenate files
  - Template: `FROM/A/M,AS=TO/A`

### Step 8.2: File Information Commands
- [ ] **PROTECT** - Change file protection bits
  - Template: `FILE/A,FLAGS/K,ADD/S,SUB/S,ALL/S`
- [ ] **FILENOTE** - Set file comment
  - Template: `FILE/A,COMMENT,ALL/S`
- [ ] **LIST** - Detailed directory listing
  - Template: `DIR/M,P=PAT/K,KEYS/S,DATES/S,NODATES/S,TO/K,SUB/S,SINCE/K,UPTO/K,QUICK/S,BLOCK/S,NOHEAD/S,FILES/S,DIRS/S,LFORMAT/K,ALL/S`

### Step 8.3: Search & Text Commands
- [ ] **SEARCH** - Search for text in files
  - Template: `FROM/M/A,SEARCH/A,ALL/S,NONUM/S,QUIET/S,QUICK/S,FILE/S,PATTERN/S`
- [ ] **SORT** - Sort lines of text
  - Template: `FROM/A,TO/A,COLSTART/K/N,CASE/S,NUMERIC/S`
- [ ] **EVAL** - Evaluate arithmetic expressions
  - Template: `VALUE1/A,OP,VALUE2/M,TO/K,LFORMAT/K`

### Step 8.4: Environment & Variables
- [ ] **SET** - Set local variable
  - Template: `NAME,VALUE/F`
- [ ] **SETENV** - Set global environment variable
  - Template: `NAME,SAVE/S,VALUE/F`
- [ ] **GETENV** - Get environment variable value
  - Template: `NAME/A`
- [ ] **UNSET** / **UNSETENV** - Remove variables

### Step 8.5: System Commands
- [ ] **VERSION** - Display version information
  - Template: `NAME,VERSION/N,REVISION/N,FILE/S,FULL/S,RES/S`
- [ ] **WAIT** - Wait for time/event
  - Template: `TIME,SEC/S,MIN/S,UNTIL/S`
- [ ] **INFO** - Display disk information
  - Template: `DEVICE/M`
- [ ] **DATE** - Display/set system date
  - Template: `DAY,DATE,TIME,TO=VER/K`
- [ ] **MAKELINK** - Create hard/soft links
  - Template: `FROM/A,TO/A,HARD/S,FORCE/S`

### Step 8.6: Final Polish
- Documentation update: `README.md` with all commands
- AROS reference review for edge cases
- Test coverage for all commands

---

## Implementation Rules
1. **Clean Room Approach**: AROS/Kickstart source is for architectural reference ONLY.
2. **Host-First**: Prefer host-side (`emucall`) implementations for filesystem tasks.
3. **Internal vs External**: Compiled-in Shell commands for speed, separate `C:` binaries for extensibility.
4. **Amiga Authenticity**: Commands must use AmigaDOS argument syntax (templates, keywords, switches) not Unix-style flags.
   - Use `ReadArgs()` template parsing in all C: commands
   - Templates: `DIR,OPT/K,ALL/S,DIRS/S` not `dir -la`
   - Keywords: `ALL`, `DIRS`, `TO` not `-a`, `-d`, `-o`

## Next Steps: Phase 7.5 - Command Feature Completeness

### Immediate Priority:
Before adding new commands (Phase 8), existing commands must match authentic AmigaDOS behavior.

### Phase 7.5: Command Fixes (Current)
Critical issues that need attention:
1. **DIR ALL/S**: Currently shows hidden files instead of recursive traversal - **CRITICAL BUG**
2. **DIR INTER**: Interactive mode not implemented
3. **TYPE pause/resume**: Missing Space/Return keypress handling
4. **DELETE patterns**: Wildcard file matching not supported
5. **Ctrl+C handling**: All commands need break signal support

### Phase 8: Advanced Utilities (Next)
After Phase 7.5 is complete:
- **Utilities**: `COPY` (recursive), `JOIN`, `SORT`, `EVAL`, `SEARCH`
- **Compatibility**: `VERSION`, `WAIT`, `MAKELINK`
- **Device Control**: `MOUNT`, `RELABEL`, `LOCK` (deferred from Phase 7)

### Future Enhancements
The following are optional improvements:
- Step 6.5.3: AddIntServer/RemIntServer APIs for extensibility
- Step 6.5.4: Full CIA emulation  
- Step 6.5.5: timer.device implementation for precise timing

---

## Status Summary

### ✅ Phase 1-7 Complete

All foundational phases are complete:
- **Phase 1**: Exec multitasking (signals, messages, semaphores, processes)
- **Phase 2**: Configuration & VFS (drive mapping, case-insensitive paths)
- **Phase 3**: Filesystem API (locks, examine, directories, file operations)
- **Phase 4**: Userland commands (DIR, TYPE, DELETE, MAKEDIR with templates)
- **Phase 5**: Interactive Shell (scripting, control flow, argument passing)
- **Phase 6**: Build System Migration (CMake, installation, LXA_PREFIX)
- **Phase 6.5**: Preemptive Multitasking (50Hz timer, interrupt-driven scheduler)
- **Phase 7**: System Management (Assignments, STATUS, AVAIL, STACK, RUN, BREAK, CHANGETASKPRI)

### 🚧 Phase 7.5 In Progress: Command Feature Completeness (January 2026)

**Issue**: Existing commands have gaps compared to authentic AmigaDOS behavior.

**Critical Bugs Found**:
| Command | Issue | Severity |
|---------|-------|----------|
| DIR | ALL/S shows hidden files instead of recursive traversal | 🔴 CRITICAL |
| DIR | INTER (interactive mode) not implemented | 🟠 HIGH |
| DIR | No two-column output, no sorting | 🟡 MEDIUM |
| TYPE | No pause/resume on keypress | 🟡 MEDIUM |
| DELETE | No pattern matching support | 🟡 MEDIUM |
| ALL | No Ctrl+C break signal handling | 🟡 MEDIUM |

**See Phase 7.5 section above for detailed implementation tasks.**

### ✅ Phase 7 Complete: System Management (January 2026)

**Implemented**: Full system management tools for process control and memory inspection.

**System Commands Added**:
- `STATUS` - List running processes with CLI number, priority, state, stack, name
- `AVAIL` - Show memory availability (chip/fast/total with largest free block)
- `STACK` - Show/set stack size for current process
- `RUN` - Execute commands in background without waiting
- `BREAK` - Send Ctrl-C/D/E/F signals to processes
- `CHANGETASKPRI` - Change task priority (-128 to 127)
- `ASSIGN` - Manage logical drive assignments

**Exec APIs Implemented**:
- `_exec_AvailMem()` - Query available memory with MEMF_LARGEST support
- `_exec_SetTaskPri()` - Change task priority with proper queue reordering

### ✅ Phase 6.5 Complete: Preemptive Multitasking (January 2025)

**Implemented**: Timer-driven preemptive multitasking using host-side `setitimer()` at 50Hz.

**Key Features**:
- 50Hz timer interrupt (SIGALRM) triggers Level 3 interrupt
- Exec scheduler runs on each timer tick when quantum expires
- `Delay()` properly yields to scheduler when tasks are ready
- Process exit cleanup properly handles TaskArray entries

**Critical Bug Fixes**:
1. **SFF_QuantumOver bit mismatch**: Fixed C code to use `(1 << 14)` matching assembly's bit 6 of HIGH byte
2. **Process exit cleanup**: `_defaultTaskExit()` now calls `Exit()` for NT_PROCESS tasks
3. **Delay() scheduler integration**: Sets SFF_QuantumOver and calls Schedule() via Supervisor()

**Results**:
- ✅ DIR command output appears immediately in correct order
- ✅ All 6 integration tests pass
- ✅ Multitasking works correctly (signal_pingpong demonstrates task switching)
- ✅ Shell commands complete cleanly without output timing issues

**Future Enhancements** (not required for basic operation):
- Step 6.5.3: AddIntServer/RemIntServer APIs
- Step 6.5.4: Full CIA emulation
- Step 6.5.5: timer.device for precise timing

### 📋 Current Focus: Phase 7.5 - Fix Existing Commands

Before adding new commands, the priority is fixing existing command behavior:
1. Fix DIR ALL/S to do recursive traversal (not hidden files)
2. Implement DIR INTER interactive mode
3. Add two-column output and sorting to DIR
4. Add pause/resume to TYPE
5. Add pattern matching to DELETE
6. Add Ctrl+C handling to all commands
