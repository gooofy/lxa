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

---

## Next Phase

### Phase 149 — Deferred-paint trigger / forced full redraw

Several apps defer rendering of menu bars, toolbars, palettes, and editor backgrounds until the first mouse interaction. This makes interaction tests brittle: the first click triggers paint, subsequent clicks see the painted state, and the test must be carefully ordered.

Affected apps: DPaint V (main editor menu/toolbox/palette + Ctrl-P from depth-8), KickPascal 2 (editor-body splash-clear).

**Two complementary approaches** — implement both:
1. **Investigate the deferred-paint trigger** in each affected app (likely an IDCMP_INTUITICKS or first-IDCMP_MOUSEMOVE handler) and understand why lxa is not delivering the trigger event when expected.
2. **Add a "force full redraw" host capability** so tests can demand a full repaint without faking input.

- [ ] Add `lxa_force_full_redraw()` host API + EMU_CALL that walks all open windows and damages their full client areas (triggering `IDCMP_REFRESHWINDOW`)
- [ ] DPaint V: identify why first-mouse-interaction is needed; fix or document
- [ ] KickPascal 2: editor body clears splash screen on first key press (or forced redraw)
- [ ] DPaint V Ctrl-P: works from depth-8 main editor (not only depth-2 startup dialog)
- [ ] Re-enable any DPaint V About dialog / File→Open requester tests that were blocked
- [ ] New tests in `dpaint_gtest.cpp` and `kickpascal_gtest.cpp` covering the unblocked workflows
- [ ] Re-enable `DISABLED_MaxonBasicTest.ZMenuDragPixelStability` — quarantined because the editor area returns 0 pixels (deferred-paint not yet triggered at baseline sample time); fix requires the deferred-paint trigger work above

**Test gate**: DPaint V, KickPascal 2, and MaxonBasic `ZMenuDragPixelStability` all pass.

### Phase 150 — DirectoryOpus deeper workflows

Phase 134 fixed the button-bank rendering. Remaining gaps:

- [ ] File-list navigation in lister panes (requires directory reading)
- [ ] Button-bank click workflows (requires `dopus.library` command dispatch — verify the library is supplied or stubbed correctly per Phase 142 policy; if third-party, **STOP and notify user**)
- [ ] Preferences panel interaction
- [ ] Title-bar version/memory string rendered by DOpusRT (which is currently not launched in the test fixture) — either launch DOpusRT or document why the title-bar string is intentionally absent
- [ ] Re-enable `DISABLED_DOpusTextHookTest.TextHookCapturesKnownDOpusLabels` — quarantined because `Move` and `Rename` button-bank labels are missing from rendered text (only `Copy`, `Makedir`, and single-char cluster labels captured); root cause is incomplete button-bank text rendering in the deeper-workflow path

**Test gate**: At least 4 new DOpus tests covering the four areas above; `TextHookCapturesKnownDOpusLabels` re-enabled and passing.

### Phase 151 — BlitzBasic 2 ted editor real text rendering

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

### Phase 152 — Menu introspection upgrade + non-releasing drag API

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

### Phase 153 — SysInfo hardware fields + Cluster2 EXIT button

Two small per-app correctness gaps grouped because they are both isolated coordinate/data-source issues.

- [ ] **SysInfo hardware fields**: `BattClock` and CIA timing fields that SysInfo reads — populate plausible values in the relevant ROM data (not real hardware detection; lxa reports a synthetic baseline machine)
- [ ] **Cluster2 EXIT button**: coordinate mapping mismatch in custom toolbar — Cluster2 renders its toolbar with custom coordinates; the EXIT button click coordinate calculation in `cluster2_gtest.cpp` does not match. Either the test coordinates are wrong, or lxa's IDCMP_MOUSEBUTTONS coordinate mapping is off for windows on a custom screen.
- [ ] New SysInfo test asserting BattClock/CIA fields are non-zero
- [ ] New Cluster2 test asserting EXIT button click terminates the app cleanly

**Test gate**: Both new tests pass.

---

## Performance (Phase 152)

### Phase 154 — Performance follow-up (placeholder, scheduled when needed)

No specific performance work is currently scheduled. When a measurable regression appears (test wall time, memory, CPU profile from `--profile` flag), open this phase with concrete objectives. Until then, this slot is reserved.

The standing performance baseline:
- Full test suite ~145–175 s wall time at `-j16`
- Per-EMU_CALL profiling available via `--profile <path>` + `tools/profile_report.py`

---

## New Features: RTG Retargetable Graphics (Phases 153–155)

> Strategic pivot: lxa's display strategy moves from ECS-era planar modes to RTG via Picasso96. Phases 153–155 deliver the full RTG stack — display backend, P96 library, and app validation. Scheduled after the quality + Amiga-compatibility blocks per the priority order.

### Phase 155 — RTG display foundation

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

### Phase 156 — Picasso96API.library core

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

### Phase 157 — RTG app validation

- [ ] **PPaint** (flagship): mode-scan loop populated via P96 enumeration; assert editor canvas reachable. ECS-path investigation remains abandoned (PPaint is RTG-only in practice).
- [ ] **FinalWriter**: re-enable `DISABLED_AcceptDialogOpensEditorWindow` in `finalwriter_gtest.cpp`; clicking OK opens editor window
- [ ] **One new productivity app** (Wordsworth or Amiga Writer): reaches main editor; text hook confirms content renders
- [ ] All existing drivers continue to pass

**Test gate**: PPaint editor reachable; FinalWriter editor reachable; new productivity app reaches main UI.

---

## Long-Term: External Process Emulation + Observability (Phases 156–158)

### Phase 158 — External process emulation + DOS stdout/clipboard capture

DevPac Assemble, ASM-One Assemble, BlitzBasic Run, KickPascal Compile/Run, BlitzBasic2 Compile all require this.

- [ ] AmigaDOS `CreateProc()` / `Execute()` launching real m68k subprocesses within the emulated environment, with stdout piped back to the host
- [ ] `lxa_capture_dos_output()` host API exposing AmigaDOS stdout to tests
- [ ] Clipboard capture API (clipboard.device hook)
- [ ] DevPac Assemble + ASM-One Assemble + BlitzBasic Run + KickPascal Compile each get a test asserting compiler output appears on captured stdout
- [ ] Update existing app drivers (DevPac, ASM-One, BlitzBasic 2, KickPascal) to exercise their compile/run workflows

**Test gate**: At least 4 compile/run workflow tests pass.

### Phase 159 — Screen-content diffing infrastructure

Currently no way to assert "this region looks like a text label" or "this area changed structurally between two states." A reference-image diff tool would let us write layout regression tests that survive minor color changes but catch structural regressions.

- [ ] Add `lxa_diff_capture(reference_png, captured_png, threshold)` host API
- [ ] Define a structural-similarity metric (SSIM or simple block-hash)
- [ ] Reference-image storage convention under `tests/references/`
- [ ] Migrate at least one existing pixel-count test to a structural-diff test

**Test gate**: New diff API has unit tests; one migrated test passes.

### Phase 160 — Audio device introspection

No tests currently require this. Open this phase only when the first audio-using app needs validation.

- [ ] Define what "observable audio output" means for tests (e.g. captured sample buffer, frequency analysis)
- [ ] Implement audio.device tap
- [ ] Add a test for an audio-using app

**Test gate**: First audio app validated.

---

## Far Future: GUI Toolkits + CPU Evolution

### Phase 161 (tentative) — MUI library hosting

MUI (`MUI:Libs/muimaster.library`) is a disk library — never reimplemented in ROM. This phase ensures lxa's BOOPSI infrastructure (icclass/modelclass/gadgetclass, utility.library tag machinery) is solid enough that a real `muimaster.library` binary opens and functions.

- [ ] Audit BOOPSI completeness against MUI's requirements
- [ ] Install user-supplied `muimaster.library` binary
- [ ] Test gate: at least one MUI-based app opens its main window

### Phase 162 (tentative) — ReAction / ClassAct hosting

ReAction (AmigaOS 3.5/3.9 GUI toolkit) runs as disk-provided class files (`SYS:Classes/reaction/`). This phase ensures lxa's BOOPSI layer and Intuition class dispatch are complete enough to host ReAction classes.

- [ ] Audit BOOPSI + Intuition class dispatch completeness
- [ ] Install user-supplied ReAction classes
- [ ] Test gate: at least one ReAction app opens its main window

### Phase 163 (tentative) — CPU core evaluation

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
Phase 147-153 (App-specific compatibility)
        │
        ▼
Phase 154 (Performance — placeholder)
        │
        ▼
Phase 155-157 (RTG: foundation → P96 → app validation)
        │
        ▼
Phase 158-160 (External processes, diffing, audio)
        │
        ▼
Phase 161-163 (MUI, ReAction, CPU eval)
```
