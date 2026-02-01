# lxa Development Guide for Agents

This document provides essential information for AI agents working on the `lxa` codebase.

## 1. Project Overview
`lxa` is an AmigaOS-compatible environment running on Linux. It consists of:
- **`src/lxa`**: The emulator/host process (runs on Linux).
- **`src/rom`**: The replacement Kickstart ROM (compiled for m68k).
- **`sys/`**: AmigaDOS system commands and tools (compiled for m68k).

### Quality & Stability Requirements
**`lxa` is a runtime environment and operating system implementation. This demands exceptional stability and reliability.**

- **Zero Tolerance for Instability**: Memory leaks, crashes, race conditions, and undefined behavior are unacceptable.
- **Test-Driven Development**: Write tests BEFORE implementation whenever possible.
- **100% Code Coverage**: Every line of code must be exercised by tests. No exceptions.
- **Comprehensive Testing**: Unit tests for functions, integration tests for components, stress tests for robustness.
- **Continuous Validation**: All tests must pass before committing. No broken builds.

**Why This Matters:**
- Users depend on `lxa` for running their software - crashes destroy productivity.
- Debugging m68k/host interaction issues is extremely difficult - prevention is critical.
- Edge cases in DOS/Exec can corrupt memory or filesystem state.
- A single memory leak can make long-running sessions unusable.

## 2. Roadmap Driven Development

### Core Principles

#### Roadmap Alignment
The `roadmap.md` at the project root is the "source of truth."
* **Before starting any task:** Consult `roadmap.md` to identify the current Phase and the next logical task.
* **During development:** If implementation details change or new sub-tasks are discovered, update the roadmap immediately to reflect the reality of the architecture.
* **Upon completion:** Mark the task as complete in `roadmap.md`.

#### The "Done" Definition
A task or phase is only considered **Complete** when:
1.  **Functionality:** The feature works as described in the roadmap.
2.  **Test Coverage:** New code achieves **100% test coverage** (or the maximum feasible limit).
3.  **Stability:** The entire codebase compiles without warnings.
4.  **Verification:** All existing and new tests pass successfully.
5.  **Documentation:** The `README.md` and `roadmap.md` are updated to reflect the new state of the project.

### Operational Workflow

#### Phase Start / Task Initiation
- Open and read `roadmap.md`.
- Announce the specific Phase and Task currently being addressed.
- Analyze the requirements and plan the test suite before writing implementation code.

#### Development Loop
- Write tests first (TDD preferred).
- Implement the minimum code necessary to satisfy the roadmap task.
- Ensure no compiler warnings are introduced. If warnings exist, resolve them before proceeding.

#### Post-Implementation & Cleanup
Whenever a task is finished, you **must** perform the following:
1.  **Update `roadmap.md`**:
    - Use `[x]` for completed items.
    - Add any "lessons learned" or newly discovered technical debt as new bullet points in the current or future phases.
2.  **Update `README.md`**:
    - Update usage instructions, API documentation, or project descriptions to match the new features.
3.  **Final Validation**:
    - Run the full test suite.
    - Generate a coverage report to verify the 100% coverage requirement.

### Error Handling
If a task cannot be completed due to unforeseen technical blockers:
1. Update `roadmap.md` with a "Blocker" note in the relevant phase.
2. Propose a pivot or a temporary workaround in the roadmap before stopping.

## 3. Build System

The project uses **CMake** for building. A convenience script `build.sh` handles the full build process.

### Quick Build
```bash
# Full build (recommended)
./build.sh

# Or manually with CMake
mkdir -p build && cd build
cmake ..
make -j$(nproc)
make install
```

### Build Outputs
After building, artifacts are located at:
- `build/host/bin/lxa` - Host emulator executable
- `build/target/rom/lxa.rom` - AmigaOS ROM image
- `build/target/sys/C/` - C: commands (dir, type, delete, makedir)
- `build/target/sys/System/` - System tools (Shell)

### Installation
```bash
make -C build install
```
Installs to `usr/` prefix by default:
- `usr/bin/lxa` - Emulator binary
- `usr/share/lxa/` - ROM, system files, config template

### Running Tests
```bash
make -C tests test
```
Runs all integration tests. Individual tests can be run:
```bash
make -C tests/dos/helloworld run-test
```

**IMPORTANT**: **Always** use a timeout when running tests - OS tests may hang!

### Toolchains
- **Host**: Standard `gcc` for Linux components
- **Amiga**: `m68k-amigaos-gcc` (libnix) for ROM and system tools
- **Paths**: Cross-compiler typically at `/opt/amiga`

### CMake Structure
- `CMakeLists.txt` - Root build configuration
- `cmake/m68k-amigaos.cmake` - Cross-compilation toolchain file
- `host/CMakeLists.txt` - Host emulator build
- `target/rom/CMakeLists.txt` - ROM build
- `target/sys/CMakeLists.txt` - System commands build

## 4. Testing Strategy & Requirements

### Testing Philosophy
**Every feature requires comprehensive test coverage before it can be considered complete.** As a runtime/OS, `lxa` must be rock-solid - a single bug can crash user applications or corrupt filesystem state.

### Test Types

#### Integration Tests (Required for All Features)
Located in `tests/`, these test complete workflows from the m68k perspective.

**Structure:**
```
tests/
  ├── dos/          # DOS library functionality
  ├── exec/         # Exec library functionality  
  ├── shell/        # Shell features
  └── commands/     # Command-line tool testing
```

**Creating Integration Tests:**
1. Create directory: `tests/<category>/<testname>/`
2. Add `Makefile` including `../../Makefile.config`
3. Write `main.c` - m68k code that tests the feature
4. Create `expected.out` - expected output
5. Run with `make run-test` - compiles, runs via lxa, diffs output

**Example:**
```c
// tests/dos/readargs_simple/main.c
#include <dos/rdargs.h>

int main(void) {
    LONG args[2] = {0};
    struct RDArgs *rda = ReadArgs("NAME/A,SIZE/N", args, NULL);
    
    if (rda) {
        Printf("Name: %s\n", (STRPTR)args[0]);
        Printf("Size: %ld\n", args[1]);
        FreeArgs(rda);
        return 0;
    }
    return 20;
}
```

#### Unit Tests (Recommended for Complex Logic)
For host code and ROM functions with complex logic, unit tests verify individual functions in isolation.

**Future Setup:** (Phase 9.1)
- Framework: CMocka or Unity
- Location: `tests/unit/`
- Target: `make test-unit`
- Coverage: Track with gcov/lcov

**When to Write Unit Tests:**
- Complex parsing logic (e.g., ReadArgs template parsing)
- Algorithm implementations (e.g., pattern matching)
- Data structure operations (e.g., list manipulation)
- Memory management functions
- VFS path resolution logic

#### Stress Tests (Required for Core Systems)
Verify stability under heavy load and edge conditions.

**Examples:**
- **Memory stress**: Allocate/free thousands of blocks, verify no leaks
- **Task stress**: Spawn 50+ concurrent tasks, check scheduler
- **Filesystem stress**: Create/delete thousands of files
- **Long-running stability**: 24-hour continuous operation

### Coverage Requirements

**Mandatory Coverage Targets:**
- **Critical Functions**: 100% line and branch coverage
  - All DOS functions (Open, Read, Lock, etc.)
  - All Exec functions (Signal, AllocMem, etc.)
  - ReadArgs and pattern matching
  - VFS layer and emucalls
- **Commands**: 100% of code paths tested
  - All switches and options
  - Error conditions
  - Edge cases (empty input, max size, etc.)
- **Overall Project**: Minimum 95% coverage

**Coverage Verification:**
```bash
# Generate coverage report
make coverage

# View HTML report
firefox coverage/index.html

# Coverage gate - fail build if below threshold
make coverage-check
```

### Test Requirements Checklist

Before marking any task complete, verify:

- [ ] **Functionality Tests** - Happy path works correctly
- [ ] **Error Handling** - All error conditions tested
  - Invalid parameters (NULL pointers, out of range)
  - Resource exhaustion (out of memory, disk full)
  - Permission denied, file not found, etc.
- [ ] **Edge Cases** - Boundary conditions covered
  - Empty input, zero-length files
  - Maximum sizes (long filenames, large files)
  - Special characters, unusual patterns
- [ ] **Break Handling** - Ctrl+C interruption
  - Long-running operations can be interrupted
  - Resources cleaned up properly
  - State left consistent
- [ ] **Memory Safety** - No leaks or corruption
  - All AllocMem/AllocVec paired with Free
  - No buffer overflows
  - Stack usage within limits (see §10)
- [ ] **Concurrency** - Thread-safe if applicable
  - Signal delivery under load
  - Multiple tasks accessing same resources
  - Race condition testing
- [ ] **Documentation** - Test purpose and setup clear
  - Test name describes what it validates
  - Comments explain non-obvious checks
  - Expected.out is correct and complete

### Test-Driven Development Workflow

**Recommended Approach:**

1. **Read Roadmap** - Identify the next task
2. **Plan Tests** - Before writing any code, outline:
   - What needs to be tested?
   - What are the edge cases?
   - What could go wrong?
3. **Write Test Skeleton** - Create test structure
   - Set up test directory
   - Write test harness
   - Define expected.out
4. **Implement Feature** - Write minimum code to pass tests
   - Start simple (happy path)
   - Add error handling
   - Cover edge cases
5. **Run Tests** - Verify all pass
   ```bash
   make -C tests/<category>/<test> run-test
   ```
6. **Measure Coverage** - Check if 100% achieved
7. **Add More Tests** - If coverage gaps exist, add tests first, then fix code
8. **Final Validation** - Full test suite passes
   ```bash
   make -C tests test
   ```

### Common Testing Patterns

#### Testing DOS Functions
```c
// Test Lock/Examine
BPTR lock = Lock("SYS:C", SHARED_LOCK);
if (!lock) {
    Printf("FAIL: Could not lock SYS:C\n");
    return 20;
}

struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
if (Examine(lock, fib)) {
    Printf("OK: %s is directory: %s\n", 
           fib->fib_FileName,
           fib->fib_DirEntryType > 0 ? "YES" : "NO");
} else {
    Printf("FAIL: Examine failed\n");
}

FreeDosObject(DOS_FIB, fib);
UnLock(lock);
```

#### Testing ReadArgs
```c
// Test with various input
Process *me = (Process *)FindTask(NULL);
me->pr_Arguments = "myfile SIZE=1024";

LONG args[2] = {0};
struct RDArgs *rda = ReadArgs("FILE/A,SIZE/K/N", args, NULL);

if (!rda) {
    Printf("FAIL: ReadArgs rejected valid input\n");
    return 20;
}

Printf("File: %s, Size: %ld\n", (STRPTR)args[0], args[1]);
FreeArgs(rda);
```

#### Testing Error Conditions
```c
// Verify proper error handling
BPTR fh = Open("NONEXISTENT:FILE", MODE_OLDFILE);
if (fh) {
    Printf("FAIL: Open succeeded on invalid path\n");
    Close(fh);
    return 20;
}

LONG err = IoErr();
if (err != ERROR_OBJECT_NOT_FOUND && err != ERROR_DEVICE_NOT_MOUNTED) {
    Printf("FAIL: Wrong error code: %ld\n", err);
    return 20;
}

Printf("OK: Correct error handling\n");
```

#### Testing Memory Management
```c
// Verify allocation and cleanup
void *mem1 = AllocMem(1024, MEMF_PUBLIC);
void *mem2 = AllocMem(2048, MEMF_CLEAR);

if (!mem1 || !mem2) {
    Printf("FAIL: Allocation failed\n");
    return 20;
}

// Verify MEMF_CLEAR zeroed memory
UBYTE *p = (UBYTE *)mem2;
for (int i = 0; i < 2048; i++) {
    if (p[i] != 0) {
        Printf("FAIL: MEMF_CLEAR did not zero memory\n");
        return 20;
    }
}

FreeMem(mem1, 1024);
FreeMem(mem2, 2048);
Printf("OK: Memory operations correct\n");
```

### Debugging Failed Tests

When a test fails:

1. **Compare outputs:**
   ```bash
   diff expected.out actual.out
   ```

2. **Run with debug output:**
   ```bash
   ../../build/host/bin/lxa -d -r ../../build/target/rom/lxa.rom ./testprog
   ```

3. **Check for crashes:**
   - Segmentation fault → memory access bug in host code
   - Assertion failure → logic error in ROM code
   - Silent failure → check error codes with IoErr()

4. **Add debug prints:**
   ```c
   Printf("DEBUG: About to call Lock\n");
   BPTR lock = Lock(path, SHARED_LOCK);
   Printf("DEBUG: Lock returned %08lx, IoErr=%ld\n", 
          (ULONG)lock, IoErr());
   ```

5. **Verify test expectations:**
   - Is `expected.out` actually correct?
   - Did requirements change?
   - Is test setup correct (files present, permissions set)?

### Test Maintenance

- **Keep tests passing** - Never commit broken tests
- **Update tests with features** - When behavior changes, update expected.out
- **Delete obsolete tests** - If feature removed, remove test
- **Run full suite regularly** - Catch regressions early
- **Review test failures carefully** - Failed test = bug (usually)

### Running Tests
- **Run All Tests**:
  ```bash
  cd tests && make test
  ```
- **Run Single Test**:
  To run a specific test (e.g., `dos/helloworld`):
  ```bash
  make -C tests/dos/helloworld run-test
  ```
  Or navigate to the directory:
  ```bash
  cd tests/dos/helloworld
  make run-test
  ```

### Current Test Categories
- `tests/dos/` - DOS library tests (helloworld, lock_examine)
- `tests/exec/` - Exec library tests (signal_pingpong)
- `tests/shell/` - Shell functionality tests (alias, controlflow, script)
- `tests/graphics/` - Graphics library tests (see §12 for details)
- `tests/intuition/` - Intuition library tests (see §12 for details)

## 5. Code Style & Conventions

### General
- **Indentation**: 4 spaces.
- **Naming**: `snake_case` for variables and internal functions.
- **Comments**: C-style `/* ... */` preferred, `//` allowed in host code.

### Component-Specific Guidelines

#### ROM Code (`src/rom/`)
- **Brace Style**: **Allman** (braces on new line).
  ```c
  if (condition)
  {
      do_something();
  }
  ```
- **Types**: Use Amiga types (`LONG`, `ULONG`, `BPTR`, `STRPTR`, `BOOL`).
- **Logging**: Use `DPRINTF(LOG_DEBUG, ...)` or `LPRINTF(LOG_ERROR, ...)`.
- **ASM**: Inline assembly `__asm("...")` is common for register arguments.
- **Structure**: Follows Amiga library structure (Jump tables, `__asm` register args).

#### System Commands (`sys/`)
- **Brace Style**: **K&R** (braces on same line).
  ```c
  if (condition) {
      do_something();
  }
  ```
- **Types**: Amiga types (`LONG`, `BPTR`, `STRPTR`).
- **IO**: Use `Printf`, `Read`, `Write` (AmigaDOS API), not `stdio.h` if possible (though `libnix` maps stdio).
- **Entry**: `main(int argc, char **argv)` is supported via `libnix` startup.

#### Host Code (`src/lxa/`)
- **Brace Style**: Mixed, but prefer **Allman** for consistency with core logic or mimic surrounding code.
- **Types**: Standard C types (`uint32_t`, `int`, `char*`) and Amiga types when interacting with memory.
- **Memory**: Access m68k memory via `m68k_read_memory_*` / `m68k_write_memory_*`.
- **Logging**: `DPRINTF` / `LPRINTF`.

## 6. Implementation Rules
1.  **Conventions**: Respect existing naming and formatting in the file you are editing.
2.  **Safety**:
    - Use `DPRINTF` for debugging traces.
    - Check pointers before access (especially `BADDR` conversions).
    - Handle AmigaDOS error codes (e.g., `IoErr()`).
3.  **Proactiveness**:
    - If adding a new DOS packet or call, add the `EMU_CALL` definition in `src/include/emucalls.h`.
    - If implementing a new command, add it to `sys/C` or `sys/System` and update the roadmap.

## 7. Common Patterns
- **BPTR Conversion**: Use `BADDR(bptr)` to convert Amiga BPTR to host pointer, `MKBADDR(ptr)` for reverse.
- **TagItems**: Use `GetTagData` for parsing tag lists in system calls.
- **Libraries**: `DOSBase`, `SysBase` are usually passed in registers (`a6`) for ROM code, or available as globals in user commands.

## 8. Debugging & Logging

### DPRINTF vs LPRINTF
ROM code (`src/rom/`) has two logging macros defined in `src/rom/util.h`:

- **`LPRINTF(level, ...)`**: Always enabled. Use for errors and important info.
- **`DPRINTF(level, ...)`**: Only enabled when `ENABLE_DEBUG` is defined. Use for verbose debug traces.

```c
// util.h
#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARNING 2
#define LOG_ERROR   3

// LPRINTF is always active
#define LPRINTF(lvl, ...) do { if (lvl >= LOG_LEVEL) lprintf(lvl, __VA_ARGS__); } while (0)

// DPRINTF requires ENABLE_DEBUG to be defined
//#define ENABLE_DEBUG  // Uncomment to enable DPRINTF
#ifdef ENABLE_DEBUG
#define DPRINTF(lvl, ...) LPRINTF (lvl, __VA_ARGS__)
#else
#define DPRINTF(lvl, ...) do { } while (0)  // No-op
#endif
```

**When debugging ROM code:**
1. For quick debugging, use `LPRINTF(LOG_INFO, ...)` - always visible
2. For permanent debug traces, use `DPRINTF(LOG_DEBUG, ...)` and uncomment `ENABLE_DEBUG` in `util.h`
3. Run with `-d` flag for additional host-side debug output

## 9. ROM Code Constraints

### No Writable Static Data
ROM code is loaded into read-only memory. **Static variables with initial values or that need to be written will NOT work correctly.**

```c
// BAD - static arrays in ROM code don't work!
static STRPTR my_array[16];  // This is in ROM - can't write to it!
my_array[0] = some_ptr;      // Will silently fail or corrupt memory

// GOOD - dynamically allocate writable storage
STRPTR *my_array = AllocVec(16 * sizeof(STRPTR), MEMF_CLEAR);
if (my_array) {
    my_array[0] = some_ptr;  // Works - allocated in RAM
    // ... use array ...
    FreeVec(my_array);
}
```

### Implications for ROM Functions
- Use `AllocVec`/`AllocMem` for any data structures that need to be modified
- Constants and lookup tables are fine as `static const`
- Function-local variables on the stack are fine (but see stack size limits below)

## 10. Stack Size Considerations

### Process Stack Limits
Processes created via `SystemTagList()` have a default stack size of **4096 bytes** (4KB). This is easily exhausted by large local arrays.

```c
// BAD - 4KB buffer on a 4KB stack = overflow!
static int my_command(void) {
    UBYTE buffer[4096];  // This alone exhausts the stack
    // ... more local variables ...
    // Stack overflow corrupts memory, causing mysterious crashes
}

// GOOD - use smaller buffers or heap allocation
static int my_command(void) {
    UBYTE buffer[512];   // Reasonable stack usage
    // ... or ...
    UBYTE *buffer = AllocVec(4096, MEMF_ANY);
    if (!buffer) return ERROR_NO_FREE_STORE;
    // ... use buffer ...
    FreeVec(buffer);
}
```

### Stack Overflow Symptoms
- Function arguments appear corrupted
- Pointers that were valid become garbage
- Memory reads return wrong data
- Crashes in seemingly unrelated code

### Safe Stack Usage Guidelines
- Keep local arrays under **256 bytes** in commands
- For larger buffers, use `AllocVec()` from the heap
- Consider that function call depth also consumes stack
- If a command needs more stack, increase `NP_StackSize` in `SystemTagList()` call

## 11. Memory Debugging Tips

### Tracing Pointer Issues
When a pointer appears corrupted:
1. Print the pointer value at each stage of passing
2. Check if the value is a valid m68k address (typically 0x20000-0x80000 range)
3. Verify the memory at that address contains expected data
4. Look for stack overflow (large local arrays)
5. Look for ROM static data issues

### Common Address Ranges
- `0x00000-0x00400`: Exception vectors (don't touch)
- `0x00400-0x20000`: System structures (ExecBase, DOSBase, etc.)
- `0x20000-0x80000`: Typical user memory (AllocMem/AllocVec)
- ROM addresses are much higher and read-only

## 12. Graphics and Intuition Testing

Graphics and UI libraries require special testing approaches. See `doc/graphics_testing.md` for the comprehensive testing strategy.

### Key Principles

1. **Headless Testing**: All graphics tests run **without SDL2 display**. The graphics library performs software rendering to BitMaps that can be verified programmatically.

2. **Verify BitMap Contents**: Instead of visual inspection, read pixels back using `ReadPixel()` and verify expected colors/patterns.

3. **Test Drawing Operations**: Verify that drawing primitives produce correct results:
   ```c
   // Example: Test RectFill
   SetAPen(&rp, 1);
   RectFill(&rp, 10, 10, 30, 30);
   
   // Verify filled area
   for (WORD y = 10; y <= 30; y++) {
       for (WORD x = 10; x <= 30; x++) {
           if (ReadPixel(&rp, x, y) != 1) {
               Printf("FAIL: pixel (%d,%d) wrong color\n", x, y);
               return 20;
           }
       }
   }
   ```

### Graphics Test Categories

| Category | Purpose |
|----------|---------|
| `init_rastport` | Verify `InitRastPort()` sets correct defaults |
| `init_bitmap` | Verify `InitBitMap()` calculates BytesPerRow correctly |
| `alloc_raster` | Test `AllocRaster()`/`FreeRaster()` memory handling |
| `pen_state` | Test pen/mode functions (SetAPen, GetAPen, etc.) |
| `pixel_ops` | Test `WritePixel()`/`ReadPixel()` for various colors |
| `line_draw` | Test `Draw()` for horizontal, vertical, diagonal lines |
| `rect_fill` | Test `RectFill()` including COMPLEMENT mode |
| `set_rast` | Test `SetRast()` clears to correct color |

### Creating Graphics Tests

1. **Setup BitMap with allocated planes:**
   ```c
   struct BitMap bm;
   struct RastPort rp;
   InitBitMap(&bm, 2, 64, 64);  // 2 planes = 4 colors
   bm.Planes[0] = AllocRaster(64, 64);
   bm.Planes[1] = AllocRaster(64, 64);
   InitRastPort(&rp);
   rp.BitMap = &bm;
   ```

2. **Perform drawing operations**

3. **Verify results using ReadPixel()**

4. **Cleanup:**
   ```c
   FreeRaster(bm.Planes[0], 64, 64);
   FreeRaster(bm.Planes[1], 64, 64);
   ```

### Intuition Test Categories

| Category | Purpose |
|----------|---------|
| `screen_basic` | Verify `OpenScreen()` creates correct structures |
| `window_basic` | Verify `OpenWindow()` and IDCMP setup |
| `idcmp` | Test IDCMP message delivery and ModifyIDCMP |

### Important Notes

- **No host display required**: Graphics tests do NOT need SDL2 or a display
- **All tests must be deterministic**: Same input → same output
- **Complete quickly**: Each test should run in < 5 seconds
- **Clean up resources**: Always free allocated planes and close screens/windows
