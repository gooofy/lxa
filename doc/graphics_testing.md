# Graphics and Intuition Testing Strategy

This document outlines the comprehensive testing approach for `graphics.library` and `intuition.library` in lxa, ensuring 100% test coverage for both implemented and upcoming features.

## 1. Testing Challenges

Graphics and UI libraries present unique testing challenges:

1. **Visual Output**: Traditional output comparison doesn't work - we need to verify bitmap contents programmatically
2. **No Display Required**: Tests must run headless (no SDL2 window) for CI/CD
3. **Hardware Abstraction**: No actual Amiga hardware to validate against
4. **Interactivity**: Input events and IDCMP messaging require simulation

## 2. Testing Architecture

### 2.1 Headless Testing Mode

All graphics tests run in **headless mode** - SDL2 display is optional. The graphics library performs software rendering to BitMaps, which can be verified without any display.

```
┌─────────────────────────────────────────────────────────────┐
│                    Integration Test                          │
│  ┌─────────────────┐    ┌─────────────────────────────────┐ │
│  │  Test Program   │───>│  graphics.library (ROM)          │ │
│  │  (m68k)         │    │  - Software rendering to BitMap  │ │
│  └─────────────────┘    │  - No emucalls for drawing       │ │
│           │             └─────────────────────────────────┘ │
│           v                                                   │
│  ┌─────────────────┐                                         │
│  │  Verify BitMap  │    No SDL2/display required!            │
│  │  Contents       │                                         │
│  └─────────────────┘                                         │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Test Categories

| Category | Location | Purpose |
|----------|----------|---------|
| `tests/graphics/` | Integration | Test graphics.library functions via m68k programs |
| `tests/intuition/` | Integration | Test intuition.library functions |
| `tests/unit/test_display.c` | Unit | Test host display.c functions |

## 3. Graphics Library Testing

### 3.1 Implemented Functions (Phase 13) - MUST TEST

These functions are already implemented and require immediate test coverage:

#### 3.1.1 Initialization Functions

| Function | Test File | Test Cases |
|----------|-----------|------------|
| `InitRastPort()` | `init_rastport/` | Default values, NULL handling |
| `InitBitMap()` | `init_bitmap/` | Various depths/sizes, word alignment |

**Test: `tests/graphics/init_rastport/main.c`**
```c
// Verify InitRastPort sets correct defaults
struct RastPort rp;
InitRastPort(&rp);
TEST_ASSERT(rp.Mask == 0xFF, "Default mask");
TEST_ASSERT(rp.FgPen == 1, "Default FgPen");
TEST_ASSERT(rp.BgPen == 0, "Default BgPen");
TEST_ASSERT(rp.DrawMode == JAM2, "Default DrawMode");
TEST_ASSERT(rp.LinePtrn == 0xFFFF, "Default line pattern");
```

**Test: `tests/graphics/init_bitmap/main.c`**
```c
// Verify InitBitMap calculates BytesPerRow correctly
struct BitMap bm;
InitBitMap(&bm, 4, 320, 200);  // 4 planes, 320x200
TEST_ASSERT(bm.BytesPerRow == 40, "320 pixels = 40 bytes (word aligned)");
TEST_ASSERT(bm.Rows == 200, "Rows set correctly");
TEST_ASSERT(bm.Depth == 4, "Depth set correctly");

// Test non-word-aligned width
InitBitMap(&bm, 1, 17, 10);
TEST_ASSERT(bm.BytesPerRow == 4, "17 pixels rounds up to 32 bits = 4 bytes");
```

#### 3.1.2 Memory Functions

| Function | Test File | Test Cases |
|----------|-----------|------------|
| `AllocRaster()` | `alloc_raster/` | Various sizes, MEMF_CHIP allocation |
| `FreeRaster()` | `alloc_raster/` | Proper deallocation, NULL handling |

**Test: `tests/graphics/alloc_raster/main.c`**
```c
// Allocate raster memory
PLANEPTR plane = AllocRaster(320, 200);
TEST_ASSERT(plane != NULL, "Allocation succeeded");

// Verify memory is cleared (MEMF_CLEAR)
BOOL all_zero = TRUE;
for (int i = 0; i < RASSIZE(320, 200); i++) {
    if (plane[i] != 0) all_zero = FALSE;
}
TEST_ASSERT(all_zero, "Raster memory cleared");

FreeRaster(plane, 320, 200);
Printf("PASS: AllocRaster/FreeRaster\n");
```

#### 3.1.3 Pen and State Functions

| Function | Test File | Test Cases |
|----------|-----------|------------|
| `SetAPen()` | `pen_state/` | Set foreground pen |
| `SetBPen()` | `pen_state/` | Set background pen |
| `SetDrMd()` | `pen_state/` | Set draw mode (JAM1, JAM2, COMPLEMENT) |
| `GetAPen()` | `pen_state/` | Retrieve foreground pen |
| `GetBPen()` | `pen_state/` | Retrieve background pen |
| `GetDrMd()` | `pen_state/` | Retrieve draw mode |
| `SetABPenDrMd()` | `pen_state/` | Combined pen/mode setting |
| `Move()` | `pen_state/` | Move cursor position |

**Test: `tests/graphics/pen_state/main.c`**
```c
struct RastPort rp;
InitRastPort(&rp);

SetAPen(&rp, 5);
TEST_ASSERT(GetAPen(&rp) == 5, "SetAPen/GetAPen");

SetBPen(&rp, 3);
TEST_ASSERT(GetBPen(&rp) == 3, "SetBPen/GetBPen");

SetDrMd(&rp, COMPLEMENT);
TEST_ASSERT(GetDrMd(&rp) == COMPLEMENT, "SetDrMd/GetDrMd");

SetABPenDrMd(&rp, 7, 2, JAM1);
TEST_ASSERT(GetAPen(&rp) == 7, "SetABPenDrMd - APen");
TEST_ASSERT(GetBPen(&rp) == 2, "SetABPenDrMd - BPen");
TEST_ASSERT(GetDrMd(&rp) == JAM1, "SetABPenDrMd - DrMd");

Move(&rp, 100, 50);
TEST_ASSERT(rp.cp_x == 100 && rp.cp_y == 50, "Move");
```

#### 3.1.4 Drawing Functions (Core - Highest Priority)

| Function | Test File | Test Cases |
|----------|-----------|------------|
| `WritePixel()` | `pixel_ops/` | Single pixel, bounds check, modes |
| `ReadPixel()` | `pixel_ops/` | Read back, bounds check |
| `Draw()` | `line_draw/` | Horizontal, vertical, diagonal, clipping |
| `RectFill()` | `rect_fill/` | Various sizes, modes, clipping |
| `SetRast()` | `set_rast/` | Clear to various colors |

**Test: `tests/graphics/pixel_ops/main.c`**
```c
// Setup: Create bitmap with allocated planes
struct BitMap bm;
struct RastPort rp;
InitBitMap(&bm, 2, 64, 64);  // 2 planes (4 colors)
bm.Planes[0] = AllocRaster(64, 64);
bm.Planes[1] = AllocRaster(64, 64);
InitRastPort(&rp);
rp.BitMap = &bm;

// Test WritePixel/ReadPixel
SetAPen(&rp, 3);  // Color 3 (binary 11)
WritePixel(&rp, 10, 10);
ULONG color = ReadPixel(&rp, 10, 10);
TEST_ASSERT(color == 3, "WritePixel/ReadPixel color 3");

SetAPen(&rp, 1);  // Color 1 (binary 01)
WritePixel(&rp, 20, 20);
color = ReadPixel(&rp, 20, 20);
TEST_ASSERT(color == 1, "WritePixel/ReadPixel color 1");

// Test bounds - should return -1 for out of bounds
color = ReadPixel(&rp, -1, 0);
TEST_ASSERT(color == (ULONG)-1, "ReadPixel out of bounds X");
color = ReadPixel(&rp, 0, 100);
TEST_ASSERT(color == (ULONG)-1, "ReadPixel out of bounds Y");

// Cleanup
FreeRaster(bm.Planes[0], 64, 64);
FreeRaster(bm.Planes[1], 64, 64);
```

**Test: `tests/graphics/line_draw/main.c`**
```c
// Test Draw() - Bresenham line algorithm
struct BitMap bm;
struct RastPort rp;
// ... setup 64x64 bitmap with 1 plane ...

SetAPen(&rp, 1);

// Horizontal line
Move(&rp, 0, 10);
Draw(&rp, 63, 10);
// Verify all pixels on row 10 are set
for (int x = 0; x < 64; x++) {
    TEST_ASSERT(ReadPixel(&rp, x, 10) == 1, "Horizontal line");
}

// Vertical line
Move(&rp, 32, 0);
Draw(&rp, 32, 63);
// Verify all pixels in column 32 are set
for (int y = 0; y < 64; y++) {
    TEST_ASSERT(ReadPixel(&rp, 32, y) == 1, "Vertical line");
}

// Diagonal line (45 degrees)
SetRast(&rp, 0);  // Clear
Move(&rp, 0, 0);
Draw(&rp, 31, 31);
// Verify diagonal pixels
for (int i = 0; i < 32; i++) {
    TEST_ASSERT(ReadPixel(&rp, i, i) == 1, "Diagonal line");
}
```

**Test: `tests/graphics/rect_fill/main.c`**
```c
// Test RectFill
struct BitMap bm;
struct RastPort rp;
// ... setup 64x64 bitmap ...

SetAPen(&rp, 1);
RectFill(&rp, 10, 10, 30, 30);

// Verify filled area
for (int y = 10; y <= 30; y++) {
    for (int x = 10; x <= 30; x++) {
        TEST_ASSERT(ReadPixel(&rp, x, y) == 1, "Inside rect");
    }
}

// Verify outside area is clear
TEST_ASSERT(ReadPixel(&rp, 9, 10) == 0, "Outside rect left");
TEST_ASSERT(ReadPixel(&rp, 31, 10) == 0, "Outside rect right");
TEST_ASSERT(ReadPixel(&rp, 10, 9) == 0, "Outside rect top");
TEST_ASSERT(ReadPixel(&rp, 10, 31) == 0, "Outside rect bottom");

// Test COMPLEMENT mode
SetDrMd(&rp, COMPLEMENT);
SetAPen(&rp, 1);
RectFill(&rp, 15, 15, 25, 25);  // XOR inner area
TEST_ASSERT(ReadPixel(&rp, 20, 20) == 0, "COMPLEMENT mode");
```

#### 3.1.5 Timing/Sync Functions (No-ops)

| Function | Test File | Test Cases |
|----------|-----------|------------|
| `WaitTOF()` | `sync/` | Verify no crash, returns |
| `WaitBlit()` | `sync/` | Verify no crash, returns |

**Test: `tests/graphics/sync/main.c`**
```c
// These are no-ops in lxa, just verify they don't crash
WaitTOF();
WaitBlit();
Printf("PASS: Sync functions (no-ops)\n");
```

### 3.2 Stub Functions (Phase 14+) - Document Coverage Needs

Functions currently stubbed that will need tests when implemented:

| Priority | Function | Phase | Test Complexity |
|----------|----------|-------|-----------------|
| High | `BltBitMap()` | 15 | Complex - verify blitter ops |
| High | `Text()` | 15 | Medium - verify glyph rendering |
| High | `OpenFont()` | 15 | Medium - font loading |
| Medium | `ClipBlit()` | 15 | Complex - verify clipping |
| Medium | `ScrollRaster()` | 15 | Medium - verify scroll ops |
| Low | Sprite functions | Future | Complex |
| Low | Copper functions | N/A | Not planned |

## 4. Intuition Library Testing

### 4.1 Testing Strategy for UI

Intuition testing requires a different approach since it involves windows and user interaction.

#### 4.1.1 Headless UI Testing

For CI/CD, we test Intuition **without creating actual host windows**:

```c
// Test OpenScreen/CloseScreen structures are properly created
struct NewScreen ns = {
    .Width = 320,
    .Height = 200,
    .Depth = 2,
    .Type = CUSTOMSCREEN,
};

struct Screen *scr = OpenScreen(&ns);
if (scr) {
    // Verify screen structure
    TEST_ASSERT(scr->Width == 320, "Screen width");
    TEST_ASSERT(scr->Height == 200, "Screen height");
    TEST_ASSERT(scr->BitMap.Depth == 2, "Screen depth");
    
    // Verify RastPort is initialized
    TEST_ASSERT(scr->RastPort.BitMap == &scr->BitMap, "RastPort bitmap");
    
    CloseScreen(scr);
}
```

#### 4.1.2 Window Tests

| Function | Test File | Test Cases |
|----------|-----------|------------|
| `OpenScreen()` | `screen_basic/` | Structure creation, bitmap allocation |
| `CloseScreen()` | `screen_basic/` | Proper cleanup |
| `OpenWindow()` | `window_basic/` | Window creation, IDCMP setup |
| `CloseWindow()` | `window_basic/` | Proper cleanup |
| `RefreshWindowFrame()` | `window_refresh/` | No crash |
| `BeginRefresh()/EndRefresh()` | `window_refresh/` | Refresh handling |

**Test: `tests/intuition/screen_basic/main.c`**
```c
struct NewScreen ns = {0};
ns.Width = 640;
ns.Height = 480;
ns.Depth = 4;
ns.Type = CUSTOMSCREEN;
ns.DefaultTitle = "Test Screen";

struct Screen *scr = OpenScreen(&ns);
TEST_ASSERT(scr != NULL, "OpenScreen succeeded");

if (scr) {
    TEST_ASSERT(scr->Width == 640, "Screen width correct");
    TEST_ASSERT(scr->Height == 480, "Screen height correct");
    TEST_ASSERT(scr->BitMap.Depth == 4, "BitMap depth correct");
    TEST_ASSERT(scr->BitMap.BytesPerRow == 80, "BytesPerRow correct (640/8)");
    
    // Verify all planes allocated
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(scr->BitMap.Planes[i] != NULL, "Plane allocated");
    }
    
    CloseScreen(scr);
}
Printf("PASS: Screen basic\n");
```

**Test: `tests/intuition/window_basic/main.c`**
```c
// First open a screen
struct NewScreen ns = {0};
ns.Width = 320;
ns.Height = 200;
ns.Depth = 2;
ns.Type = CUSTOMSCREEN;

struct Screen *scr = OpenScreen(&ns);
TEST_ASSERT(scr != NULL, "Screen for window test");

// Open a window
struct NewWindow nw = {0};
nw.LeftEdge = 10;
nw.TopEdge = 10;
nw.Width = 200;
nw.Height = 100;
nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS;
nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR;
nw.Screen = scr;
nw.Type = CUSTOMSCREEN;

struct Window *win = OpenWindow(&nw);
TEST_ASSERT(win != NULL, "OpenWindow succeeded");

if (win) {
    TEST_ASSERT(win->LeftEdge == 10, "Window position X");
    TEST_ASSERT(win->TopEdge == 10, "Window position Y");
    TEST_ASSERT(win->Width == 200, "Window width");
    TEST_ASSERT(win->Height == 100, "Window height");
    TEST_ASSERT(win->WScreen == scr, "Window screen link");
    TEST_ASSERT(win->UserPort != NULL, "IDCMP port created");
    
    CloseWindow(win);
}

CloseScreen(scr);
Printf("PASS: Window basic\n");
```

### 4.2 IDCMP Message Testing

Testing input delivery without actual user input:

**Test: `tests/intuition/idcmp/main.c`**
```c
// Test IDCMP message delivery (simulated)
struct Window *win = /* open window with IDCMP_CLOSEWINDOW */;

// In headless mode, we can manually post an IDCMP message
// to test the receiving code (if this capability is added)

// For now, verify IDCMP port is properly configured
TEST_ASSERT(win->UserPort != NULL, "IDCMP port exists");
TEST_ASSERT(win->IDCMPFlags & IDCMP_CLOSEWINDOW, "CLOSEWINDOW flag set");

// Test ModifyIDCMP
ModifyIDCMP(win, IDCMP_MOUSEBUTTONS);
TEST_ASSERT(!(win->IDCMPFlags & IDCMP_CLOSEWINDOW), "CLOSEWINDOW cleared");
TEST_ASSERT(win->IDCMPFlags & IDCMP_MOUSEBUTTONS, "MOUSEBUTTONS set");
```

## 5. Host Display Unit Tests

Unit tests for `src/lxa/display.c` verify the host-side SDL2 abstraction.

**Location:** `tests/unit/test_display.c`

```c
#include "unity.h"
#include "../../src/lxa/display.h"

void setUp(void) {}
void tearDown(void) {}

void test_display_available_without_sdl(void)
{
    // When SDL2 is not initialized, display_available returns false
    // (This test runs in headless mode)
    #ifndef SDL2_FOUND
    TEST_ASSERT_FALSE(display_available());
    #endif
}

void test_palette_conversion(void)
{
    // Test RGB4 to ARGB32 conversion
    // RGB4: 0xF00 = bright red
    // Should become: 0xFFFF0000 (ARGB32)
    display_t disp = {0};
    display_set_color(&disp, 0, 0xF, 0x0, 0x0);  // RGB4 values
    TEST_ASSERT_EQUAL_HEX32(0xFFFF0000, disp.palette[0]);
}

void test_planar_to_chunky_conversion(void)
{
    // Test conversion of 2-plane data to chunky
    // Plane 0: 10101010 = pixels 0,2,4,6 have bit 0 set
    // Plane 1: 11001100 = pixels 0,1,4,5 have bit 1 set
    // Result:  pixel 0 = 3, pixel 1 = 2, pixel 2 = 1, etc.
    
    uint8_t plane0[1] = {0xAA};  // 10101010
    uint8_t plane1[1] = {0xCC};  // 11001100
    const uint8_t *planes[2] = {plane0, plane1};
    
    uint8_t chunky[8];
    // Call internal conversion function (if exposed for testing)
    // planar_to_chunky(planes, chunky, 8, 1, 2);
    
    // Expected: 3, 2, 1, 0, 3, 2, 1, 0
    // TEST_ASSERT_EQUAL(3, chunky[0]);
    // TEST_ASSERT_EQUAL(2, chunky[1]);
    // ...
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_display_available_without_sdl);
    RUN_TEST(test_palette_conversion);
    RUN_TEST(test_planar_to_chunky_conversion);
    return UNITY_END();
}
```

## 6. Test Implementation Plan

### Phase 1: Immediate (Graphics Foundation Tests)

Create tests for all currently implemented functions:

| # | Test | Priority | Status |
|---|------|----------|--------|
| 1 | `tests/graphics/init_rastport/` | High | TODO |
| 2 | `tests/graphics/init_bitmap/` | High | TODO |
| 3 | `tests/graphics/alloc_raster/` | High | TODO |
| 4 | `tests/graphics/pen_state/` | High | TODO |
| 5 | `tests/graphics/pixel_ops/` | High | TODO |
| 6 | `tests/graphics/line_draw/` | High | TODO |
| 7 | `tests/graphics/rect_fill/` | High | TODO |
| 8 | `tests/graphics/set_rast/` | High | TODO |
| 9 | `tests/graphics/sync/` | Low | TODO |

### Phase 2: With Phase 13.4 (Screen Management)

| # | Test | Priority | Status |
|---|------|----------|--------|
| 1 | `tests/intuition/screen_basic/` | High | TODO |
| 2 | `tests/intuition/screen_bitmap/` | High | TODO |

### Phase 3: With Phase 14 (Intuition Basics)

| # | Test | Priority | Status |
|---|------|----------|--------|
| 1 | `tests/intuition/window_basic/` | High | TODO |
| 2 | `tests/intuition/window_gadgets/` | Medium | TODO |
| 3 | `tests/intuition/idcmp/` | High | TODO |

### Phase 4: With Phase 15 (Advanced Graphics)

| # | Test | Priority | Status |
|---|------|----------|--------|
| 1 | `tests/graphics/blt_bitmap/` | High | TODO |
| 2 | `tests/graphics/text_render/` | High | TODO |
| 3 | `tests/graphics/clip_blit/` | Medium | TODO |

## 7. Coverage Requirements

### Graphics Library

| Component | Required Coverage | Notes |
|-----------|-------------------|-------|
| Initialization | 100% | InitRastPort, InitBitMap |
| Memory | 100% | AllocRaster, FreeRaster |
| State | 100% | All pen/mode functions |
| Drawing | 100% | All pixel/line/rect ops |
| Blitter | 100% | When implemented |

### Intuition Library

| Component | Required Coverage | Notes |
|-----------|-------------------|-------|
| Screen | 100% | Open/Close, structure init |
| Window | 100% | Open/Close, IDCMP setup |
| Gadgets | 100% | When implemented |
| Input | 100% | IDCMP delivery |

### Host Display

| Component | Required Coverage | Notes |
|-----------|-------------------|-------|
| SDL init/shutdown | 100% | When SDL2 available |
| Palette | 100% | Color conversion |
| Surface update | 100% | Planar/chunky updates |

## 8. Test Fixtures

### Standard Test BitMap

Create a helper to set up a standard test bitmap:

```c
/* tests/graphics/common/test_bitmap.h */
#ifndef TEST_BITMAP_H
#define TEST_BITMAP_H

#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>

/* Create a test bitmap with specified dimensions and depth */
struct BitMap *create_test_bitmap(UWORD width, UWORD height, UBYTE depth);

/* Free test bitmap and all planes */
void free_test_bitmap(struct BitMap *bm, UWORD width, UWORD height);

/* Create RastPort linked to bitmap */
void init_test_rastport(struct RastPort *rp, struct BitMap *bm);

#endif
```

## 9. Verification Helpers

### BitMap Content Verification

```c
/* Verify a rectangular region contains expected color */
BOOL verify_rect_color(struct RastPort *rp, 
                       WORD x1, WORD y1, WORD x2, WORD y2,
                       ULONG expected_color)
{
    for (WORD y = y1; y <= y2; y++) {
        for (WORD x = x1; x <= x2; x++) {
            if (ReadPixel(rp, x, y) != expected_color) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

/* Count pixels of a specific color in a region */
ULONG count_pixels_of_color(struct RastPort *rp,
                            WORD x1, WORD y1, WORD x2, WORD y2,
                            ULONG color)
{
    ULONG count = 0;
    for (WORD y = y1; y <= y2; y++) {
        for (WORD x = x1; x <= x2; x++) {
            if (ReadPixel(rp, x, y) == color) {
                count++;
            }
        }
    }
    return count;
}
```

## 10. CI/CD Integration

All graphics tests must:

1. **Run headless** - No SDL2 window creation required
2. **Be deterministic** - Same input produces same output
3. **Complete quickly** - Under 5 seconds per test
4. **Use standard test framework** - Output compatible with test_runner.sh

### Test Runner Integration

```makefile
# tests/graphics/Makefile
TESTS = init_rastport init_bitmap alloc_raster pen_state \
        pixel_ops line_draw rect_fill set_rast sync

test:
	@for t in $(TESTS); do \
		$(MAKE) -C $$t run-test || exit 1; \
	done
	@echo "All graphics tests passed"
```

## 11. Future Considerations

### Visual Regression Testing (Optional)

For debugging and development, consider adding optional visual verification:

1. Render test bitmap to PNG file
2. Compare against reference image
3. Flag for manual inspection if different

This is **not required for CI/CD** but useful for development.

### Performance Testing

Add benchmarks for critical drawing operations:

```c
// Benchmark RectFill performance
ULONG start = /* get time */;
for (int i = 0; i < 10000; i++) {
    RectFill(&rp, 0, 0, 319, 199);
}
ULONG end = /* get time */;
Printf("RectFill: %ld ops/sec\n", 10000000 / (end - start));
```
