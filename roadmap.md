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

## Phase 8: Comprehensive Test Coverage & Quality Assurance
**Goal**: Achieve robust, production-grade test coverage across all existing system components before expanding functionality. As a runtime/OS implementation, rock-solid stability is paramount.

### Step 8.1: Test Infrastructure Enhancement
- [x] Basic integration test framework (test_runner.sh)
- [x] **Unit Testing Framework** - Integrate Unity or similar for ROM/host C code
  - [x] Setup Unity test framework (tests/unit/unity/)
  - [x] Configure CMake for unit test compilation
  - [x] Create `tests/unit/` directory structure
  - [x] Add `make test-unit` target
- [x] **Coverage Reporting** - Enable code coverage metrics
  - [x] Configure gcov/lcov for coverage analysis (via CMake COVERAGE option)
  - [x] Add `make coverage` target
  - [x] Set up coverage reports (HTML output)
  - [ ] Track coverage metrics per component
- [x] **Test Data Management** - Organize test fixtures
  - [x] Create `tests/fixtures/` for shared test data
  - [x] Standard directory structures for filesystem tests
  - [x] Sample files with various permissions/attributes
- [ ] **Continuous Testing** - Automated test execution
  - [ ] Pre-commit hook for running critical tests
  - [ ] `make test-all` runs both unit and integration tests
  - [ ] Test result reporting and tracking

### Step 8.2: DOS Library Testing
**Critical**: DOS is the foundation - every function must be bulletproof.

#### Filesystem Operations (tests/dos/fs/)
- [x] **Lock/Unlock** - Lock acquisition and release
  - [x] Shared vs exclusive locks (tests/dos/lock_shared)
  - [x] Lock hierarchy (tests/dos/lock_examine)
  - [x] Error cases (non-existent paths)
  - [ ] Lock leak detection
- [x] **Examine/ExNext** - Directory enumeration
  - [x] Directory traversal (tests/dos/lock_examine)
  - [x] File metadata accuracy
  - [x] Large directory handling (tests/dos/dir_enum - 10+ entries)
  - [x] Empty directories, single file (tests/dos/examine_edge)
  - [x] Special characters in filenames (tests/dos/special_chars)
- [x] **Open/Close/Read/Write** - File I/O (tests/dos/fileio)
  - [x] MODE_OLDFILE, MODE_NEWFILE
  - [x] Read/write operations
  - [x] Large file handling (tests/dos/fileio_advanced - 1KB verified)
  - [x] Append mode behavior (tests/dos/fileio_advanced - MODE_READWRITE)
  - [ ] Error handling (disk full, read-only)
- [x] **Seek** - File positioning (tests/dos/fileio)
  - [x] OFFSET_BEGINNING, OFFSET_CURRENT, OFFSET_END
  - [x] Read at EOF
  - [x] Seek beyond EOF (tests/dos/fileio_advanced)
  - [ ] Seek on console handles
- [x] **Delete/Rename** - File manipulation (tests/dos/delete_rename)
  - [x] Delete files
  - [x] Delete non-empty directories (should fail) (tests/dos/delete_rename)
  - [x] Rename across directories (tests/dos/delete_rename)
  - [ ] Rename with open handles
- [x] **CreateDir/ParentDir** - Directory operations (tests/dos/createdir)
  - [x] Nested directory creation (tests/dos/createdir)
  - [ ] Permission inheritance
  - [x] ParentDir traversal (tests/dos/createdir, tests/dos/lock_examine)

#### Pattern Matching (tests/dos/patterns/)
- [x] **ParsePattern/MatchPattern** - Wildcard support (tests/dos/patterns - 54 tests)
  - [x] Simple wildcards: `#?`, `*`, `?`
  - [ ] Character classes: `[abc]`, `[a-z]` (not implemented)
  - [x] Complex patterns: `#?.c`, `test#?`
  - [ ] Case sensitivity (Amiga is case-insensitive) - current impl is case-sensitive
  - [x] Pattern tokenization edge cases

#### Argument Parsing (tests/dos/readargs/, tests/dos/readargs_full/)
- [x] **ReadArgs** - Template parsing and validation (tests/dos/readargs, tests/dos/readargs_full)
  - [x] Simple arguments: `/A` (required)
  - [x] Keyword arguments: `/K` (with space or = syntax)
  - [x] Switch arguments: `/S`
  - [x] Numeric arguments: `/N` (returns pointer to LONG)
  - [ ] Multiple arguments: `/M`, `/M/A`
  - [x] Keywords with values: `KEY=value`
  - [x] Error cases: missing required args (ERROR_REQUIRED_ARG_MISSING)
  - [x] Quoted strings with spaces
  - [ ] Empty input handling
  - [ ] Template parsing edge cases
- [x] **FreeArgs** - Memory cleanup validation (properly frees DAList allocations)

#### Process Management (tests/dos/process/)
- [ ] **SystemTagList** - Process spawning
  - [ ] Spawn with various stack sizes
  - [ ] Environment variable inheritance
  - [ ] Input/output redirection
  - [ ] Exit code propagation
  - [ ] Background process cleanup
- [ ] **Execute** - Script execution
  - [ ] Multi-line scripts
  - [ ] Error handling in scripts
  - [ ] Break handling (Ctrl+C)

### Step 8.3: Exec Library Testing
**Critical**: Multitasking foundation must be rock-solid.

#### Task Management (tests/exec/tasks/)
- [x] Signal ping-pong (basic signal test)
- [ ] **CreateTask/AddTask** - Task creation
  - Task with various stack sizes
  - Task priority levels
  - Task naming and lookup
- [ ] **FindTask** - Task lookup by name/NULL
- [ ] **SetTaskPri** - Dynamic priority changes
- [ ] **Wait/Signal** - Signal semantics
  - Multiple signals simultaneously
  - Signal mask handling
  - Wait timeout behavior

#### Memory Management (tests/exec/memory/)
- [ ] **AllocMem/FreeMem** - Basic allocation
  - Various memory types (MEMF_CHIP, MEMF_FAST, MEMF_PUBLIC)
  - MEMF_CLEAR verification
  - Boundary conditions (size 0, huge allocations)
  - Memory leak detection
- [ ] **AllocVec/FreeVec** - Vector allocation
  - Size tracking verification
  - Alignment requirements
- [ ] **Memory Fragmentation** - Stress testing
  - Allocate/free in various patterns
  - Check for fragmentation issues

#### Synchronization (tests/exec/sync/)
- [ ] **Semaphores** - Mutual exclusion
  - ObtainSemaphore/ReleaseSemaphore
  - Shared vs exclusive access
  - Nested semaphore acquisition
  - Deadlock scenarios (should not hang)
- [ ] **Message Ports** - Inter-task communication
  - CreateMsgPort/DeleteMsgPort
  - PutMsg/GetMsg/ReplyMsg
  - Message queuing order
  - Port naming and lookup

#### Lists (tests/exec/lists/)
- [ ] **List Manipulation** - Core data structures
  - AddHead/AddTail/Remove
  - Enqueue (priority ordering)
  - FindName/RemHead/RemTail
  - Empty list handling

### Step 8.4: Command Testing (tests/commands/)
**Every command must have comprehensive tests.**

#### File Commands
- [ ] **DIR** - Directory listing
  - Default listing (current directory)
  - Pattern matching: `dir #?.c`, `dir sys:c/#?`
  - Recursive listing (ALL switch)
  - DIRS/FILES filtering
  - Interactive mode (INTER)
  - Empty directories
  - Large directories (>100 files)
  - Ctrl+C handling
- [ ] **DELETE** - File deletion
  - Single file deletion
  - Multiple files: `delete #?.bak`
  - Recursive deletion (ALL)
  - Protected files (with/without FORCE)
  - QUIET mode
  - Ctrl+C during deletion
  - Error cases (read-only, in-use)
- [ ] **TYPE** - File display
  - Text file output
  - Multiple files: `type file1 file2`
  - HEX mode for binary files
  - NUMBER mode (line numbers)
  - TO redirection
  - Large files (>100KB)
  - Binary files with special characters
- [ ] **MAKEDIR** - Directory creation
  - Single directory creation
  - Multiple directories: `makedir dir1 dir2 dir3`
  - Nested paths: `makedir a/b/c` (should create all)
  - Error cases (already exists, invalid chars)

#### System Commands
- [ ] **ASSIGN** - Drive assignments
  - Simple assign: `assign FOO: dir`
  - Multi-directory paths
  - Deferred assigns
  - Remove assignments
  - List all assignments
  - Circular assignment detection
- [ ] **STATUS** - Process information
  - List all processes
  - Full detail mode
  - TCB address display
  - Filter by command name
  - Priority and state display
- [ ] **BREAK** - Process signaling
  - Send break to specific CLI
  - Break all processes
  - Break non-existent process (error)
- [ ] **RUN** - Background execution
  - Simple command execution
  - Command with arguments
  - Multiple simultaneous background tasks

#### Shell Features (tests/shell/)
- [x] Control flow (IF/ELSE/ENDIF)
- [x] Alias support
- [x] Script execution
- [ ] **Variables** - Shell variable handling
  - SET/UNSET local variables
  - Variable substitution in commands
  - Numeric operations (EVAL)
- [ ] **Redirection** - I/O redirection
  - Output redirection: `cmd > file`
  - Input redirection: `cmd < file`
  - Append mode: `cmd >> file`
  - Pipe support: `cmd1 | cmd2`
- [ ] **Script Features** - Advanced scripting
  - SKIP/LAB (goto labels)
  - WHILE loops
  - Command sequencing (`;`)
  - Comment handling (`;` at line start)
  - Error propagation

### Step 8.5: Host/Emulation Testing (tests/host/)
**Verify the emulation layer itself.**

- [ ] **Memory Access** - m68k memory operations
  - Read/write byte/word/long
  - Alignment handling
  - ROM vs RAM access enforcement
  - Out-of-bounds detection
- [ ] **VFS Layer** - Virtual filesystem
  - Case-insensitive path resolution
  - Drive mapping (SYS:, HOME:, CWD:)
  - Path normalization
  - Assign resolution
  - Invalid path handling
- [ ] **Emucall Interface** - Host/guest bridge
  - All emucall handlers tested
  - Parameter passing correctness
  - Return value handling
  - Error code propagation
- [ ] **Configuration** - config.ini parsing
  - Drive path configuration
  - ROM path detection
  - Invalid config handling
  - Default value fallbacks

### Step 8.6: Stress & Regression Testing

- [ ] **Memory Stress** - Allocation patterns
  - Allocate/free 1000s of blocks
  - Verify no memory leaks
  - Check fragmentation limits
- [ ] **Task Stress** - Many concurrent tasks
  - Spawn 50+ tasks simultaneously
  - Verify scheduler fairness
  - Check signal delivery under load
- [ ] **Filesystem Stress** - High I/O load
  - Create/delete 1000s of files
  - Read/write large files concurrently
  - Directory traversal stress
- [ ] **Regression Suite** - Never break what works
  - All previous tests must pass
  - Automated regression detection
  - Bisecting tool for finding breaking changes

### Step 8.7: Documentation & Test Guidelines

- [ ] **Testing Documentation** - Test writing guide
  - Document test framework usage
  - Examples of unit vs integration tests
  - How to add new tests
  - Coverage target enforcement (100%)
- [ ] **Test Naming Conventions** - Consistency
  - `test_<component>_<function>_<scenario>`
  - Descriptive test names
  - Organized directory structure
- [ ] **CI/CD Integration** - Automation
  - GitHub Actions or similar
  - Run tests on every commit
  - Block merges with failing tests
  - Coverage regression detection

---

## Phase 9: New Commands & Advanced Utilities
**Goal**: Reach Milestone 1.0 with a polished, well-tested toolset. All new features must have tests written first (TDD).

### Step 9.1: File Manipulation Commands
- [x] **COPY** - Copy files/directories with recursive support
  - Template: `FROM/M/A,TO/A,ALL/S,CLONE/S,DATES/S,NOPRO/S,COM/S,QUIET/S`
  - **Tests required**: Single file, multiple files, directories, ALL mode, error cases
- [x] **RENAME** - Rename/move files
  - Template: `FROM/A,TO/A,QUIET/S`
  - **Tests required**: Same directory, across directories, open files, errors
- [x] **JOIN** - Concatenate files
  - Template: `FROM/A/M,AS=TO/A`
  - **Tests required**: Multiple files, large files, binary files

### Step 9.2: File Information Commands
- [ ] **PROTECT** - Change file protection bits
  - Template: `FILE/A,FLAGS/K,ADD/S,SUB/S,ALL/S`
  - **Tests required**: Set/clear bits, recursive, validation
- [ ] **FILENOTE** - Set file comment
  - Template: `FILE/A,COMMENT,ALL/S`
  - **Tests required**: Set/clear comments, long comments, special chars
- [ ] **LIST** - Detailed directory listing
  - Template: `DIR/M,P=PAT/K,KEYS/S,DATES/S,NODATES/S,TO/K,SUB/S,SINCE/K,UPTO/K,QUICK/S,BLOCK/S,NOHEAD/S,FILES/S,DIRS/S,LFORMAT/K,ALL/S`
  - **Tests required**: All switches, patterns, output redirection

### Step 9.3: Search & Text Commands
- [ ] **SEARCH** - Search for text in files
  - Template: `FROM/M/A,SEARCH/A,ALL/S,NONUM/S,QUIET/S,QUICK/S,FILE/S,PATTERN/S`
  - **Tests required**: Text search, patterns, multiple files, binary files
- [ ] **SORT** - Sort lines of text
  - Template: `FROM/A,TO/A,COLSTART/K/N,CASE/S,NUMERIC/S`
  - **Tests required**: Various sort modes, large files, edge cases
- [ ] **EVAL** - Evaluate arithmetic expressions
  - Template: `VALUE1/A,OP,VALUE2/M,TO/K,LFORMAT/K`
  - **Tests required**: All operations, overflow, negative numbers

### Step 9.4: Environment & Variables
- [ ] **SET** / **SETENV** - Manage local and global variables
  - **Tests required**: Set/get, persistence, scope
- [ ] **GETENV** - Retrieve environment variables
  - **Tests required**: Existing/non-existing vars
- [ ] **UNSET** / **UNSETENV** - Remove variables
  - **Tests required**: Remove existing, remove non-existing

### Step 9.5: System Commands
- [ ] **VERSION** - Display version information
  - **Tests required**: Output format, component versions
- [ ] **WAIT** - Wait for time/event
  - **Tests required**: Time delays, break handling
- [ ] **INFO** - Display disk information
  - **Tests required**: Various drives, unmounted drives
- [ ] **DATE** - Display/set system date
  - **Tests required**: Display, set, validation
- [ ] **MAKELINK** - Create hard/soft links
  - **Tests required**: Hard/soft links, errors

### Step 9.6: Final Polish
- [ ] Documentation update: `README.md` with all commands
- [ ] AROS reference review for edge cases
- [ ] 100% Test coverage verification for all new commands
- [ ] Performance profiling and optimization
- [ ] Milestone 1.0 release preparation

---

## Implementation Rules
1. **Clean Room Approach**: AROS/Kickstart source is for architectural reference ONLY.
2. **Host-First**: Prefer host-side (`emucall`) implementations for filesystem tasks.
3. **Amiga Authenticity**: Commands must use AmigaDOS argument syntax (templates, keywords, switches).
4. **Tooling**: Use `ReadArgs()` template parsing in all `C:` commands.
5. **Test Everything**: Every function, every command, every edge case. 100% coverage is mandatory.
