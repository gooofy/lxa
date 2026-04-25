# Amiga GadTools Cycle Gadget — Clean-Room Implementation Specification

**Source version:** gadtools.library V40 (Kickstart 3.1)  
**Also known as:** NWay gadget (`NWAY_KIND`, `CYCLE_KIND = 7`)

---

## 1. Purpose and Concept

The cycle gadget is a button-like widget that allows the user to cycle forward or backward through a fixed list of mutually exclusive string labels. Exactly one label is always "active." Clicking the gadget (without modifier keys) advances to the next label; shift-clicking goes backwards. The gadget reports the newly active item index in the `Code` field of an `IDCMP_GADGETUP` message.

---

## 2. Visual Anatomy

```
+------------------------------------------+
| [cycle-glyph|20px] | [  Active Label  ]  |
|                    |                     |
+------------------------------------------+
```

The gadget body is divided horizontally into two sections separated by a 2-pixel vertical divider at x-offset 20 (measured from the gadget's left edge):

1. **Left section (0–19 px wide):** Contains the cycle arrow glyph.
2. **Divider (x=20 and x=21):** Two vertical lines — shadow + shine — forming a 3D-looking separator.
3. **Right section (22 px to gadget-width−1):** Contains the current active label, centered vertically and horizontally.

The entire gadget is surrounded by a standard button bevel frame (`FRAME_BUTTON` from Intuition's `frameiclass`).

---

## 3. Key Constants

| Symbol | Value | Meaning |
|--------|-------|---------|
| `CYCLE_KIND` | 7 | Gadget kind identifier |
| `CYCLEGLYPHWIDTH` | 20 | Width of cycle-glyph area (pixels) |
| `CB_COUNT` | 16 | Number of (x,y) vertex pairs in the glyph polygon |
| `LEFTTRIM` | 4 | Minimum trim inside border (left) |
| `LRTRIM` | 8 | Total left+right trim |
| `TOPTRIM` | 2 | Minimum trim inside border (top) |
| `TBTRIM` | 4 | Total top+bottom trim |
| `BEVELXSIZE` | 2 | Horizontal bevel thickness |
| `BEVELYSIZE` | 1 | Vertical bevel thickness |
| `INTERWIDTH` | 8 | Suggested inter-element horizontal gap |
| `INTERHEIGHT` | 4 | Suggested inter-element vertical gap |
| `DESIGNSHINE` | 1 | Border `BackPen` sentinel: render with SHINEPEN |
| `DESIGNSHADOW` | 2 | Border `BackPen` sentinel: render with SHADOWPEN |
| `DESIGNTEXT` | 3 | Border `BackPen` sentinel: render with TEXTPEN |

---

## 4. IDCMP Flags Required

```c
#define CYCLEIDCMP  (IDCMP_GADGETUP)
```

The application window must have `IDCMP_GADGETUP` set in its IDCMP flags to receive cycle gadget events.

---

## 5. Data Structures

### 5.1 Public: `struct NewGadget`

```c
struct NewGadget {
    WORD    ng_LeftEdge, ng_TopEdge;   /* gadget position */
    WORD    ng_Width,    ng_Height;    /* gadget size */
    UBYTE  *ng_GadgetText;            /* external label (placed to the left by default) */
    struct TextAttr *ng_TextAttr;     /* font for all text in the gadget */
    UWORD   ng_GadgetID;
    ULONG   ng_Flags;                 /* PLACETEXT_* flags; see §6 */
    APTR    ng_VisualInfo;            /* result of GetVisualInfoA() */
    APTR    ng_UserData;
};
```

### 5.2 Private: `struct XCycle` (instance data)

```c
struct CycleBorder {
    struct Border cb_Border;          /* base Border structure */
    WORD          cb_Points[32];      /* CB_COUNT*2 = 32 words (16 x,y pairs) */
};

struct XCycle {
    struct SpecialGadget cyid_Gadget;     /* base: ExtGadget + event/set/get hooks */
    /* --- instance data below --- */
    STRPTR          *cyid_Labels;         /* NULL-terminated array of choice strings */
    UWORD            cyid_MaxLabel;       /* index of last valid label (count - 1) */
    WORD             cyid_Active;         /* index of currently displayed label */
    struct IntuiText cyid_IText;          /* IntuiText for the active label */
    struct CycleBorder cyid_CycleBorder;  /* cycle-arrow polygon Border */
    struct Border    cyid_DarkStroke;     /* shadow vertical divider line */
    struct Border    cyid_LightStroke;    /* shine vertical divider line */
    WORD             cyid_StrokePoints[4];/* two (x,y) pairs for divider lines */
};
```

The `SpecialGadget` wraps an `ExtGadget` and adds:

```c
struct SpecialGadget {
    struct ExtGadget sg_Gadget;
    struct ExtGadget *sg_Parent;
    BOOL  (*sg_EventHandler)(struct ExtGadget *, struct IntuiMessage *);
    void  (*sg_Refresh)     (struct ExtGadget *, struct Window *, BOOL);
    void  (*sg_SetAttrs)    (struct ExtGadget *, struct Window *, struct TagItem *);
    struct GetTable *sg_GetTable;
    ULONG sg_Flags;               /* SG_EXTRAFREE_DISPOSE = 8 for cycle gadgets */
};
```

---

## 6. Tag Reference

### 6.1 Creation Tags (`CreateGadget(CYCLE_KIND, ...)`)

| Tag | Type | Default | Notes |
|-----|------|---------|-------|
| `GTCY_Labels` | `STRPTR *` | **required** | NULL-terminated string array; creation fails if absent or if `labels[0]` is NULL |
| `GTCY_Active` | `UWORD` | `0` | Initial active index; silently clamped to `MaxLabel` if out of range |
| `GA_Disabled` | `BOOL` | `FALSE` | If TRUE, gadget renders ghosted/disabled |

### 6.2 Set Tags (`GT_SetGadgetAttrsA(...)`)

Same tags as creation. Additionally:

- `GTCY_Labels = NULL` → label array unchanged (old array preserved).
- `GTCY_Labels = non-NULL` → replaces label array; `labels[0]` must not be NULL.

### 6.3 Get Tags (`GT_GetGadgetAttrsA(...)`)

| Tag | Return type | Notes |
|-----|-------------|-------|
| `GTCY_Labels` | `STRPTR *` | Pointer to the current label array |
| `GTCY_Active` | `UWORD` | Current active index (sign-extended to LONG by get mechanism) |
| `GA_Disabled` | `BOOL` | TRUE if gadget is disabled |

### 6.4 `ng_Flags` Placement Flags

Control where `ng_GadgetText` (the external descriptive label) is placed:

| Flag | Value | Placement |
|------|-------|-----------|
| `PLACETEXT_LEFT` | `0x0001` | Right-aligned to the left of the gadget (**default**) |
| `PLACETEXT_RIGHT` | `0x0002` | Left-aligned to the right of the gadget |
| `PLACETEXT_ABOVE` | `0x0004` | Centered above the gadget |
| `PLACETEXT_BELOW` | `0x0008` | Centered below the gadget |
| `PLACETEXT_IN` | `0x0010` | Centered on/inside the gadget |
| `NG_HIGHLABEL` | `0x0020` | Render external label in HIGHLIGHTTEXTPEN instead of TEXTPEN |

If no placement flag is set in `ng_Flags`, `PLACETEXT_LEFT` is used.

---

## 7. Creation Sequence

### Step 1: Allocate base gadget

`CreateGenericBase()` calls `CreateGadget(GENERIC_KIND, ...)` with the extra size `CYCLE_IDATA_SIZE = sizeof(XCycle) - sizeof(SpecialGadget)`.

The allocation is one contiguous `AllocVec(..., MEMF_CLEAR)` block of:

```
sizeof(SpecialGadget) + ALIGNSIZE(CYCLE_IDATA_SIZE) + [optional label IntuiText memory]
```

Fields set on the base `ExtGadget`:
- `LeftEdge`, `TopEdge`, `Width`, `Height` from `ng`
- `GadgetID`, `UserData` from `ng`
- `GadgetType = GADTOOL_TYPE (0x0100)`
- `BoundsLeftEdge/TopEdge/Width/Height` = same as position/size
- `MoreFlags = GMORE_BOUNDS | GMORE_GADGETHELP`
- `Flags = GFLG_EXTENDED`

If `ng_GadgetText` is non-NULL, an `IntuiText` struct is allocated immediately after the instance data, with:
- `IText = ng_GadgetText` (pointer reference, not a copy)
- `FrontPen = dri_Pens[TEXTPEN]` (or `HIGHLIGHTTEXTPEN` if `NG_HIGHLABEL` set)
- `DrawMode = JAM1`
- `ITextFont = ng_TextAttr`

### Step 2: Set cycle-specific gadget flags

```c
gad->Flags      |= GADGIMAGE | GADGHIMAGE;
gad->Activation  = RELVERIFY;
gad->GadgetType |= BOOLGADGET;
```

### Step 3: Hook up function pointers

```c
SGAD(gad)->sg_EventHandler = HandleCycle;
SGAD(gad)->sg_SetAttrs     = SetCycleAttrsA;
SGAD(gad)->sg_GetTable     = Cycle_GetTable;
SGAD(gad)->sg_Flags        = SG_EXTRAFREE_DISPOSE;
```

### Step 4: Place external gadget text

```c
placeGadgetText(gad, ng->ng_Flags, PLACETEXT_LEFT, NULL);
```

This computes `LeftEdge`/`TopEdge` offsets on `gad->GadgetText` so it appears at the right location relative to the gadget. Also calls `PlaceHelpBox()` to extend the gadget's `Bounds` rectangle to encompass the label.

### Step 5: Set up label IntuiText

```c
CYID(gad)->cyid_IText.DrawMode  = JAM1;
CYID(gad)->cyid_IText.ITextFont = ng->ng_TextAttr;
```

The `FrontPen` of `cyid_IText` is set at render time by `GTButtonIClass`.

### Step 6: Build the cycle glyph border chain

The three borders are chained: `cyid_CycleBorder` → `cyid_DarkStroke` → `cyid_LightStroke` (NextBorder = NULL).

Call `MakeCycleBorder()` to fill in the polygon for `cyid_CycleBorder`. See §8 for exact coordinates.

Fill in `cyid_DarkStroke`:
```
LeftEdge = CYCLEGLYPHWIDTH (20)
TopEdge  = 2
BackPen  = DESIGNSHADOW
Count    = 2
XY       = cyid_StrokePoints    (zero-initialized)
XY[3]    = gad->Height - 5      (y-coordinate of second point)
```
Because the memory was zero-cleared, `XY[0..2]` are all 0, so the two stroke points are `(0, 0)` and `(0, gad->Height - 5)`, positioned relative to `(LeftEdge=20, TopEdge=2)`.

Fill in `cyid_LightStroke` as a copy of `cyid_DarkStroke`, then:
```
LeftEdge = CYCLEGLYPHWIDTH + 1 (21)
BackPen  = DESIGNSHINE
NextBorder = NULL
```

### Step 7: Create the GTButtonIClass image object

```c
gad->SelectRender = gad->GadgetRender = NewObject(
    GadToolsBase->gtb_GTButtonIClass, NULL,
    IA_Width,     gad->Width,
    IA_Height,    gad->Height,
    IA_Data,      &CYID(gad)->cyid_CycleBorder,  /* border list head */
    IA_FrameType, FRAME_BUTTON,
    TAG_DONE
);
```

Both `GadgetRender` and `SelectRender` point to the same object; state selection is handled inside the image class by examining `imp_State`.

### Step 8: Apply creation tags and initial text

```c
SetCycleAttrsA(gad, NULL, taglist);   /* sets cyid_Labels, cyid_Active, cyid_MaxLabel */
```

Fails (creation returns NULL) if `cyid_Labels` is still NULL after this call.

---

## 8. Cycle Arrow Glyph — Exact Pixel Coordinates

The glyph is constructed by `MakeCycleBorder()`.

### Glyph Height Calculation

```c
LONG height = max(gadHeight - 5, 9);
```

The glyph is always at least 9 pixels tall, regardless of gadget size.

### Glyph Placement (relative to gadget origin)

```c
cb_Border.LeftEdge = LEFTTRIM + 2;               /* = 6 */
cb_Border.TopEdge  = (gadHeight - height) / 2;   /* vertically centred */
cb_Border.BackPen  = DESIGNTEXT;                 /* TEXTPEN / FILLTEXTPEN */
cb_Border.Count    = CB_COUNT;                   /* 16 vertices */
```

### Polygon Vertices (x, y pairs, relative to LeftEdge/TopEdge of the glyph)

With `height = max(gadHeight - 5, 9)`:

| Index | x | y | Notes |
|-------|---|---|-------|
| 0 | 7 | 0 | Arrow top centre |
| 1 | 7 | 5 | Arrow stem bottom |
| 2 | 5 | 3 | Left barb of arrowhead |
| 3 | 10 | 3 | Horizontal arrow span (right tip) |
| 4 | 8 | 5 | Right shoulder |
| 5 | 8 | 1 | Right side going back up |
| 6 | 7 | 0 | Back to arrow top (closes arrow arc) |
| 7 | 1 | 0 | Top-left of box top edge |
| 8 | 0 | 1 | Top-left beveled corner |
| 9 | 0 | height−2 | Bottom-left start of corner |
| 10 | 1 | height−1 | Bottom-left beveled corner |
| 11 | 1 | 1 | Inner left — going back up (double-thickness left border) |
| 12 | 1 | height−1 | Inner left — going back down |
| 13 | 7 | height−1 | Bottom edge rightward |
| 14 | 7 | height−2 | Small right-side fragment |
| 15 | 8 | height−2 | End of glyph outline |

`DrawBorder` connects these 16 vertices as a continuous polyline (15 line segments total).

### Visual Interpretation of the Glyph

The polyline draws two interleaved shapes:

**Cycle arrow (vertices 0–6):**
- A downward-pointing arc with a horizontal crossbar resembling a ↻ rotation indicator:
  - Stem descends from (7,0) to (7,5)
  - Arrowhead: left barb at (5,3), horizontal body extends to (10,3), right shoulder at (8,5)
  - Right side of arc returns to (8,1) then closes at (7,0)

**Box outline (vertices 6–15):**
- Top edge rightward: (7,0)→(1,0) (shares top with arrow)
- Beveled top-left corner: (1,0)→(0,1)
- Full left edge: (0,1)→(0,height−2)
- Beveled bottom-left corner: (0,height−2)→(1,height−1)
- Double-drawn left inner border: (1,height−1)→(1,1)→(1,height−1)
- Bottom edge: (1,height−1)→(7,height−1)
- Partial right side: (7,height−1)→(7,height−2)→(8,height−2)

**Effective bounding box of glyph polygon:**  
x: 0–10 (11 pixels), y: 0–(height−1)  
The glyph occupies only the left portion of the 20-pixel-wide `CYCLEGLYPHWIDTH` column.

---

## 9. Vertical Divider Strokes

Two borders separate the glyph area from the label area. Both are stored inside the `XCycle` instance data and linked into the glyph border chain.

### `cyid_DarkStroke` (shadow line)

| Field | Value |
|-------|-------|
| `LeftEdge` | 20 (`CYCLEGLYPHWIDTH`) |
| `TopEdge` | 2 |
| `BackPen` | `DESIGNSHADOW` → SHADOWPEN (swapped to SHINEPEN when selected) |
| `Count` | 2 |
| XY point 0 | (0, 0) relative to (LeftEdge, TopEdge) |
| XY point 1 | (0, gadHeight − 5) relative to (LeftEdge, TopEdge) |

Absolute coordinates: vertical line from **(20, 2)** to **(20, gadHeight − 3)**.

### `cyid_LightStroke` (shine line)

Identical to `cyid_DarkStroke` except:

| Field | Value |
|-------|-------|
| `LeftEdge` | 21 (`CYCLEGLYPHWIDTH + 1`) |
| `BackPen` | `DESIGNSHINE` → SHINEPEN (swapped to SHADOWPEN when selected) |

Absolute coordinates: vertical line from **(21, 2)** to **(21, gadHeight − 3)**.

Together these two lines give the divider a pressed-bevel appearance (shadow on the left, shine on the right).

---

## 10. Label Text Placement

The active choice label is rendered via `cyid_IText`:

```c
struct IntuiText cyid_IText = {
    .DrawMode  = JAM1,
    .ITextFont = ng->ng_TextAttr,
    /* FrontPen set at render time: TEXTPEN (normal), FILLTEXTPEN (selected) */
};
```

### Label Area Rectangle

```
MinX = CYCLEGLYPHWIDTH  (20)
MinY = 0
MaxX = gad->Width - 1
MaxY = gad->Height - 1
```

### Text Placement Algorithm

On each `UpdateCycle()` call (triggered by creation or user interaction):

1. `cyid_IText.IText = cyid_Labels[cyid_Active]` — point to the new string.
2. Reset `cyid_IText.LeftEdge = 0` and `cyid_IText.TopEdge = 0`.
3. Call `PlaceIntuiText(&cyid_IText, &rect, PLACETEXT_IN)`:
   - **Vertical centering:**  
     `TopEdge = (rect.MaxY + rect.MinY + 2 − textHeight) >> 1`
   - **Horizontal centering:**  
     `LeftEdge = (rect.MaxX + rect.MinX + 2 − textWidth) >> 1`
   
   Where `textWidth`/`textHeight` are computed by `TextExtent()` using the gadget's font.

4. Pass updated `cyid_IText` to the image object:
   ```c
   SetAttrs(gad->GadgetRender, MYIA_IText, &cyid_IText, TAG_DONE);
   ```

`MYIA_IText` is a private tag alias for `IA_APattern`, used to store the IntuiText pointer inside the `GTButtonIClass` local object data.

---

## 11. Rendering Pipeline

### 11.1 Image Class Hierarchy

```
GTButtonIClass  (private subclass of frameiclass)
    └── frameiclass  (Intuition built-in)
```

The cycle gadget image object is created with `IA_FrameType = FRAME_BUTTON`, which produces the standard raised button bevel. `GTButtonIClass` extends the draw method as follows:

### 11.2 Draw Method (`IM_DRAW` / `IM_DRAWFRAME`)

1. **Call superclass** (`frameiclass`) to render the button bevel frame.

2. **Resolve pen colors** from `DrawInfo` (or fall back to pen 1 = text, pen 2 = shine, pen 1 = shadow if `DrawInfo` absent):
   ```
   penText     = dri_Pens[TEXTPEN]
   penShine    = dri_Pens[SHINEPEN]
   penShadow   = dri_Pens[SHADOWPEN]
   penFillText = dri_Pens[FILLTEXTPEN]
   ```

3. **Determine selected state:**
   ```c
   selected = (imp_State == IDS_SELECTED || imp_State == IDS_INACTIVESELECTED);
   ```

4. **Set design pen assignments:**

   | State | `DESIGNSHINE` maps to | `DESIGNSHADOW` maps to | `DESIGNTEXT` maps to |
   |-------|-----------------------|------------------------|----------------------|
   | Normal | SHINEPEN | SHADOWPEN | TEXTPEN |
   | Selected | SHADOWPEN | SHINEPEN | FILLTEXTPEN |

5. **Walk the border chain** (`IA_Data` = `&cyid_CycleBorder`):
   - For each `struct Border` in the chain:
     - If `Border->BackPen == DESIGNSHINE`: set `Border->FrontPen = designShine`
     - If `Border->BackPen == DESIGNSHADOW`: set `Border->FrontPen = designShadow`
     - If `Border->BackPen == DESIGNTEXT`: set `Border->FrontPen = designText`
     - Otherwise: leave `FrontPen` unchanged.
   - Call `DrawBorder(rport, (Border *)IM(o)->ImageData, box.Left, box.Top)`.

6. **Print label text** (if `lod_IntuiText` is set):
   - Set `itext->FrontPen = penText` (or `penFillText` if selected).
   - Call `PrintIText(rport, itext, box.Left, box.Top)`.

### 11.3 Coordinate System

All Border and IntuiText coordinates are **relative to the gadget's upper-left corner** (the `box.Left, box.Top` origin that `DrawBorder` / `PrintIText` receive). Gadget-relative means 0,0 is the top-left pixel of the gadget box.

### 11.4 Disabled State

When `GFLG_DISABLED` is set on the gadget, Intuition renders it with the appropriate ghost overlay. GadTools does not add extra disabled rendering beyond what Intuition provides for `BOOLGADGET` with `GADGHIMAGE`/`GADGIMAGE`.

---

## 12. Event Handling

The cycle gadget sets `Activation = RELVERIFY`, making it a standard boolean gadget that fires on mouse button release while the pointer is inside the gadget boundaries.

### 12.1 HandleCycle

Called by the GadTools dispatcher on `IDCMP_GADGETUP`:

```c
BOOL HandleCycle(struct ExtGadget *gad, struct IntuiMessage *imsg) {
    if (imsg->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) {
        /* Cycle backward */
        if (cyid_Active-- == 0)
            cyid_Active = cyid_MaxLabel;
    } else {
        /* Cycle forward */
        if (cyid_Active++ == cyid_MaxLabel)
            cyid_Active = 0;
    }
    UpdateCycle(gad, imsg->IDCMPWindow);
    imsg->Code = cyid_Active;   /* set return code before delivery */
    return TRUE;                /* deliver message to application */
}
```

### 12.2 Message Delivered to Application

The `IntuiMessage` received by the application via `GT_GetIMsg()`:

| Field | Value |
|-------|-------|
| `Class` | `IDCMP_GADGETUP` |
| `Code` | Index of newly active item (0-based) |
| `IAddress` | Pointer to the cycle gadget (`struct Gadget *`) |
| `Qualifier` | Keyboard qualifier at the time of release |

### 12.3 Cycling Behavior Summary

| Input | Behaviour | Wrap |
|-------|-----------|------|
| Click (no modifier) | Advance to next item (`cyid_Active++`) | Last → 0 |
| Shift+Click | Go to previous item (`cyid_Active--`) | 0 → Last |

---

## 13. UpdateCycle — Gadget Update Procedure

Called after every change to `cyid_Active` (including at creation time):

```
1. removeGadget(win, gad)          — detach from window while modifying
2. cyid_IText.IText = cyid_Labels[cyid_Active]
3. cyid_IText.LeftEdge = 0
4. cyid_IText.TopEdge  = 0
5. PlaceIntuiText(&cyid_IText, &rect, PLACETEXT_IN)   — re-centre in label area
6. SetAttrs(gad->GadgetRender, MYIA_IText, &cyid_IText, TAG_DONE)
7. AddRefreshGadget(gad, win, pos) — re-attach and refresh
```

If `win` is NULL (called from creation or `GT_SetGadgetAttrsA` with `win=NULL`), steps 1 and 7 are no-ops.

---

## 14. SetCycleAttrsA — Attribute Update

Called at creation time and via `GT_SetGadgetAttrsA`:

1. Extract `GTCY_Active` (defaults to existing `cyid_Active` if tag absent).
2. Extract `GTCY_Labels`:
   - If non-NULL and `labels[0]` != NULL: adopt the new labels array and recompute `cyid_MaxLabel` by walking to the first NULL entry.
   - If NULL: keep existing label array unchanged.
3. Clamp `cyid_Active = min(newactive, cyid_MaxLabel)`.
4. Call `UpdateCycle(gad, win)` to redraw.
5. Call `TagAbleGadget(gad, win, taglist)` to handle `GA_Disabled` if present.

### cyid_MaxLabel Computation

```c
for (cyid_MaxLabel = 0;
     cyid_Labels[cyid_MaxLabel + 1];
     cyid_MaxLabel++)
    ;
```

`cyid_MaxLabel` is the index of the last non-NULL label (= label count − 1).

---

## 15. Memory Allocation and Lifecycle

### Allocation

Single contiguous `AllocVec(size, MEMF_CLEAR)`:

```
┌────────────────────────────────────┐
│  struct SpecialGadget              │  sizeof(SpecialGadget)
├────────────────────────────────────┤
│  cycle instance data (XCycle tail) │  ALIGNSIZE(CYCLE_IDATA_SIZE)
├────────────────────────────────────┤
│  struct IntuiText (if label text)  │  sizeof(IntuiText)  [optional]
│  font / underline buffer           │  [optional, if GT_Underscore used]
└────────────────────────────────────┘
```

All bytes are zero-initialized by `MEMF_CLEAR`, which is why `cyid_StrokePoints[0..2]` are zero without explicit assignment.

### Image Object

The `GTButtonIClass` image object is separately allocated via `NewObject()`. The `SG_EXTRAFREE_DISPOSE` flag on `sg_Flags` ensures `DisposeObject(gad->GadgetRender)` is called when `FreeGadgets()` frees the gadget.

### Destruction

`FreeGadgets()` walks the gadget list and for any gadget with `GADTOOL_TYPE`:
1. If `SG_EXTRAFREE_DISPOSE`: call `DisposeObject(gad->GadgetRender)`.
2. Call `FreeVec(gad)` to free the contiguous block.

The `cyid_Labels` array is **not** owned by the gadget; the application retains ownership.

---

## 16. External Label (ng_GadgetText)

The descriptive label to the left (or at another position) of the gadget is stored in a separately embedded `IntuiText` at the end of the allocation block. At creation:

- Rendered in `TEXTPEN` (or `HIGHLIGHTTEXTPEN` if `NG_HIGHLABEL` is set in `ng_Flags`).
- `DrawMode = JAM1`.
- `ITextFont = ng_TextAttr`.
- Positioned by `placeGadgetText(gad, ng_Flags, PLACETEXT_LEFT, NULL)`.
- `LeftEdge`/`TopEdge` are gadget-relative (may be negative if label is to the left).
- The gadget's `BoundsLeftEdge/Width/Height` are expanded by `PlaceHelpBox()` to encompass the label area (for gadget help hit-testing).

---

## 17. GT_Underscore Support

If the `GT_Underscore` tag is provided at creation:

- An additional `IntuiText` + `TTextAttr` + string buffer is allocated after the main label `IntuiText`.
- The underscore character is stripped from the displayed string.
- The character following the underscore is rendered with `FSF_UNDERLINED` style in a second `IntuiText` (`NextText`).
- The underlined portion is rendered at the correct pixel offset computed by `IntuiTextLength`.

---

## 18. GetTable (GT_GetGadgetAttrsA)

```c
struct GetTable Cycle_GetTable[] = {
    { GTCY_Labels - GT_TagBase, LONG_ATTR | ATTR_OFFSET(CYID, cyid_Labels) },
    { GTCY_Active - GT_TagBase, WORD_ATTR | ATTR_OFFSET(CYID, cyid_Active) },
    { ATTR_DISABLED, 0 }  /* terminates table; enables GA_Disabled query */
};
```

- `GTCY_Labels` returns a 32-bit pointer to the current label array.
- `GTCY_Active` returns the 16-bit active index, sign-extended to a LONG for storage.
- `GA_Disabled` checks `gad->Flags & GFLG_DISABLED`.

---

## 19. Pen Color Summary

| Gadget state | Glyph drawn with DESIGNTEXT | Glyph drawn with DESIGNSHINE | Glyph drawn with DESIGNSHADOW |
|--------------|---------------------------|------------------------------|-------------------------------|
| Normal | TEXTPEN | SHINEPEN | SHADOWPEN |
| Selected/Active | FILLTEXTPEN | SHADOWPEN | SHINEPEN |

The label `IntuiText` FrontPen mirrors the DESIGNTEXT assignment.

---

## 20. Implementation Checklist for Clean-Room Reimplementation

- [ ] Allocate contiguous gadget memory block (SpecialGadget + instance data + optional label IntuiText), zero-cleared.
- [ ] Set `GadgetType |= GADTOOL_TYPE | BOOLGADGET`.
- [ ] Set `Flags |= GADGIMAGE | GADGHIMAGE`.
- [ ] Set `Activation = RELVERIFY`.
- [ ] Place external label (`ng_GadgetText`) at `PLACETEXT_LEFT` by default; extend Bounds to include it.
- [ ] Build cycle glyph polygon per §8 with exact point coordinates; position it at `LeftEdge = LEFTTRIM + 2`, vertically centred, with `BackPen = DESIGNTEXT`.
- [ ] Build two vertical divider Borders: dark at x=20, light at x=21, from y=2 to y=(gadHeight−3), using DESIGNSHADOW and DESIGNSHINE respectively.
- [ ] Chain all three Borders: glyph → dark stroke → light stroke (NextBorder = NULL at end).
- [ ] Create `frameiclass`-derived image object (`FRAME_BUTTON`) using the border chain as `IA_Data`.
- [ ] Assign both `GadgetRender` and `SelectRender` to this image.
- [ ] At render time: swap SHINEPEN/SHADOWPEN when selected; use FILLTEXTPEN for text when selected.
- [ ] Center active label text in the rectangle `[CYCLEGLYPHWIDTH, 0, Width−1, Height−1]`.
- [ ] On click: advance label index; on shift-click: retreat; wrap at both ends.
- [ ] Set `imsg->Code` = new active index before delivering `IDCMP_GADGETUP` to the application.
- [ ] On `UpdateCycle`: removeGadget → update text → re-centre text → SetAttrs on image → AddRefreshGadget.
- [ ] On free: `DisposeObject` the image, then `FreeVec` the gadget block.
- [ ] Clamp `cyid_Active` to `[0, cyid_MaxLabel]` when setting attributes.
- [ ] Reject creation if `cyid_Labels` is NULL or `labels[0]` is NULL.
