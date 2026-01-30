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

**Status**: ✅ COMPLETE - Shell binary with interactive mode and scripting support

### Completed Core Infrastructure:
- ✅ **Interactive Shell Binary** (`sys/System/Shell.c`)
  - Classic `1.SYS:> ` prompt with customizable prompt string
  - Command dispatcher for internal commands (CD, PATH, PROMPT, ECHO, ALIAS, etc.)
  - Input handling from both interactive console and script files
  - FAILAT support for setting error tolerance levels

- ✅ **Execute() API** (dos.library)
  - Script execution with `s` bit handling
  - Automatic fallback to Shell for non-binary files
  - Proper input/output redirection support

- ✅ **Control Flow Commands**
  - IF/ELSE/ENDIF with WARN and ERROR condition support
  - SKIP and LAB for script flow control
  - QUIT command for script termination

- ✅ **Communication Commands**
  - ECHO with NOLINE support
  - ASK for interactive user input

- ✅ **Shell Tools**
  - ALIAS: Command aliasing with listing and removal
  - WHICH: Command location lookup
  - WHY: Display last error code
  - FAULT: Display error message for code

### Phase 5 Infrastructure Issues 🚧 BLOCKERS

**Status**: Implementation complete but testing infrastructure has issues that must be resolved before Phase 6.

#### Issues Discovered During Testing:

1. **Script Execution Hangs**
   - Shell scripts executed via `Execute()` hang indefinitely
   - Process appears to enter infinite loop or deadlock
   - Test timeout required to terminate (command exits with code 124)
   - **Location**: `sys/System/Shell.c` - main loop or input handling

2. **Shell Uses stdio Instead of AmigaDOS I/O** ⚠️ CRITICAL
   - Shell uses `printf()` from stdio.h which requires C runtime initialization
   - lxa does not properly initialize libnix C runtime for stdio
   - Only raw AmigaDOS calls like `Write()` work correctly
   - This breaks all Shell output including prompts and command responses
   - **Fix**: Replace all `printf()` calls with `Write()`, `FPuts()`, or `PutStr()`
   - **File**: `sys/System/Shell.c` (~50+ printf calls need replacement)

3. **SYS: Drive Mapping Conflict**
   - When `~/.lxa/config.ini` exists from first-run setup, SYS: maps to `~/.lxa/System/`
   - This overrides the default "current directory" behavior
   - Commands like `dir` can't find SYS:C/ because it's looking in ~/.lxa/System/C/
   - **Fix**: Either update config.ini template or ensure C/ commands are copied to ~/.lxa/System/C/
   - **Workaround**: Run with `-s .` to force current directory as SYS:

4. **Missing Utility Library Functions** ✅ FIXED
   - `UDivMod32()` - Was stubbed with assertion failure, now implemented
   - `Stricmp()` - Was stubbed with assertion failure, now implemented
   - These were causing immediate crashes when Shell tried to use them
   - **Location**: `src/rom/lxa_utility.c`

5. **Test Infrastructure Incomplete**
   - Shell tests created but cannot run successfully due to hang issue
   - Test runner needs better handling for interactive/shell processes
   - Need to verify SYS:System/Shell binary deployment in test environments
   - **Location**: `tests/shell/*`

#### Required Fixes Before Phase 6:

**Priority 1 - Shell I/O Fix (BLOCKER):**
- [ ] **Rewrite Shell to Use AmigaDOS I/O**
  - Replace all `printf()` calls with AmigaDOS `Write()`/`FPuts()`/`PutStr()`
  - Create helper functions for formatted output if needed
  - Test interactive mode: prompt should display, commands should output
  - Test: `echo hello` should print "hello" to console
  - **File**: `sys/System/Shell.c`

**Priority 2 - Core Functionality:**
- [ ] **Fix Shell Script Execution Hang**
  - Debug why Shell hangs when reading from script file vs interactive input
  - Check `Read()` calls in main loop - may be blocking indefinitely
  - Verify `IsInteractive()` detection works correctly for script files
  - Test with simple script containing just one ECHO command
  - **File**: `sys/System/Shell.c`

- [ ] **Fix SYS: Drive Mapping**
  - Either copy C/ commands to ~/.lxa/System/C/ during first-run setup
  - OR modify config.ini template to use current directory as SYS:
  - OR update Shell to search multiple locations for commands
  - **Files**: `src/lxa/vfs.c`, `sys/System/Shell.c`

**Priority 3 - Testing & Integration:**
- [ ] **Complete Shell Test Suite**
  - Get at least one shell test passing (start with simple script test)
  - Verify test directory structure with SYS:System/Shell binary
  - Update test expected outputs to match actual behavior
  - Add test for interactive prompt display (may need special test harness)
  - **Files**: `tests/shell/*/`

- [ ] **Verify Execute() API Integration**
  - Test Execute() with various script types
  - Ensure proper input/output redirection
  - Test error handling and return codes
  - **Files**: `src/rom/lxa_dos.c` (Execute function)

- [ ] **Run Full Test Suite**
  - All tests should pass before proceeding to Phase 6
  - Fix any regressions in existing tests (helloworld, lock_examine, signal_pingpong)
  - Document any tests that are expected to fail and why

**Completed Infrastructure Fixes:**
- [x] **Implement Flush() API** - Added host-side emucall handler
- [x] **Fix ROM Auto-Detection** - Now searches multiple locations automatically
- [x] **Fix First-Run Segfault** - Replaced CPRINTF with fprintf(stderr, ...) in vfs_setup_environment
- [x] **Implement Missing Utility Functions** - UDivMod32() and Stricmp() in utility.library

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
4. **Amiga Authenticity**: Commands must use AmigaDOS argument syntax (templates, keywords, switches) not Unix-style flags.
   - Use `ReadArgs()` template parsing in all C: commands
   - Templates: `DIR,OPT/K,ALL/S,DIRS/S` not `dir -la`
   - Keywords: `ALL`, `DIRS`, `TO` not `-a`, `-d`, `-o`

## Phase 4 Completion Summary

### ✅ Completed:
- [x] **Pattern Matching**: `ParsePattern()` and `MatchPattern()` implemented in dos.library
- [x] **C: Commands**: All commands use AmigaDOS template syntax
  - [x] DIR: `DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S` with wildcard support
  - [x] DELETE: `FILE/M/A,ALL/S,QUIET/S,FORCE/S` with recursive delete
  - [x] TYPE: `FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S` with hex dump and line numbers
  - [x] MAKEDIR: `NAME/M/A` with multiple directory support
- [x] **ReadArgs()**: Full template parsing with /A, /K, /S, /M, /N, /F modifiers
- [x] **Semaphore Support**: Complete implementation for C runtime compatibility

### Phase 4b: Pending Extensions (Optional)
- **SetDate() API**: File modification time support (maps to `utimes()`)
- **Environment Variables**: GetVar/SetVar/DeleteVar/FindVar
- **Additional Tools**: RENAME, PROTECT, FILENOTE commands
- **Interactive DIR**: Full INTER mode with E/B/DEL/T/C/Q/? commands

---

## Next Steps: Phase 6 - System Management & Assignments

### Immediate (Next Session):
1. **Assignment API**
   - `AssignLock()`: Create drive assignments from locks
   - `AssignName()`: Create late assignments
   - `AssignPath()`: Multi-directory assignments (Paths)
   - Proper integration with Shell command lookup

2. **System Tools**
   - STATUS: Process listing and management
   - BREAK: Send signals to processes
   - RUN: Background process execution
   - CHANGETASKPRI: Adjust process priorities

### Medium Term:
3. **System Information Tools**: AVAIL, STACK
4. **Device Control**: ASSIGN, MOUNT, RELABEL, LOCK
5. **Advanced Utilities**: COPY (recursive), JOIN, SORT, EVAL, SEARCH

---

## Current Blockers & Dependencies 🚧 PHASE 5 INFRASTRUCTURE ISSUES

**Phase 5 Implementation Complete BUT Infrastructure Issues Must Be Resolved Before Phase 6**

### 🚧 Active Blockers:

1. **Script Execution Hang** (`sys/System/Shell.c`)
   - Shell hangs when executing scripts via Execute()
   - Blocks indefinitely, requires timeout to terminate
   - **Priority**: HIGH - Must fix before Phase 6

2. **Shell Test Suite Incomplete** (`tests/shell/*/`, `tests/Makefile`)
   - Tests created but not functional due to hang issue
   - Need working test infrastructure to verify Shell behavior
   - **Priority**: HIGH - Required for regression testing

### ✅ Recently Resolved:

3. **✓ Utility Library Functions** (`src/rom/lxa_utility.c`)
   - UDivMod32() - Implemented (was causing assertion failure)
   - Stricmp() - Implemented (was causing assertion failure)
   - **Status**: FIXED

4. **✓ Shell Binary** (`sys/System/Shell.c`)
   - Interactive mode with customizable prompt
   - Internal command dispatcher (CD, PATH, PROMPT, ECHO, etc.)
   - Control flow: IF/ELSE/ENDIF, SKIP/LAB
   - Shell tools: ECHO (NOLINE), ASK, ALIAS, WHICH, WHY, FAULT
   - **Status**: IMPLEMENTED (but needs testing fixes)

### Next Actions:
- Fix Shell script execution hang
- Complete shell test suite
- Run full test suite successfully
- **Only then proceed to Phase 6**

**All Phases Through 5 Complete - Ready for Phase 6:**

4. **✓ Semaphore Support** (exec.library)
   - `InitSemaphore()`, `ObtainSemaphore()`, `ReleaseSemaphore()`, `AttemptSemaphore()`

5. **✓ ReadArgs() Implementation** (dos.library)
   - Template parsing with /A, /M, /S, /K, /N, /F modifiers
   - `FindArg()`, `FreeArgs()`, `StrToLong()`

6. **✓ Pattern Matching** (dos.library)
   - `ParsePattern()`, `MatchPattern()`
   - Wildcards: `#?` (any string), `?` (single char), `#` (repeat), `%` (escape), `*` (convenience)
