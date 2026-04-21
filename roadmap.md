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
| 122 | KickPascal 2 driver expansion: default-workspace acceptance reaches EDITOR mode (status-bar pixel proof), RMB menu drag survival + geometry stability, splash content stability across idle, missing-library/PANIC log guards | v0.8.87 |
| 123 | Typeface deferred — bgui.library v39+ required; real binary now available at `others/bgui`, installed to `share/lxa/System/Libs/bgui.library`; full driver deferred to Phase 136 | — |
| 124 | FinalWriter driver expansion: dialog-state coverage (title regression, gadget count, OK/Cancel button discovery, display-options list discovery, missing-library/PANIC log guards, idle-time content stability); FinalWriter sharding bug fix (orphaned tests); broken `AcceptDialogOpensEditorWindow` quarantined as DISABLED_ | v0.8.88 |
| 0 | CMake shard coverage safety: audit all sharded drivers (4 orphaned GadTools tests fixed), `tools/check_shard_coverage.py` written and registered as `shard_coverage_check` CTest meta-test | v0.8.89 |
| 125 | `lxa.c` decomposition: split 9,960-line monolith into `lxa_custom.c`, `lxa_dispatch.c`, `lxa_dos_host.c`, `lxa_internal.h`, `lxa_memory.h`; `lxa.c` reduced to ~1,658 lines | v0.8.90 |
| 126 | Profiling infrastructure: `lxa_profile.c` with per-EMU_CALL counters + timing; `--profile <path>` flag on lxa binary; `tools/profile_report.py` analysis tool; `ZStartupLatency` baseline tests in SysInfo, DOpus, FinalWriter, SIGMAth2, Cluster2, BlitzBasic2, Vim drivers; profiling API unit tests in `api_gtest.cpp` | v0.9.0 |
| 127 | Memory access fast paths: direct bswap-based 16/32-bit RAM and ROM reads/writes in `m68k_read/write_memory_16/32`; `__builtin_expect` hints on hot paths in `mread8`/`mwrite8`; 4 byte-order correctness unit tests in `api_gtest.cpp` | v0.9.1 |
| 128 | Display pipeline optimization: dirty-region scanline tracking (min/max dirty row → partial SDL texture upload); SSE2 planar-to-chunky acceleration (8 pixels/iter via byte broadcast + mullo); coalesced VBlank uploads; all visual regression tests pass unchanged | v0.9.2 |
| 129 (partial) | Screen-mode emulation ROM improvements: `g_known_display_ids[]` expanded from 6 to 36 entries (DEFAULT/NTSC/PAL monitors × 12 ECS modes incl. LORESLACE, HIRESLACE, SUPER, HAM, HAMLACE, HIRESHAM, EHB, EHBLACE); `graphics_virtualize_display_id()` maps unknown IDs to closest physical mode (Wine strategy); `GetDisplayInfoData(DTAG_DIMS)` returns realistic raster ranges (HIRES 320–1280, LORES 160–640); `DIPF_IS_HAM`/`EXTRAHALFBRITE`/`LACE`/`ECS` advertised in `DTAG_DISP`; `BestModeIDA()` honors `BIDTAG_DIPFMustHave`/`MustNotHave`; `FindDisplayInfo(INVALID_ID)` correctly rejects; `LockIBase()` returns 0; `OpenScreen` accepts WBENCHSCREEN via CUSTOMSCREEN + flag; VPModeID set from SA_DisplayID; OpenFont loads screen font; `OpenWorkBench` uses CUSTOMSCREEN trick; diskfont registers fonts by basename; GfxBase DisplayFlags/VBlankFrequency/PowerSupplyFrequency populated; display_viewport test expanded with virtualization + HAM/EHB/LACE property-flag + BestModeIDA MustNotHave coverage. **PPaint launch-path still blocked** — see Known Limitations | v0.9.3 |

**Known open limitations** (not yet addressed):
- ASM-One / MaxonBASIC flickery menus — needs architectural double-buffered menu rendering (Phase 133)
- KickPascal layout/menus — depends on deeper arp/req library functionality
- SysInfo hardware fields (BattClock, CIA timing) — requires hardware detection
- Cluster2 EXIT button — coordinate mapping mismatch in custom toolbar
- DPaint V main editor defers menu bar / toolbox / palette rendering until first mouse interaction; canvas remains blank-pen until tool/palette state is exercised. Palette/pencil/fill interaction tests deferred — require either the deferred-paint trigger to be reverse-engineered or a new "force full redraw" capability.
- DPaint V Ctrl-P (Screen Format reopen) only works on the depth-2 startup dialog, not from the depth-8 main editor. About dialog and File→Open requester not yet exercised — depends on resolving deferred-paint to make menu bar interactable.
- MaxonBASIC About/Settings/Run interaction: menus are Intuition-managed but reachable only via RMB drag through hardcoded coordinates; the German menu titles ("Projekt", "Editieren", "Suchen") and lack of programmatic menu introspection make item-level activation brittle. Phase 116 verified menu reveal and editor text rendering; deeper menu-driven workflows defer until host-side menu introspection (Phase 131 gap #4) is implemented.
- DevPac File→Open requester: Phase 117 verified Amiga-O does not crash, but req.library stub returns failure cleanly so no requester appears. Real file requester behavior requires a non-stub req.library implementation (out of scope per AGENTS.md "do not re-implement third-party libraries in ROM"). Assemble/Run workflow not exercised — would require a loaded valid m68k source and external assembler invocation, both of which need external-process emulation (Phase 137+).
- BlitzBasic2 ted editor — copper/blitter improvements (Phase 113/114) reach the surface (~13% of editor body fills with paint after menu interaction) but ted still does not render real editable text content. Phase 119 captured vision-model analysis: menu bar and dropdowns render correctly (PROJECT/EDIT/SOURCE/SEARCH/COMPILER), but the editor body remains a flat surface with only menu-residue and status overlay paint. Root cause is deeper than line-mode blitter and copper colour cycling — likely involves blitter copy modes, sprite hardware, or specific copper waits that ted uses for its custom overlay (Phase 135).
- ASM-One Assemble/Run workflow not exercised (Phase 118) — same external-process constraint as DevPac; deferred until Phase 137+. Menu pixel-flicker verification uses a content-floor guard (before→after cannot collapse to <10% of before) because ASM-One defers editor repaint until first interaction, so pixel counts legitimately *grow* after the first menu drag. Host-side observation of IDCMP qualifier bits is not currently possible; qualifier propagation is guarded via survival + subsequent-input responsiveness only.
- PPaint still exits early with rv=26 even after the Phase 129 ROM improvements (v0.9.3). Deep-dive tracing identified the precise block: PPaint's internal mode-scan loop at PC `$0x26d78` iterates 40 entries of a table at `A4+$34f0` (count stored at `A4-$1cdc`). Each 8-byte entry's fourth byte is tested with `tst.b`; in every iteration the byte is zero, the `beq` at `$0x26d9c` is taken, and no entry ever succeeds. The loop falls through to cleanup at `$0x26c44` → CloseLibrary loop at `$0x26ca2` → ILLEGAL opcode → bootstrap returns rv=26. The 4th-byte field is not a DisplayID LSB (mode IDs like 0x8000, 0x11000, 0x29000 all have low byte 0x00 anyway); it appears to be a depth/flag/pointer field that PPaint's probe subtask was supposed to populate from `GetDisplayInfoData` results but never does. Next debugging steps (deferred): (a) disassemble the PPaint binary around `$0x26d78` with `m68k-amigaos-objdump` to identify the table-population routine and learn the 4th-byte's semantics; (b) set a memory-write watchpoint on `A4+$34f0 .. A4+$34f0+320` via `m68k_write_memory_*` hooks; (c) investigate PAL/NTSC mismatch — PPaint probes NTSC_HIRES (0x11000) but GfxBase reports PAL. The `DISABLED_AcceptDialogOpensEditorWindow` test in FinalWriter remains disabled.
- PPaint and FinalWriter visual regression: CloantoScreenManager and FinalWriter launch sequences probe screen modes in ways that require either full PPaint/FW mode-table reverse engineering (per above) or broader virtualization work. Deferred.
- Menu pixel introspection during a drag: `lxa_inject_drag` performs press → drag → release atomically, so we cannot capture screen state while a menu is open. Tests verify menu interaction via side effects (window count, app survival) instead. A future infrastructure improvement would be a non-releasing drag API or a "capture during drag" hook.
- DirectoryOpus 4 renders a skeleton UI in lxa: window opens at 640×245, dual file-lister frames + scroll gadgets + small button cluster (B/R/S/A) + bottom toolbar (?/E/F/C/I/Q) all draw correctly, but text labels, the title-bar version/memory string, and the famous main button bank (Copy/Move/Delete/etc.) are absent. Vision-model review (Phase 121) attributed this to font/path resolution or `dopus.config` parsing not completing (Phase 134). DOpus also requests `commodities.library` and `inovamusic.library`, which lxa does not ship as stubs; both are tolerated by DOpus itself but logged as missing-library errors. File-list navigation, button-bank workflows, and Preferences interaction tests deferred until Phase 134.
- CMake test sharding with hardcoded GTest filters can silently orphan newly added tests. Phase 0 audited all sharded drivers (fixing 4 orphaned GadTools tests) and added `tools/check_shard_coverage.py` as a `shard_coverage_check` CTest meta-test. **Any future test additions to a sharded driver MUST update the corresponding FILTER strings** — CI will now catch violations automatically.
- KickPascal 2 (Phase 122): editor body never clears the splash screen (HiS logo + "Pascal" graphic) so typed Pascal source is overlaid on stale pixels. Compile/run workflows are not exercised (same external-process constraint as DevPac/ASM-One; Phase 137+). Editor-body splash-clear deferred until KickPascal's deferred-paint trigger is reverse-engineered or a forced full redraw can be issued.
- Typeface (Phase 123, deferred): the font previewer requires `bgui.library` v39+. The real binary is now available at `others/bgui/bgui/libs/bgui.library` and is installed to `share/lxa/System/Libs/bgui.library` (picked up by the static install rule). Typeface should now pass the `OpenLibrary` version check; full font-list / preview / Text() rendering coverage from Typeface deferred to Phase 136.
- FinalWriter (Phase 124): clicking OK in the startup dialog causes FW to attempt opening a new screen mode; the two child processes spawn and immediately Exit(0), and FinalWriter terminates without ever opening the editor window. The `DISABLED_AcceptDialogOpensEditorWindow` test is preserved in source for the day Phase 129 screen-mode emulation makes the editor reachable. All editor-mode workflows deferred.

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

#### Infrastructure Gaps (with phase assignments)

1. **No screen-content diffing** (Phase 132+): No way to assert "this region looks like a text label" or "this area changed between two states." A reference-image diff tool would let us write layout regression tests that survive minor color changes but catch structural regressions.

2. **No text extraction from screen** → **Phase 130**: Apps render text via ROM `Text()` calls. Adding a host-side `lxa_set_text_hook()` callback in `_graphics_Text()` would let tests assert `"About MaxonBASIC"` appeared without pixel-exact matching. The hook already has a `DPRINTF` log at that call site — the infrastructure change is small.

3. **No window/screen event log** → **Phase 131**: No structured log of Intuition events (OpenWindow, CloseWindow, OpenScreen, requester open/close). Tests infer these from window counts or pixel changes. A circular event buffer exposed via `lxa_drain_events()` would make assertions semantic and fragile-free.

4. **No menu-state introspection** → **Phase 131**: Cannot query which menus/items exist, their names, or enabled/disabled state from the host side. Menu testing relies on RMB drag by hardcoded coordinates. `lxa_get_menu_strip()` traversing the Intuition `MenuStrip` linked list would unblock MaxonBASIC and DevPac deeper workflows.

5. **No clipboard / string output capture** (Phase 137+): Apps writing to AmigaDOS stdout or the clipboard have no way to expose output to the host test. A `lxa_capture_dos_output()` API would allow asserting compiler output, BASIC program results, etc.

6. **Vision model loop not automated**: `screenshot_review.py` is a manual tool. No test-phase hook automatically invokes it when a pixel assertion fails. Remains a developer investigation tool; automation deferred.

7. **No audio state introspection**: Apps using audio.device have no observable test output. No tests require this yet.

---

## Short-Term: Architecture + Observability (Phases 128–131)

### Phase 128: Display pipeline optimization

- Dirty-region tracking: skip unchanged scanlines in `display_update_planar()`.
- SIMD planar-to-chunky: use SSE2 for the inner loop of the bit-plane interleave.
- Eliminate redundant SDL texture uploads: batch multiple `display_refresh_all()` calls within a single VBlank.

**Test gate**: Same as Phase 127. Visual regression tests (pixel-level) must not change output.

---

### Phase 129 (partial, v0.9.3): Screen-mode emulation — ROM side done, PPaint still blocked

> **Status**: ROM-side screen-mode virtualization is complete and shipped in v0.9.3. All infrastructure changes in the original plan landed. However, **PPaint still exits rv=26** because its internal mode-scan loop depends on a private table whose fourth byte is never populated — that block is orthogonal to the ROM APIs we improved. See the Known Limitations entry above for the deferred next-steps (PPaint binary disassembly, memory-write watchpoint on `A4+$34f0`, PAL/NTSC mismatch investigation). FinalWriter's `DISABLED_AcceptDialogOpensEditorWindow` remains disabled pending this investigation.

**Landed in v0.9.3** (permanent):
1. `g_known_display_ids[]` expanded from 6 to 36 entries (DEFAULT/NTSC/PAL × 12 ECS modes).
2. `GetDisplayInfoData(DTAG_DIMS)` returns realistic `Min`/`MaxRasterWidth` ranges (HIRES 320–1280, LORES 160–640).
3. `BestModeIDA()` honors `BIDTAG_DIPFMustHave`/`MustNotHave` and `BIDTAG_DesiredWidth`/`Height`.
4. `OpenScreenTagList()` virtualization: `SA_DisplayID` mapping, WBENCHSCREEN accepted via CUSTOMSCREEN trick, auto-HIRES for width ≥ 640, screen font opened via `OpenFont()`.
5. `LockIBase()` correctly returns 0 (previously 1 → caused apps to hang on UnlockIBase).
6. `FindDisplayInfo(INVALID_ID)` correctly rejects (not virtualized).
7. `graphics_virtualize_display_id()`: unknown-but-plausible IDs map to closest physical mode (Wine strategy).
8. Diskfont registers fonts by basename so plain OpenFont() can find them after a pathful OpenDiskFont().
9. GfxBase `DisplayFlags = PAL|REALLY_PAL`, `VBlankFrequency = 50`, `PowerSupplyFrequency = 50`.
10. `display_viewport` test expanded with HAM/EHB/LACE property-flag coverage, LORES/HIRES DTAG_DIMS range checks, virtualization, and BestModeIDA MustNotHave tests.

**Test gate** (met): All 66 test drivers pass. No warnings. `graphics_gtest` includes new Phase 129 coverage.

**Deferred** (to a future phase when the PPaint block is investigated):
- PPaint mode-scan loop reverse-engineering (see Known Limitations).
- FinalWriter `AcceptDialogOpensEditorWindow` re-enablement.
- Visual regression tests that require PPaint's editor canvas to be reachable.

---

### Phase 129 (original plan — for reference)

> **Highest-value architectural improvement. Directly unblocks PPaint editing UI, FinalWriter editor, and any future app that probes display modes.**

**Root cause identified**: `g_known_display_ids[]` contains only 6 entries (bare LORES/HIRES + NTSC/PAL variants). `GetDisplayInfoData(DTAG_DIMS)` returns `MinRasterWidth == MaxRasterWidth` (no size range), causing screen-mode probes like CloantoScreenManager to conclude the mode is unsuitable. `BestModeIDA()` ignores `BIDTAG_DIPFMustHave`/`MustNotHave` flags entirely.

**What to fix** (most items landed in v0.9.3; PPaint editor-reach deferred):
1. Extend `g_known_display_ids[]` with the full ECS/AGA standard mode set from `<graphics/displayinfo.h>`: LACE variants, SUPER_HIRES, HAM, EHB, EXTRAHALFBRITE — at minimum the modes that real ECS software probes for. **[DONE v0.9.3]**
2. `GetDisplayInfoData(DTAG_DIMS)`: return proper `MinRasterWidth`/`MaxRasterWidth` ranges (e.g., 320–1280 for HIRES), not pinned equal values. **[DONE v0.9.3]**
3. `BestModeIDA()`: respect `BIDTAG_DIPFMustHave`/`MustNotHave` flags before returning a mode ID. **[DONE v0.9.3]**
4. `OpenScreenTagList()` with `SA_DisplayID`: when an app requests a valid mode that maps to our physical LORES/HIRES display, accept and virtualize it. **[DONE v0.9.3]**
5. Add `screenmode_gtest.cpp` driver asserting: PPaint reaches its main editing window; FinalWriter's `DISABLED_AcceptDialogOpensEditorWindow` can be re-enabled and passes. **[DEFERRED — PPaint still blocked by internal mode-scan loop; see Known Limitations]**

**Test gate** (original): PPaint no longer exits rv=26; FinalWriter editor opens after OK; `DISABLED_AcceptDialogOpensEditorWindow` renamed back to active.

**Test gate met** (revised for v0.9.3): ROM-side virtualization complete and fully tested. App-side blocker documented for future investigation.

---

### Phase 130: Text() interception — semantic test assertions

> **Unlocks: DOpus label verification, KickPascal editor content, FinalWriter document area, any app whose UI is text-heavy.**

`_graphics_Text()` in `lxa_graphics.c` already has a `DPRINTF` that logs the string, position, and font. The infrastructure change is small:

```c
/* lxa_api.h addition */
typedef void (*lxa_text_hook_t)(const char *str, int len, int x, int y, void *userdata);
void lxa_set_text_hook(lxa_text_hook_t hook, void *userdata);

/* _graphics_Text() addition (before rendering) */
if (g_text_hook)
    g_text_hook((const char *)string, count, rp->cp_x, rp->cp_y, g_text_hook_userdata);
```

In tests:
```cpp
std::vector<std::string> text_log;
lxa_set_text_hook([](const char *s, int n, int, int, void *ud) {
    ((std::vector<std::string>*)ud)->push_back(std::string(s, n));
}, &text_log);
// ... run app ...
EXPECT_TRUE(std::any_of(text_log.begin(), text_log.end(),
    [](const auto &s){ return s.find("Copy") != std::string::npos; }));
```

- [ ] Add `lxa_set_text_hook()` / `lxa_clear_text_hook()` to `lxa_api.c` / `lxa_api.h`
- [ ] Add unit tests in `api_gtest.cpp` for hook registration and invocation
- [ ] Update `dopus_gtest.cpp`: assert "Copy", "Move", "Delete" appear (or confirm absence pinpoints font-resolution bug)
- [ ] Update `kickpascal_gtest.cpp`: assert "EDITOR" / "Insert" appear in text log after workspace accept
- [ ] Update `finalwriter_gtest.cpp` (once editor opens via Phase 129): assert document area renders text

---

### Phase 131: Window/screen event log + menu introspection

> **Combines infrastructure gaps 3 and 4. Replaces brittle window-count polling with semantic assertions. Unblocks MaxonBASIC and DevPac deeper menu workflows.**

**Event log**: Add a circular event buffer (256 entries) on the host side. ROM Intuition's `OpenWindow`, `CloseWindow`, `OpenScreen`, `CloseScreen`, requester open/close all call through `lxa_api.c`. Add `lxa_push_event()` there and expose `lxa_drain_events()` to tests.

**Menu introspection**: The Intuition `MenuStrip` is already a linked list in emulated memory. Add:
```c
/* lxa_api.h */
lxa_menu_strip_t *lxa_get_menu_strip(int window_index);
int lxa_get_menu_count(lxa_menu_strip_t *strip);
lxa_menu_info_t lxa_get_menu_info(lxa_menu_strip_t *strip, int menu_idx, int item_idx);
/* lxa_menu_info_t: { char name[64]; bool enabled; bool checked; int x, y, w, h; } */
```

- [ ] Implement event log in `lxa_api.c`; add `LxaTest::DrainEvents()` helper
- [ ] Update `simplemenu_gtest.cpp`: replace window-count polling with event log assertions
- [ ] Implement `lxa_get_menu_strip()` via emulated memory traversal
- [ ] Add menu introspection tests in `maxonbasic_gtest.cpp`: assert German menu titles exist before coordinate-based drag
- [ ] Add `devpac_gtest.cpp` menu introspection: assert Project/Edit/Assemble menus present

---

## Mid-Term: Rendering Fidelity (Phases 132–136)

### Phase 132: SMART_REFRESH full backing store (second attempt)

The first attempt (Phase 111) was reverted due to regressions (SIGMAth2 Analysis window, requester test timeouts). By Phase 132 the following tools exist that make this tractable:

- Text interception (Phase 130): assert dialog content before and after
- Window event log (Phase 131): detect open/close cycles precisely
- Screen-mode fixes (Phase 129): fewer spurious failures from unrelated mode issues
- Clean lxa.c modules (Phase 125): easier to isolate the backing store code path

**Approach**:
1. Write regression tests for the *known* failure cases from the first attempt *before* touching backing store code.
2. Implement layer-level backing store for `WFLG_SMART_REFRESH` windows.
3. Verify Phase 111's `DamageExposedAreas()` skip for SMART_REFRESH remains as a fallback for the edge cases.

**Test gate**: Dialog-over-window scenarios don't stamp content; SIGMAth2 and requester tests pass.

---

### Phase 133: Menu double-buffering (ASM-One / MaxonBASIC flickering)

ASM-One and MaxonBASIC exhibit pixel flicker during menu drags (documented in known limitations since Phase 118/116). Root cause: the menu bitmap is drawn and blit to screen without atomic compositing. Fix: render menu frame to an off-screen BitMap, then blit the completed frame in one operation.

- [ ] Identify the menu render path in `lxa_intuition.c` / `lxa_layers.c`
- [ ] Add off-screen buffer; render menu items to it; atomic blit to display
- [ ] Update `ZMenuFlickerCheck` in `asm_one_gtest.cpp` from a content-floor guard to a pixel-stability assertion (before→after pixel count must be within 5%)
- [ ] Verify MaxonBASIC menu drag no longer shows residue artifacts

---

### Phase 134: DirectoryOpus font/path resolution

DOpus skeleton UI is correct (Phase 121), but text labels and the main button bank are absent. With Phase 130's text hook, the root cause is now diagnosable without pixel guessing:

1. Enable text hook; run DOpus; inspect what (if anything) is logged.
2. If nothing is logged, the drawing code is never reached — suspect `dopus.config` parse failure or a `MatchFirst()`/`ReadArgs()` call returning early.
3. If the font name is logged but no bitmap appears, suspect `FONTS:` assign resolution.

- [ ] Diagnose with text hook + Phase 131 event log
- [ ] Implement the missing call or assign that blocks config/font loading
- [ ] Update `dopus_gtest.cpp`: upgrade `MainWindowContainsRenderedContent` to assert specific button-bank labels appear

---

### Phase 135: BlitzBasic 2 ted editor deep dive

Phase 119 established that ~13% of the editor surface fills with paint after menu interaction (menu-residue and status overlay), but the real editable text content never appears. The roadmap's current hypothesis: blitter copy modes, sprite hardware, or specific copper waits.

With Phase 126's profiling data and Phase 125's clean modules, the investigation path is:

1. Add blitter operation logging (mode, source, dest, size) for the ted render phase.
2. Compare against the known ted behavior (BlitzBasic source/community docs).
3. Identify unimplemented blitter copy modes (specifically `ABC` + `NABC` combinations used for text background clearing).
4. Implement the missing modes; re-run vision-model review to confirm text renders.

**Test gate**: `YEditorSurfaceShowsContentAfterMenuInteraction` in `blitzbasic2_gtest.cpp` upper bound (<50% fill) is replaced by a lower bound proving real text pixel density (>30% fill in the editor body).

---

### Phase 136: bgui.library → Typeface

The real `bgui.library` v41 binary is available at `others/bgui/bgui/libs/bgui.library` and is already installed to `share/lxa/System/Libs/bgui.library` via the static install rule. No stub required — Typeface should pass the `OpenLibrary("bgui.library", 39)` check and load the real library. Also present: `gadgets/bgui_bar.gadget`, `bgui_layoutgroup.gadget`, `bgui_palette.gadget`, `bgui_popbutton.gadget`, `bgui_treeview.gadget` in `others/bgui/bgui/libs/gadgets/`.

- [ ] Verify `bgui.library` loads cleanly under lxa: add a startup test that asserts `OpenLibrary` returns non-NULL and Typeface does not exit with an error dialog
- [ ] Copy the gadget binaries from `others/bgui/bgui/libs/gadgets/` to the appropriate `share/lxa` location (likely `share/lxa/System/Libs/gadgets/`) so BGUI gadget classes are also available
- [ ] Add `typeface_gtest.cpp` driver: assert Typeface reaches a non-exit state (font list window visible or >0 fonts enumerated)
- [ ] Use text hook (Phase 130) to observe what the font preview area renders

---

## Long-Term: Extended Coverage (Phase 137+)

### Phase 137: External process emulation

DevPac Assemble, ASM-One Assemble, BlitzBasic Run, KickPascal Compile/Run all require this. Needed: AmigaDOS `CreateProc()` / `Execute()` launching a real m68k subprocess within the emulated environment, with stdout piped back to the host.

This is the deepest deferred item. Prerequisites: Phase 131 (event log to detect process lifetime), Phase 130 (capture output), and a stable foundation from all prior phases.

---

## Far Future: CPU Emulation Evolution

Musashi (MIT, C89, interpreter) is adequate for correctness but limits throughput. No clean permissively-licensed x86-64 68k JIT exists today. Options assessed: Moira (MIT, no 68030 yet), UAE JIT (GPL, framework-entangled), Emu68 (MPL-2.0, ARM-only), Unicorn/QEMU (GPL).

**Recommendation**: Stay on Musashi. Host-side overhead dominates (~95% of wall time per Phase 126 estimate). Revisit when Moira adds 68030 support or host optimizations (Phases 127/128) are exhausted.

- **Phase 138** (tentative): CPU core evaluation — re-evaluate Moira 68030 support; prototype if available; assess whether 68020 mode suffices for all tested apps.
- **Phase 139** (tentative): Production readiness assessment — define use-case targets; set performance baselines; decide on JIT investment.

---

## Dependency Graph (Critical Path)

```
Phase 125 (decompose lxa.c) ──► Phase 126 (profiling)
                                                                │
                                               ┌────────────────┤
                                               ▼                ▼
                                        Phase 127/128      Phase 129
                                        (perf opts)    (screen-mode emulation)
                                                            │
                                                    ┌───────┴────────┐
                                                    ▼                ▼
                                            Phase 130            Phase 131
                                          (Text hook)        (event log +
                                                             menu introspect)
                                                │                    │
                                         ┌──────┴──────┐            │
                                         ▼             ▼            ▼
                                   Phase 132      Phase 134    Phase 133
                                 (SMART_REFRESH)  (DOpus)     (menu dbl-buf)
                                                      │
                                               Phase 135 (BlitzBasic ted)
                                                      │
                                               Phase 136 (bgui/Typeface)
                                                      │
                                               Phase 137 (ext processes)
                                                      │
                                             Phase 138/139 (CPU evolution)
```

---

*Each phase must include updated GTest host-side drivers under `tests/drivers/`, regression assertions, and saved failure artifacts. Use `tools/screenshot_review.py` during investigation, then translate findings into deterministic pixel/geometry assertions for CI.*
