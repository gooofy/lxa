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
- Can query window state, gadget geometry, and capture program output
- Runs headlessly without SDL2 display
- Standard GTest assertions and reporting

**Current helper surface**:
- `WaitForWindowDrawn()` / `lxa_wait_window_drawn()` for visible-content readiness
- `GetGadgetCount()`, `GetGadgetInfo()`, and `GetGadgets()` for Intuition gadget introspection
- `ClickGadget()` for geometry-driven interaction
- `CaptureWindow()` / `lxa_capture_screen()` for screenshots on failure

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
3. Write or extend the GTest Driver.
4. Prefer readiness and gadget-introspection helpers over fixed sleeps or fragile raw coordinates.
5. Implement Feature.
6. Run Tests via `ctest` or `make lxa_tests`.
7. Measure Coverage.
8. Iterate until 100% coverage and passing.

## 5. Running Tests

### 5.1 Build First
```bash
./build.sh          # From project root (/home/guenter/projects/amiga/lxa/src/lxa)
```

### 5.2 Full Test Suite
```bash
# ALWAYS use -j16 for parallel execution, ALWAYS use --timeout 180 to detect hangs
cd build && ctest --output-on-failure --timeout 180 -j16

# Equivalent from project root:
ctest --test-dir build --output-on-failure --timeout 180 -j16
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
split into shards and use persistent fixtures, which keeps the full-suite wall
time around ~145 seconds on this machine class. Higher parallelism remains safe,
but usually does not improve the end-to-end runtime enough to justify using it
as the default.

**Tests are fully isolated**: Each test runs its own emulator instance with
independent memory. No shared files, ports, or state. Any parallelism level
is safe.

### 5.4 Running Specific Tests
```bash
# By test name (via ctest):
ctest --test-dir build --output-on-failure --timeout 180 -R shell_gtest

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
    ctest --test-dir build --output-on-failure --timeout 180 -j16 2>&1 | grep "tests passed"
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
- `WaitForWindowDrawn(index, timeout_ms)` - Wait for non-empty visible window content

**Event Injection**:
- `Click(x, y, button)` - Click at position
- `PressKey(rawkey, qualifier)` - Key press
- `TypeString(str)` - Type string
- `ClickGadget(index, window_index, button)` - Click gadget by queried geometry

**Introspection / Artifacts**:
- `GetGadgetCount(window_index)` - Get tracked gadget count
- `GetGadgetInfo(gadget_index, info, window_index)` - Query one gadget
- `GetGadgets(window_index)` - Enumerate gadgets
- `CaptureWindow(path, index)` - Save tracked window image
- `lxa_capture_screen(path)` - Save full screen image

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

## 8. Performance-Aware Test Writing

### 8.1 Current Test Suite Performance
The test suite runs 63 tests in ~145 seconds wall-time with `-j16`. Previous
optimizations (Phases 106-107) reduced this from ~210s through:

- **Headless display skip**: `display_update_planar()`/`display_refresh_all()`
  are skipped in headless VBlank; `s_display_dirty` flag auto-flushes on
  `lxa_read_pixel()`/`lxa_capture_*()` calls.
- **Idle detection**: `lxa_is_idle()` checks if TaskReady list is empty.
  `lxa_run_until_idle()` returns early when all tasks are blocked.
- **Persistent fixtures**: Multi-test drivers use `SetUpTestSuite()` /
  `TearDownTestSuite()` to load the app once per binary, not once per test.
- **Reduced cycle budgets**: `lxa_inject_string()` uses 10x50K with idle
  early-return. `lxa_inject_mouse_click()` uses 6 settle iterations with idle.
  `lxa_inject_drag()` uses 3x200K per step (VBlank-driven, not idle-detected,
  because Intuition renders in interrupt context).

### 8.2 Current Cycle Budget Reference

| Operation | Current Budget | Real 68000 Need | Over-Provision |
|:----------|---------------:|----------------:|---------------:|
| Key press (inject_string) | 500,000 (10×50K) | ~2,000 | 250× |
| Mouse click | ~300,000 (6 iters) | ~10,000 | 30× |
| Drag step (inject_drag) | 600,000 (3×200K) | ~70,000 | 9× |
| App startup (typical) | 5-10M | ~1-2M | 5× |
| Settling after action | 1-3M | ~200K | 5-15× |

### 8.3 Guidelines for New Tests
- **Prefer event-driven waiting**: Use `WaitForWindowDrawn()`, window count
  checks, or pixel change detection instead of flat `RunCyclesWithVBlank(200, 50000)`.
- **Do not inflate cycle budgets to fix flaky tests**: Understand the root cause
  first. If the test is flaky, the issue is likely a missing synchronization
  point, not insufficient cycles.
- **Use persistent fixtures**: If writing a multi-test driver, use
  `SetUpTestSuite()` to load the app once. Follow the pattern in
  `simplegad_gtest.cpp` or `cluster2_gtest.cpp`.
- **Consider test sharding early**: If a driver has >4 test cases and takes
  >30 seconds, shard it from the start.
- **Shard filter safety**: When adding a `TEST_F` to a sharded driver, you MUST
  update the corresponding `add_gtest_driver_shard` FILTER string in
  `tests/drivers/CMakeLists.txt`. Missing this silently orphans the test — ctest
  runs the binary but the new test is never executed. The `tools/check_shard_coverage.py`
  CI check (Phase 0) will catch violations, but update FILTER in the same commit.
- **Custom UI apps**: Some apps (Cluster2) use zero Intuition gadgets and
  render everything custom. `GetGadgetCount()` returns 0 for these. Tests
  must use raw coordinate clicks and pixel-change verification instead of
  gadget introspection.
- **Place destructive tests last**: Tests that close windows or quit the app
  must be the last test in a persistent fixture suite.

## 9. Upcoming API Extensions (Phase 130+)

These APIs are planned but not yet implemented. Do not use until the corresponding phase lands.

### 9.1 Text Hook (Phase 130)
```cpp
// After Phase 130: semantic text assertions without pixel-counting
std::vector<std::string> text_log;
lxa_set_text_hook([](const char *s, int n, int, int, void *ud) {
    ((std::vector<std::string>*)ud)->push_back(std::string(s, n));
}, &text_log);
// ... run app ...
lxa_clear_text_hook();
EXPECT_TRUE(std::any_of(text_log.begin(), text_log.end(),
    [](const auto &s){ return s.find("EDITOR") != std::string::npos; }));
```

### 9.2 Window/Screen Event Log (Phase 131)
```cpp
// After Phase 131: detect open/close cycles without window-count polling
lxa_drain_events(events, &count);
// events[] contains LXA_EVENT_OPEN_WINDOW, LXA_EVENT_CLOSE_WINDOW, etc.
```

### 9.3 Menu Introspection (Phase 131)
```cpp
// After Phase 131: assert menu structure without hardcoded coordinates
lxa_menu_strip_t *strip = lxa_get_menu_strip(0);
lxa_menu_info_t info = lxa_get_menu_info(strip, 0, -1); // menu 0, no item
EXPECT_STREQ(info.name, "Project");
EXPECT_TRUE(info.enabled);
```
