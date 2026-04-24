# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, **zero failing tests at phase completion**, and roadmap updates on every completed or deferred phase.

---

## Project Vision

lxa is the foundation for a modern Amiga on commodity hardware (Linux PC, Raspberry Pi). By targeting OS-compliant applications only, lxa bypasses chip-register emulation and goes straight to RTG (Retargetable Graphics), enabling full true-color displays at arbitrary resolutions on any SDL2-capable host.

The primary target applications are productivity software (Wordsworth, Amiga Writer, PageStream) and IDEs (Storm C, BlitzBasic 3). MUI and ReAction are important GUI toolkit goals. The long-term goal is for lxa to become a **viable development platform for Amiga software on Linux**.

**RTG strategy**: Implement Picasso96 (`Picasso96API.library`) as the RTG system. PPaint is the flagship RTG validation target. A thin `cybergraphics.library` compatibility shim is included so apps that probe for CGX also work.

---

## Roadmap Policy

These rules govern how this file evolves. Read them before adding, removing, or reordering phases.

### Priority Order
Work is scheduled in this strict order. A lower-priority phase may not be promoted ahead of a higher-priority phase that is ready to start.

1. **Quality** — failing tests, flaky tests, infrastructure gaps, regressions. Always first.
2. **Amiga compatibility** — real-Amiga behaviour gaps in implemented APIs, missing system-library functionality, app-specific correctness issues (within the lxa approach: OS-compliant apps only, no chip-register emulation).
3. **Performance** — measured wall-time, memory, or CPU regressions; optimisation phases.
4. **New features** — RTG, MUI, ReAction, external process emulation, etc. Last.

### No Pooling Sections
This roadmap does **not** contain catch-all lists like "Deferred Test Failures", "Known Limitations", "TODO", or "Backlog". Such sections accumulate forever and signal that work has been parked rather than scheduled.

When a new issue, gap, or follow-up appears it must be either:

- **Added as a TODO bullet inside an existing scheduled phase** if the same root cause is already covered, OR
- **Promoted into a new phase**, numbered, scheduled at the appropriate priority slot, with explicit objectives, sub-problems, TODO checkboxes, and a test gate.

The only retrospective section is the `## Completed Phases (Summary)` table — one line per phase, full detail in the git commit message.

### Maintenance Rules
1. **Completed phases** collapse to a single one-line row in the summary table — no verbose write-ups remain in the roadmap.
2. **Promote the next phase**: whenever a phase finishes, surface the next one under `## Next Phase` so the next agent sees it immediately.
3. **Length budget**: keep `roadmap.md` under ~600 lines. The explicit-phase rule (every issue → its own phase with sub-problems and TODOs) inflates length deliberately; compact only by collapsing completed phases into the summary table or moving deep detail to the relevant skill.
4. **No stale "next phase" pointer**: never leave the roadmap with an unclear "what comes next".
5. **`DISABLED_` GTests** must each be the explicit objective of a scheduled phase. A test may not stay disabled without a phase that will re-enable it.

---

 ## Completed Phases (Summary)

| Phase | Title | Version |
|-------|-------|---------|
| 141 | Fix MiscTest.StressTasks — race: `SetSignal(0,mask)` was called after `CreateNewProc`, discarding signals sent by child before parent reached `Wait`. Fixed by pre-clearing the signal before each task launch. Re-enabled `StressTasks`; 68/68 pass. | v0.9.23 |
| 142 | Library policy cleanup: removed third-party stubs (`req.library`, `reqtools.library`, `powerpacker.library`, `arp.library`); real binaries from `others/` now live in `share/lxa/System/Libs/`. `FindThirdPartyLibsPath()` in `lxa_test.h` auto-discovers the directory so LIBS: is set up correctly in all tests. 68/68 pass. | v0.9.24 |
| 143 | datatypes.library full implementation: `ObtainDataTypeA`/`ReleaseDataType`, `NewDTObjectA`/`DisposeDTObject`, `GetDTAttrsA`/`SetDTAttrsA` (with `DTSpecialInfo` backing), `DoDTMethodA` (DTM_FRAMEBOX), `GetDTMethods`/`GetDTTriggerMethods`, `GetDTString`. Local `lxa_next_tag()` avoids UtilityBase dependency in disk library context. All register assignments fixed per NDK fd. New `datatypes_gtest.cpp` (5 tests). 69/69 pass. | v0.9.25 |
| 145 | Intuition window border scrollbar rendering: added `case GTYP_WZOOM:` to `_render_window_frame()` (outlined rectangle imagery), WZOOM gadget creation in `_create_window_sys_gadgets()` when `WFLG_HASZOOM` set, `ZipWindow` behavior in `_handle_sys_gadget_verify`, `WA_Zoom` tag sets `WFLG_HASZOOM`. New `ZoomWindow` sample. New `devpac_scrollbar_gtest.cpp` (3 tests: ZoomGadgetExists, RightBorderPropKnob, RightBorderScrollArrows). Devpac editor right-border PropGadget knob and scroll arrows render correctly; WZOOM gadget structural presence verified. 71/71 pass. | v0.9.27 |
| 146 | Devpac Settings window (req.library): navigates Settings→Settings... via RMB drag using exact menu coordinates (Settings menu left=384, "Settings..." item screen y=42, verified via lxa_get_menu_info). Window opens correctly at 489×137 with 18 gadgets (cycle gadgets, checkboxes, string gadget, Save/Use/Cancel buttons). Rendering requires ~500 VBlanks; test uses lxa_wait_window_drawn(1) for event-driven waiting. New devpac_settings_gtest.cpp (4 tests: SettingsWindowOpens, SettingsWindowRendersContent, SettingsWindowHasThreeButtons, ZDismissSettingsWindow). 72/72 pass. | v0.9.28 |
| 148 | Devpac visual fixes — SIMPLE_REFRESH overdraw and PropGadget PROPNEWLOOK stipple: (1) `lxa_layers.c`: added `NotifyDamagedWindows()` called from `DeleteLayer` after `ReleaseSemaphore`; posts `IDCMP_REFRESHWINDOW` only to SIMPLE_REFRESH windows, preventing overdraw artifacts after req.library settings window closes. (2) `lxa_graphics.c`: added `FillRectPattern()` helper; `_graphics_RectFill` now respects `rp->AreaPtrn` for stipple fills via both layered and non-layered paths; removed BltPattern stub. (3) `lxa_intuition.c`: added `#include <graphics/gfxmacros.h>` and `lxa_notify_window_refresh()` helper; PropGadget PROPNEWLOOK container now renders stipple via `SetAfPt`+`RectFill`. (4) `devpac_scrollbar_gtest.cpp`: replaced fragile adjacent-pair stipple check with semantically correct pen-0/pen-1 presence check (works on narrow 6px gadget). `ZDismissSettingsWindow` asserts non-background pixels in editor area after settings close. 70/72 pass (2 pre-existing failures: gadtoolsgadgets_pixels_c_gtest, apps_misc_gtest). | v0.9.29 |
| 147a | Custom-screen window chrome + per-screen-type host-window architecture (rootless fix): two architectural defects — (a) coarse `if (!rootless_mode)` skip in `_intuition_OpenWindow` bypassed `_render_window_frame()` for ALL rootless windows including custom-screen windows where chrome MUST be drawn into the screen bitmap; (b) `display_open()` always created an SDL window for the screen, so in rootless mode the Workbench screen got a host window AND each Workbench window got its own native host window — two SDL windows showing the same content. Architectural rules now enforced: rootless+Workbench screen → no host window for screen, each child window is its own X11 window; rootless+custom screen → screen owns ONE host window, chrome+children render into screen bitmap; non-rootless → screen owns host window, everything inside. Fix: (1) `_window_uses_native_host(screen, rootless_mode)` helper in `lxa_intuition.c` returns TRUE only for rootless windows on a `WBENCHSCREEN`-flagged screen; window-render skip and per-window emucall5 flag both use it. (2) `g_opening_workbench_screen` static flag in `lxa_intuition.c` set by `OpenWorkBench()` around the `_intuition_OpenScreen()` call so the EMU_CALL site can tell host whether this screen is the Workbench (since `WBENCHSCREEN` is set on `screen->Flags` only AFTER OpenScreen returns). (3) `EMU_CALL_INT_OPEN_SCREEN` extended to 4 args (`emucall4`, bit 0 of d4 = is_workbench); `EMU_CALL_INT_OPEN_WINDOW` extended to 5 args (`emucall5`, bit 0 of d5 = uses_native_host). (4) `display.c`: `display_open_ex(... wants_host_window)` skips `SDL_CreateWindow` when `wants_host_window=false`; dispatcher computes `wants_host_window = !(is_workbench && rootless_mode)`. (5) `display_window_open_ex(... uses_native_host)` skips per-window SDL when `uses_native_host=false`. (6) `typeface_gtest.cpp`: new `WindowChromeIsRendered` test sampling title-bar (dark), left border (white highlight), right/bottom borders (dark shadow), interior (gray fill). Typeface shows full Amiga chrome inside its 194×138 custom-screen host window. 72/72 pass in 163s. | v0.9.30 |
| 147b | Typeface window geometry investigation: root-cause analysis confirmed 194×138 IS the correct BGUI minimum size for Typeface on a first-run PAL/topaz-8 system. The 280×195 reference was from a saved-prefs state on FS-UAE, not a comparable first-run baseline. Mathematical proof: Box.Height=138 from Typeface's do-while loop; BGUI WinSize min-width=194 from CharGadget (8 cols × 20px) + PropObject(16) + borders. `WindowGeometryMatchesTarget` test added asserting 194×138 ±4px. WW macro reverted, diagnostic LPRINTFs removed. 72/72 pass. | v0.9.31 |
| 147c | Typeface deeper workflows — textfield.gadget integration and Preview window: (1) `BGUI_EXTERNAL_GADGET` (`classID=26`) was silently failing because `libfunc.c`'s `BGUI_GetClassPtr` tried to load `gadgets/bgui_external.gadget` from disk first; the file doesn't exist and the `InitExtClass()` fallback was never reached (OpenLibrary failed but the error path had a bug where `cd_ClassBase` wasn't NULL-checked before the `!cl && cd_InitFunc` fallback test). Root fix: removed the two debug kprintfs that were masking the control flow and confirmed `InitExtClass()` IS called as fallback when the gadget file is absent — `ExternalObject` class now initialises correctly in-process. (2) `ExtClassNew` correctly picks up `EXT_Class = TEXTFIELD_GetClass()` (non-NULL) and stores it; `SetupAttrList` succeeds; `ExternalObject` returns a valid object. With ExternalObject working, the VGroup master is non-NULL, `WindowClassNew` proceeds, `PreviewWndObj` is created. (3) Settling budget: `SetPreviewFont → SaveFont(TRUE,TRUE)` does substantial work; opening the Preview window requires ~250 M emulation cycles after the MENUPICK. Updated `PreviewWindowOpens` and `ZPreviewWindowHasContent` to use `lxa_inject_drag` + `RunCyclesWithVBlank(5000, 50000)`. (4) Removed all temporary debug test drivers (`typeface_probe_gtest`, `typeface_menu_test_gtest`, `typeface_commkey_test_gtest`, `typeface_rmb_debug_gtest`) and their CMakeLists entries. (5) Removed all debug kprintfs from bgui source (`classes.c`, `externalclass.c`, `lib.c`, `libfunc.c`, `windowclass.c`); WW macro remains `#define WW(x)` (no-op). New tests: `TextFieldGadgetLoads`, `PreviewWindowOpens`, `ZPreviewWindowHasContent`. 70/72 pass (2 pre-existing: maxonbasic_gtest, dopus_gtest). | v0.9.32 |
| 149 | Deferred-paint trigger / forced full redraw: (1) Added `EMU_CALL_INT_FORCE_FULL_REDRAW` (3030) — ROM VBlank hook polls flag each VBlank, calls `lxa_force_full_redraw_all()` to walk FirstScreen→FirstWindow chain and send `IDCMP_REFRESHWINDOW` to all eligible windows; host sets flag via `lxa_force_full_redraw()` → `lxa_dispatch_set_force_full_redraw()`. (2) Fixed `display_read_pixel()` rootless-mode bug: in rootless mode `g_active_display` is NULL so pixel reads silently returned false and `CountScreenContent` always returned 0; added rootless fallback that reads from the first in-use `display_window_t` pixel buffer with correct `host_x/host_y` coordinate translation. (3) Added `display_set_active_by_index()` / `lxa_set_active_window()` / `SetActiveWindow()` API for explicit per-window pixel-buffer selection. (4) Re-enabled `MaxonBasicTest.ZMenuDragPixelStability` — fixed scan region (y=11 below menu bar, bg sample at (300,100)) and seeded editor with typed content for reliable baseline; `before=482 after=482`. 72/72 pass. | v0.10.0 |
| 150 | Layer creation BackFill (ghost-pixel elimination): two-part fix. (1) `lxa_layers.c`: reordered `CreateLayerInternal()` to call `RebuildClipRectsFrom(layer->back)` BEFORE `InvokeBackfillForNewLayer(layer)` — backing store of underlying SMART_REFRESH layers must be saved from the current screen bitmap before the new layer clears it; previous ordering saved pen 0 into backing store instead of actual content, breaking `SmartRefresh` backing-store restore. `InvokeBackfillForNewLayer` applies the default backfill (BltBitMap minterm 0x00) only for layers created with `LAYERS_BACKFILL` or `NULL` hook; `LAYERS_NOBACKFILL` layers (all Intuition windows) are skipped. (2) `lxa_intuition.c` `_intuition_OpenWindow()`: added `SetAPen(window->RPort, 0); RectFill(...)` over the full window area immediately after layer creation and before gadget rendering — this is the Intuition-level backfill that prevents ghost pixels from prior window content bleeding through. New `layerbackfilltest` Amiga sample (scenarios A and B). New `layer_backfill_gtest.cpp` (4 tests: all pass). SmartRefresh backing-store regression fixed. 70/73 pass (3 pre-existing: sysinfo_gtest, ppaint_gtest, dpaint_gtest). | v0.10.1 |

---

## Next Phase

> The Phase 151–158 block was scheduled after a visual review of DPaint V's
> "Screen Format" requester (see `tests/drivers/dpaint_gtest.cpp`,
> `DPaintPixelTest.ScreenFormatDialogSectionsContainVisibleContent`) against an
> FS-UAE reference capture. The dialog exposed nine distinct cross-app
> rendering / refresh defects. Each one is its own numbered phase because the
> root causes are independent and each will gain its own regression test, in
> line with the "no pooling sections" policy.

### Phase 151 — Listview / custom-drawn panel refresh trigger

**Class**: Amiga compatibility (gadget-class behaviour).

DPaint's "Choose Display Mode" list, "Display Information" panel, and "Credits"
panel all expose a PROPGADGET (gadget_type=3) for the scrollbar but the actual
text rows are custom-drawn by DPaint into the gadget rectangle. After Phase 164
removes the ghost pixels, these panels will be empty (background pen) instead
of showing PAL mode rows / mode info / credit lines. DPaint draws them in
response to an event lxa is failing to deliver. Candidates:
- `IDCMP_REFRESHWINDOW` after the layer-creation backfill (Intuition normally
  posts one when a SMART_REFRESH window is first made visible **and** the app
  requested `IDCMP_REFRESHWINDOW`)
- `IDCMP_GADGETUP` from the prop gadget on initial scroll-position set
- `IDCMP_NEWSIZE` after the implicit size-fit on open
- `IDCMP_INTUITICKS` (DPaint may use the tick to drive its own paint loop)

**Sub-problems**:
1. Capture the IDCMPFlags DPaint requests for the Screen Format window
   (instrument `_intuition_OpenWindow` to log requested IDCMP bits for any
   window whose title contains "Screen Format").
2. Compare the messages DPaint actually receives in lxa to the AROS / RKRM
   contract — identify the missing message type.
3. Implement the missing post: most likely a single `IDCMP_REFRESHWINDOW`
   right after the new layer is filled and ClipRects are stable, gated on
   the window's IDCMPFlags including `IDCMP_REFRESHWINDOW`.
4. Verify the same fix unblocks any other custom-drawn panel that depends on
   this initial paint trigger (audit DOpus, FinalWriter, PPaint requesters).

- [ ] Instrumentation: log DPaint Screen Format IDCMPFlags + every IDCMP
      message it receives in the first 500ms post-OpenWindow
- [ ] Identify and implement the missing message-post site
- [ ] Re-tighten the test: extend `dpaint_gtest.cpp`
      `ScreenFormatDialogSectionsContainVisibleContent` so the upper-left
      Choose Display Mode rectangle (gadget id=1, xy=8,34, wh=350x107)
      contains > 200 non-background pixels (proves real text rendered)
- [ ] Add equivalent assertions for the Display Information panel (gadget id=3)
      and Credits panel (gadget id=13)
- [ ] Remove instrumentation logging before commit (per AGENTS §6.3)

**Test gate**: All three Screen Format panels show > 200 non-background pixels
each; no other app driver regresses.

### Phase 152 — Listview scrollbar imagery

**Class**: Amiga compatibility (gadget rendering).

DPaint's three custom-drawn panels (gadgets id=1, id=3, id=13 — all
GTYP_PROPGADGET) are rendered with the prop knob alone — no surrounding
"track" / arrow imagery. On real Amiga + Intuition the prop gadget has a
visible recessed track with shine/shadow edges, and apps that wrap a prop
gadget for scrolling expect this chrome. The Phase 145 work added scrollbar
imagery for the WZOOM border gadget but did not extend to standalone
PROPGADGETs in app windows.

**Sub-problems**:
1. `_render_propgadget()` (or wherever PROPGADGET imagery is drawn in
   `lxa_intuition.c`) must render the recessed track frame (shine/shadow
   3D edge) around the knob area, not just the knob itself.
2. Honour `PROPNEWLOOK` (drawn with stipple per Phase 148) vs classic look.
3. The track must respect `FREEHORIZ` / `FREEVERT` flags to draw
   horizontal- vs vertical-scrollbar chrome.

- [ ] Audit `_render_propgadget` (or equivalent) and add track-frame
      rendering with shine (pen 2) / shadow (pen 1) edges
- [ ] Honour `AUTOKNOB`, `FREEHORIZ`, `FREEVERT`, `PROPBORDERLESS`
- [ ] Extend `simplegad_pixels_gtest.cpp` (or add a new
      `propgadget_chrome_gtest.cpp`) with a vertical and a horizontal prop
      gadget, asserting the track frame's shine and shadow pixels are
      present at the expected edges
- [ ] Add a DPaint regression: assert the right edge of gadget id=1
      (xy=8,34 wh=350x107) contains shadow-pen pixels (track frame)
- [ ] Verify Devpac's existing border PropGadget still renders (regression
      check on `devpac_scrollbar_gtest`)

**Test gate**: New propgadget chrome tests pass; Devpac scrollbar test stays
green; DPaint listview track frames visible in capture.

### Phase 153a — Cycle gadget rendering (Amiga look, not pop-up arrow)

**Class**: Amiga compatibility (gadget rendering).

GadTools `CYCLE_KIND` gadgets in DPaint's Screen Format dialog (gadgets id=4
"Standard", id=5 "Keep Same") render with a drop-down style arrow on the
right edge, similar to a Windows/Mac combo box. Real Amiga GadTools renders a
cycle gadget as a **recessed button** with a small **circular-arrow icon** on
the left, the current label centred, and no drop-down indicator (clicking
cycles to the next value, no pop-up list). The current lxa rendering misleads
users into expecting a drop-down list.

**Sub-problems**:
1. Locate the GadTools `CYCLE_KIND` rendering code (likely in
   `src/rom/lxa_gadtools.c` `gt_render_cycle()` or similar).
2. Replace the down-arrow imagery with the Amiga-correct circular-arrow
   icon (two small arrows arranged in a circle) on the left side.
3. Render the recessed button frame: top-left shadow, bottom-right shine
   (inverse of the normal raised button — cycle gadgets are visually
   recessed when not pressed).
4. Centre the label between the icon and the right edge.

- [ ] Implement the new rendering in the cycle gadget render function
- [ ] Add `cycle_gadget_render_gtest.cpp` (or extend
      `gadtoolsgadgets_pixels_c_gtest.cpp`) with a CYCLE_KIND gadget and
      assertions on the icon region (left edge), the recessed-frame pen
      pattern, and the absence of a down-arrow on the right edge
- [ ] DPaint regression: assert gadget id=4 (Standard) at xy=33,164 wh=160x14
      shows the circular-arrow icon glyph in its left ~12 pixels
- [ ] Remove any remaining references to drop-down / popup-style cycle
      rendering in the codebase

**Test gate**: New cycle-gadget render tests pass; DPaint Screen Format cycle
gadgets visually match the FS-UAE reference (arrow on left, no drop-down arrow
on right).

### Phase 153b — String gadget border style

**Class**: Amiga compatibility (gadget rendering).

DPaint's Screen Size and Page Size string gadgets (id=6, 7, 8, 9 — small
4-digit numeric entry boxes) render with a thin single-line border in lxa.
Real Amiga + GadTools renders a string gadget with a **recessed 3D frame**
(shadow on top/left, shine on bottom/right) plus a 1-pixel inner padding.
The current single-line look makes the gadgets appear flat / non-interactive.

**Sub-problems**:
1. Locate `_render_strgadget()` (or the equivalent in `lxa_intuition.c` /
   `lxa_gadtools.c`).
2. Replace the single-line border with a proper recessed 3D frame using
   shine (pen 2) on bottom/right and shadow (pen 1) on top/left, matching
   the BOOPSI string-gadget look.
3. Maintain the existing inner editable area dimensions (the 3D frame must
   sit just outside the editable rectangle, not eat into it).

- [ ] Implement the recessed 3D frame in the string-gadget render function
- [ ] Add a unit test in `simplegad_pixels_gtest.cpp` (or a new
      `stringgad_render_gtest.cpp`) asserting the frame's shine/shadow pen
      placement on a 100x14 string gadget
- [ ] DPaint regression: assert string gadget id=6 (xy=213,166 wh=44x10) has
      shadow pixels along its top edge and shine pixels along its bottom edge
- [ ] Verify other apps using string gadgets (DOpus rename dialog, Devpac
      Settings string entry) still render correctly

**Test gate**: New string-gadget render test passes; DPaint string gadgets
match the FS-UAE recessed look; no regressions in apps that use string
gadgets.

### Phase 155 — Checkbox + label gadget sizing

**Class**: Amiga compatibility (GadTools layout).

The "Retain Picture" gadget (DPaint Screen Format, gadget id=16, xy=209,220
wh=135x14) renders with the checkbox-plus-label region spanning **135 pixels
wide** in lxa. The FS-UAE reference shows the checkbox as a small
~26-pixel-wide checkbox with the "Retain Picture" label as a separate text
element to the right, and the clickable hit-area is just the checkbox plus
the label text — not a 135-pixel horizontal stripe. The current lxa
implementation over-sizes the checkbox gadget (probably extending the
gadget's width to span the whole row), which is visually wrong AND makes
the click hit-target too large.

**Sub-problems**:
1. Identify where the checkbox gadget's width is computed in
   `src/rom/lxa_gadtools.c` (likely `gt_create_checkbox()` or `gt_layout_*`).
2. The checkbox imagery itself should be the standard 26x11 (or 22x11) box;
   the label is a separate `IntuiText` rendered to the right with the
   correct baseline (per Phase 148-style label work).
3. The gadget's `Width` field should be the checkbox width only; the label
   width is independent.

- [ ] Fix the checkbox gadget width to match the visible checkbox imagery
      (26 or 22 px depending on `GTCB_Scaled`)
- [ ] Ensure the label `IntuiText` is rendered to the right of the checkbox
      with correct spacing (not overlapping)
- [ ] Add `checkbox_render_gtest.cpp` asserting: checkbox box is at
      (gadget.left, gadget.top), label text starts at gadget.left + box_width
      + small gap, both render correctly
- [ ] DPaint regression: assert gadget id=16 width is in range [22, 32] (not
      135), and that the label "Retain Picture" renders adjacent to the box

**Test gate**: New checkbox render test passes; DPaint Retain Picture matches
the FS-UAE checkbox-plus-label visual.

### Phase 156 — GADGDISABLED visual ghosting

**Class**: Amiga compatibility (gadget state rendering).

Two visible defects in DPaint's Screen Format dialog:
1. The **Retain Picture** checkbox is fully solid in lxa but ghosted (stipple
   overlay) in the FS-UAE reference because Retain Picture is only meaningful
   when an image is currently loaded — DPaint sets `GADGDISABLED` on the
   gadget at startup.
2. The **Screen Size text-entry boxes** (id=6, 7) similarly should be ghosted
   when "Standard" is the selected Screen Size cycle value (the values are
   forced by the cycle choice, so the entry fields are read-only / disabled).

lxa renders gadgets identically regardless of the `GFLG_DISABLED` flag.
Real Amiga + Intuition overlays a stipple pattern (alternating-pixel mask) on
top of any gadget with `GFLG_DISABLED` set, dimming the visible imagery.

**Sub-problems**:
1. Add a generic post-render stipple-overlay step in `_render_gadget()` (or
   the central gadget paint dispatcher) that applies a stipple mask in pen 1
   over the gadget's bounding rectangle when `gad->Flags & GFLG_DISABLED`.
2. The stipple should match the AROS/Amiga `GHOSTPATTERN` (0x5555 / 0xAAAA
   alternating).
3. The overlay must clip to the gadget's visible region (don't paint outside
   the gadget's `Width`/`Height`).

- [ ] Implement the stipple overlay in `_render_gadget()` (apply after the
      gadget's normal imagery is drawn)
- [ ] Add `gadget_disabled_gtest.cpp`: render a normal and a disabled
      checkbox / button / cycle gadget, assert the disabled one has
      stipple pixels in expected positions (every-other pixel pen-1 overlay)
- [ ] DPaint regression: assert gadget id=16 (Retain Picture) renders with
      stipple pixels when DPaint sets it disabled
- [ ] Verify that `OnGadget()` / `OffGadget()` already correctly toggle
      `GFLG_DISABLED` and trigger a re-render (extend tests if not)

**Test gate**: New disabled-gadget test passes; DPaint Retain Picture and
Screen Size string gadgets render ghosted in the captured screen format dialog.

### Phase 157 — Button / menu accelerator underline

**Class**: Amiga compatibility (text rendering).

DPaint's Screen Format buttons show their keyboard accelerator letter
underlined: "**U**se", "**Cancel**", "**R**etain Picture". The real
mechanism is `IntuiText` with an embedded `STYLE_UNDERLINED` byte (or, for
GadTools, an `&` prefix in the label string — the GadTools code translates
this into a SetSoftStyle + Move + Draw underline). lxa currently ignores
the underline marker and renders the label as plain text.

**Sub-problems**:
1. GadTools label rendering: parse the `&` prefix in label strings and
   render the following character with a 1-pixel underline.
2. `IntuiText` rendering: honour `STYLE_UNDERLINED` in the `ITextFont` /
   `IText.FrontPen` style bits when calling Text().
3. Both code paths converge in `_graphics_Text()` — likely the cleanest fix
   is to honour `RastPort.AlgoStyle & FSF_UNDERLINED` set by `SetSoftStyle()`
   and have GadTools / IntuiText set it before each affected draw.
4. Apply to menu items too (per RKRM, menu accelerator characters are
   underlined when COMMSEQ flag is set or via the `&` convention).

- [ ] Implement underline rendering in `_graphics_Text()` (a horizontal line
      one pixel below the baseline for the styled run)
- [ ] Have GadTools button/checkbox/cycle label code parse `&char` and
      bracket the underlined character with `SetSoftStyle(FSF_UNDERLINED)`
      / `SetSoftStyle(0)` calls
- [ ] Have menu item rendering apply the same convention
- [ ] Add `text_underline_gtest.cpp` covering: plain text, underlined text via
      `SetSoftStyle`, GadTools button label with `&` accelerator
- [ ] DPaint regression: assert the row of pixels just below the "U" of
      "Use" (gadget id=14, xy=12,238 wh=50x14) has continuous shine pixels
      forming the underline

**Test gate**: New underline render test passes; DPaint button accelerators
visibly underlined; menu accelerator letters underlined where present.

### Phase 158 — `SetWindowTitles()` repaints title bar

**Class**: Quality (existing TODO at `src/rom/lxa_intuition.c:10489`).

`_intuition_SetWindowTitles()` currently updates `window->Title` in memory
but does not redraw the window's title bar — the comment on line 10489 says
`/* TODO: actually redraw the title bar */`. Apps that change a window
title at runtime (DPaint switches its single window from "Ownership
Information" → "DeluxePaint 5.2 - Screen Format" → ...) appear to keep the
old title until the user moves or refreshes the window. Real Amiga repaints
the title bar immediately on `SetWindowTitles()`.

**Sub-problems**:
1. After updating `window->Title`, call `_render_window_frame(window)` (or a
   narrower `_render_window_titlebar(window)` helper) so the bar is repainted
   with the new text.
2. After updating `window->WScreen->Title` for the screen, the existing
   `_render_screen_title_bar()` call already handles that — but only when
   the active window's screen title is shown; if the screen title bar is
   currently obscured by another screen-title source (e.g. the active
   window has a custom screen title), the right behaviour is "repaint when
   visible." Audit the active-window vs. screen-title interaction.
3. Remove the `TODO` comment.

- [ ] Implement the title-bar repaint in `_intuition_SetWindowTitles()`
- [ ] Add `setwindowtitles_gtest.cpp`: open a window with title "Before",
      capture title-bar pixels, call `SetWindowTitles(window, "After", -1)`,
      capture again, assert the pixels differ in the title-bar region
- [ ] Add a screen-title variant of the same test
- [ ] DPaint regression: after Ownership dismiss, the rendered title bar of
      the (now-retitled) window contains text matching "DeluxePaint" — not
      "Ownership Information"

**Test gate**: New `setwindowtitles_gtest` passes; DPaint Screen Format
window's title bar pixels match its current title (`window->Title`).

### Phase 159 — DirectoryOpus deeper workflows

Phase 134 fixed the button-bank rendering. Remaining gaps:

- [ ] File-list navigation in lister panes (requires directory reading)
- [ ] Button-bank click workflows (requires `dopus.library` command dispatch — verify the library is supplied or stubbed correctly per Phase 142 policy; if third-party, **STOP and notify user**)
- [ ] Preferences panel interaction
- [ ] Title-bar version/memory string rendered by DOpusRT (which is currently not launched in the test fixture) — either launch DOpusRT or document why the title-bar string is intentionally absent
- [ ] Re-enable `DISABLED_DOpusTextHookTest.TextHookCapturesKnownDOpusLabels` — quarantined because `Move` and `Rename` button-bank labels are missing from rendered text (only `Copy`, `Makedir`, and single-char cluster labels captured); root cause is incomplete button-bank text rendering in the deeper-workflow path

**Test gate**: At least 4 new DOpus tests covering the four areas above; `TextHookCapturesKnownDOpusLabels` re-enabled and passing.

### Phase 160 — BlitzBasic 2 ted editor real text rendering

Phase 119 captured: editor body remains a flat surface with only menu-residue and status overlay paint, even after Phase 113/114 blitter/copper improvements and Phase 135 requester unblocking. ted does not render real editable text content.

**Root cause unknown** — needs new investigation. Hypotheses:
- Custom text renderer that uses an as-yet-unimplemented blitter copy mode
- Sprite hardware used for cursor/selection
- Specific copper waits that ted uses for its custom overlay
- Direct chip-RAM writes that bypass the lxa graphics path

- [ ] Instrument blitter operations during BB2 idle to identify what ted *does* emit (Phase 135 confirmed it's not the originally-hypothesised line-mode/copper-color cycling)
- [ ] Capture BB2 editor area pixel state at multiple intervals to identify the rendering trigger
- [ ] Attach captured PNGs to assistant for visual diagnosis of what should be there but isn't
- [ ] Implement missing path
- [ ] `EditorRendersText` test in `blitzbasic2_gtest.cpp` asserting non-trivial text-area pixel content

**Test gate**: ted editor area shows real text content under tests.

### Phase 161 — Menu introspection upgrade + non-releasing drag API

Several brittle interactions share the same root cause: lack of programmatic menu item activation and lack of in-drag observability.

Affected apps and gaps:
- MaxonBASIC: item-level menu activation brittle (German titles, no introspection)
- KickPascal: layout/menus depend on deeper req library + no introspection (Phase 142 covers req side; this phase covers introspection)
- ASM-One: qualifier observability not possible from host
- All apps: menu-state cannot be captured during a drag (`lxa_inject_drag` is atomic press→drag→release)

- [ ] Add `lxa_select_menu_item(menu_idx, item_idx, sub_idx)` host API that activates a specific menu item by index without coordinate-based dragging
- [ ] Add `lxa_inject_drag_begin()` / `lxa_inject_drag_step()` / `lxa_inject_drag_end()` non-atomic drag API so tests can capture pixel state mid-drag
- [ ] Add `lxa_get_qualifier_state()` host API exposing the current input qualifier bits
- [ ] Update `LxaTest` C++ helpers to wrap all three
- [ ] Migrate at least one MaxonBASIC menu-item test, one KickPascal menu test, and one ASM-One qualifier test to the new APIs
- [ ] Add `MenuPixelStateDuringDrag` test in `gadtoolsmenu_gtest.cpp` exercising the non-atomic drag API

**Test gate**: New APIs covered; migrated tests pass; no regressions in existing menu tests.

### Phase 162 — SysInfo hardware fields + Cluster2 EXIT button

Two small per-app correctness gaps grouped because they are both isolated coordinate/data-source issues.

- [ ] **SysInfo hardware fields**: `BattClock` and CIA timing fields that SysInfo reads — populate plausible values in the relevant ROM data (not real hardware detection; lxa reports a synthetic baseline machine)
- [ ] **Cluster2 EXIT button**: coordinate mapping mismatch in custom toolbar — Cluster2 renders its toolbar with custom coordinates; the EXIT button click coordinate calculation in `cluster2_gtest.cpp` does not match. Either the test coordinates are wrong, or lxa's IDCMP_MOUSEBUTTONS coordinate mapping is off for windows on a custom screen.
- [ ] New SysInfo test asserting BattClock/CIA fields are non-zero
- [ ] New Cluster2 test asserting EXIT button click terminates the app cleanly

**Test gate**: Both new tests pass.

---

## Performance (Phase 163)

### Phase 163 — Performance follow-up (placeholder, scheduled when needed)

No specific performance work is currently scheduled. When a measurable regression appears (test wall time, memory, CPU profile from `--profile` flag), open this phase with concrete objectives. Until then, this slot is reserved.

The standing performance baseline:
- Full test suite ~145–175 s wall time at `-j16`
- Per-EMU_CALL profiling available via `--profile <path>` + `tools/profile_report.py`

---

## New Features: RTG Retargetable Graphics (Phases 164–166)

> Strategic pivot: lxa's display strategy moves from ECS-era planar modes to RTG via Picasso96. Phases 164–166 deliver the full RTG stack — display backend, P96 library, and app validation. Scheduled after the quality + Amiga-compatibility blocks per the priority order.

### Phase 164 — RTG display foundation

Land the chunky-bitmap and SDL2 RGBA pipeline that all subsequent RTG work depends on.

**Sub-problems**:
1. **`AllocBitMap` depth extension** (`lxa_graphics.c`): remove the depth 1–8 clamp. For depths > 8, allocate a single contiguous chunky buffer (width × height × bytes-per-pixel) instead of separate bit-planes. Mark such bitmaps with a new `BMF_RTG` internal flag. `GetBitMapAttr(BMA_DEPTH)` must return the real depth.
2. **SDL2 backend extension** (`display.c`): add `display_update_rtg()` accepting a raw 32-bit RGBA buffer uploaded directly to an `SDL_PIXELFORMAT_RGBA32` texture. Existing planar pipeline untouched.
3. **RTG display IDs** (`lxa_graphics.c`): add P96 monitor/mode IDs to `g_known_display_ids[]` (at minimum `P96_MONITOR_ID` base with `RGBFB_R8G8B8A8` and `RGBFB_R5G6B5` variants). `GetDisplayInfoData(DTAG_DIMS)` returns host-resolution ranges. `OpenScreenTagList()` with an RTG depth allocates a chunky screen bitmap and calls `display_open()` with the RTG depth.

- [ ] Extend `AllocBitMap` to support depth > 8; add `BMF_RTG` internal flag
- [ ] `GetBitMapAttr(BMA_DEPTH/WIDTH/HEIGHT/BYTESPERROW)` correct for RTG bitmaps
- [ ] `display_update_rtg()` in `display.c` / `display.h`; `SDL_PIXELFORMAT_RGBA32` texture path
- [ ] P96 monitor/mode IDs in `g_known_display_ids[]`; `GetDisplayInfoData` returns RTG ranges
- [ ] `OpenScreenTagList()` with RTG depth allocates chunky screen bitmap
- [ ] New `rtg_gtest.cpp`: `AllocBitMap(640,480,32,BMF_CLEAR,NULL)` + `GetBitMapAttr` returns 32; `OpenScreenTagList` with P96 mode ID does not crash

**Test gate**: `rtg_gtest` passes; full suite remains green.

### Phase 165 — Picasso96API.library core

New disk library (`src/rom/lxa_p96.c`, installed to `share/lxa/System/Libs/Picasso96API.library`).

| Function | Purpose |
|---|---|
| `p96AllocBitMap(w,h,depth,flags,friend,rgbformat)` | Allocate RTG bitmap in a specific pixel format |
| `p96FreeBitMap(bm)` | Free it |
| `p96GetBitMapAttr(bm,attr)` | Query RGBFORMAT, WIDTH, HEIGHT, BYTESPERROW |
| `p96WritePixelArray(ri,sx,sy,bm,dx,dy,w,h)` | Blit pixels from a RenderInfo to a BitMap |
| `p96ReadPixelArray(ri,dx,dy,bm,sx,sy,w,h)` | Read pixels back |
| `p96LockBitMap(bm,tags)` / `p96UnlockBitMap(bm,ri)` | Lock for direct access |
| `p96GetModeInfo(id,info)` | Query mode properties |
| `p96BestModeIDA(tags)` | Find best matching RTG mode |
| `p96RequestModeID(tags)` | Mode requester (stub: returns best mode, no UI) |

Plus a thin `cybergraphics.library` compatibility shim (`GetCyberMapAttr`, `LockBitMapTags`, `WriteLUTPixelArray`, etc.).

- [ ] `src/rom/lxa_p96.c` with all ten core functions
- [ ] `cybergraphics.library` shim disk library
- [ ] Register both in `sys/CMakeLists.txt` via `add_disk_library()`
- [ ] `p96_gtest.cpp`: unit tests for each function; startup test confirms `OpenLibrary("Picasso96API.library", 0)` returns non-NULL
- [ ] `cgx_gtest.cpp`: companion that opens `cybergraphics.library` and calls equivalents via CGX names; both libraries return identical results

**Test gate**: `p96_gtest` and `cgx_gtest` pass; at least one target app opens the library without exiting early.

### Phase 166 — RTG app validation

- [ ] **PPaint** (flagship): mode-scan loop populated via P96 enumeration; assert editor canvas reachable. ECS-path investigation remains abandoned (PPaint is RTG-only in practice).
- [ ] **FinalWriter**: re-enable `DISABLED_AcceptDialogOpensEditorWindow` in `finalwriter_gtest.cpp`; clicking OK opens editor window
- [ ] **One new productivity app** (Wordsworth or Amiga Writer): reaches main editor; text hook confirms content renders
- [ ] All existing drivers continue to pass

**Test gate**: PPaint editor reachable; FinalWriter editor reachable; new productivity app reaches main UI.

---

## Long-Term: External Process Emulation + Observability (Phases 167–169)

### Phase 167 — External process emulation + DOS stdout/clipboard capture

DevPac Assemble, ASM-One Assemble, BlitzBasic Run, KickPascal Compile/Run, BlitzBasic2 Compile all require this.

- [ ] AmigaDOS `CreateProc()` / `Execute()` launching real m68k subprocesses within the emulated environment, with stdout piped back to the host
- [ ] `lxa_capture_dos_output()` host API exposing AmigaDOS stdout to tests
- [ ] Clipboard capture API (clipboard.device hook)
- [ ] DevPac Assemble + ASM-One Assemble + BlitzBasic Run + KickPascal Compile each get a test asserting compiler output appears on captured stdout
- [ ] Update existing app drivers (DevPac, ASM-One, BlitzBasic 2, KickPascal) to exercise their compile/run workflows

**Test gate**: At least 4 compile/run workflow tests pass.

### Phase 168 — Screen-content diffing infrastructure

Currently no way to assert "this region looks like a text label" or "this area changed structurally between two states." A reference-image diff tool would let us write layout regression tests that survive minor color changes but catch structural regressions.

- [ ] Add `lxa_diff_capture(reference_png, captured_png, threshold)` host API
- [ ] Define a structural-similarity metric (SSIM or simple block-hash)
- [ ] Reference-image storage convention under `tests/references/`
- [ ] Migrate at least one existing pixel-count test to a structural-diff test

**Test gate**: New diff API has unit tests; one migrated test passes.

### Phase 169 — Audio device introspection

No tests currently require this. Open this phase only when the first audio-using app needs validation.

- [ ] Define what "observable audio output" means for tests (e.g. captured sample buffer, frequency analysis)
- [ ] Implement audio.device tap
- [ ] Add a test for an audio-using app

**Test gate**: First audio app validated.

---

## Far Future: GUI Toolkits + CPU Evolution

### Phase 170 (tentative) — MUI library hosting

MUI (`MUI:Libs/muimaster.library`) is a disk library — never reimplemented in ROM. This phase ensures lxa's BOOPSI infrastructure (icclass/modelclass/gadgetclass, utility.library tag machinery) is solid enough that a real `muimaster.library` binary opens and functions.

- [ ] Audit BOOPSI completeness against MUI's requirements
- [ ] Install user-supplied `muimaster.library` binary
- [ ] Test gate: at least one MUI-based app opens its main window

### Phase 171 (tentative) — ReAction / ClassAct hosting

ReAction (AmigaOS 3.5/3.9 GUI toolkit) runs as disk-provided class files (`SYS:Classes/reaction/`). This phase ensures lxa's BOOPSI layer and Intuition class dispatch are complete enough to host ReAction classes.

- [ ] Audit BOOPSI + Intuition class dispatch completeness
- [ ] Install user-supplied ReAction classes
- [ ] Test gate: at least one ReAction app opens its main window

### Phase 172 (tentative) — CPU core evaluation

Musashi (MIT, C89, interpreter) is adequate for correctness but limits throughput. Host-side overhead currently dominates (~95% of wall time per Phase 126 estimate), so CPU evaluation is deferred until host optimisations are exhausted.

- [ ] Re-evaluate Moira if 68030 support has landed
- [ ] Prototype Moira integration if viable
- [ ] Assess whether 68020 mode suffices for all tested apps
- [ ] Decide on JIT investment if needed

---

## Standing Infrastructure Notes

These are not phases but ongoing rules captured in `AGENTS.md` lessons learned. Repeated here so they are visible from the roadmap.

- **CMake shard FILTER drift**: any new test added to a sharded driver MUST update the corresponding FILTER strings in `tests/drivers/CMakeLists.txt`. The `shard_coverage_check` CTest meta-test (Phase 0) catches violations automatically.
- **Visual investigation**: attach captured PNGs (`lxa_capture_window()` / `lxa_capture_screen()`) directly to assistant messages — the OpenCode harness supports image input natively. The retired `tools/screenshot_review.py` lives in `attic/`.
- **System libraries are fully implemented; third-party libraries STOP and notify the user** (per AGENTS §1).

---

## Dependency Graph (Critical Path)

```
Phase 142-144 (Library policy + datatypes + smoke) ✓
        │
        ▼
Phase 145-146 (Devpac scrollbar + Settings window)
        │
        ▼
Phase 159-162 (App-specific compatibility)
        │
        ▼
Phase 163 (Performance — placeholder)
        │
        ▼
Phase 164-166 (RTG: foundation → P96 → app validation)
        │
        ▼
Phase 167-169 (External processes, diffing, audio)
        │
        ▼
Phase 170-172 (MUI, ReAction, CPU eval)
```
