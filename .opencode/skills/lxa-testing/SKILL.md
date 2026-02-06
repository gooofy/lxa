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

## 5. liblxa API Reference (C++ via lxa_test.h)

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

## 6. Debugging Failures
1. **GTest Output**: Check failure messages and stack traces.
2. **Debug Log**: Run with `-v` (verbose).
3. **Crashes**: Check host-side core dumps or ROM assertions.
4. **Screenshots**: Some drivers capture screenshots on failure.

## 7. Running Tests
- All tests: `cd build && ctest`
- Specific suite: `./build/tests/drivers/lxa_tests --gtest_filter=SimpleGadgetTest.*`
