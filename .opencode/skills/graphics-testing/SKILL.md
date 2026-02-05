---
name: graphics-testing
description: Specific strategies for headless Graphics and Intuition library testing.
---

# Graphics Testing Skill

Specialized testing strategies for `graphics.library` and `intuition.library`.

## 1. Core Principles
1. **Headless Testing**: No SDL2 display required.
2. **Host-Side Drivers**: Preferred for interactive UI testing.
3. **Verify BitMap**: Read pixels programmatically (`ReadPixel`).
4. **Deterministic**: Same input = same output.

## 2. Host-Side Test Drivers (PREFERRED)

For any test involving user interaction (mouse clicks, keyboard input, menu selection), use the **liblxa host-side driver infrastructure**.

### Location
`tests/drivers/` - Host-side C programs using liblxa API

### Example: Gadget Click Test
```c
#include "lxa_api.h"

int main(void) {
    lxa_config_t config = {
        .rom_path = "target/rom/lxa.rom",
        .sys_drive = "target/samples/Samples",
        .headless = true,
    };
    
    lxa_init(&config);
    lxa_load_program("SYS:SimpleGad", "");
    
    // Wait for window
    lxa_wait_windows(1, 5000);
    lxa_window_info_t info;
    lxa_get_window_info(0, &info);
    
    // CRITICAL: Let task reach WaitPort() before injecting events
    for (int i = 0; i < 200; i++) {
        lxa_run_cycles(10000);
    }
    lxa_clear_output();
    
    // Click gadget at center
    int x = info.x + 70;  // gadget center
    int y = info.y + 56;  // including title bar
    lxa_inject_mouse_click(x, y, LXA_MOUSE_LEFT);
    
    // Process events through VBlank
    for (int i = 0; i < 20; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(50000);
    }
    
    // Let task produce output
    for (int i = 0; i < 100; i++) {
        lxa_run_cycles(10000);
    }
    
    // Verify output
    char output[4096];
    lxa_get_output(output, sizeof(output));
    assert(strstr(output, "GADGETDOWN") != NULL);
    
    lxa_shutdown();
    return 0;
}
```

### Critical Timing Pattern
```
1. lxa_load_program() → task starts running
2. lxa_wait_windows() → wait for UI to appear
3. Run cycles WITHOUT VBlanks → task reaches WaitPort()
4. lxa_clear_output() → clear initialization messages
5. lxa_inject_mouse_click() → queue event
6. VBlanks + cycles → Intuition processes event, posts IDCMP
7. More cycles WITHOUT VBlanks → task processes message, prints output
8. lxa_get_output() → verify results
```

### Why This Pattern?
- The Amiga task must be in `TS_WAIT` state (inside `WaitPort()`) before events are injected
- VBlanks trigger `_intuition_VBlankInputHook()` which polls events and posts IDCMP messages
- After `Signal()` wakes the task, it needs CPU cycles to run `GetMsg()` and `printf()`

## 3. Graphics Library (`tests/graphics/`)
- **Init**: `InitRastPort`, `InitBitMap`.
- **Memory**: `AllocRaster`, `FreeRaster`.
- **Drawing**: `WritePixel`, `Draw` (lines), `RectFill`.
- **Verification**: Loop through BitMap area and assert colors via `ReadPixel`.

### Setup Example
```c
struct BitMap bm;
struct RastPort rp;
InitBitMap(&bm, 2, 64, 64);
bm.Planes[0] = AllocRaster(64, 64);
bm.Planes[1] = AllocRaster(64, 64);
InitRastPort(&rp);
rp.BitMap = &bm;
// ... Test ...
FreeRaster(bm.Planes[0], 64, 64);
FreeRaster(bm.Planes[1], 64, 64);
```

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
- Any user interaction

**MUST use host-side driver infrastructure** (`tests/drivers/`)

## 5. Migration: test_inject.h → Host Drivers

The legacy `test_inject.h` approach (in-ROM self-testing) is being phased out.

**Current test_inject.h samples** (to be migrated):
- `SimpleMenu` - RMB menu selection
- `UpdateStrGad` - Keyboard input to string gadgets  
- `SimpleGTGadget` - GadTools gadget clicks

**Already using host drivers**:
- `SimpleGad` - tested via `tests/drivers/simplegad_test`

**Migration Priority**:
1. Remaining test_inject.h samples
2. Deep dive app tests (KickPascal, Devpac, etc.)
3. All new interactive tests

## 6. liblxa Event Injection API

```c
// Mouse
bool lxa_inject_mouse_click(int x, int y, int button);
bool lxa_inject_mouse(int x, int y, int buttons, int type);

// Keyboard  
bool lxa_inject_key(int rawkey, int qualifier, bool down);
bool lxa_inject_keypress(int rawkey, int qualifier);
bool lxa_inject_string(const char *str);

// Constants
#define LXA_MOUSE_LEFT   1
#define LXA_MOUSE_RIGHT  2
#define LXA_MOUSE_MIDDLE 4
```

## 7. Notes
- Tests must run in < 30 seconds (interactive tests may need more cycles).
- Always clean up resources (close screens/windows).
- Use `lxa_clear_output()` before the section you want to verify.
- Check for specific strings with `strstr()`, don't compare entire output.
