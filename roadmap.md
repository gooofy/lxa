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

### Phase 1-7: Foundation (COMPLETE)
- **Phase 1**: Exec Core & Tasking - Signals, Message Ports, Processes, Stack Management
- **Phase 2**: Configuration & VFS - Host config, case-insensitive filesystem mapping
- **Phase 3**: First-Run & Filesystem API - Auto-provisioning, Locks, Examination API
- **Phase 4**: Basic Userland - ReadArgs, Pattern Matching, initial C: tools
- **Phase 5**: Interactive Shell - Shell with scripts and control flow
- **Phase 6**: CMake Build System - Cross-compilation toolchain
- **Phase 6.5**: Interrupt System - 50Hz timer-driven preemptive scheduler
- **Phase 7**: System Management - STATUS, RUN, BREAK, ASSIGN commands
- **Phase 7.5**: Command Completeness - DIR, TYPE, DELETE wildcards and Ctrl+C

### Phase 8: Test Coverage & Quality Assurance (COMPLETE)
Comprehensive test infrastructure with 100+ integration tests covering:
- **DOS Library**: Lock/Unlock, Examine/ExNext, File I/O, Seek, Delete/Rename, CreateDir, Pattern Matching, ReadArgs, SystemTagList, Execute
- **Exec Library**: Tasks, Signals, Memory, Semaphores, Message Ports, Lists
- **Commands**: DIR, DELETE, TYPE, MAKEDIR, ASSIGN, STATUS, BREAK, RUN
- **Shell**: Control flow, aliases, scripts, variables
- **Host/Emulation**: Memory access, VFS layer, emucalls, configuration
- **Stress Tests**: Memory (1000s of allocations), Tasks (50+), Filesystem (200+ files)

Key bug fixes: ROM static data, SystemTagList use-after-free, ReadArgs stack overflow, Execute() crash

### Phase 9: New Commands & Utilities (COMPLETE)
Full command set implemented:
- **File Commands**: COPY, RENAME, JOIN, PROTECT, FILENOTE, LIST, SEARCH, SORT
- **System Commands**: VERSION, WAIT, INFO, DATE, MAKELINK, EVAL
- **Environment**: SET, SETENV, GETENV, UNSET, UNSETENV
- **DOS Functions**: SetVar, GetVar, DeleteVar, FindVar

### Phase 10: DOS Library Completion (COMPLETE)
~80% AmigaOS DOS compatibility achieved:
- **Steps 10.1-10.4**: Path utilities (FilePart, PathPart, AddPart), Character I/O (FGetC, FPutC, UnGetC, FGets, FPuts, PutStr, WriteChars), Formatted output (VFPrintf, VPrintf), I/O redirection (SelectInput, SelectOutput)
- **Step 10.5**: File handle utilities (DupLockFromFH, ExamineFH, NameFromFH, ParentOfFH, OpenFromLock)
- **Step 10.6**: Break handling (CheckSignal, WaitForChar)
- **Step 10.7**: Pattern matching (MatchFirst/MatchNext/MatchEnd, MatchPatternNoCase, ParsePatternNoCase)
- **Step 10.8**: Date/Time (DateToStr, StrToDate, CompareDates)
- **Step 10.9**: Error handling (Fault, PrintFault with full error message table)
- **Step 10.10**: DosList access (LockDosList, UnLockDosList, FindDosEntry, NextDosEntry, IsFileSystem)
- **Default assigns**: C:, S:, L:, LIBS:, DEVS:, T:, ENV:, ENVARC: auto-created on startup

---

## Phase 11: Exec Library Completion

**Goal**: Implement remaining HIGH priority Exec functions for robust multitasking support.

### Step 11.1: List Operations
- [x] **RemTail** - Remove node from list tail

### Step 11.2: Memory Functions
- [x] **TypeOfMem** - Identify memory type (returns MEMF_* flags)
- [x] **CopyMemQuick** - Longword-aligned fast copy

### Step 11.3: Memory Pools
- [x] **CreatePool** / **DeletePool** - Memory pool management
- [x] **AllocPooled** / **FreePooled** - Pool allocation

### Step 11.4: Formatted Output
- [x] **RawDoFmt** - Core formatting engine for Printf, etc.

### Step 11.5: Asynchronous I/O
- [ ] **SendIO** / **WaitIO** / **CheckIO** / **AbortIO** - Async I/O operations

### Step 11.6: Software Interrupts
- [ ] **Cause** - Trigger software interrupt for deferred processing

### Step 11.7: Shared Semaphores
- [ ] **ObtainSemaphoreShared** / **AttemptSemaphoreShared** - Read-heavy synchronization

### Step 11.8: Named Semaphores
- [ ] **AddSemaphore** / **RemSemaphore** / **FindSemaphore** - Named semaphore registry

---

## Phase 12: Utility Library Completion

**Goal**: Implement remaining essential Utility functions.

### Step 12.1: String Functions
- [ ] **Strnicmp** - Length-limited case-insensitive compare

### Step 12.2: Date Functions
- [ ] **Date2Amiga** - ClockData to Amiga seconds
- [ ] **CheckDate** - Validate ClockData structure

### Step 12.3: Tag Item Management
- [ ] **AllocateTagItems** / **FreeTagItems** - Tag array allocation
- [ ] **CloneTagItems** / **RefreshTagItemClones** - Tag duplication
- [ ] **FilterTagItems** / **TagInArray** - Tag filtering

### Step 12.4: Hook Functions
- [ ] **CallHookPkt** - Call hook function with object and message

### Step 12.5: 64-bit Math (Optional)
- [ ] **SMult64** / **UMult64** - 64-bit multiplication

---

## Phase 13: Console Device Enhancement

**Goal**: Provide full ANSI/CSI escape sequence support for terminal applications.

### Step 13.1: Cursor Control
- [ ] **Cursor Positioning** - CSI row;col H, relative movement (A/B/C/D), save/restore

### Step 13.2: Line/Screen Control
- [ ] **Clear Line/Screen** - CSI K and CSI J variants
- [ ] **Insert/Delete** - Characters and lines with scrolling

### Step 13.3: Text Attributes
- [ ] **SGR Attributes** - Bold, underline, reverse, colors (30-47)

### Step 13.4: Window Queries & Raw Mode
- [ ] **Window Bounds** - Report dimensions
- [ ] **SetMode** - Raw/cooked mode switching

---

## Phase 14: Additional Commands

**Goal**: Expand command set to cover common AmigaOS utilities.

### Step 14.1: File Management
- [ ] **AVAIL** - Display memory information
- [ ] **RESIDENT** - Manage resident commands

### Step 14.2: Script Support
- [ ] **ASK** - Interactive yes/no prompt
- [ ] **REQUESTCHOICE** - Text-based choice dialog

### Step 14.3: Shell Enhancements
- [ ] **CD** with no args - Show current directory
- [ ] **WHICH** - Locate command in search path
- [ ] **PATH** - Manage command search path

---

## Phase 15: Quality & Stability Hardening

**Goal**: Ensure production-grade reliability.

### Step 15.1: Memory Debugging
- [ ] Memory tracking, leak detection, double-free detection, buffer overflow guards

### Step 15.2: Stress Testing
- [ ] 24-hour continuous operation, 10,000 file operations, 100 concurrent processes

### Step 15.3: Compatibility Testing
- [ ] Test with common Amiga software, document issues, create compatibility database

### Step 15.4: Performance Optimization
- [ ] Profile critical paths, optimize VFS, memory allocation, emucall overhead

---

## Known Limitations & Future Work

### Shell Features Not Implemented
- I/O redirection (>, >>, <) - commands use internal TO/FROM arguments
- Pipe support (`cmd1 | cmd2`)
- Variable substitution in commands
- SKIP/LAB (goto labels), WHILE loops

### DOS Functions Stubbed
- GetDeviceProc/FreeDeviceProc - handler-based architecture not implemented
- MakeLink - filesystem links not supported

### Notes
- Stack size hardcoded to 4096 in SystemTagList (NP_StackSize tag supported)
- Environment variable inheritance not implemented for child processes
- DosList functions simplified - assigns enumerated via host VFS

---

## Implementation Rules
1. **Clean Room Approach**: AROS/Kickstart source is for architectural reference ONLY.
2. **Host-First**: Prefer host-side (`emucall`) implementations for filesystem tasks.
3. **Amiga Authenticity**: Commands must use AmigaDOS argument syntax (templates, keywords, switches).
4. **Tooling**: Use `ReadArgs()` template parsing in all `C:` commands.
5. **Test Everything**: Every function, every command, every edge case. 100% coverage is mandatory.
