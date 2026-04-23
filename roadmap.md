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

---

## Next Phase

### Phase 143 — datatypes.library full implementation

Replace the Phase 136-h minimal stub with a complete RKRM/NDK implementation.

Key areas:
- [ ] `ObtainDataType()` / `ReleaseDataType()` — type detection via magic bytes
- [ ] `NewDTObject()` / `DisposeDTObject()` — object lifecycle
- [ ] `GetDTAttrs()` / `SetDTAttrs()` — attribute access
- [ ] `DoDTMethod()` — method dispatch to DataTypes classes
- [ ] `DrawDTObject()` — render into a RastPort
- [ ] `FindToolNode()` / `AddDTObject()` / `RemoveDTObject()` — gadget management
- [ ] `GetDTMethods()` / `GetDTTriggerMethods()` — method introspection
- [ ] DataTypes class loading: scan `SYS:Classes/DataTypes/` for `.datatype` descriptors
- [ ] Built-in detection for ILBM, 8SVX, FTXT, ANIM
- [ ] New `datatypes_gtest.cpp` covering core lifecycle + at least one IFF type

**Test gate**: Apps using `datatypes.library` (picture viewers, sound players, Workbench icon loading) open DataTypes objects without crashing.

### Phase 144 — Startup smoke test suite

Catch "app crashes on startup" regressions automatically. The Typeface crash (datatypes.library NULL pointer) went undetected because no test existed for Typeface yet.

- [ ] Add `smoke_gtest.cpp` in `tests/drivers/`: parameterised test that, for each known app binary in `lxa-apps/`, loads the app, runs 100 VBlank settle iterations, and asserts (1) no SIGABRT, (2) no `PANIC` in output, (3) no `rv=26` unless allowlisted
- [ ] Self-contained: missing app binary → `GTEST_SKIP()` rather than failure
- [ ] Allowlist apps that legitimately exit immediately (command-line tools)

**Test gate**: All currently known apps that open windows pass the smoke test.

### Phase 145 — Intuition window border scrollbar rendering (Devpac editor)

**Root cause** (confirmed via FS-UAE screenshots vs lxa captures):

The Devpac 3 editor window has a **right border PropGadget** (vertical scrollbar) and **up/down arrow system gadgets** in the bottom-right corner of the right border. In lxa these are not rendered — the right border appears as a plain filled rectangle with no knob, no arrows.

Two distinct sub-problems:

**Sub-problem A — Up/Down arrow system gadgets missing**

`_create_window_sys_gadgets()` only creates CLOSE, DEPTH, and SIZING system gadgets. It does not create scroll-arrow gadgets even when an app adds them to the window's gadget list as `GTYP_SYSGADGET | GTYP_WDRAGGING`-style gadgets. On a real Amiga, apps that request scroll arrows receive `GTYP_SYSGADGET | 0x0060` (up-arrow) and `GTYP_SYSGADGET | 0x0070` (down-arrow) gadgets from Intuition's internal gadget creation, **not** by calling `AddGadget()` themselves. lxa does not create or render these. The render switch in `_render_window_frame()` (line 9219) has no cases for up/down arrow types.

**Sub-problem B — PropGadget in right border not rendered**

The Devpac editor window uses a user-supplied `GTYP_PROPGADGET` placed in the right border (x = WindowWidth - BorderRight, full height). `_render_window_frame()` renders system gadgets (lxa-created GTYP_SYSGADGET ones), then calls `_render_window_user_gadgets()` which iterates all non-SYSGADGET gadgets. The prop gadget is a user gadget, so it *should* be rendered — but `_calculate_gadget_box()` may be computing wrong coordinates for border-placed prop gadgets, or the knob body/pot values cause the knob to vanish. Additionally, the `PROPNEWLOOK` rendering path does not draw the container outline and the arrow imagery.

**Sub-problem C — WZOOM gadget has no render imagery**

`_render_window_frame()` switch (line 9219) has no `case GTYP_WZOOM:` — the zoom gadget is allocated and tracked but draws as a plain blank frame. The FS-UAE reference shows it as a small rectangle in the top-right title bar (next to the depth gadget).

**TODOs**:
- [ ] Investigate why Devpac's right-border PropGadget does not render its knob (enable DPRINTF for prop rendering, capture lxa log during startup)
- [ ] Add `case GTYP_WZOOM:` to `_render_window_frame()` switch with correct imagery (two arrows or filled box per real Amiga look)
- [ ] Verify `_calculate_gadget_box()` handles border-placed gadgets with `GACT_RIGHTBORDER` correctly
- [ ] Add `devpac_scrollbar_gtest.cpp` (or extend `devpac_gtest.cpp`) with pixel assertions: right border has non-background pixels in the knob area; bottom of right border has arrow glyphs
- [ ] Check whether Devpac adds scroll-arrow gadgets itself (via `AddGadget`) or expects Intuition to create them — inspect lxa log for `AddGadget` calls with GTYP_SYSGADGET types at startup

**Test gate**: Devpac editor window right border shows prop knob and up/down arrows; zoom gadget icon visible in title bar; devpac_gtest scrollbar assertions pass.

### Phase 146 — Devpac Settings window (req.library requester layout)

**Root cause** (confirmed via FS-UAE screenshot devpac2.png):

The Settings window is opened by DevPac via `req.library`'s `DoRequest()` / `BuildSysRequest()` — now using the real binary (Phase 142). The FS-UAE reference shows:
- A properly positioned sub-window (centered on screen, ~480×140 px)
- Cycle gadgets (`[<]` button + current-value text) for: End of Line, Word Qualifier, Save Before Run, Program Window
- A string gadget for Public Screen
- Four checkboxes on the right (Auto-Indent, Make Backups, Stack New Projects, Shift-Backspace, IBM Keypad)
- Three push-buttons at the bottom: Save, Use, Cancel

This window was previously never reachable (Phase 142 had the req.library stub silently failing). Now that the real binary loads, we need to:
1. Navigate to Settings→Settings in lxa's devpac test and capture the result
2. Compare visually to the FS-UAE reference
3. Identify specific rendering defects (missing gadgets, wrong positions, truncated text, wrong colors)

The most likely issues are:
- **Cycle gadget arrow rendering**: The `[<]` button is a cycle gadget using req.library's own internal gadget imagery — lxa's gadget renderer may not know about req.library's custom image format
- **String gadget border**: req.library string gadgets may use a non-standard border style
- **Layout/positioning**: req.library computes its own layout from font metrics; if `tf_XSize`/`tf_YSize` differ from what req.library expects, all gadget positions shift

**TODOs**:
- [ ] Add a `DevpacTest.SettingsWindow` test: navigate Settings→Settings via RMB drag, wait for a new window, capture it
- [ ] Attach capture PNG and compare to FS-UAE reference (devpac2.png in `screenshots/`)
- [ ] Identify and fix any rendering defects found
- [ ] Assert: window opens (count goes from 1 to 2), window contains at least 3 push-buttons, cycle gadget for "End of Line" is visible

**Test gate**: `DevpacTest.SettingsWindow` passes; settings window looks correct when compared to screenshot.



Typeface currently renders a 194×138 character grid; the real app shows 280×195 with a window border, title bar, and vertical scrollbar. The grid (BGUI primary content) is correct; the surrounding chrome is missing.

**Root cause hypothesis**: Either BGUI's window-frame layout is computing the wrong client area, or the lxa Intuition window-decoration code is not rendering Typeface's requested borders/scrollbar gadgets. Determine which side.

- [ ] Capture window via `lxa_capture_window()` and attach PNG to assistant for visual diagnosis (per `graphics-testing` SKILL §8)
- [ ] Compare BGUI-requested geometry vs lxa-rendered geometry (Phase 131 `lxa_get_window_info`)
- [ ] If BGUI side: inspect `WindowObject` tags Typeface passes
- [ ] If lxa side: audit `_intuition_OpenWindow` border/title-bar/scrollbar paths
- [ ] Fix and add `WindowChromeMatchesTarget` test in `typeface_gtest.cpp` asserting target dimensions ±a few pixels

**Test gate**: Typeface window matches target geometry; full suite green.

### Phase 146 — Typeface deeper workflows

After Phase 145 gives Typeface a correct window, exercise the actual font-browser interactions.

- [ ] `gadgets/textfield.gadget` integration: ensure `GADGETS:` assign resolves, gadget loads, text-entry preview widget renders
- [ ] Font-list interaction: click a font name; assert preview pane updates (text hook captures the chosen font name)
- [ ] Preview pane rendering: pixel-region assertion that the preview area shows non-background content after font selection
- [ ] At least one font is installed via lxa-apps test fixture (or test skips with `GTEST_SKIP()` if no fonts present)

**Test gate**: 3 new Typeface tests pass.

### Phase 147 — Deferred-paint trigger / forced full redraw

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

**Test gate**: DPaint V and KickPascal 2 deeper workflows reachable from tests.

### Phase 148 — DirectoryOpus deeper workflows

Phase 134 fixed the button-bank rendering. Remaining gaps:

- [ ] File-list navigation in lister panes (requires directory reading)
- [ ] Button-bank click workflows (requires `dopus.library` command dispatch — verify the library is supplied or stubbed correctly per Phase 142 policy; if third-party, **STOP and notify user**)
- [ ] Preferences panel interaction
- [ ] Title-bar version/memory string rendered by DOpusRT (which is currently not launched in the test fixture) — either launch DOpusRT or document why the title-bar string is intentionally absent

**Test gate**: At least 4 new DOpus tests covering the four areas above.

### Phase 149 — BlitzBasic 2 ted editor real text rendering

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

### Phase 150 — Menu introspection upgrade + non-releasing drag API

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

### Phase 151 — SysInfo hardware fields + Cluster2 EXIT button

Two small per-app correctness gaps grouped because they are both isolated coordinate/data-source issues.

- [ ] **SysInfo hardware fields**: `BattClock` and CIA timing fields that SysInfo reads — populate plausible values in the relevant ROM data (not real hardware detection; lxa reports a synthetic baseline machine)
- [ ] **Cluster2 EXIT button**: coordinate mapping mismatch in custom toolbar — Cluster2 renders its toolbar with custom coordinates; the EXIT button click coordinate calculation in `cluster2_gtest.cpp` does not match. Either the test coordinates are wrong, or lxa's IDCMP_MOUSEBUTTONS coordinate mapping is off for windows on a custom screen.
- [ ] New SysInfo test asserting BattClock/CIA fields are non-zero
- [ ] New Cluster2 test asserting EXIT button click terminates the app cleanly

**Test gate**: Both new tests pass.

---

## Performance (Phase 152)

### Phase 152 — Performance follow-up (placeholder, scheduled when needed)

No specific performance work is currently scheduled. When a measurable regression appears (test wall time, memory, CPU profile from `--profile` flag), open this phase with concrete objectives. Until then, this slot is reserved.

The standing performance baseline:
- Full test suite ~145–175 s wall time at `-j16`
- Per-EMU_CALL profiling available via `--profile <path>` + `tools/profile_report.py`

---

## New Features: RTG Retargetable Graphics (Phases 153–155)

> Strategic pivot: lxa's display strategy moves from ECS-era planar modes to RTG via Picasso96. Phases 153–155 deliver the full RTG stack — display backend, P96 library, and app validation. Scheduled after the quality + Amiga-compatibility blocks per the priority order.

### Phase 153 — RTG display foundation

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

### Phase 154 — Picasso96API.library core

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

### Phase 155 — RTG app validation

- [ ] **PPaint** (flagship): mode-scan loop populated via P96 enumeration; assert editor canvas reachable. ECS-path investigation remains abandoned (PPaint is RTG-only in practice).
- [ ] **FinalWriter**: re-enable `DISABLED_AcceptDialogOpensEditorWindow` in `finalwriter_gtest.cpp`; clicking OK opens editor window
- [ ] **One new productivity app** (Wordsworth or Amiga Writer): reaches main editor; text hook confirms content renders
- [ ] All existing drivers continue to pass

**Test gate**: PPaint editor reachable; FinalWriter editor reachable; new productivity app reaches main UI.

---

## Long-Term: External Process Emulation + Observability (Phases 156–158)

### Phase 156 — External process emulation + DOS stdout/clipboard capture

DevPac Assemble, ASM-One Assemble, BlitzBasic Run, KickPascal Compile/Run, BlitzBasic2 Compile all require this.

- [ ] AmigaDOS `CreateProc()` / `Execute()` launching real m68k subprocesses within the emulated environment, with stdout piped back to the host
- [ ] `lxa_capture_dos_output()` host API exposing AmigaDOS stdout to tests
- [ ] Clipboard capture API (clipboard.device hook)
- [ ] DevPac Assemble + ASM-One Assemble + BlitzBasic Run + KickPascal Compile each get a test asserting compiler output appears on captured stdout
- [ ] Update existing app drivers (DevPac, ASM-One, BlitzBasic 2, KickPascal) to exercise their compile/run workflows

**Test gate**: At least 4 compile/run workflow tests pass.

### Phase 157 — Screen-content diffing infrastructure

Currently no way to assert "this region looks like a text label" or "this area changed structurally between two states." A reference-image diff tool would let us write layout regression tests that survive minor color changes but catch structural regressions.

- [ ] Add `lxa_diff_capture(reference_png, captured_png, threshold)` host API
- [ ] Define a structural-similarity metric (SSIM or simple block-hash)
- [ ] Reference-image storage convention under `tests/references/`
- [ ] Migrate at least one existing pixel-count test to a structural-diff test

**Test gate**: New diff API has unit tests; one migrated test passes.

### Phase 158 — Audio device introspection

No tests currently require this. Open this phase only when the first audio-using app needs validation.

- [ ] Define what "observable audio output" means for tests (e.g. captured sample buffer, frequency analysis)
- [ ] Implement audio.device tap
- [ ] Add a test for an audio-using app

**Test gate**: First audio app validated.

---

## Far Future: GUI Toolkits + CPU Evolution

### Phase 159 (tentative) — MUI library hosting

MUI (`MUI:Libs/muimaster.library`) is a disk library — never reimplemented in ROM. This phase ensures lxa's BOOPSI infrastructure (icclass/modelclass/gadgetclass, utility.library tag machinery) is solid enough that a real `muimaster.library` binary opens and functions.

- [ ] Audit BOOPSI completeness against MUI's requirements
- [ ] Install user-supplied `muimaster.library` binary
- [ ] Test gate: at least one MUI-based app opens its main window

### Phase 160 (tentative) — ReAction / ClassAct hosting

ReAction (AmigaOS 3.5/3.9 GUI toolkit) runs as disk-provided class files (`SYS:Classes/reaction/`). This phase ensures lxa's BOOPSI layer and Intuition class dispatch are complete enough to host ReAction classes.

- [ ] Audit BOOPSI + Intuition class dispatch completeness
- [ ] Install user-supplied ReAction classes
- [ ] Test gate: at least one ReAction app opens its main window

### Phase 161 (tentative) — CPU core evaluation

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
Phase 142-144 (Library policy + datatypes + smoke)
        │
        ▼
Phase 145-146 (Devpac scrollbar + Settings window)
        │
        ▼
Phase 147-153 (App-specific compatibility)
        │
        ▼
Phase 152 (Performance — placeholder)
        │
        ▼
Phase 153-155 (RTG: foundation → P96 → app validation)
        │
        ▼
Phase 156-158 (External processes, diffing, audio)
        │
        ▼
Phase 159-161 (MUI, ReAction, CPU eval)
```
