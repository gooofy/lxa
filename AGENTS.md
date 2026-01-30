# lxa Development Guide for Agents

This document provides essential information for AI agents working on the `lxa` codebase.

## 1. Project Overview
`lxa` is an AmigaOS-compatible environment running on Linux. It consists of:
- **`src/lxa`**: The emulator/host process (runs on Linux).
- **`src/rom`**: The replacement Kickstart ROM (compiled for m68k).
- **`sys/`**: AmigaDOS system commands and tools (compiled for m68k).

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

## 4. Testing
Integration tests are located in `tests/`.

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

### Creating Tests
- Create a new directory in `tests/<category>/<testname>`.
- Add a `Makefile` that includes `../../Makefile.config`.
- Create a `main.c` (m68k source) that performs the test.
- Create `expected.out` with the expected output.
- `make run-test` compiles the test, runs it via `lxa`, and diffs the output.

### Current Test Categories
- `tests/dos/` - DOS library tests (helloworld, lock_examine)
- `tests/exec/` - Exec library tests (signal_pingpong)
- `tests/shell/` - Shell functionality tests (alias, controlflow, script)

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
