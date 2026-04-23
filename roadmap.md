# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, **zero failing tests at phase completion**, and roadmap updates on every completed or deferred phase.

---

## Project Vision

lxa is the foundation for a modern Amiga on commodity hardware (Linux PC, Raspberry Pi). By targeting OS-compliant applications only, lxa bypasses chip-register emulation and goes straight to RTG (Retargetable Graphics), enabling full true-color displays at arbitrary resolutions on any SDL2-capable host.

The primary target applications are productivity software (Wordsworth, Amiga Writer, PageStream) and IDEs (Storm C, BlitzBasic 3). MUI and ReAction are important GUI toolkit goals. The long-term goal is for lxa to become a **viable development platform for Amiga software on Linux**.

**RTG strategy**: Implement Picasso96 (`Picasso96API.library`) as the RTG system. PPaint is the flagship RTG validation target. A thin `cybergraphics.library` compatibility shim is included so apps that probe for CGX also work.

---

## Next Phase

### Phase 137 — RTG display foundation

**Goal**: Land the chunky-bitmap and SDL2 RGBA pipeline that all subsequent RTG work depends on. This is the gateway to PPaint, FinalWriter, and modern productivity-app validation.

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

**Test gate**: `rtg_gtest` passes; full suite remains green (zero failures, zero timeouts).

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

---

## Planned Work

### Library Policy Cleanup (Phases 141–143)

These phases correct a longstanding policy violation: several third-party library stubs (`req.library`, `reqtools.library`, `powerpacker.library`) were implemented in lxa in Phase 110. This was wrong — these are not AmigaOS system libraries and must never be implemented or emulated by lxa. Real binaries must be supplied by the user. Similarly, `datatypes.library` is a genuine AmigaOS system library that deserves a full, correct implementation rather than the minimal stub introduced in Phase 136-h.

#### Phase 141 — Remove third-party library stubs; wire in real binaries
- Delete `src/rom/lxa_reqlib.c`, `src/rom/lxa_reqtools.c`, `src/rom/lxa_powerpacker.c`.
- Remove the corresponding `add_disk_library()` entries from `sys/CMakeLists.txt`.
- Locate or request real binaries; install to `share/lxa/System/Libs/` (same pattern as `bgui.library`).
- Update tests that previously validated "stub returns failure cleanly" to validate real library functionality (or skip if binary missing).
- `arp.library` is ambiguous — treat as third-party (CBM did not ship it with AmigaOS); apply the same removal pattern.

**Test gate**: Full suite passes. Apps that depended on stub-returned failures (DevPac File→Open) should still not crash.

#### Phase 142 — datatypes.library full implementation
Replace the Phase 136-h stub with a complete, RKRM-correct implementation per the AmigaOS 3.x NDK and RKRM: Libraries.

Key areas: `ObtainDataType()`/`ReleaseDataType()`, `NewDTObject()`/`DisposeDTObject()`, `GetDTAttrs()`/`SetDTAttrs()`, `DoDTMethod()`, `DrawDTObject()`, `FindToolNode()`, `GetDTMethods()`, DataTypes class loading from `SYS:Classes/DataTypes/`, built-in detection for ILBM/8SVX/FTXT/ANIM.

**Test gate**: Apps using `datatypes.library` (picture viewers, sound players, Workbench icon loading) open DataTypes objects without crashing. New `datatypes_gtest.cpp` covering core lifecycle + at least one IFF type.

#### Phase 143 — Startup smoke test suite
Catch "app crashes on startup" regressions automatically, before a dedicated driver is written. The Typeface crash (datatypes.library NULL pointer) went undetected because no test existed yet for Typeface.

- Add `smoke_gtest.cpp`: parameterised test that, for each known app binary in `lxa-apps/`, loads the app, runs 100 VBlank settle iterations, and asserts: (1) no SIGABRT, (2) no `PANIC` in output, (3) no `rv=26` unless allowlisted.
- Self-contained: missing app binary → `GTEST_SKIP()` rather than failure.

**Test gate**: All currently known apps that open windows pass the smoke test.

---

### RTG: Retargetable Graphics (Phases 137–139)

Phase 137 is the **Next Phase** above. The follow-up phases:

#### Phase 138 — Picasso96API.library core
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

`RenderInfo { APTR Memory; WORD BytesPerRow; WORD pad; RGBFTYPE RGBFormat; }` maps onto the Phase 137 chunky BitMap representation.

**Test gate**: `p96_gtest` passes; at least one target app opens the library without exiting early.

#### Phase 139 — RTG app validation
- **PPaint** (flagship): mode-scan loop populated via P96 enumeration; assert editor canvas reachable.
- **FinalWriter**: re-enable `DISABLED_AcceptDialogOpensEditorWindow`; clicking OK opens editor window.
- **One new productivity app** (Wordsworth or Amiga Writer): reaches main editor; text hook confirms content renders.
- **Typeface chrome gap** (carryover from Phase 136-b4): window currently renders 194×138 vs target 280×195 — missing window border, title bar, and vertical scrollbar. Investigate whether this is RTG-related or a BGUI layout issue; fix during the validation pass.

**Test gate**: PPaint editor reachable; FinalWriter editor reachable; new productivity app reaches main UI.

---

### Long-Term: Extended Coverage (Phase 140+)

#### Phase 140 — External process emulation
DevPac Assemble, ASM-One Assemble, BlitzBasic Run, KickPascal Compile/Run all require this. Needed: AmigaDOS `CreateProc()` / `Execute()` launching a real m68k subprocess within the emulated environment, with stdout piped back to the host. Prerequisites: Phase 131 (event log), Phase 130 (capture output), and a stable foundation from all prior phases.

#### Far Future: GUI Toolkits + CPU Evolution
- **MUI library stubs** (tentative): MUI runs as a disk library. Goal: BOOPSI infrastructure (icclass/modelclass/gadgetclass) solid enough that real `muimaster.library` opens and functions. Test gate: at least one MUI app opens its main window.
- **ReAction / ClassAct stubs** (tentative): runs as disk-provided class files (`SYS:Classes/reaction/`). Goal: BOOPSI + Intuition class dispatch complete enough to host ReAction classes.
- **CPU core evaluation**: stay on Musashi (host overhead dominates per Phase 126). Re-evaluate Moira if 68030 support lands; assess JIT investment when host optimizations are exhausted.

---

## Observability and Test Infrastructure

> **Philosophy**: Before sweeping through apps, ensure we can truly *see* and *experience* how they behave — both in automated CI runs and in AI-driven investigation sessions. Every gap in observability is a gap in correctness confidence.

### Current Capabilities
- `lxa_capture_window()` / `lxa_capture_screen()` — PNG artifact capture; attach the PNG directly to assistant messages for visual triage (the OpenCode harness handles image input natively; see `graphics-testing` skill §8).
- `lxa_read_pixel()` / pixel-count change detection — basic visual regression.
- `lxa_get_gadget_count()` / `lxa_get_gadget_info()` / `LxaTest::ClickGadget()` — Intuition gadget introspection.
- `lxa_wait_window_drawn()` / `LxaTest::WaitForWindowDrawn()` — event-driven UI readiness.
- `lxa_set_text_hook()` (Phase 130) — semantic text-render assertions.
- `lxa_drain_intui_events()` / `lxa_get_menu_strip()` (Phase 131) — window/menu introspection.
- `lxa_inject_string()` / `lxa_inject_drag()` / `lxa_inject_mouse_click()` — input injection.

### Open Infrastructure Gaps
1. **No screen-content diffing**: no way to assert "this region looks like a text label" or "this area changed between two states." A reference-image diff tool would let us write layout regression tests that survive minor color changes but catch structural regressions.
2. **No clipboard / DOS stdout capture** (Phase 140+ adjacency): apps writing to AmigaDOS stdout or the clipboard have no host-test exposure. A `lxa_capture_dos_output()` API would let us assert compiler output, BASIC program results, etc.
3. **No menu-state capture during a drag**: `lxa_inject_drag` performs press → drag → release atomically. Tests verify menu interaction via side effects (window count, app survival). A non-releasing drag API or "capture during drag" hook would close this gap.
4. **No audio state introspection**: apps using `audio.device` produce no observable test output. No tests require this yet.

---

## Deferred Test Failures

These tests are quarantined as `DISABLED_<TestName>` and must be fixed in dedicated phases. Listed in priority order.

1. **`DosTest.LoadSegRuntime`** (`tests/drivers/dos_gtest.cpp:283`) — real implementation bug: `_dos_RunCommand` in `src/rom/lxa_dos.c` does not propagate the spawned child's exit code back to the caller. Test 3 of the LoadSegRuntime sample asserts the wrong return code. **Priority**: high (correctness bug in a DOS API).
2. **`GadToolsGadgetsPixelTest.ResizeKeepsSizeGadgetBordersClean`** (`tests/drivers/gadtoolsgadgets_gtest.cpp:962`) — real rendering bug: after a window resize, the size-gadget border has zero non-background pixels. Likely the layer-rebuild path in `_intuition_SizeWindow` does not invalidate gadget borders. **Priority**: high (visible regression in standard GadTools UI).
3. **`GadToolsMenuPixelTest.DISABLED_SubmenuHoverDoesNotCorruptLowerMainItems`** (`tests/drivers/gadtoolsmenu_gtest.cpp:320`) — hangs indefinitely (timeout). The previous test (`HoverRedrawReturnsToSamePixels`) passes in ~69 s; the fixture itself is fine. The submenu hover sequence triggers an event-injection deadlock between menu_drag step processing and submenu redraw. **Priority**: medium (test-only deadlock; production menus work).
4. **`ProWriteInteractionTest.DISABLED_AboutDialogOpensAndCanBeDismissed`** (`tests/drivers/apps_misc_gtest.cpp:1075`) — About requester opens but OK-button click / IDCMP_CLOSEWINDOW handling does not return ProWrite to the editor state the test expects. Candidate area: Phase 135 ActiveWindow routing change. **Priority**: medium.
5. **`MiscTest.DISABLED_StressTasks`** (`tests/drivers/misc_gtest.cpp:144`) — two assertion failures (Stress sample lines 33, 41) during a 60 s task-stress workload. Likely an `exec.c` scheduler/signal-delivery race that only manifests under sustained AddTask/RemoveTask churn. **Priority**: low (stress test only; nothing in the wild has hit this).

---

## Known Open Limitations

- **KickPascal layout/menus** — depends on deeper arp/req library functionality.
- **SysInfo hardware fields** (BattClock, CIA timing) — requires hardware detection.
- **Cluster2 EXIT button** — coordinate mapping mismatch in custom toolbar.
- **DPaint V main editor** — defers menu/toolbox/palette rendering until first mouse interaction; canvas remains blank-pen until tool/palette state is exercised. Palette/pencil/fill interaction tests deferred (require deferred-paint trigger or "force full redraw" capability).
- **DPaint V Ctrl-P** — only works on depth-2 startup dialog, not from the depth-8 main editor. About dialog and File→Open requester not yet exercised.
- **MaxonBASIC About/Settings/Run** — menus reachable only via RMB drag through hardcoded coordinates; German titles + lack of programmatic menu introspection make item-level activation brittle.
- **DevPac File→Open requester** — req.library stub returns failure cleanly, so no requester appears. Real file-requester behaviour requires non-stub req.library (Phase 141).
- **BlitzBasic2 ted editor** — ted does not render real editable text content even after Phase 113/114 blitter/copper improvements. Phase 119 vision-model analysis: menu bar and dropdowns render correctly; editor body remains a flat surface with menu-residue and status overlay paint only.
- **ASM-One Assemble/Run workflow** — same external-process constraint as DevPac; Phase 140+.
- **PPaint** — RTG application; ECS-path investigation abandoned. Validation deferred to Phase 139.
- **FinalWriter `DISABLED_AcceptDialogOpensEditorWindow`** — disabled pending Phase 139 RTG validation.
- **DirectoryOpus 4 deeper workflows** — file-list navigation requires directory reading in lister panes; button-bank click workflows require dopus.library command dispatch; Preferences panel interaction; title-bar version/memory string (rendered by DOpusRT, not launched in current tests).
- **CMake shard FILTER drift** — Phase 0 added `tools/check_shard_coverage.py` and registered it as the `shard_coverage_check` CTest meta-test. Any new test added to a sharded driver MUST update the corresponding FILTER strings in `tests/drivers/CMakeLists.txt`.
- **KickPascal 2** — editor body never clears the splash screen (HiS logo + "Pascal" graphic), so typed source overlays stale pixels. Same external-process constraint for Compile/Run.
- **Typeface deeper workflows** — font-list interaction (clicking font names), preview pane, and text-entry preview widget (requires `gadgets/textfield.gadget`) deferred. Window-chrome gap (194×138 vs 280×195) tracked in Phase 139.

---

## Dependency Graph (Critical Path)

```
Phase 125 ──► Phase 126 ──► 127/128 ──► 129 ──► 130/131 ──► 132/133/134 ──► 135 ──► 136
                                                                                       │
                                                                                       ▼
                                                                            Phase 137 (RTG foundation, NEXT)
                                                                                       │
                                                                                       ▼
                                                                            Phase 138 (Picasso96API)
                                                                                       │
                                                              ┌────────────────────────┤
                                                              ▼                        ▼
                                                     Phase 139 (RTG apps)     Phase 141/142 (lib policy)
                                                              │
                                                              ▼
                                                     Phase 140 (ext processes)
                                                              │
                                                              ▼
                                                     MUI / ReAction / CPU eval
```

---

*Each phase must include updated GTest host-side drivers under `tests/drivers/` and regression assertions. For visual investigation of failure artifacts, attach captured PNGs directly to assistant messages (the harness supports image input natively); the retired `tools/screenshot_review.py` helper now lives in `attic/`.*
