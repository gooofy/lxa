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

- **Phase 1: Exec Core & Tasking Infrastructure**
  - Signals, Message Ports, Processes, and Stack Management.
- **Phase 2: Configuration & VFS Layer**
  - Host configuration (`config.ini`) and case-insensitive Linux filesystem mapping.
- **Phase 3: First-Run Experience & Filesystem API**
  - Auto-provisioning of `~/.lxa`, Locks, and Examination API (`Lock`, `Examine`, etc.).
- **Phase 4: Basic Userland & Metadata**
  - Core command framework (`ReadArgs`), Pattern Matching, and initial `C:` tools.
- **Phase 5: The Interactive Shell & Scripting**
  - Amiga-compatible Shell with interactive mode, scripts, and control flow.
- **Phase 6: Build System Migration (CMake)**
  - Cross-compilation toolchain and standardized installation layout.
- **Phase 6.5: Interrupt System & Preemptive Multitasking**
  - 50Hz timer-driven scheduler and proper interrupt handling.
- **Phase 7: System Management & Assignments**
  - Process management (`STATUS`, `RUN`, `BREAK`), logical assigns (`ASSIGN`), and system diagnostics.
- **Phase 7.5: Command Feature Completeness**
  - Advanced behavior for `DIR`, `TYPE`, and `DELETE` (Wildcards, Ctrl+C, Interactive mode).

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
- [ ] **SET** / **SETENV** - Manage local and global variables
- [ ] **GETENV** - Retrieve environment variables
- [ ] **UNSET** / **UNSETENV** - Remove variables

### Step 8.5: System Commands
- [ ] **VERSION** - Display version information
- [ ] **WAIT** - Wait for time/event
- [ ] **INFO** - Display disk information
- [ ] **DATE** - Display/set system date
- [ ] **MAKELINK** - Create hard/soft links

### Step 8.6: Final Polish
- Documentation update: `README.md` with all commands
- AROS reference review for edge cases
- 100% Test coverage for all commands

---

## Implementation Rules
1. **Clean Room Approach**: AROS/Kickstart source is for architectural reference ONLY.
2. **Host-First**: Prefer host-side (`emucall`) implementations for filesystem tasks.
3. **Amiga Authenticity**: Commands must use AmigaDOS argument syntax (templates, keywords, switches).
4. **Tooling**: Use `ReadArgs()` template parsing in all `C:` commands.
