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

### New Driver Helpers
- `WaitForWindowDrawn()` avoids clicking before Intuition has rendered visible content.
- `GetGadgetCount()`, `GetGadgetInfo()`, and `GetGadgets()` let tests inspect live gadget geometry instead of guessing coordinates.
- `ClickGadget()` uses the queried gadget bounds to inject a reliable click.
- `CaptureWindow()` and `lxa_capture_screen()` provide failure artifacts for visual regressions.

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

Preferred flow for interactive app tests:
1. Wait for the expected window count.
2. Call `WaitForWindowDrawn()`.
3. Let the task settle with `RunCyclesWithVBlank()` / `WaitForEventLoop()`.
4. Use gadget introspection helpers when available.
5. Capture screenshots only for debugging or failure artifacts.

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
- Tests must run in < 30 seconds per shard (interactive apps may need sharding — see lxa-testing §8.3).
- Always clean up resources (close screens/windows).
- Use `ClearOutput()` before the section you want to verify.
- Check for specific strings with `output.find()`, don't compare entire output.
- If a UI test depends on a third-party helper library, load the real library from disk instead of adding a ROM stub for it.

## 9. Screen-Mode–Aware Testing

Apps that probe display modes at startup may exit early with rv=26 or open probe windows that are immediately discarded. As of Phase 129 (v0.9.3), ECS mode emulation is complete. Modern productivity apps (PPaint, FinalWriter) probe for **RTG / P96 display IDs** — they will remain blocked until Phases 137–139 land.

**Symptom diagnosis**:

| Symptom | Likely cause |
|---|---|
| App exits (rv ≥ 26), flood of `FindDisplayInfo`/`GetDisplayInfoData` in log | RTG mode probe — app needs P96 (Phase 137+) |
| App exits but no display-info flood | Different root cause; check OpenLibrary failures first |
| App opens probe windows briefly then exits | RTG screen-open attempt fails (Phase 137+) |

**ECS-era apps** (DPaint, DevPac, ASM-One, etc.): Phase 129 ECS virtualisation covers these. If an ECS app still exits early, check `g_known_display_ids[]` and `GetDisplayInfoData(DTAG_DIMS)` ranges.

**RTG apps** (PPaint, FinalWriter, productivity/IDE apps): do not attempt ECS workarounds. Write survival-only tests until Phase 139 lands; then upgrade to full UI assertions.

## 10. Text Extraction (Phase 130+)

After Phase 130 lands, prefer `lxa_set_text_hook()` over pixel-counting for content assertions. The hook fires on every `Text()` call in the ROM, regardless of which app or window is active.

**Typical driver pattern**:
```cpp
class MyAppTest : public LxaUITest {
    std::vector<std::string> text_log_;
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        lxa_set_text_hook([](const char *s, int n, int, int, void *ud) {
            static_cast<std::vector<std::string>*>(ud)->push_back({s, (size_t)n});
        }, &text_log_);
        // ... load app ...
    }
    void TearDown() override {
        lxa_clear_text_hook();
        LxaUITest::TearDown();
    }
    bool TextSeen(const char *needle) const {
        return std::any_of(text_log_.begin(), text_log_.end(),
            [&](const auto &s){ return s.find(needle) != std::string::npos; });
    }
};

TEST_F(MyAppTest, EditorStatusBarRenders) {
    // ... accept workspace prompt ...
    EXPECT_TRUE(TextSeen("EDITOR"));
}
```

**When to prefer text hook over pixel counting**:
| Assertion | Prefer |
|-----------|--------|
| "Some text appears in this region" | Text hook |
| "At least N non-grey pixels in this band" | Pixel count |
| "UI hasn't changed after idle period" | Pixel count (stability) |
| "This specific label renders" | Text hook |
| "Menu title is correct" | Menu introspection (Phase 131) |

## 8. Visual Investigation with `tools/screenshot_review.py`

When a captured screenshot needs human-readable visual analysis — especially when a pixel assertion fires but the defect is not obvious from numbers alone — use the OpenRouter vision helper.

### Prerequisites
```bash
export OPENROUTER_API_KEY=<your-key>
```

### Basic usage
```bash
# Single capture review
python tools/screenshot_review.py path/to/capture.png

# Side-by-side comparison (e.g. before/after a fix, or lxa vs reference)
python tools/screenshot_review.py before.png after.png

# Focus the model on a specific subsystem
python tools/screenshot_review.py \
    --prompt "Describe any clipping or overdraw around the menu separator" \
    capture.png

# Use a specific vision model
python tools/screenshot_review.py \
    --model anthropic/claude-sonnet-4.6 \
    capture.png

# Machine-readable output for scripting or logging
python tools/screenshot_review.py --output json capture.png
```

### Typical debugging workflow
1. Run the GTest driver to reproduce the visual defect and collect an artifact:
   ```bash
   ./build/tests/drivers/my_app_gtest --gtest_filter="MyTest.BadMenu"
   # driver calls lxa_capture_window() on failure → saves capture.png
   ```
2. Send the artifact to the vision model:
   ```bash
   python tools/screenshot_review.py /tmp/lxa_capture_*.png
   ```
3. Use the model's region/coordinate hints to focus a targeted pixel assertion or a narrower code search.
4. Once the root cause is confirmed in code, write or update the pixel-level GTest regression so the defect is caught automatically in future runs.

### When to use it
| Situation | Use the tool? |
|-----------|---------------|
| GTest failure artifact needs visual diagnosis | Yes |
| Comparing lxa output to a real-Amiga reference image | Yes |
| Triaging a layout/clipping/menu rendering regression | Yes |
| Bug is purely algorithmic (no visual component) | No |
| No screenshot has been captured yet | No — capture first |
| Automated CI path | No — requires live API key |

### Notes
- The default model is `google/gemini-3-flash-preview`. Override with `--model` when a stronger model is needed for subtle rendering details.
- The default prompt is tuned for Amiga UI review. Supply `--prompt` only when narrowing to a specific subsystem helps.
- The tool is for **developer investigation only**; never add API calls to it in automated test code.

## 11. RTG / Picasso96 Testing (Phase 137+)

After Phase 137 lands, RTG bitmaps and screens become testable. Key patterns:

**Detecting RTG bitmaps in tests** — RTG bitmaps have `GetBitMapAttr(bm, BMA_DEPTH)` returning 15, 16, 24, or 32. The `BMF_RTG` internal flag distinguishes them from planar bitmaps. Never call `ReadPixel()` (which goes through planar rendering) on an RTG bitmap — use `p96ReadPixelArray()` instead.

**P96 pixel verification pattern**:
```cpp
// After Phase 138 lands:
struct RenderInfo ri;
p96LockBitMap(bm, (struct TagItem *)nullptr);  // returns ri
uint32_t *pixels = (uint32_t *)ri.Memory;
uint32_t pixel = pixels[y * (ri.BytesPerRow / 4) + x];
EXPECT_EQ(pixel & 0x00FFFFFF, 0x00FF0000u);  // red pixel at (x,y)
p96UnlockBitMap(bm, &ri);
```

**RTG screen test structure** — follow `rtg_gtest.cpp` (added in Phase 137) as the reference driver. Use `LxaUITest` base class; open screen with `SA_Depth=32` and a P96 mode ID; assert screen opens without crash; use `p96GetBitMapAttr` to verify depth.

**cybergraphics shim testing** — add a `cgx_gtest.cpp` companion that opens `cybergraphics.library` and calls the same functions via CGX names. Both libraries must return identical results for the same underlying RTG bitmap.

**Symptom of missing RTG infrastructure**: `OpenLibrary("Picasso96API.library", 0)` returns NULL in the test log — check that `lxa_p96.c` is compiled and `sys/CMakeLists.txt` has the `add_disk_library()` entry.
