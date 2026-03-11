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
- **Unified Infrastructure**: All tests run through Google Test.
- **Third-party scope**: Do not add ROM-coverage tests for third-party libraries; only exercise them when real copies are supplied on disk.

## 2. Test Types

### 2.1 Google Test Host-Side Drivers (MANDATORY for UI tests)
**Location**: `tests/drivers/`

Host-side test drivers use the `liblxa` API and Google Test framework. This is the **standard approach for all interactive UI testing**.

**Key Features**:
- Full control over timing and event injection via `LxaUITest` base class
- Can inject mouse clicks, key presses, and menu selections
- Can query window state and capture program output
- Runs headlessly without SDL2 display
- Standard GTest assertions and reporting

**Example**: `tests/drivers/simplegad_gtest.cpp`
```cpp
#include "lxa_test.h"

using namespace lxa::testing;

class SimpleGadgetTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        ASSERT_EQ(lxa_load_program("SYS:SimpleGad", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        // Let task reach event loop
        RunCyclesWithVBlank(20);
    }
};

TEST_F(SimpleGadgetTest, ClickButton) {
    ClearOutput();
    Click(window_info.x + 50, window_info.y + 40);
    RunCyclesWithVBlank(20);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("GADGETUP"), std::string::npos);
}
```

### 2.2 Integration Tests
**Location**: `tests/dos/`, `tests/exec/`, etc.

- m68k code testing complete workflows.
- Wrapped in GTest drivers (e.g., `exec_gtest.cpp`, `dos_gtest.cpp`).
- Good for non-interactive functionality testing.

### 2.3 Unit Tests
**Location**: `tests/unit/`

- For complex logic (parsing, algorithms).
- Host-side C code using Google Test.

## 3. Checklist
Before completing a task:
- [ ] Functionality (Happy Path)
- [ ] Error Handling (NULLs, resources, permissions)
- [ ] Edge Cases (Boundaries, empty/max inputs)
- [ ] Break Handling (Ctrl+C)
- [ ] Memory Safety (Alloc/Free paired)
- [ ] Concurrency
- [ ] Documentation (Clear test purpose)

## 4. TDD Workflow
1. Read Roadmap.
2. Plan Tests (Edge cases, errors).
3. Write GTest Driver.
4. Implement Feature.
5. Run Tests via `ctest` or `make lxa_tests`.
6. Measure Coverage.
7. Iterate until 100% coverage and passing.

## 5. Running Tests

### 5.1 Build First
```bash
./build.sh          # From project root (/home/guenter/projects/amiga/lxa/src/lxa)
```

### 5.2 Full Test Suite
```bash
# ALWAYS use -j16 for parallel execution
cd build && ctest --output-on-failure -j16

# Equivalent from project root:
ctest --test-dir build --output-on-failure -j16
```

### 5.3 Parallelism Guidelines

| Flag   | Wall Time | When to Use                                    |
|:------:|:---------:|:-----------------------------------------------|
| `-j1`  | Slowest   | Only for debugging unusual resource conflicts  |
| `-j4`  | Good      | Acceptable local parallelism                   |
| `-j16`  | Best default | Project standard for routine runs          |
| `-j16` | Similar to -j16 | Usually no meaningful wall-time gain     |
| `-j38` | Similar to -j16 | Safe, but typically unnecessary           |

**Why `-j16` is optimal**: the formerly oversized interactive suites are now
split into shards, which keeps the full-suite wall time around ~95 seconds on
this machine class. Higher parallelism remains safe, but usually does not
improve the end-to-end runtime enough to justify using it as the default.

**Tests are fully isolated**: Each test runs its own emulator instance with
independent memory. No shared files, ports, or state. Any parallelism level
is safe.

### 5.4 Running Specific Tests
```bash
# By test name (via ctest):
ctest --test-dir build --output-on-failure -R shell_gtest

# By GTest filter (direct binary execution):
./build/tests/drivers/shell_gtest --gtest_filter="ShellTest.Variables"

# Multiple filters:
./build/tests/drivers/dos_gtest --gtest_filter="DosTest.Lock*"

# List all tests in a binary:
./build/tests/drivers/exec_gtest --gtest_list_tests
```

### 5.5 Reliability Testing
When debugging intermittent failures, use a loop:
```bash
# Run a specific test N times with timeout
for i in $(seq 1 50); do
    timeout 30 ./build/tests/drivers/shell_gtest \
        --gtest_filter="ShellTest.Variables" 2>&1 | tail -1
done

# Full suite reliability (N consecutive runs):
for run in $(seq 1 5); do
    echo "=== Run $run ==="
    ctest --test-dir build --output-on-failure -j16 2>&1 | grep "tests passed"
done
```

### 5.6 Test Timeouts
Tests with explicit CTest timeouts (in `tests/drivers/CMakeLists.txt`):

| Test                         | Timeout | Typical Use |
|:-----------------------------|:-------:|:------------|
| `simplegad_behavior_gtest`   | 60s     | behavior shard |
| `simplegad_pixels_gtest`     | 90s     | pixel shard |
| `easyrequest_gtest`          | 120s    | requester flow |
| `rgbboxes_gtest`             | 25s     | graphics regression |
| `cluster2_navigation_gtest`  | 60s     | interactive app shard |

All other tests use CTest's default timeout (1500s). If adding a new test
that takes >60 seconds, add an explicit `TIMEOUT` property in CMakeLists.txt.

## 6. liblxa API Reference (C++ via lxa_test.h)

**Execution**:
- `RunProgram(path, args)` - Load and run program until exit
- `RunCycles(n)` - Run n CPU cycles
- `RunCyclesWithVBlank(iterations, cycles_per_iteration)` - Run cycles with VBlank interrupts
- `WaitForWindows(count, timeout_ms)` - Wait for windows to open
- `GetWindowInfo(index, info)` - Get window position/size

**Event Injection**:
- `Click(x, y, button)` - Click at position
- `PressKey(rawkey, qualifier)` - Key press
- `TypeString(str)` - Type string

**Output Capture**:
- `GetOutput()` - Get program output as string
- `ClearOutput()` - Clear output buffer

## 7. Debugging Failures
1. **GTest Output**: Check failure messages and stack traces.
2. **Debug Log**: Run with `-v` (verbose).
3. **Crashes**: Check host-side core dumps or ROM assertions.
4. **Screenshots**: Some drivers capture screenshots on failure.
5. **Intermittent hangs**: See `doc/test-reliability-report.md` for methodology.
   Use the reliability loop from Section 5.5 to reproduce.
