---
name: graphics-testing
description: Specific strategies for headless Graphics and Intuition library testing.
---

# Graphics Testing Skill

Specialized testing strategies for `graphics.library` and `intuition.library`.

## 1. Core Principles
1. **Headless Testing**: No SDL2 display required.
2. **Verify BitMap**: Read pixels programmatically (`ReadPixel`).
3. **Deterministic**: Same input = same output.

## 2. Graphics Library (`tests/graphics/`)
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

## 3. Intuition Library (`tests/intuition/`)
- **Headless UI**: Test structures without opening host windows.
- **Screen**: `OpenScreen` (check width/height/depth/bitmap).
- **Window**: `OpenWindow` (check flags, IDCMP port).
- **IDCMP**: Verify `UserPort` and flags using `ModifyIDCMP`.

## 4. Notes
- Tests must run in < 5 seconds.
- Always clean up resources (close screens/windows).
