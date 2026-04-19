# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, and roadmap updates on every completed phase.

---

## Completed Phases (Summary)

| Phase | Description | Version |
|-------|-------------|---------|
| 106 | Test performance: headless display skip, idle detection, cycle budget cuts (−38% wall time) | v0.8.68 |
| 107 | Test performance: persistent fixtures, SetUp deduplication (−49% combined with 106) | v0.8.69–70 |
| 108 | App testing: SysInfo + Cluster2 drivers; Musashi IRQ pulse fix; ExecBase fields | v0.8.71–72 |
| 108-d | Manual testing sweep: 13 fixes across 7 apps; event queue, GfxBase, GadTools, SMART_REFRESH, IDCMP_INTUITICKS | v0.8.73 |
| 109 | Missing IDCMP types (REQSET/REQCLEAR, SIZEVERIFY, MENUVERIFY, DELTAMOVE); GfxBase audit | v0.8.74 |
| 110 | Third-party disk library stubs: req, reqtools, powerpacker, arp | v0.8.75 |
| 111 | SMART_REFRESH full backing store: CreateObscuredClipRect, Save/RestoreBackingStore, CR-aware graphics | v0.8.76 |
| 112 | Menu double-buffering: pixel-accurate save/restore; native BltBitMapCore via EMU_CALL | v0.8.77 |
| 113 | Blitter line-draw mode: Bresenham, all octants, SING/ONEDOT, ASH cross-word wrap | v0.8.78 |
| 114 | Copper list interpreter: MOVE/WAIT/SKIP, VBlank execution, CDANG, color register shadow | v0.8.79 |
| 115 | DPaint V driver expansion: Screen Format gadgets, depth-2→8 transition, main editor render | v0.8.80 |
| 116 | MaxonBASIC driver expansion: text rendering, RMB menu reveal, scrollbar gadgets, stress survival | v0.8.81 |
| 117 | DevPac driver expansion: Amiga-key shortcut survival, full menu bar sweep, req.library stub validation | v0.8.82 |
| 118 | ASM-One driver expansion: menu flicker guard, event-queue rapid-motion stress, qualifier-key propagation survival | v0.8.83 |
| 119 | BlitzBasic 2 driver expansion: editor-paint regression bounds, secondary-window tracking, multi-menu stability, editor keyboard survival | v0.8.84 |
| 120 | PPaint driver expansion: probe-window geometry bounds, GfxBase regression guards, null-BitMap probe guards, capture-pipeline survival; ROM path helper fix | v0.8.85 |
| 121 | DirectoryOpus driver creation: 10 tests covering startup, bundled-library resolution, geometry, content rendering, gadget enumeration safety, capture pipeline, menu-probe survival, idle-time stability; PPaint sharding bug fix (orphaned Z-tests) | v0.8.86 |

**Known open limitations** (not yet addressed):
- ASM-One / MaxonBASIC flickery menus — needs architectural double-buffered menu rendering
- KickPascal layout/menus — depends on deeper arp/req library functionality
- SysInfo hardware fields (BattClock, CIA timing) — requires hardware detection
- Cluster2 EXIT button — coordinate mapping mismatch in custom toolbar
- DPaint V main editor defers menu bar / toolbox / palette rendering until first mouse interaction; canvas remains blank-pen until tool/palette state is exercised. Palette/pencil/fill interaction tests deferred — require either the deferred-paint trigger to be reverse-engineered or a new "force full redraw" capability.
- DPaint V Ctrl-P (Screen Format reopen) only works on the depth-2 startup dialog, not from the depth-8 main editor. About dialog and File→Open requester not yet exercised — depends on resolving deferred-paint to make menu bar interactable.
- MaxonBASIC About/Settings/Run interaction: menus are Intuition-managed but reachable only via RMB drag through hardcoded coordinates; the German menu titles ("Projekt", "Editieren", "Suchen") and lack of programmatic menu introspection make item-level activation brittle. Phase 116 verified menu reveal and editor text rendering; deeper menu-driven workflows defer until host-side menu introspection (gap #4) is implemented.
- DevPac File→Open requester: Phase 117 verified Amiga-O does not crash, but req.library stub returns failure cleanly so no requester appears. Real file requester behavior requires a non-stub req.library implementation (out of scope per AGENTS.md "do not re-implement third-party libraries in ROM"). Assemble/Run workflow not exercised — would require a loaded valid m68k source and external assembler invocation, both of which need external-process emulation.
- BlitzBasic2 ted editor — copper/blitter improvements (Phase 113/114) reach the surface (~13% of editor body fills with paint after menu interaction) but ted still does not render real editable text content. Phase 119 captured vision-model analysis: menu bar and dropdowns render correctly (PROJECT/EDIT/SOURCE/SEARCH/COMPILER), but the editor body remains a flat surface with only menu-residue and status overlay paint. Root cause is deeper than line-mode blitter and copper colour cycling — likely involves blitter copy modes, sprite hardware, or specific copper waits that ted uses for its custom overlay. Test bounds: editor body must show >100 non-grey pixels (proves paint reaches the surface) but <50% fill (catches the day ted finally renders content).
- ASM-One Assemble/Run workflow not exercised (Phase 118) — same external-process constraint as DevPac; deferred until external-process emulation exists. Menu pixel-flicker verification uses a content-floor guard (before→after cannot collapse to <10% of before) because ASM-One defers editor repaint until first interaction, so pixel counts legitimately *grow* after the first menu drag. Host-side observation of IDCMP qualifier bits is not currently possible; qualifier propagation is guarded via survival + subsequent-input responsiveness only.
- PPaint exits early with rv=26 from CloantoScreenManager screen-mode probing — Phase 120 vision-model review confirmed the probe windows are empty 320×200 (and one 640×256) buffers with no UI content drawn before exit. Real PPaint editing UI (palette, canvas, toolbox) is never reached, so all menu/palette/drawing-tool tests in the original Phase 120 plan are infeasible. Coverage instead guards what works: probe-window geometry bounds, no missing-library/null-BitMap/PANIC log entries, and probe-window capture pipeline (capture taken in SetUp before exit). Root cause is deeper screen-mode emulation that the probe expects but lxa does not yet provide.
- Menu pixel introspection during a drag: `lxa_inject_drag` performs press → drag → release atomically, so we cannot capture screen state while a menu is open. Tests verify menu interaction via side effects (window count, app survival) instead. A future infrastructure improvement would be a non-releasing drag API or a "capture during drag" hook.
- DirectoryOpus 4 renders a skeleton UI in lxa: window opens at 640×245, dual file-lister frames + scroll gadgets + small button cluster (B/R/S/A) + bottom toolbar (?/E/F/C/I/Q) all draw correctly, but text labels, the title-bar version/memory string, and the famous main button bank (Copy/Move/Delete/etc.) are absent. Vision-model review (Phase 121) attributed this to font/path resolution or `dopus.config` parsing not completing. DOpus also requests `commodities.library` and `inovamusic.library`, which lxa does not ship as stubs; both are tolerated by DOpus itself but logged as missing-library errors. File-list navigation, button-bank workflows, and Preferences interaction tests deferred until the skeleton is filled in.
- CMake test sharding with hardcoded GTest filters can silently orphan newly added tests. Phase 121 fixed the PPaint shard (its filter listed only the original 3 names — including a non-existent one — so the 4 Phase 120 Z-tests never ran via ctest). Future test additions to *any* sharded driver (`simplegad`, `simplemenu`, `menulayout`, `fontreq`, `simplegtgadget`, `talk2boopsi`, `gadtoolsgadgets`, `vim`, `finalwriter`, `sigma`) MUST update the corresponding `add_gtest_driver_shard` FILTER strings in `tests/drivers/CMakeLists.txt`. Verified Phase 121: SIGMAth2 shards are currently complete; remaining sharded drivers were not re-audited.

---

## Active Development

### Observability and Test Infrastructure

> **Philosophy**: Before sweeping through apps, ensure we can truly *see* and *experience* how they behave — both in automated CI runs and in AI-driven investigation sessions. Every gap in observability is a gap in correctness confidence.

#### Current Capabilities
- `lxa_capture_window()` / `lxa_capture_screen()` — PNG artifact capture on failure
- `lxa_read_pixel()` / pixel-count change detection — basic visual regression
- `lxa_get_gadget_count()` / `lxa_get_gadget_info()` / `LxaTest::ClickGadget()` — Intuition gadget introspection
- `lxa_wait_window_drawn()` / `LxaTest::WaitForWindowDrawn()` — event-driven UI readiness
- `tools/screenshot_review.py` — OpenRouter-backed vision model analysis for failure artifacts
- `lxa_inject_string()`, `lxa_inject_drag()`, `lxa_inject_mouse_click()` — input injection

#### Infrastructure Gaps (address opportunistically during app sweep)

These gaps make it hard to write reliable tests and to understand *why* an app looks or behaves wrong:

1. **No screen-content diffing**: Tests compare individual pixels or pixel counts. There is no way to assert "this region looks like a text label" or "this area changed between two states." A reference-image diff tool (host-side, deterministic, no vision model) would let us write layout regression tests that survive minor color changes but catch structural regressions.

2. **No text extraction from screen**: We cannot assert that a specific string is visible anywhere in a window. Apps render text via ROM `Text()` calls — if we intercept and log those calls (position, string, font), tests could assert `"About MaxonBASIC"` appeared at roughly the right location without pixel-exact matching.

3. **No window/screen event log**: There is no structured log of Intuition events (OpenWindow, CloseWindow, OpenScreen, requester open/close). Tests infer these from window counts or pixel changes. An event log would make test assertions more semantic and less fragile.

4. **No menu-state introspection**: We cannot query which menus/items exist, their names, enabled/disabled state, or checkmark state from the host side. Menu testing currently relies on RMB drag by hardcoded coordinates.

5. **No clipboard / string output capture**: Apps that write to AmigaDOS stdout or use the clipboard have no way to expose output to the host test. A `lxa_capture_dos_output()` API would allow asserting compiler output, BASIC program results, etc.

6. **Vision model loop not automated**: `screenshot_review.py` is a manual tool. There is no test-phase hook that automatically invokes it when a pixel assertion fails and attaches the structured analysis to the test report.

7. **No audio state introspection**: Apps that use audio.device have no observable output in tests.

8. **No timing/performance baseline per app**: We know total suite wall time but not per-app startup latency. Regressions in emulation speed are invisible until the whole suite slows down.

**Priority for app sweep**: Address gaps 1–5 incrementally as specific app tests expose the need. Do not pre-build infrastructure that no test requires yet.

---

## App Testing Sweep (Phases 115–124)

> **Ground rules for every app phase:**
> 1. Run (or create) a startup driver — verify the app reaches an interactive state.
> 2. Capture a screenshot; use `tools/screenshot_review.py` to audit the visual state.
> 3. Identify what the app *should* do on a real Amiga (consult RKRM, app manuals, existing knowledge).
> 4. Write interaction tests that exercise core workflows — not just "does it open."
> 5. Any new infrastructure gap encountered goes into the list above *and* gets a concrete ticket.
> 6. End each phase with pixel/geometry regression assertions so CI catches future regressions.

---

### Phase 118: ASM-One — assembler and monitor ✅
> Existing driver: `asm_one_gtest.cpp` (15 tests, v0.8.83)

**Goal**: Verify ASM-One's monitor/editor, menu system, and assemble workflow. ASM-One uses raw IDCMP keyboard events (no COMMSEQ) — all menu interaction must use RMB drag.

- [x] Audit current driver coverage (11 pre-existing tests covered startup, menu bar RMB, About dialog, file requester, editor input, cursor keys)
- [x] Startup: monitor window, prompt visible (pre-existing `StartupSkipsAllocateWorkspacePromptAndReachesEditor`)
- [x] Menu system: RMB drag opens Project menu without hang (pre-existing `MenuBarInteractionDoesNotLogInvalidRastPortBitmap` + About/FileRequester paths)
- [x] Flicker check: `ZMenuFlickerCheck` — before/after menu-cancel pixel counts; content must not collapse
- [x] Event queue: `ZEventQueueSurvivesRapidMouseMoves` — 300 rapid motion events, app stays alive and responsive
- [x] Qualifier propagation: `ZQualifierPropagationShiftedKeys` — shifted/ctrl keypresses plus plain-key follow-up survive cleanly
- [ ] Assemble a trivial snippet — **deferred** (external-process constraint, see Known Limitations)
- [x] Screenshot review and pixel/interaction regression guards (captures on About/FileRequester paths)

---

### Phase 119: BlitzBasic 2 — editor rendering ✅
> Existing driver: `blitzbasic2_gtest.cpp` (9 tests, v0.8.84)

**Goal**: Re-test after blitter line mode (Phase 113) and copper interpreter (Phase 114). Determine whether the "ted" editor now renders correctly.

- [x] Startup screenshot review — vision model confirmed editor body remains flat grey at startup; menu bar and dropdowns render correctly
- [x] Menu interaction: `YMultipleMenuOpensRemainStable` opens and cancels all 5 top-level menus (PROJECT/EDIT/SOURCE/SEARCH/COMPILER); none crash, hang, or spawn extra windows
- [x] Editor still does not render real text content — Phase 113/114 paint DOES reach the surface (~13% fill after interaction) but is menu-residue and status overlay, not editable text. Bounds checked by `YEditorSurfaceShowsContentAfterMenuInteraction` (>100 pixels prove paint, <50% catches the day it fills)
- [x] Editor keyboard input survival: `YEditorAcceptsKeyboardInputWithoutCrash` types into the editor, app stays alive, no PANIC
- [x] Secondary window tracking: `YSecondaryWindowIsTracked` confirms both main (640x244) and ted control overlay (292x75) are enumerated
- [ ] Run a trivial BlitzBasic program — **deferred** (depends on functional editor; same external-process constraint as DevPac/ASM-One Assemble)
- [x] Known limitations updated (see top of roadmap): documented exact paint percentage, vision-model findings, and test bounds for future regression detection

---

### Phase 120: PPaint — pixel painting ✅
> Existing driver: `ppaint_gtest.cpp` (7 tests, v0.8.85)

**Goal**: Extend beyond startup to verify PPaint behaviour given the well-documented CloantoScreenManager rv=26 early-exit pattern.

**Outcome**: Vision-model review confirmed PPaint's probe windows are empty (no palette / canvas / toolbox / menu bar drawn before rv=26). Real editing UI is never reached, so palette / drawing-tool / file-requester / About / zoom tests are infeasible without deeper screen-mode emulation. Coverage instead guards what works:

- [x] Audited startup test; vision-model review of probe-window capture
- [x] **Probe geometry bounds**: `ZScreenModeProbeProducesValidGeometry` asserts probe windows are 320..1280 × 200..1024
- [x] **GfxBase regression guard**: `ZGfxBaseDisplayFlagsSetForPalDetection` ensures no `DisplayFlags=0` or missing-library log lines (AGENTS.md §6.11 root cause for an earlier rv=26)
- [x] **Null-BitMap probe guard**: `ZNoBitmapNullDereferenceDuringProbe` checks the rapid screen-open/close sequence does not trigger `invalid RastPort BitMap` errors or PANIC
- [x] **Capture-pipeline survival**: `ZProbeWindowCanBeCapturedAsArtifact` captures the probe window in SetUp (before rv=26) so failure-triage artifacts are always available
- [x] Fixed hardcoded ROM path in driver (now uses `FindRomPath()` helper) — driver now portable across developer machines and CI
- [ ] Palette / drawing tools / About / zoom — **deferred** (require UI that PPaint never reaches; documented in known limitations)

---

### Phase 121: DirectoryOpus — file manager ✅
> New driver: `dopus_gtest.cpp` (10 tests, v0.8.86)

**Goal**: Establish a startup-and-rendering baseline for Directory Opus 4 — the most-used third-party file manager on the Amiga — and exercise its custom-rendered UI without depending on libraries lxa does not ship.

**Outcome**: DOpus reaches an interactive state and renders a skeleton UI (file-lister frames, scroll gadgets, mini-button cluster, bottom toolbar) but text labels and the main button bank do not paint (vision-model attribution: font/path resolution or `dopus.config` parsing). Bundled disk libraries (`dopus`, `arp`, `iff`, `powerpacker`, `Explode.Library`, `Icon.Library`) all resolve via lxa's automatic PROGDIR libs/ prepend — no stubs needed in ROM. `commodities.library` and `inovamusic.library` are absent and tolerated by DOpus. Coverage focuses on what works:

- [x] **Driver created from scratch** (no prior coverage); `dopus_gtest.cpp` with 10 tests, all passing.
- [x] **Startup**: `StartupOpensVisibleMainWindow` asserts a tracked window with plausible PAL geometry (320..1280 × 180..800).
- [x] **Bundled-library resolution**: `StartupBundledLibrariesLoad` verifies each of the 6 shipped DOpus libraries does *not* appear in a missing-library log line; commodities/inovamusic deliberately excluded.
- [x] **Stability**: `StartupHasNoPanicOrBitmapErrors` and `StartupRemainsRunning` guard against PANIC, null-BitMap rendering paths, and premature exits.
- [x] **Rendering proof**: `MainWindowContainsRenderedContent` requires >500 non-grey pixels in the content region — confirms DOpus actually paints something, not just opens an empty window.
- [x] **Custom-UI safety**: `GadgetEnumerationIsSafe` calls `GetGadgetCount(0)` and asserts a non-negative result; DOpus draws most controls by hand so a low gadget count is expected.
- [x] **Capture pipeline**: `StartupCaptureProducesArtifact` saves `/tmp/dopus_startup.png` and validates the PNG is non-trivial — used for vision-model triage.
- [x] **Menu probe survival**: `ZRightMouseMenuProbeKeepsWindowAlive` performs an RMB drag in the menu-bar band; verifies window survives without resizing.
- [x] **Idle stability**: `ZWindowGeometryStableAcrossSettle` and `ZContentPixelCountSurvivesIdlePeriod` confirm the UI persists across an idle period (catches INTUITICKS / refresh corruption regressions).
- [x] **Sharding bug fix**: PPaint test target was sharded with a hardcoded filter that excluded the 4 Phase 120 Z-tests (and referenced a non-existent test name). Replaced with `add_gtest_driver(ppaint_gtest)` so all 7 PPaint tests run via ctest. SIGMAth2 shards audited and confirmed complete.
- [ ] File-list navigation, button-bank workflows, About dialog, Preferences — **deferred** (require text rendering and main button bank to paint; documented in known limitations).

---

### Phase 122: KickPascal 2 — Pascal IDE
> Existing driver: startup test only (or none)

**Goal**: Verify the Pascal IDE opens correctly with req.library stub in place (Phase 110).

- [ ] Startup: verify editor window opens without crash; screenshot review
- [ ] Menu bar: verify all menus present and navigate cleanly via RMB drag
- [ ] Editor: inject a short Pascal program; verify text appears
- [ ] Compile: invoke compile action; verify output window or error dialog (even if compile fails gracefully)
- [ ] req.library: verify any file requester triggered by the app fails gracefully (not crashes)
- [ ] arp.library: same check for any arp-dependent paths
- [ ] Screenshot review; pixel/interaction regression guards
- [ ] Document remaining limitations (deep library functionality)

---

### Phase 123: Typeface — font previewer
> Existing driver: startup test only (or none)

**Goal**: Verify the font previewer renders fonts correctly — a key test for `Text()` rendering.

- [ ] Startup: verify main window opens with font list; screenshot review
- [ ] Font list: select a different font; verify preview area updates (pixel change)
- [ ] Preview text: verify rendered characters are legible (vision model analysis on capture)
- [ ] Font requester: if accessible, open font requester; verify dialog appears
- [ ] Zoom / size change: adjust preview size; verify re-render
- [ ] About dialog: open and close
- [ ] Text rendering regression: this app is a natural regression guard for `Text()` — add pixel assertions for known-good glyph positions
- [ ] Screenshot review all states

---

### Phase 124: FinalWriter — word processor
> Existing driver: `finalwriter_gtest.cpp` (startup + basic interaction)

**Goal**: Extend to document editing, menu interaction, and requester dialogs.

- [ ] Audit current driver coverage; screenshot review of startup state
- [ ] Document editing: inject text into the document area; verify it appears
- [ ] Menu bar: test Format, View, and File menus via RMB drag
- [ ] Font dialog: open font chooser (reqtools-based); verify graceful handling with stub
- [ ] File → New / File → Open: verify requesters appear or fail gracefully
- [ ] Scroll: inject cursor-down keypresses on a multi-line document; verify scroll
- [ ] Print requester: trigger print dialog; verify it opens and can be cancelled
- [ ] Screenshot review all states; pixel regression guards

---

## Mid-term: Architecture and Performance

- **Phase 125**: `lxa.c` decomposition — split the ~9,400-line monolith into `lxa_memory.c`, `lxa_custom.c`, `lxa_dispatch.c`, `lxa_dos_host.c`. Refactoring only; all tests must pass before and after.

- **Phase 126**: Profiling infrastructure — `--profile` flag with per-ROM-function call counts and cycle costs; `perf`-friendly build config; top-10 hotspot report.

- **Phase 127**: Memory access fast paths — direct word/long-aligned loads in `m68k_read/write_memory_16/32` for RAM/ROM; `__builtin_expect()` hints. Target: 2× reduction in per-instruction host overhead.

- **Phase 128**: Display pipeline optimization — dirty-region tracking; SIMD planar-to-chunky; eliminate redundant SDL texture uploads.

---

## Long-term: CPU Emulation Evolution

Musashi (MIT, C89, interpreter) is adequate for correctness but limits throughput. No clean permissively-licensed x86-64 68k JIT exists today. Options assessed: Moira (MIT, no 68030 yet), UAE JIT (GPL, framework-entangled), Emu68 (MPL-2.0, ARM-only), Unicorn/QEMU (GPL).

**Recommendation**: Stay on Musashi. Host-side overhead dominates (~95% of wall time). Revisit when Moira adds 68030 support or host optimizations are exhausted.

- **Phase 129** (tentative): CPU core evaluation — re-evaluate Moira 68030 support; prototype if available; assess whether 68020 mode suffices for all tested apps.
- **Phase 130** (tentative): Production readiness assessment — define use-case targets; set performance baselines; decide on JIT investment.

---

*Each phase must include updated GTest host-side drivers under `tests/drivers/`, regression assertions, and saved failure artifacts. Use `tools/screenshot_review.py` during investigation, then translate findings into deterministic pixel/geometry assertions for CI.*
