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

**Known open limitations** (not yet addressed):
- ASM-One / MaxonBASIC flickery menus — needs architectural double-buffered menu rendering
- BlitzBasic2 empty editor windows — copper/blitter-dependent rendering beyond basic interpreter
- KickPascal layout/menus — depends on deeper arp/req library functionality
- SysInfo hardware fields (BattClock, CIA timing) — requires hardware detection
- Cluster2 EXIT button — coordinate mapping mismatch in custom toolbar
- DPaint V main editor defers menu bar / toolbox / palette rendering until first mouse interaction; canvas remains blank-pen until tool/palette state is exercised. Palette/pencil/fill interaction tests deferred — require either the deferred-paint trigger to be reverse-engineered or a new "force full redraw" capability.
- DPaint V Ctrl-P (Screen Format reopen) only works on the depth-2 startup dialog, not from the depth-8 main editor. About dialog and File→Open requester not yet exercised — depends on resolving deferred-paint to make menu bar interactable.
- MaxonBASIC About/Settings/Run interaction: menus are Intuition-managed but reachable only via RMB drag through hardcoded coordinates; the German menu titles ("Projekt", "Editieren", "Suchen") and lack of programmatic menu introspection make item-level activation brittle. Phase 116 verified menu reveal and editor text rendering; deeper menu-driven workflows defer until host-side menu introspection (gap #4) is implemented.

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

### Phase 117: DevPac — assembler workflow
> Existing driver: `devpac_gtest.cpp`

**Goal**: Verify the assembler IDE opens cleanly, can edit source, and invokes the assembler.

- [ ] Audit current driver coverage
- [ ] Startup: verify editor window title and menu bar (screenshot review)
- [ ] About dialog: open, verify AmigaGuide stub does not crash (regression from Phase 110)
- [ ] File requester: File → Open, verify req.library stub allows requester to appear or fail gracefully
- [ ] Type source: inject a trivial m68k source snippet into editor
- [ ] Assemble: invoke Assemble action via menu (RMB drag); verify output window or error requester
- [ ] Error navigation: if assembler reports an error, verify cursor moves to error line
- [ ] Screenshot review all states; pixel regression guards

---

### Phase 118: ASM-One — assembler and monitor
> Existing driver: `asm_one_gtest.cpp`

**Goal**: Verify ASM-One's monitor/editor, menu system, and assemble workflow. ASM-One uses raw IDCMP keyboard events (no COMMSEQ) — all menu interaction must use RMB drag.

- [ ] Audit current driver coverage
- [ ] Startup: verify monitor window opens, prompt visible (screenshot review)
- [ ] Menu system: open and navigate all top-level menus via RMB drag; verify no hang or crash
- [ ] Flicker check: capture before/after menu open — confirm double-buffering holds (compare captures)
- [ ] Assemble a trivial snippet: inject source lines, invoke assemble, verify result
- [ ] Event queue: inject rapid mouse moves; verify no queue overflow (regression from Phase 108-d)
- [ ] Qualifier propagation: hold shift + click, verify qualifier bits set in events
- [ ] Screenshot review; pixel/interaction regression guards

---

### Phase 119: BlitzBasic 2 — editor rendering
> Existing driver: `blitzbasic2_gtest.cpp`

**Goal**: Re-test after blitter line mode (Phase 113) and copper interpreter (Phase 114). Determine whether the "ted" editor now renders correctly.

- [ ] Startup: screenshot review — does the editor window now show text content?
- [ ] If editor renders: inject text, verify it appears; test cursor movement
- [ ] If editor still blank: capture + vision model analysis; identify which rendering primitive is missing; update infrastructure gap list
- [ ] Menu interaction: verify menus open and close cleanly
- [ ] Run a trivial BlitzBasic program if editor is functional
- [ ] Document exactly which features remain broken and why; update known limitations

---

### Phase 120: PPaint — pixel painting
> Existing driver: startup test only

**Goal**: Extend beyond startup to verify PPaint's drawing tools and palette work correctly.

- [ ] Audit startup test; screenshot review of initial state
- [ ] Palette: verify 32-color palette bar renders; click a color, verify selection indicator changes
- [ ] Drawing tools: test pencil/line tool on canvas; verify pixels placed correctly
- [ ] Screen format: verify PAL screen opens at correct resolution (DisplayFlags regression check)
- [ ] File requester: File → Open via menu, verify reqtools stub allows graceful handling
- [ ] About dialog: open and close cleanly
- [ ] Zoom view: if accessible via menu, test zoom in/out; verify canvas scales
- [ ] Screenshot review all states; pixel regression guards

---

### Phase 121: DirectoryOpus — file manager
> Existing driver: startup test only (or none)

**Goal**: Test the most-used file manager on the Amiga. DOpus has a complex gadget-driven UI.

- [ ] Create or audit startup driver; verify both file list panes open (screenshot review)
- [ ] File list: verify directory entries appear in at least one pane
- [ ] Toolbar buttons: enumerate gadgets via `GetGadgetInfo()`; click each button, verify response
- [ ] Path entry: type a path into the location gadget, verify navigation
- [ ] About dialog: Help → About, verify dialog opens and closes
- [ ] Preferences: open Preferences window, verify gadgets present, cancel cleanly
- [ ] Identify any custom-rendered UI (like Cluster2); document coordinate-based fallback if needed
- [ ] Screenshot review all states; regression guards

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
