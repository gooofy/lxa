---
name: lxa-testing
description: General TDD workflow, integration/unit testing strategies, and test requirements checklist.
---

# lxa Testing Skill

This skill details the testing requirements and strategies for the lxa project.

## 1. Philosophy
- **Rock-solid stability**: No crashes allowed.
- **TDD**: Write tests first.
- **100% Coverage**: Mandatory.

## 2. Test Types

### 2.1 Host-Side Test Drivers (PREFERRED for UI tests)
**Location**: `tests/drivers/`

Host-side test drivers use the `liblxa` API to control the emulator from C code running on the host. This is the **preferred approach for all interactive UI testing**.

**Key Features**:
- Full control over timing and event injection
- Can inject mouse clicks, key presses, and menu selections
- Can query window state and capture program output
- Runs headlessly without SDL2 display

**Example**: `tests/drivers/simplegad_test.c`
```c
#include "lxa_api.h"

int main(void) {
    lxa_config_t config = {
        .rom_path = "path/to/lxa.rom",
        .sys_drive = "path/to/samples",
        .headless = true,
    };
    
    lxa_init(&config);
    lxa_load_program("SYS:MyProgram", "");
    lxa_wait_windows(1, 5000);  // Wait for window to open
    
    // CRITICAL: Run cycles to let task reach WaitPort()
    for (int i = 0; i < 200; i++) {
        lxa_run_cycles(10000);
    }
    
    // Inject mouse click
    lxa_inject_mouse_click(100, 50, LXA_MOUSE_LEFT);
    
    // Run cycles with VBlanks to process event
    for (int i = 0; i < 20; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(50000);
    }
    
    // Run additional cycles to let task produce output
    for (int i = 0; i < 100; i++) {
        lxa_run_cycles(10000);
    }
    
    // Check output
    char output[4096];
    lxa_get_output(output, sizeof(output));
    assert(strstr(output, "Expected message") != NULL);
    
    lxa_shutdown();
    return 0;
}
```

**Critical Timing Notes**:
1. After initialization, run many cycles WITHOUT VBlanks to let task reach `WaitPort()`
2. After injecting events, trigger VBlanks to process input through Intuition
3. After VBlanks, run additional cycles WITHOUT VBlanks to let task print output
4. Only then check the output buffer

**Build**: Add to `tests/drivers/CMakeLists.txt`, link against `liblxa`

### 2.2 In-ROM Self-Testing (Legacy)
**Location**: `samples/` with `test_inject.h`

Some samples use `test_inject.h` to perform self-testing within the m68k code. This approach is being phased out in favor of host-side drivers.

**Still using test_inject.h**: SimpleMenu, UpdateStrGad, SimpleGTGadget

### 2.3 Integration Tests
**Location**: `tests/dos/`, `tests/exec/`, etc.

- m68k code testing complete workflows.
- Run with `make run-test` (compiles, runs, diffs `expected.out`).
- Good for non-interactive functionality testing.

### 2.4 Unit Tests
**Location**: `tests/unit/`

- For complex logic (parsing, algorithms).
- Host-side C code testing specific functions.

### 2.5 Stress Tests
- For stability (memory leaks, concurrency).

## 3. Migration Plan: Host-Side Drivers

**Goal**: All interactive UI tests should use host-side drivers.

**Priority Migration**:
1. Existing in-ROM `test_inject.h` samples → host-side drivers
2. Deep dive app tests (KickPascal, Devpac, etc.) → host-side drivers
3. Any new UI tests → MUST use host-side drivers

**Why Host-Side Drivers are Better**:
- Full control from host code (no m68k limitations)
- Better debugging (can add breakpoints in host code)
- More flexible timing control
- Can query any internal state via liblxa API
- Tests are portable and don't require m68k compilation

## 4. Checklist
Before completing a task:
- [ ] Functionality (Happy Path)
- [ ] Error Handling (NULLs, resources, permissions)
- [ ] Edge Cases (Boundaries, empty/max inputs)
- [ ] Break Handling (Ctrl+C)
- [ ] Memory Safety (Alloc/Free paired)
- [ ] Concurrency
- [ ] Documentation (Clear test purpose)

## 5. TDD Workflow
1. Read Roadmap.
2. Plan Tests (Edge cases, errors).
3. Write Test Skeleton (for UI: host-side driver; for logic: integration test).
4. Implement Feature.
5. Run Tests.
6. Measure Coverage.
7. Iterate until 100% coverage and passing.

## 6. liblxa API Reference

**Initialization**:
- `lxa_init(config)` - Initialize emulator
- `lxa_load_program(path, args)` - Load Amiga program
- `lxa_shutdown()` - Clean up

**Execution**:
- `lxa_run_cycles(n)` - Run n CPU cycles
- `lxa_trigger_vblank()` - Set VBlank pending flag
- `lxa_wait_exit(timeout_ms)` - Run until program exits

**Event Injection**:
- `lxa_inject_mouse_click(x, y, button)` - Click at position
- `lxa_inject_mouse(x, y, buttons, type)` - Raw mouse event
- `lxa_inject_key(rawkey, qualifier, down)` - Key event
- `lxa_inject_keypress(rawkey, qualifier)` - Key down+up
- `lxa_inject_string(str)` - Type string

**State Query**:
- `lxa_get_window_count()` - Number of open windows
- `lxa_get_window_info(index, info)` - Window position/size
- `lxa_wait_windows(count, timeout_ms)` - Wait for windows

**Output Capture**:
- `lxa_get_output(buffer, size)` - Get program stdout
- `lxa_clear_output()` - Clear output buffer

## 7. Common Patterns
- **DOS**: `Lock`, `Examine`, `Open`, `IoErr`.
- **ReadArgs**: Verify parsing with `RDArgs`.
- **Memory**: `AllocMem` (check NULL), `MEMF_CLEAR` (check zeros), `FreeMem`.

## 8. Debugging Failures
1. **Diff**: `diff expected.out actual.out`
2. **Debug Log**: Run with `-d`.
3. **Crashes**: Check segfaults (host) or assertions (ROM).
4. **Print**: Add `Printf` ("DEBUG: ...").
5. **Host Driver Debug**: Add printf in driver, check lxa_get_output()

## 9. Running Tests
- All samples: `cd tests/samples && make run-test`
- Single sample: `make -C tests/samples/SimpleTask run-test`
- Host drivers: `./build/tests/drivers/simplegad_test`
