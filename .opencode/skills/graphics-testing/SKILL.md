---
name: graphics-testing
description: Specific strategies for headless Graphics and Intuition library testing.
---

# Graphics Testing Skill

Specialized testing strategies for `graphics.library` and `intuition.library`.

## 1. Core Principles
1. **Headless Testing**: No SDL2 display required.
2. **Host-Side Drivers**: Mandatory for interactive UI testing via Google Test.
3. **Verify BitMap**: Read pixels programmatically (`ReadPixel`).
4. **Deterministic**: Same input = same output.

## 2. Google Test Host-Side Drivers (MANDATORY)

For any test involving user interaction (mouse clicks, keyboard input, menu selection), use the **Google Test host-side driver infrastructure**.

### Location
`tests/drivers/` - GTest suites using `LxaUITest` base class.

### Example: Gadget Click Test
```cpp
#include "lxa_test.h"

using namespace lxa::testing;

class GadgetTest : public LxaUITest {
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

TEST_F(GadgetTest, ClickButton) {
    ClearOutput();
    Click(window_info.x + 50, window_info.y + 40);
    RunCyclesWithVBlank(20);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("GADGETUP"), std::string::npos);
}
```

### Why This Pattern?
- The Amiga task must be in `TS_WAIT` state (inside `WaitPort()`) before events are injected.
- `RunCyclesWithVBlank()` triggers the VBlank interrupt which processes input through Intuition.
- After `Signal()` wakes the task, it needs CPU cycles to run `GetMsg()` and `printf()`.

## 3. Graphics Library (`tests/graphics/`)
- **Init**: `InitRastPort`, `InitBitMap`.
- **Memory**: `AllocRaster`, `FreeRaster`.
- **Drawing**: `WritePixel`, `Draw` (lines), `RectFill`.
- **Verification**: Assert colors via `ReadPixel(x, y)` in the GTest driver.

## 4. Intuition Library (`tests/intuition/`)

### Non-Interactive Tests (Integration)
- **Headless UI**: Test structures without user input.
- **Screen**: `OpenScreen` (check width/height/depth/bitmap).
- **Window**: `OpenWindow` (check flags, IDCMP port).
- **IDCMP**: Verify `UserPort` and flags using `ModifyIDCMP`.

### Interactive Tests (Host-Side Drivers)
For tests requiring:
- Mouse clicks on gadgets
- Keyboard input to string gadgets
- Menu selection (RMB)
- Window dragging or resizing

**MUST use Google Test host-side driver infrastructure** (`tests/drivers/`)

## 5. Migration: test_inject.h
The legacy `test_inject.h` approach (in-ROM self-testing) has been removed. All tests must be driven from the host side via Google Test.

## 6. liblxa Event Injection API (C++ via lxa_test.h)

```cpp
// Mouse
void Click(int x, int y, int button = LXA_MOUSE_LEFT);

// Keyboard  
void PressKey(int rawkey, int qualifier = 0);
void TypeString(const char *str);

// Screen Info
int ReadPixel(int x, int y);
bool ReadPixelRGB(int x, int y, uint8_t* r, uint8_t* g, uint8_t* b);
int CountContentPixels(int x1, int y1, int x2, int y2, int bg_color = 0);
```

## 7. Notes
- Tests must run in < 30 seconds.
- Always clean up resources (close screens/windows).
- Use `ClearOutput()` before the section you want to verify.
- Check for specific strings with `output.find()`, don't compare entire output.
