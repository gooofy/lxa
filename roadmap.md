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

## Next Phase

### Phase 140 — Fix ProWriteInteractionTest.AboutDialogOpensAndCanBeDismissed

**Priority**: Quality. Currently `DISABLED_AboutDialogOpensAndCanBeDismissed` in `tests/drivers/apps_misc_gtest.cpp:1075`.

**Root cause hypothesis**: About requester opens, but the OK-button click / `IDCMP_CLOSEWINDOW` handling does not return ProWrite to the editor state the test expects. Candidate area: Phase 135 ActiveWindow routing change (RAWKEY routes to `IntuitionBase->ActiveWindow`) may have shifted requester-vs-editor active-window tracking.

**Sub-problems**:
1. Capture the IDCMP event log around the dismiss (Phase 131 `lxa_drain_intui_events()`).
2. Verify `ActiveWindow` is correctly restored to ProWrite's editor when the requester closes.
3. Cross-check against RKRM: requester close should restore the previously-active window of the same screen.

- [ ] Capture event log diagnostic
- [ ] Identify ActiveWindow state at dismiss time
- [ ] Fix routing if needed
- [ ] Re-enable test
- [ ] Full suite green

**Test gate**: `ProWriteInteractionTest.AboutDialogOpensAndCanBeDismissed` passes; full suite green.

---

## Completed Phases (Summary)

Detailed write-ups live in the git commit messages. This table is the single index.

| Phase | Description | Version |
|-------|-------------|---------|
| 106 | Test perf: headless display skip, idle detection (−38% wall time) | v0.8.68 |
| 107 | Test perf: persistent fixtures, SetUp dedup (−49% combined) | v0.8.69–70 |
| 108 | App testing: SysInfo + Cluster2; Musashi IRQ pulse fix; ExecBase fields | v0.8.71–72 |
| 108-d | Manual sweep: 13 fixes / 7 apps; GfxBase, GadTools, SMART_REFRESH, INTUITICKS | v0.8.73 |
| 109 | Missing IDCMP types (REQSET/REQCLEAR, SIZEVERIFY, MENUVERIFY, DELTAMOVE) | v0.8.74 |
| 110 | Third-party disk-library stubs: req, reqtools, powerpacker, arp | v0.8.75 |
| 111 | SMART_REFRESH backing store; CR-aware graphics; Save/RestoreBackingStore | v0.8.76 |
| 112 | Menu double-buffering: pixel-accurate save/restore via EMU_CALL | v0.8.77 |
| 113 | Blitter line-draw mode: Bresenham, all octants, ASH cross-word wrap | v0.8.78 |
| 114 | Copper list interpreter: MOVE/WAIT/SKIP, VBlank exec, color shadow | v0.8.79 |
| 115 | DPaint V driver: Screen Format gadgets, depth-2→8 transition | v0.8.80 |
| 116 | MaxonBASIC driver: text rendering, RMB menu, scrollbar, stress survival | v0.8.81 |
| 117 | DevPac driver: Amiga-key shortcut survival, full menu sweep | v0.8.82 |
| 118 | ASM-One driver: menu flicker guard, qualifier propagation survival | v0.8.83 |
| 119 | BlitzBasic 2 driver: editor-paint regression bounds, multi-menu stability | v0.8.84 |
| 120 | PPaint driver: probe-window geometry, GfxBase guards, capture survival | v0.8.85 |
| 121 | DirectoryOpus driver (10 tests); PPaint sharding bug fix | v0.8.86 |
| 122 | KickPascal 2 driver: EDITOR-mode reach, missing-library/PANIC guards | v0.8.87 |
| 123 | Typeface deferred — bgui.library v39+ required (lifted in 136) | — |
| 124 | FinalWriter driver: dialog coverage, sharding fix, broken accept disabled | v0.8.88 |
| 0 | CMake shard coverage audit; `tools/check_shard_coverage.py` registered | v0.8.89 |
| 125 | `lxa.c` decomposition: 9,960 → ~1,658 lines + new module files | v0.8.90 |
| 126 | Profiling: per-EMU_CALL counters/timing; `--profile` flag; report tool | v0.9.0 |
| 127 | Memory access fast paths: bswap RAM/ROM 16/32-bit; `__builtin_expect` | v0.9.1 |
| 128 | Display pipeline: dirty-region tracking, SSE2 planar→chunky, coalesced VBlank | v0.9.2 |
| 129 (partial) | Screen-mode emulation: 36 ECS modes, virtualization, BestModeIDA flags | v0.9.3 |
| 130 | Text hook: `lxa_set_text_hook()` via EMU_CALL; tests in api/dopus/kickpascal | v0.9.4 |
| 131 | Event log + menu introspection: `lxa_drain_intui_events()`, `lxa_get_menu_strip()` | v0.9.5 |
| 132 | SMART_REFRESH backing store validation; defensive workaround removed | v0.9.6 |
| 133 | Menu double-buffering: persistent compose BitMap; ASM-One/MaxonBASIC stable | v0.9.7 |
| 134 | DirectoryOpus button bank: settle budget 150 → 600 VBlank; text-hook label assertions | v0.9.8 |
| 135 | BlitzBasic 2 ted: off-screen requester recentre + RAWKEY → ActiveWindow | v0.9.9 |
| 136 | bgui.library / Typeface driver: 5 tests pass; gadget binaries staged | v0.9.10 |
| 136-h | datatypes.library hotfix: wired into ROM build (was orphan source) | v0.9.11 |
| 136-h2 | commodities.library wired into ROM; GADGETS: assign added | v0.9.12 |
| 136-b1 | bgui source build foundation: 44/49 BGUI files compile under bebbo | v0.9.13 |
| 136-b2 | bgui source build: all 49 files compile (varargs/baserel/getreg shims) | v0.9.14 |
| 136-b3 | lxa-built bgui.library replaces SAS/C prebuilt; `src/bgui/lxa_shims.c` | v0.9.15 |
| 136-b4 | bgui calling-convention fixes (R5/R6/R7); Typeface renders character grid | v0.9.16 |
| 136-c | Roadmap overhaul; screenshot_review retired to attic; 5 failing tests quarantined as DISABLED_ | v0.9.17 |
| 136-d | Roadmap+AGENTS policy: priority order, no pooling sections, every DISABLED_ test gets a phase | v0.9.18 |
| 137 | RunCommand exit-code propagation: NP_ExitCode/NP_ExitData honoured in CreateNewProc; LoadSegRuntime re-enabled | v0.9.19 |
| 138 | GadToolsGadgetsPixelTest.ResizeKeepsSizeGadgetBordersClean re-enabled (test-side readiness fix; SizeWindow updates Width/Height before render — wait for size-gadget pixels, not just dims) | v0.9.20 |
| 139 | GadToolsMenuPixelTest.SubmenuHoverDoesNotCorruptLowerMainItems re-enabled (no code change required; hang resolved by Phases 132/133 menu compose+backing-store work; verified stable across 5 consecutive runs) | v0.9.21 |

---

## Quality (Phases 138–141)

Re-enable each `DISABLED_` GTest. Done in priority order by user impact (DOS API correctness first, GadTools rendering next, app-interaction last).

### Phase 138 — see Completed Phases (v0.9.20).

### Phase 139 — see Completed Phases (v0.9.21).### Phase 140 — Fix ProWriteInteractionTest.AboutDialogOpensAndCanBeDismissed

**Priority**: Quality. Currently `DISABLED_AboutDialogOpensAndCanBeDismissed` in `tests/drivers/apps_misc_gtest.cpp:1075`.

**Root cause hypothesis**: About requester opens, but the OK-button click / `IDCMP_CLOSEWINDOW` handling does not return ProWrite to the editor state the test expects. Candidate area: Phase 135 ActiveWindow routing change (RAWKEY routes to `IntuitionBase->ActiveWindow`) may have shifted requester-vs-editor active-window tracking.

**Sub-problems**:
1. Capture the IDCMP event log around the dismiss (Phase 131 `lxa_drain_intui_events()`).
2. Verify `ActiveWindow` is correctly restored to ProWrite's editor when the requester closes.
3. Cross-check against RKRM: requester close should restore the previously-active window of the same screen.

- [ ] Capture event log diagnostic
- [ ] Identify ActiveWindow state at dismiss time
- [ ] Fix routing if needed
- [ ] Re-enable test
- [ ] Full suite green

### Phase 141 — Fix MiscTest.StressTasks

**Priority**: Quality. Currently `DISABLED_StressTasks` in `tests/drivers/misc_gtest.cpp:144`. Two assertion failures in the Stress sample (lines 33, 41) under sustained AddTask/RemoveTask churn over 60 s.

**Root cause hypothesis**: `exec.c` scheduler / signal-delivery race that only manifests under sustained task churn.

**Sub-problems**:
1. Reduce the stress-test workload until the failure is deterministic, then bisect to identify the minimal repro.
2. Audit `_exec_AddTask`, `_exec_RemoveTask`, signal delivery (`Signal`/`Wait`), and the ready-list manipulation for non-atomic sequences.
3. Cross-check Forbid/Permit nesting around task-list edits.

- [ ] Reduce repro
- [ ] Identify race
- [ ] Fix and re-enable
- [ ] Full suite green over 50 consecutive runs (reliability loop per `lxa-testing` skill)

---

## Amiga Compatibility (Phases 142–151)

Real-Amiga behaviour gaps. Each phase has explicit objectives derived from the formerly-pooled "Known Limitations" list.

### Phase 142 — Library policy cleanup: third-party stub removal

Correct the longstanding policy violation from Phase 110: third-party stubs (`req.library`, `reqtools.library`, `powerpacker.library`) must not exist in lxa. Real binaries supplied by the user, same as `bgui.library`.

- [ ] Delete `src/rom/lxa_reqlib.c`, `src/rom/lxa_reqtools.c`, `src/rom/lxa_powerpacker.c`
- [ ] Remove the corresponding `add_disk_library()` entries from `sys/CMakeLists.txt`
- [ ] real `req.library`, `reqtools.library`, `powerpacker.library` binaries are in others/ directory
- [ ] Install supplied binaries to `share/lxa/System/Libs/` (same pattern as `bgui.library`)
- [ ] Update tests that previously validated "stub returns failure cleanly" to validate real library functionality (or skip if binary missing)
- [ ] `arp.library`: treat as third-party (CBM did not ship with AmigaOS); apply same removal pattern
- [ ] DevPac File→Open requester now opens with real `req.library` (was stubbed silently failing)

**Test gate**: Full suite passes. Apps that depended on stub-returned failures (DevPac File→Open) still do not crash; with real binaries, the requester actually appears.

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

### Phase 145 — Typeface window chrome rendering

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
Phase 137-141 (Quality: re-enable DISABLED_ tests)
        │
        ▼
Phase 142-144 (Library policy + datatypes + smoke)
        │
        ▼
Phase 145-151 (App-specific compatibility)
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
