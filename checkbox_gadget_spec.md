# Amiga GadTools Checkbox Gadget — Clean-Room Implementation Specification

**Source version:** gadtools.library V40 (Kickstart 3.1)  
**Kind identifier:** `CHECKBOX_KIND = 2`

---

## 1. Purpose and Concept

The checkbox gadget is a two-state toggle widget: checked or unchecked. The user clicks it to toggle the state; Intuition automatically handles the visual inversion via `TOGGLESELECT`. The application receives an `IDCMP_GADGETUP` message whose `Code` field contains the new checked state (1 = checked, 0 = unchecked). GadTools wraps Intuition's built-in `sysiclass` `CHECKIMAGE` system image, which handles all check-mark rendering.

---

## 2. Visual Anatomy

```
+---------------------+     ← label to left (default)
| [checkbox image 26×11]  |  Label text
+---------------------+
```

The gadget body is the checkbox image itself. The gadget's own `Width` and `Height` describe the checkbox box only; the external descriptive label (`ng_GadgetText`) is placed outside (left by default) via GadTools text-placement machinery.

In **unscaled mode** (default):
- Gadget is always 26 × 11 pixels regardless of `ng_Width`/`ng_Height`.
- TopEdge may be shifted downward to vertically centre the box against the label text for fonts larger than 8 pixels.

In **scaled mode** (`GTCB_Scaled = TRUE`):
- Gadget is exactly `ng_Width` × `ng_Height`.
- No TopEdge adjustment is applied.

---

## 3. Key Constants and Defaults

| Symbol | Value | Meaning |
|--------|-------|---------|
| `CHECKBOX_KIND` | 2 | Gadget kind identifier |
| `CHECKBOX_WIDTH` | 26 | Default (unscaled) gadget width in pixels |
| `CHECKBOX_HEIGHT` | 11 | Default (unscaled) gadget height in pixels |
| `CHECKIMAGE` | (sysiclass constant) | System image kind for checkbox |
| `INTERWIDTH` | 8 | Suggested horizontal gap between gadget and label |
| `INTERHEIGHT` | 4 | Suggested vertical gap for above/below text placement |

---

## 4. IDCMP Flags Required

```c
#define CHECKBOXIDCMP  (IDCMP_GADGETUP)
```

The application window must have `IDCMP_GADGETUP` in its IDCMP flags.

---

## 5. Data Structures

### 5.1 Public: `struct NewGadget`

```c
struct NewGadget {
    WORD    ng_LeftEdge, ng_TopEdge;   /* gadget position */
    WORD    ng_Width,    ng_Height;    /* desired size (overridden in unscaled mode) */
    UBYTE  *ng_GadgetText;            /* external label string */
    struct TextAttr *ng_TextAttr;     /* font for the external label */
    UWORD   ng_GadgetID;
    ULONG   ng_Flags;                 /* PLACETEXT_* / NG_HIGHLABEL; see §6.4 */
    APTR    ng_VisualInfo;            /* result of GetVisualInfoA() */
    APTR    ng_UserData;
};
```

### 5.2 Private: `struct XCheckbox` (instance data)

```c
struct XCheckbox {
    struct SpecialGadget cbid_Gadget;   /* base gadget + special hooks */
    /* --- instance data --- */
    WORD cbid_Checked;                  /* 0 = unchecked, non-zero = checked */
};

#define CHECKBOX_IDATA_SIZE  (sizeof(struct XCheckbox) - sizeof(struct SpecialGadget))
#define CBID(g)  ((struct XCheckbox *)(g))
```

`cbid_Checked` is kept in sync with the gadget's `GFLG_SELECTED` flag at all times.

---

## 6. Tag Reference

### 6.1 Creation Tags (`CreateGadget(CHECKBOX_KIND, ...)`)

| Tag | Type | Default | Notes |
|-----|------|---------|-------|
| `GTCB_Checked` | `BOOL` | `FALSE` | Initial checked state |
| `GTCB_Scaled` | `BOOL` | `FALSE` | If TRUE, scale image to `ng_Width`/`ng_Height`; if FALSE, fixed 26×11 |
| `GA_Disabled` | `BOOL` | `FALSE` | If TRUE, gadget renders ghosted/disabled |

### 6.2 Set Tags (`GT_SetGadgetAttrsA(...)`)

| Tag | Type | Notes |
|-----|------|-------|
| `GTCB_Checked` | `BOOL` | Toggle the checked state; triggers `SelectGadget()` and refresh |
| `GA_Disabled` | `BOOL` | Enable or disable the gadget; triggers refresh |

### 6.3 Get Tags (`GT_GetGadgetAttrsA(...)`)

| Tag | Return type | Notes |
|-----|-------------|-------|
| `GTCB_Checked` | `WORD` | Current checked state (0 or 1), sign-extended to LONG |
| `GA_Disabled` | `BOOL` | TRUE if `GFLG_DISABLED` is set on the gadget |

GetTable definition:
```c
struct GetTable Checkbox_GetTable[] = {
    GETTABLE_ATTR(GTCB_Checked, CBID, cbid_Checked, WORD_ATTR),
    GETTABLE_DISABLED   /* terminates table; enables GA_Disabled query */
};
```

### 6.4 `ng_Flags` Placement Flags

Control where the external label (`ng_GadgetText`) is placed:

| Flag | Value | Placement |
|------|-------|-----------|
| `PLACETEXT_LEFT` | `0x0001` | Right-aligned to the left of the gadget (**default**) |
| `PLACETEXT_RIGHT` | `0x0002` | Left-aligned to the right of the gadget |
| `PLACETEXT_ABOVE` | `0x0004` | Centered above the gadget |
| `PLACETEXT_BELOW` | `0x0008` | Centered below the gadget |
| `PLACETEXT_IN` | `0x0010` | Centered on/inside the gadget |
| `NG_HIGHLABEL` | `0x0020` | Render external label in HIGHLIGHTTEXTPEN instead of TEXTPEN |

If no placement flag is set, `PLACETEXT_LEFT` is used.

---

## 7. Creation Sequence

### Step 1: Allocate base gadget

`CreateGenericBase()` calls `CreateGadget(GENERIC_KIND, ...)` with extra size `CHECKBOX_IDATA_SIZE`.

The allocation is one contiguous `AllocVec(..., MEMF_CLEAR)` block:

```
sizeof(SpecialGadget) + ALIGNSIZE(CHECKBOX_IDATA_SIZE) + [optional label IntuiText]
```

Fields set on the base `ExtGadget`:
- `LeftEdge`, `TopEdge`, `Width`, `Height` from `ng`
- `GadgetID`, `UserData` from `ng`
- `GadgetType = GADTOOL_TYPE (0x0100)`
- `BoundsLeftEdge/TopEdge/Width/Height` = same as position/size
- `MoreFlags = GMORE_BOUNDS | GMORE_GADGETHELP`
- `Flags = GFLG_EXTENDED`

If `ng_GadgetText` is non-NULL, a label `IntuiText` is allocated immediately after the instance data, with:
- `IText = ng_GadgetText` (pointer reference, not a copy)
- `FrontPen = dri_Pens[TEXTPEN]` (or `HIGHLIGHTTEXTPEN` if `NG_HIGHLABEL` set)
- `DrawMode = JAM1`
- `ITextFont = ng_TextAttr`

### Step 2: Apply size and TopEdge adjustment (unscaled mode only)

```c
if (!getGTTagData(GTCB_Scaled, FALSE, taglist)) {
    gad->Width         = CHECKBOX_WIDTH;   /* 26 */
    gad->Height        = CHECKBOX_HEIGHT;  /* 11 */
    gad->BoundsWidth   = CHECKBOX_WIDTH;
    gad->BoundsHeight  = CHECKBOX_HEIGHT;

    /* Vertically align box with label text for fonts taller than 8 pixels */
    if (((ng->ng_Flags & PLACETEXT_MASK) <= PLACETEXT_RIGHT) && gad->GadgetText) {
        gad->TopEdge      += (ng->ng_TextAttr->ta_YSize - 7) / 2;
        gad->BoundsTopEdge = gad->TopEdge;
    }
}
```

In **scaled mode**, `Width` and `Height` stay as specified by the caller; no TopEdge adjustment is made.

### Step 3: Set image and activation flags

```c
gad->Flags      |= GADGIMAGE | GADGHIMAGE;
gad->Activation  = RELVERIFY | TOGGLESELECT;
gad->GadgetType |= BOOLGADGET;
```

`TOGGLESELECT` causes Intuition to automatically flip the `GFLG_SELECTED` flag on each click. `RELVERIFY` causes the `IDCMP_GADGETUP` message to be sent on mouse-button release.

### Step 4: Apply initial checked state

```c
SetCheckBoxAttrsA(gad, NULL, taglist);
```

This processes `GTCB_Checked` and `GA_Disabled` from the creation taglist (see §9).

### Step 5: Place external gadget label

```c
placeGadgetText(gad, ng->ng_Flags, PLACETEXT_LEFT, NULL);
```

Computes `LeftEdge`/`TopEdge` offsets on `gad->GadgetText` so the label appears correctly relative to the (possibly TopEdge-adjusted) gadget. Also calls `PlaceHelpBox()` to extend the gadget's `Bounds` to encompass the label area.

### Step 6: Hook up function pointers

```c
SGAD(gad)->sg_SetAttrs     = SetCheckBoxAttrsA;
SGAD(gad)->sg_EventHandler = HandleCheckbox;
SGAD(gad)->sg_GetTable     = Checkbox_GetTable;
/* sg_Flags is NOT set to SG_EXTRAFREE_DISPOSE — image is shared/cached */
```

Note: `sg_Refresh` is **not** set (NULL). The checkbox relies entirely on Intuition's standard gadget refresh.

### Step 7: Obtain the system image

```c
gad->SelectRender = gad->GadgetRender = getSysImage(
    ng->ng_VisualInfo,
    gad->Width,     /* 26 in unscaled mode, ng_Width in scaled mode */
    gad->Height,    /* 11 in unscaled mode, ng_Height in scaled mode */
    CHECKIMAGE
);
if (!gad->GadgetRender) return NULL;
```

`getSysImage()` searches the VisualInfo's image cache for an existing `sysiclass` image of matching `(width, height, CHECKIMAGE)`. If not found, creates one via:

```c
NewObject(NULL, "sysiclass",
    SYSIA_DrawInfo, vi->vi_DrawInfo,
    SYSIA_Which,    CHECKIMAGE,
    IA_Width,       width,
    IA_Height,      height,
    TAG_DONE)
```

The image is linked into `vi->vi_Images` for future reuse and is freed by `FreeVisualInfo()`, **not** by `FreeGadgets()`. Because of this sharing, `SG_EXTRAFREE_DISPOSE` is deliberately **not** set.

---

## 8. TopEdge Adjustment Detail

For fonts larger than 8 pixels, the fixed-size (26×11) checkbox box would appear too high relative to the text label (which scales with font size). GadTools corrects this by pushing `TopEdge` down:

```c
gad->TopEdge += (ng->ng_TextAttr->ta_YSize - 7) / 2;
```

Integer arithmetic (floor division):

| Font height (`ta_YSize`) | Adjustment (pixels down) |
|--------------------------|--------------------------|
| 7 or less | 0 |
| 8–9 | 0 |
| 9–10 | 1 |
| 11–12 | 2 |
| 13–14 | 3 |
| 15–16 | 4 |

**Condition:** Applied only when:
1. `GTCB_Scaled` is FALSE (default), **and**
2. The PLACETEXT flag is `PLACETEXT_LEFT`, `PLACETEXT_RIGHT`, or unset (i.e., `(ng_Flags & PLACETEXT_MASK) <= PLACETEXT_RIGHT`), **and**
3. `gad->GadgetText` is non-NULL.

The condition uses the fact that `PLACETEXT_LEFT = 0x0001` and `PLACETEXT_RIGHT = 0x0002`, so `<= PLACETEXT_RIGHT` correctly matches the left/right/absent cases.

This adjustment is **not** applied for `PLACETEXT_ABOVE`, `PLACETEXT_BELOW`, or `PLACETEXT_IN`.

---

## 9. Attribute Update — `SetCheckBoxAttrsA`

```c
void SetCheckBoxAttrsA(struct ExtGadget *gad, struct Window *win,
                       struct TagItem *taglist)
{
    struct TagItem *ti;
    if (ti = findGTTagItem(GTCB_Checked, taglist)) {
        cbid_Checked = (ti->ti_Data != 0);
        SelectGadget(gad, win, cbid_Checked);
    }
    TagAbleGadget(gad, win, taglist);
}
```

`SelectGadget(gad, win, select)` operates as follows:
1. `removeGadget(win, gad)` — detaches gadget while modifying.
2. If `select`: sets `gad->Flags |= GFLG_SELECTED`.
3. If not `select`: clears `gad->Flags &= ~GFLG_SELECTED`.
4. `AddRefreshGadget(gad, win, pos)` — re-attaches and triggers refresh.

When `win` is NULL, steps 1 and 4 are no-ops (attribute set without rendering update).

`TagAbleGadget()` handles `GA_Disabled`:
- If `GA_Disabled` tag is present and the state has changed:
  - `removeGadget(win, gad)`
  - Toggles `GFLG_DISABLED` via XOR.
  - `AddRefreshGadget(gad, win, pos)`.

---

## 10. Event Handling — `HandleCheckbox`

Called by the GadTools dispatcher on `IDCMP_GADGETUP`:

```c
BOOL HandleCheckbox(struct ExtGadget *gad, struct IntuiMessage *imsg) {
    imsg->Code = cbid_Checked = ((gad->Flags & GFLG_SELECTED) != 0);
    return TRUE;  /* deliver message to application */
}
```

Because `TOGGLESELECT` is set on the gadget, **Intuition has already toggled `GFLG_SELECTED` before this handler is called.** The handler simply reads back the new state and records it in `cbid_Checked` and `imsg->Code`.

### 10.1 Message Delivered to Application

| Field | Value |
|-------|-------|
| `Class` | `IDCMP_GADGETUP` |
| `Code` | `1` if now checked, `0` if now unchecked |
| `IAddress` | Pointer to the checkbox gadget (`struct Gadget *`) |
| `Qualifier` | Keyboard qualifier at time of click |

---

## 11. Rendering — `sysiclass` `CHECKIMAGE`

All visual rendering is delegated entirely to Intuition's `sysiclass` system image. GadTools makes no direct drawing calls for the checkbox itself.

### 11.1 How Intuition Draws the Image

`sysiclass` with `SYSIA_Which = CHECKIMAGE` is drawn via `DrawImageState()`. It uses the `DrawInfo` pens from the screen to produce a 3D-looking checkbox appropriate for the current screen's visual theme:

- **Unchecked state** (`IDS_NORMAL`): an empty recessed box
- **Checked state** (`IDS_SELECTED`): the same box with a checkmark drawn inside

The pens used and exact pixel patterns are defined by Intuition, not GadTools. The key pen roles are:

| Pen | Role |
|-----|------|
| `SHINEPEN` | Highlight edges of the recessed box |
| `SHADOWPEN` | Shadow edges of the recessed box |
| `TEXTPEN` | Checkmark color |
| `BACKGROUNDPEN` | Interior fill of the unchecked box |

### 11.2 State Mapping

| Gadget `Flags` | `DrawImageState` receives | Visual result |
|----------------|--------------------------|---------------|
| `GFLG_SELECTED` clear | `IDS_NORMAL` | Unchecked box |
| `GFLG_SELECTED` set | `IDS_SELECTED` | Checked box with checkmark |
| `GFLG_DISABLED` + unselected | `IDS_DISABLED` | Ghosted unchecked box |
| `GFLG_DISABLED` + selected | (depends on Intuition version) | Ghosted checked box |

Since `GADGIMAGE` and `GADGHIMAGE` are both set, `GadgetRender` is used for the normal state and `SelectRender` for the selected (highlighted) state. Because both fields point to the **same** `sysiclass` image object, the differentiation is made through the image state argument passed by Intuition at draw time.

### 11.3 Image Dimensions and Placement

The sysiclass image is created with `IA_Width = gad->Width` and `IA_Height = gad->Height`. Intuition renders it at coordinates `(gad->LeftEdge, gad->TopEdge)` in the window's RastPort. The image occupies the gadget's entire bounding box.

---

### 11.4 Checkmark Vector Polygon

The checkmark is a single filled polygon defined in Intuition's `xsysidata.asm` (`Vcheck1`). It is rendered only in the **SELECTED (checked)** state (flag `VS_S`), filled with **TEXTPEN** (`VF_TEXT`), and drawn on both mono and colour screens (`VF_BOTH`).

**Design grid:** 26 wide × 11 tall (matching the default checkbox dimensions, so at 26×11 the coordinates are pixel-exact).

**Vertex table** (9 vertices; polygon closes back to vertex 1):

| # | X  | Y | Edge to next |
|---|----|---|--------------|
| 1 | 19 | 2 | → (17,2): horizontal — top of right arm |
| 2 | 17 | 2 | → (12,7): diagonal down-left — left edge of right arm |
| 3 | 12 | 7 | → (11,7): horizontal — inner corner |
| 4 | 11 | 7 | → (9,5): diagonal up-left — inner right edge of left arm |
| 5 |  9 | 5 | → (7,5): horizontal — top of left arm |
| 6 |  7 | 5 | → (10,8): diagonal down-right — outer left edge of left arm |
| 7 | 10 | 8 | → (12,8): horizontal — bottom of checkmark |
| 8 | 12 | 8 | → (18,2): diagonal up-right — right edge of right arm |
| 9 | 18 | 2 | → (19,2): horizontal — closes to start |

**Pixel map on the 26×11 design grid** (`#` = TEXTPEN filled, `.` = transparent):

```
     col:  0         1         2
           0123456789012345678901234 5
row  0:    ..........................
row  1:    ..........................
row  2:    .................##.......
row  3:    ................##........
row  4:    ...............##.........
row  5:    .......###.....##.........
row  6:    ........###...##..........
row  7:    .........#####............
row  8:    ..........###.............
row  9:    ..........................
row 10:    ..........................
```

The checkmark occupies rows 2–8 and columns 7–18. The short **left arm** of the ✓ tip is at the upper-left (x=7–9, y=5); the long **right arm** tip is at the upper-right (x=17–18, y=2). Both arms converge at the **bottom apex** (x=10–12, y=8).

**Scanline fill per row** (derived from polygon edges):

| Row (y) | Filled columns (x) | Notes |
|---------|--------------------|-------|
| 2 | 17–18 | top of right arm (2 px) |
| 3 | 16–17 | right arm, step left |
| 4 | 15–16 | right arm, step left |
| 5 | 7–9, 14–15 | left arm tip + right arm (gap at x=10–13) |
| 6 | 8–10, 13–14 | both arms converging (gap at x=11–12) |
| 7 | 9–13 | arms merge — 5 px wide |
| 8 | 10–12 | bottom apex (3 px) |

**Scale formula** (for `GTCB_Scaled = TRUE` or non-default dimensions):

```c
ScaledCoord = (((srcSize - 1) >> 1) + (dstSize - 1) * coord) / (srcSize - 1)
```

For X: `ScaledX = Scale(26, dstWidth, x)`  
For Y: `ScaledY = Scale(11, dstHeight, y)`

At the default 26×11, this is the identity transform (coordinates are used as-is).

---

### 11.5 VIB_THICK3D Bevel Border Geometry

The image descriptor includes `VIB_THICK3D`, which causes `vectorclass` to call `embossedBoxTrim()` with `JOINS_ANGLED` around the entire image rectangle. This border is drawn in **both** visual states (normal and selected), before the checkmark polygon.

The call (from `vectorclass.c`):
```c
embossedBoxTrim(rport, &box,
    penspec[SHINEPEN], penspec[SHADOWPEN], JOINS_ANGLED);
```

For the default 26×11 box (`left=0, top=0, Width=26, Height=11`, so `right=25, bottom=10`):

| Border element | Pen | Pixels |
|----------------|-----|--------|
| Top row | SHINEPEN | y=0, x=0..24 |
| Left column 0 | SHINEPEN | x=0, y=1..10 |
| Left column 1 | SHINEPEN | x=1, y=1..9 |
| Right column 0 | SHADOWPEN | x=25, y=0..9 |
| Right column 1 | SHADOWPEN | x=24, y=1..9 |
| Bottom row | SHADOWPEN | y=10, x=1..25 |

`JOINS_ANGLED` places the two angled corners at:
- **Top-right**: x=25, y=0 → SHADOWPEN (right col 0 starts at top, SHINE top row ends at x=24)
- **Bottom-left**: x=0, y=10 → SHINEPEN (left col 0 extends to bottom, SHADOW bottom row starts at x=1)

The other two corners are blunt double-pixel SHINE (top-left) and SHADOW (bottom-right).

The **interior** fill area (x=2..23, y=1..9, 22×9 pixels) is cleared to BACKGROUNDPEN before any polygon rendering. The checkmark polygon's coordinates (x=7..18, y=2..8) fall entirely within this interior.

**Border pixel map** (`S`=SHINEPEN, `#`=SHADOWPEN, space=interior):

```
     col:  0         1         2
           0123456789012345678901234 5
row  0:    SSSSSSSSSSSSSSSSSSSSSSSSS#
row  1:    SS                      ##
row  2:    SS                      ##
row  3:    SS                      ##
row  4:    SS                      ##
row  5:    SS                      ##
row  6:    SS                      ##
row  7:    SS                      ##
row  8:    SS                      ##
row  9:    SS                      ##
row 10:    S#########################
```

(At the default 26×11 size the interior is 22 columns wide; row 9's right inner col and row 10's left outer col meet at the angled corners.)

---

### 11.6 Complete Pixel Map — Both States

**Unchecked state (`IDS_NORMAL`)** — border only, interior empty:

```
     col:  0         1         2
           0123456789012345678901234 5
row  0:    SSSSSSSSSSSSSSSSSSSSSSSSS#
row  1:    SS......................##
row  2:    SS......................##
row  3:    SS......................##
row  4:    SS......................##
row  5:    SS......................##
row  6:    SS......................##
row  7:    SS......................##
row  8:    SS......................##
row  9:    SS......................##
row 10:    S#########################
```

Legend: `S`=SHINEPEN, `#`=SHADOWPEN, `.`=BACKGROUNDPEN

**Checked state (`IDS_SELECTED`)** — border + TEXTPEN checkmark:

```
     col:  0         1         2
           0123456789012345678901234 5
row  0:    SSSSSSSSSSSSSSSSSSSSSSSSS#
row  1:    SS......................##
row  2:    SS...............TT.....##
row  3:    SS..............TT......##
row  4:    SS.............TT.......##
row  5:    SS.....TTT.....TT.......##
row  6:    SS......TTT...TT........##
row  7:    SS.......TTTTT..........##
row  8:    SS........TTT...........##
row  9:    SS......................##
row 10:    S#########################
```

Legend: `S`=SHINEPEN, `#`=SHADOWPEN, `.`=BACKGROUNDPEN, `T`=TEXTPEN (checkmark)

---

## 12. Image Caching via VisualInfo

`getSysImage()` maintains a linked list of `ImageLink` nodes in `vi->vi_Images`. Each node stores:
- `il_Image` — pointer to the `sysiclass` image object
- `il_Type` — `CHECKIMAGE`
- `il_Next` — next cached image

On each `getSysImage()` call, the list is searched for a match on `(Width, Height, Type)`. If found, the cached image is returned directly. If not found, a new image is created, prepended to the list, and returned.

**Consequence:** All checkbox gadgets created with the same `VisualInfo` and the same dimensions (typically all unscaled checkboxes at 26×11) share a single `sysiclass` image object. The image is **not** reference-counted; it lives until `FreeVisualInfo()` disposes all images in the list.

---

## 13. Memory Allocation and Lifecycle

### Allocation

Single contiguous `AllocVec(size, MEMF_CLEAR)`:

```
┌────────────────────────────────────┐
│  struct SpecialGadget              │  sizeof(SpecialGadget)
├────────────────────────────────────┤
│  checkbox instance data            │  ALIGNSIZE(CHECKBOX_IDATA_SIZE)
│  (cbid_Checked: WORD)              │
├────────────────────────────────────┤
│  struct IntuiText (label)          │  sizeof(IntuiText)     [optional]
│  font / underline buffer           │  [optional, if GT_Underscore used]
└────────────────────────────────────┘
```

All bytes are zero-initialized by `MEMF_CLEAR`; `cbid_Checked` starts as 0 (unchecked).

### SG_EXTRAFREE_DISPOSE Not Set

Unlike the cycle gadget, no `SG_EXTRAFREE_DISPOSE` flag is set. When `FreeGadgets()` frees a checkbox gadget:
1. `FreeVec(gad)` — frees the contiguous block only.
2. The `sysiclass` image is **not** disposed here; it remains alive (still in the VisualInfo cache).

### Image Lifetime

The `sysiclass` image is freed only when the application calls `FreeVisualInfo(vi)`:
```c
while (il) {
    next = il->il_Next;
    DisposeObject(il->il_Image);   /* disposes each sysiclass image */
    FreeVec(il);
    il = next;
}
```

This must happen **after** `FreeGadgets()` (since the gadget holds a pointer to the image) but **before** the screen is closed (since `DrawInfo` is still needed).

---

## 14. External Label (`ng_GadgetText`)

The descriptive label is stored in the `IntuiText` embedded at the end of the allocation block:

- `IText = ng_GadgetText` (pointer reference, not a copy)
- `FrontPen` = `dri_Pens[TEXTPEN]` or `dri_Pens[HIGHLIGHTTEXTPEN]` (if `NG_HIGHLABEL`)
- `DrawMode = JAM1`
- `ITextFont = ng_TextAttr`

Positioned by `placeGadgetText(gad, ng_Flags, PLACETEXT_LEFT, NULL)`.

For `PLACETEXT_LEFT` (default): the `IntuiText.LeftEdge` will be negative (the label appears to the left of the gadget box). The text is **right-aligned** against the gadget's left edge with an `INTERWIDTH = 8` pixel gap.

For `PLACETEXT_IN`: the text is centered horizontally and vertically within the gadget's own bounding box (rarely used for checkboxes).

`PlaceHelpBox()` expands the gadget's `Bounds` rectangle to encompass the external label position, enabling proper gadget-help hit detection.

---

## 15. GT_Underscore Support

If `GT_Underscore` is specified at creation, an additional `IntuiText` + `TTextAttr` + string buffer is allocated after the label `IntuiText`:

- The underscore-prefix character is stripped from the displayed string.
- The character immediately after the underscore is re-rendered in a second `IntuiText` with `FSF_UNDERLINED` style.
- The two `IntuiText` structures are linked via `NextText`.
- The underlined character is positioned at the correct pixel offset (computed via `IntuiTextLength`).

---

## 16. Scaled Mode (`GTCB_Scaled = TRUE`)

When `GTCB_Scaled` is TRUE (V39+):

- `gad->Width` and `gad->Height` retain the caller's `ng_Width` and `ng_Height`.
- No TopEdge adjustment is performed.
- The `sysiclass` image is created at the specified dimensions, causing it to scale its check-box artwork.
- Typical usage: set `ng->ng_Width = ng->ng_Height = ng->ng_TextAttr->ta_YSize + 1` for a box that matches the current font height.
- Under V36/V37, this tag is silently ignored (backward compatible).

---

## 17. Comparison: Unscaled vs Scaled Mode

| Property | Unscaled (default) | Scaled (`GTCB_Scaled = TRUE`) |
|----------|--------------------|-------------------------------|
| Gadget Width | 26 (`CHECKBOX_WIDTH`) | `ng_Width` |
| Gadget Height | 11 (`CHECKBOX_HEIGHT`) | `ng_Height` |
| TopEdge adjusted | Yes (for LEFT/RIGHT labels, non-8px fonts) | No |
| Image dimensions | Fixed 26×11 | `ng_Width` × `ng_Height` |
| V36/V37 compatible | Yes | Ignored (treated as unscaled) |

---

## 18. Relationship Between `cbid_Checked` and `GFLG_SELECTED`

These two fields are kept strictly in sync:

| Operation | Mechanism |
|-----------|-----------|
| User click | Intuition toggles `GFLG_SELECTED` via `TOGGLESELECT`; `HandleCheckbox` reads it into `cbid_Checked` |
| `GT_SetGadgetAttrsA(GTCB_Checked)` | `SetCheckBoxAttrsA` sets `cbid_Checked`, then calls `SelectGadget()` to update `GFLG_SELECTED` |
| `GT_GetGadgetAttrsA(GTCB_Checked)` | Returns `cbid_Checked` directly from instance data |

The authoritative truth at render time is `GFLG_SELECTED` (read by Intuition to pick the image state). `cbid_Checked` is the GadTools-level cache of that state, used for `GT_GetGadgetAttrsA` and `imsg->Code`.

---

## 19. Implementation Checklist for Clean-Room Reimplementation

- [ ] Allocate contiguous gadget memory block (SpecialGadget + instance data + optional label IntuiText), zero-cleared; `cbid_Checked` starts as 0.
- [ ] In **unscaled mode** (default): force `gad->Width = 26`, `gad->Height = 11`; apply TopEdge adjustment `+= (fontHeight - 7) / 2` only for LEFT/RIGHT/no label placement and only when `ng_GadgetText` is non-NULL.
- [ ] In **scaled mode** (`GTCB_Scaled = TRUE`): keep caller's dimensions; skip TopEdge adjustment.
- [ ] Set `GadgetType |= GADTOOL_TYPE | BOOLGADGET`.
- [ ] Set `Flags |= GADGIMAGE | GADGHIMAGE`.
- [ ] Set `Activation = RELVERIFY | TOGGLESELECT`.
- [ ] Apply initial `GTCB_Checked` and `GA_Disabled` from creation tags via `SetCheckBoxAttrsA`.
- [ ] Place external label at `PLACETEXT_LEFT` by default; extend Bounds to include label via `PlaceHelpBox`.
- [ ] Hook up `sg_EventHandler = HandleCheckbox`, `sg_SetAttrs = SetCheckBoxAttrsA`, `sg_GetTable = Checkbox_GetTable`; do **not** set `SG_EXTRAFREE_DISPOSE`.
- [ ] Obtain (or create) the `sysiclass CHECKIMAGE` image of matching dimensions via the VisualInfo cache; assign to both `GadgetRender` and `SelectRender`.
- [ ] On user click: Intuition has already toggled `GFLG_SELECTED`; handler sets `cbid_Checked = (GFLG_SELECTED != 0)` and `imsg->Code = cbid_Checked`.
- [ ] On `GT_SetGadgetAttrsA(GTCB_Checked)`: set `cbid_Checked`, call `SelectGadget()` to sync `GFLG_SELECTED` and trigger refresh.
- [ ] On `GT_GetGadgetAttrsA(GTCB_Checked)`: return `cbid_Checked` (WORD, sign-extended to LONG).
- [ ] On `GA_Disabled` set/clear: toggle `GFLG_DISABLED` and refresh via `TagAbleGadget`.
- [ ] On free: `FreeVec(gad)` only — do **not** `DisposeObject` the image (it is owned by VisualInfo).
- [ ] The `sysiclass` image is freed only when `FreeVisualInfo()` is called by the application.
