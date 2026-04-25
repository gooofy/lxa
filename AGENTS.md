# lxa Development Guide for Agents

This document is the entry point for AI agents working on the `lxa` codebase. Treat it as the short operational checklist, then load the relevant skills for the task at hand.

## 1. Core Mandates
- **Stability**: Zero tolerance for crashes.
- **Coverage**: 100% test coverage required.
- **Zero failing tests at phase completion**: The full test suite (`ctest --test-dir build -j16 --timeout 180`) must report **zero failures and zero timeouts** before any phase is declared done. Failures inherited from earlier phases are NOT acceptable as "pre-existing" — fix them, or formally quarantine them with a `DISABLED_` GTest prefix AND a roadmap entry justifying the deferral. Hand-waving "5 pre-existing failures, unrelated to my phase" is forbidden.
- **Reference**: ALWAYS consult RKRM and NDK.
- **System Libraries Must Be Fully Implemented**: AmigaOS system libraries (dos.library, exec.library, graphics.library, intuition.library, datatypes.library, etc.) must have complete, correct implementations — not stubs. Stub implementations of system libraries are never acceptable; they mask bugs and break app compatibility. If a system library function is missing or incomplete, implement it properly.
- **Third-Party Libraries: STOP and Notify the User**: Do NOT implement or stub third-party (non-Commodore-OS) libraries (e.g., req.library, reqtools.library, powerpacker.library, bgui.library, MUI, etc.). These must be supplied as real binaries on disk. If a required third-party library binary is missing, **STOP immediately and notify the user** — do not write a stub and do not proceed. The user must provide the real library binary.
- **Complete Phases if possible**: When working on a phase of the roadmap, plan ahead and make sure you create all the TODO items needed to **successfully complete** the phase. Do not stop early, but strive towards reaching the goal of finishing a phase.
- **Keep the roadmap.md file updated**: Whenever you complete, defer, or re-scope work, update `roadmap.md` so it stays clean, current, and future-focused. If you intentionally park unfinished work, document that decision explicitly instead of leaving ambiguous unchecked items behind.
- **Host-Side Test Drivers for UI Tests**: All interactive UI tests (gadget clicks, menu selection, keyboard input) MUST use the host-side driver infrastructure (`tests/drivers/` with liblxa) and Google Test. The legacy `test_inject.h` approach has been removed. All tests are integrated into the unified GTest suite.
- **Priority Order**: Schedule work strictly as **1) Quality → 2) Amiga compatibility → 3) Performance → 4) New features**. A lower-priority phase may not be promoted ahead of a higher-priority phase that is ready to start. RTG, MUI, ReAction, and external-process emulation are all "new features" and yield to any open quality or compatibility item.
- **No Pooling Sections in `roadmap.md`**: The roadmap must not contain catch-all lists like "Deferred Test Failures", "Known Limitations", "TODO", or "Backlog". Every issue is either a TODO bullet inside an existing scheduled phase (same root cause) **or** promoted into its own numbered phase with explicit objectives, sub-problems, TODO checkboxes, and a test gate. The only retrospective section permitted is the `## Completed Phases (Summary)` table.
- **Every `DISABLED_` GTest Must Be the Objective of a Scheduled Phase**: When you quarantine a test with the `DISABLED_` prefix, you must in the same commit create (or extend) a numbered phase whose explicit objective is to fix the root cause and re-enable the test. A `DISABLED_` test without an owning phase is forbidden.

## 2. Standard Workflow
1. Read `roadmap.md` and identify the active phase or the follow-up you are explicitly parking.
2. Load the relevant skills before editing code or documentation.
3. For system libraries: implement fully per RKRM/NDK. For third-party libraries: STOP and notify the user to supply the real binary.
4. Add or update automated coverage for the changed behavior.
5. Run the appropriate build/tests, then update roadmap/docs/version together.

## 3. Interactive Integration Test Tools
The host-side integration stack has grown beyond simple click injection. Future agents should use these tools instead of ad-hoc sleeps or hard-coded assumptions:

- `lxa_wait_window_drawn()` / `LxaTest::WaitForWindowDrawn()` to wait for visible UI readiness
- `lxa_get_gadget_count()` / `lxa_get_gadget_info()` plus `LxaTest::GetGadgets()` for tracked Intuition gadget introspection
- `LxaTest::ClickGadget()` for center-of-gadget interaction using queried geometry
- `lxa_capture_window()` / `lxa_capture_screen()` for failure artifacts
- tracked rootless-window to emulated-window pointer linkage, so host-side tests can correlate visible windows with Intuition state

Prefer these helpers over brittle coordinate-only scripts whenever the test can identify a real window or gadget.

### Visual Investigation of Failure Artifacts

When a GTest driver captures a failure artifact via `lxa_capture_window()` or `lxa_capture_screen()` and the pixel-level assertion message alone does not explain the visual defect, attach the captured PNG directly to your assistant message. The current OpenCode coding harness supports image input natively — no external tool or API key is required. Ask the assistant to describe layout, clipping, text rendering, or menu artifacts, then translate the findings into a deterministic pixel/geometry assertion so the defect is caught automatically in future runs.

See the `graphics-testing` skill for the full capture-and-review workflow.

## 4. Available Skills
Load the specific skill relevant to your task:

- **`develop-amiga`**:
  - AmigaOS implementation rules.
  - Code style (ROM vs System vs Host).
  - Memory management & Stack safety.
  - Debugging tips.

- **`lxa-workflow`**:
  - Roadmap-driven development process.
  - Version management rules (MAJOR.MINOR.PATCH).
  - Build system (`cmake`, `build.sh`) instructions.

- **`lxa-testing`**:
  - General TDD workflow.
  - Integration and Unit testing strategies.
  - **Host-side driver infrastructure** for UI testing.
  - Test requirements checklist.

- **`graphics-testing`**:
  - Specific strategies for headless Graphics/Intuition testing.
  - **Host-side driver patterns** for interactive testing.
  - BitMap verification techniques.

## 5. Build & Test Quick Reference

```bash
# Build (from project root):
./build.sh

# Run full test suite (ALWAYS use -j16 for parallel execution, always use timeout so we detect hanging tests):
ctest --test-dir build --output-on-failure -j16 --timeout 180

# Run a specific test:
ctest --test-dir build --output-on-failure --timeout 180 -R shell_gtest

# Run specific test cases via GTest filter:
./build/tests/drivers/shell_gtest --gtest_filter="ShellTest.Variables"

# Reliability loop (for debugging intermittent failures):
for i in $(seq 1 50); do
    timeout 30 ./build/tests/drivers/shell_gtest \
        --gtest_filter="ShellTest.Variables" 2>&1 | tail -1
done
```

**Parallelism**: `-j16` is the project default. The suite's longest interactive
drivers are now sharded, which keeps full-suite wall time around the current
~145 second range on this machine class. Higher parallelism is safe, but usually
does not improve wall time meaningfully.

See `lxa-testing` skill for detailed test execution docs and
`doc/test-reliability-report.md` for parallelism benchmarks.

## 6. Lessons Learned (Accumulated Knowledge)

These are hard-won debugging insights from Phases 98–108 that future agents
**must** know. Each item cost hours of investigation; reading them costs seconds.

### 6.1 GCC m68k Cross-Compiler Bugs

**`move.w` register argument truncation**: When GCC cross-compiles ROM code for
m68k, it may emit `move.w` instead of `move.l` for d-register arguments if it
believes the value fits in 16 bits. This silently truncates 32-bit values passed
through inline `__asm` stubs.

- **Fix**: Cast semantically 16-bit arguments with `(LONG)(WORD)val` to force
  sign-extension before the inline asm boundary.
- **Do NOT apply** to genuinely 32-bit values (IFF chunk types, DoPkt action
  codes, math operands, tag IDs) — only to coordinates, sizes, pen numbers,
  and small counts.
- **Scope**: 92+ functions across 13 ROM files were affected. Any new ROM
  function taking coordinate/size parameters must be audited for this.

**`a2` register clobber in complex functions**: GCC's m68k register allocator
sometimes clobbers `a2` across function calls in layer-creation code. The
symptom is a corrupted pointer after a `JSR` to another ROM function.

- **Fix**: Apply `__attribute__((optimize("O0")))` to the affected function.
- **Detection**: If a pointer argument suddenly points to garbage after a nested
  ROM call, suspect register clobber before suspecting memory corruption.

### 6.2 Musashi CPU Emulator Quirks

**Edge-triggered IRQ semantics**: Musashi will not re-trigger an interrupt at
the same level unless the line is lowered first. If you call `m68k_set_irq(3)`
twice without an intervening `m68k_set_irq(0)`, the second call is silently
ignored. This caused RMB menu drag events to stop processing after the first
VBlank.

- **Fix**: Always pulse: `m68k_set_irq(0); m68k_set_irq(3);`
- **Symptom**: Input events (especially multi-step drags) stop being processed
  partway through, even though VBlank appears to fire.

### 6.3 Debugging with DPRINTF/LPRINTF

The ROM has two logging macros:
- `DPRINTF(level, ...)` — compile-time gated by `ENABLE_DEBUG` in `src/rom/util.h`
- `LPRINTF(level, ...)` — always compiled in, runtime-gated by level

**Rules for diagnostic logging during debugging**:
1. It is fine to temporarily change `DPRINTF` calls to `LPRINTF(LOG_INFO, ...)`
   to get visibility without recompiling with `ENABLE_DEBUG`.
2. **Before committing**, revert ALL diagnostic `LPRINTF(LOG_INFO, ...)` back
   to `DPRINTF(LOG_DEBUG, ...)`. Leaving them in floods the log at runtime and
   makes future debugging harder.
3. ROM `LOG_LEVEL` in `src/rom/util.h` should normally be `LOG_INFO` (not
   `LOG_DEBUG`). Host `LOG_LEVEL` in `src/lxa/util.h` should be `LOG_DEBUG`.
4. If you find 15+ LPRINTF calls scattered in a function, that's a sign a
   previous debug session wasn't cleaned up — revert them.

### 6.4 ASM-One and Apps Without COMMSEQ

Some Amiga applications display keyboard shortcuts in their menus but do NOT
set the Intuition `COMMSEQ` flag on menu items. Instead, they handle shortcuts
internally via raw IDCMP keyboard events. ASM-One V1.48 is one such app.

- **Implication**: `CommKey` delivery (Amiga+key → menu item selection) will not
  work for these apps. The only reliable way to trigger their menu items from
  host-side tests is via **RMB menu drag** using `SelectMenuItem()` or
  `lxa_inject_drag()`.
- **Detection**: Check whether the menu item's `Flags` field has `COMMSEQ` set.
  If not, do not attempt CommKey-based activation.

### 6.5 Menu Interaction via RMB Drag

RMB drag menu selection is the most reliable way to interact with app menus
in host-side tests. The pattern:

1. Move mouse to menu bar area (y < 11 for screen menu bar).
2. Inject RMB down.
3. Drag through menu title → menu item → sub-item.
4. Release RMB on the target item.
5. Run settling cycles to let the app process the selection.

**Key parameters**:
- `lxa_inject_drag()` uses 500K cycles per step with 5 settling iterations.
  These values were calibrated through trial and error; reducing them causes
  intermittent failures on slower-starting apps.
- For apps that render responses in-place (not in a new window), verify the
  effect via pixel count change rather than waiting for a new window.

### 6.6 Test Timing and Cycle Budgets

Current cycle budgets are deliberately generous for correctness. A real 7 MHz
68000 processes a keypress in ~1000 cycles, but `lxa_inject_string()` uses
1,000,000 per character. This over-provisioning ensures reliability but makes
tests slow (see Performance Notes below and the roadmap for optimization plans).

**When writing new tests**:
- Do not arbitrarily increase cycle budgets to "fix" a flaky test. First
  understand why the test is flaky.
- If an app needs more cycles during startup, use `WaitForWindowDrawn()` with
  a retry loop rather than a flat `RunCyclesWithVBlank(200, 50000)`.
- Prefer event-driven waiting (window count, pixel change) over fixed cycle counts.

### 6.7 Test Sharding

When a test driver has many test cases and takes >60 seconds, split it into
shards using Google Test's `--gtest_filter` in `CMakeLists.txt`. This allows
CTest to schedule shards in parallel across cores.

Current sharded drivers: `simplegad` (behavior/pixels), `simplemenu`
(behavior/pixels), `devpac` (startup/visual/input), `maxonbasic`
(startup/input), `cluster2` (startup/input/navigation), `sigma`
(startup/interaction), `blitzbasic2` (startup/interaction), `vim`
(startup/editing).

### 6.8 SMART_REFRESH Default Behavior

`WFLG_SMART_REFRESH` has value **0**. This means every window that does not
explicitly set `WFLG_SIMPLE_REFRESH` or `WFLG_SUPER_BITMAP` is SMART_REFRESH
by default. This is architecturally significant: the vast majority of Amiga
windows expect backing-store behavior.

- **Current state (Phase 132, v0.9.6)**: Backing store is fully functional.
  Phase 111 implemented the layer-level save/restore (`SaveToBackingStore`,
  `RestoreBackingStore`, `CreateObscuredClipRect`, `RebuildClipRects` with
  restore→free→recompute→save flow in `lxa_layers.c`, plus CR-aware
  rendering primitives in `lxa_graphics.c`). Phase 132 added the
  `BackingStoreTest` sample + `backingstore_gtest.cpp` driver (4 tests)
  that prove the dialog-over-window obscure/uncover cycle restores pixels
  correctly. The defensive `IS_SMARTREFRESH` skip that Phase 111 added to
  `DamageExposedAreas()` was removed in Phase 132; the full suite passes
  67/67 with apps receiving the correct IDCMP_REFRESHWINDOW messages on
  uncover (matching real Amiga behaviour) while backing store keeps the
  visible content intact.
- **Detection macro**: `IS_SMARTREFRESH(layer)` in `lxa_layers.c` line ~53.
- **If you suspect a regression**: run `./build/tests/drivers/backingstore_gtest`
  in isolation. All four tests must pass. If any fails, do NOT re-add the
  `IS_SMARTREFRESH` skip in `DamageExposedAreas()` as a workaround — fix
  the actual save/restore path instead.

### 6.9 Library Classification: System vs Third-Party

**System libraries** (shipped with Commodore/AmigaOS): dos.library, exec.library, graphics.library, intuition.library, layers.library, utility.library, diskfont.library, datatypes.library, asl.library, gadtools.library, icon.library, workbench.library, commodities.library, iffparse.library, locale.library, keymap.library, mathffp.library, mathieeedoubbas.library, mathieeedoubtrans.library, mathieeesingbas.library, mathieeesingtrans.library, rexxsyslib.library, expansion.library, etc.
- These **must be fully implemented** in lxa. Stubs are never acceptable.
- Source lives in `src/rom/lxa_<name>.c`.
- If a function is missing or incomplete, implement it correctly per RKRM/NDK.

**Third-party libraries** (not shipped with AmigaOS): req.library, reqtools.library, powerpacker.library, bgui.library, MUI, ReAction, magic user interface, etc.
- These must **never** be implemented or stubbed in lxa.
- If an app requires one and the binary is not already in `share/lxa/System/Libs/` or the app's own `Libs/`, **STOP immediately and notify the user**.
- The user must supply the real binary. Do not proceed without it.

**Disk library build pattern** (for system libraries that are not part of ROM):
Some system libraries are disk-loaded rather than ROM-resident. Use `add_disk_library()` in `sys/CMakeLists.txt`:
1. Create source in `src/rom/lxa_<name>.c`.
2. Implement all public API vectors correctly per RKRM/NDK — no stub returns.
3. Add `add_disk_library(<target> <name>.library …)` to `sys/CMakeLists.txt`.
4. Add tests that verify correct behaviour, not just "opens without crashing".

### 6.10 Event Queue Sizing and Coalescing

SDL mouse motion events can flood the input event queue. At 60 FPS with
continuous mouse movement, SDL generates hundreds of MOUSEMOVE events per
second, which overwhelmed the original 32-slot queue.

- **Fix applied**: Queue enlarged to 256 slots (`EVENT_QUEUE_SIZE` in
  `display.c`). Added coalescing: consecutive MOUSEMOVE events are merged
  when no mouse buttons are held.
- **Design rule**: When adding new event types or input sources, consider
  whether they can generate bursts. If so, add coalescing logic.
- **Coalescing exception**: Do NOT coalesce mouse moves when buttons are held,
  because that would break drag operations (menu drag, gadget drag, etc.).

### 6.11 GfxBase Initialization Requirements

Many Amiga apps check `GfxBase` fields at startup and fail silently (immediate
exit, rv=26, or missing functionality) when they find zeros.

**Fields that must be initialized** (in `exec.c` coldstart):
- `DisplayFlags = PAL | REALLY_PAL` — apps use this to detect PAL vs NTSC
- `VBlank = 50` — PAL VBlank frequency
- `ChipRevBits0 = SETCHIPREV_ECS` — chipset revision detection
- `NormalDisplayRows/Columns`, `MaxDisplayRow/Column` — screen dimensions

**Symptom of missing initialization**: App opens and immediately closes, or
opens but shows no UI content. PPaint had rv=26 until DisplayFlags was set.

### 6.12 Custom UI Apps (No Intuition Gadgets)

Some apps use zero Intuition gadgets and render their entire UI with direct
graphics calls. Cluster2 is the prime example: gadget count is 0, MenuStrip
is NULL, `WFLG_RMBTRAP` is set. The entire toolbar, file list, and button
bar are custom-rendered.

- **Implication for testing**: `GetGadgetCount()`, `GetGadgetInfo()`, and
  `ClickGadget()` are useless for these apps. Tests must use raw coordinate
  clicks and pixel-change verification.
- **Detection**: If `GetGadgetCount()` returns 0 on a window that visually has
  buttons, the app is custom-rendering.
- **IDCMP pattern**: These apps typically request `IDCMP_MOUSEBUTTONS |
  IDCMP_RAWKEY` (and sometimes `IDCMP_INTUITICKS`) and do their own hit
  testing internally.

### 6.13 Deallocate Overlap Protection

Production Amiga apps sometimes perform double-frees or free with wrong sizes,
causing the memory free list to become corrupt. This manifests as an assertion
failure on `p1 != p1->mc_Next` (circular free list) or mysterious crashes later.

- **Fix applied**: `_exec_Deallocate()` now detects overlapping frees and
  silently skips them instead of corrupting the free list.
- **Design principle**: The emulator should be more tolerant than real hardware
  for memory management errors, because debugging these in emulation is
  extremely difficult and many commercial apps have these bugs.
- **DPaint V** was the app that triggered this — it crashed with a free-list
  corruption assertion.

### 6.14 GadTools Layout and Double-Baseline Bug

When GadTools creates labels for gadgets, it calculates the Y position of the
text. The bug was that `gt_create_label()` added `GT_FONT_BASELINE` (6) into
`IntuiText.TopEdge`, and then `_render_gadget()` in `lxa_intuition.c` added
`tf_Baseline` (also 6) again when rendering. This caused all GadTools labels
to be offset 6 pixels too low.

- **Fix**: Removed `GT_FONT_BASELINE` from all TopEdge calculations in
  `gt_create_label()` (5 sites), `gt_position_slider_level_text()` (4 sites),
  and the CYCLE_KIND inline label path.
- **Lesson**: When both GadTools and Intuition touch the same rendering
  pipeline, baseline/offset calculations must be audited end-to-end. A value
  that looks correct in isolation may be applied twice.

### 6.15 IDCMP_INTUITICKS and Qualifier Propagation

INTUITICKS fires every ~200ms (every 10th VBlank at 50 Hz PAL) to all windows
that have `IDCMP_INTUITICKS` in their IDCMPFlags. Some apps (Cluster2) toggle
this flag dynamically via `ModifyIDCMP()`.

- **Qualifier propagation**: INTUITICKS messages must carry the current input
  qualifier (shift/ctrl/alt state). This is tracked via `g_current_qualifier`
  which is updated from every mouse button, mouse move, and keyboard event.
- **Without qualifiers**: Apps that check qualifier state in INTUITICKS handlers
  (e.g., for auto-repeat while a button is held) will malfunction.

### 6.16 Screen-Mode Emulation Architecture

Many apps probe available display modes at startup using `GetDisplayInfoData()`, `BestModeIDA()`, and `NextDisplayInfo()`. As of Phase 129 (v0.9.3), lxa's ECS mode database covers 36 entries (DEFAULT/NTSC/PAL × 12 ECS modes) with realistic raster ranges and virtualisation of unknown IDs. The ECS emulation is considered complete.

**RTG supersedes ECS for modern apps**: PPaint, FinalWriter, and other productivity apps probe for P96 display IDs, not ECS modes. Their ECS-path failures are not investigated further. These apps are validation targets for Phase 139 (RTG app validation) after Phases 137–138 deliver the RTG infrastructure.

**ECS key constraints** (still relevant for legacy apps):
- `g_known_display_ids[]` in `lxa_graphics.c`: the authoritative list. Adding an ID requires `GetDisplayInfoData(DTAG_DIMS)` to also return plausible `MinRaster`/`MaxRaster` ranges.
- `graphics_virtualize_display_id()` maps unknown-but-plausible IDs to the closest physical mode (Wine strategy).
- `BestModeIDA()` honours `BIDTAG_DIPFMustHave`/`MustNotHave` flags (landed Phase 129).

**Symptom pattern**: App exits immediately (rv ≥ 26) with a flood of `FindDisplayInfo()`/`GetDisplayInfoData()` log entries — suspect RTG probe if the app is a modern productivity tool, ECS virtualisation gap if the app is an older ECS-era tool.

### 6.17 CMake Shard Coverage — Safety Rule

**Hardcoded `--gtest_filter` strings in `CMakeLists.txt` silently orphan newly added tests.** When a new `TEST_F` is added to a sharded driver's `.cpp` file but the FILTER string in `CMakeLists.txt` is not updated, `ctest` will run the shard binary but skip the new test entirely — no error, no warning.

This has already caused orphaned tests in PPaint (Phase 121) and FinalWriter (Phase 124).

**Rule**: After adding any `TEST_F` to a sharded driver, immediately update the corresponding `add_gtest_driver_shard` FILTER string in `tests/drivers/CMakeLists.txt`. The Phase 0 `tools/check_shard_coverage.py` script will catch violations in CI, but do not rely on CI to catch what you can prevent in the same commit.

**Sharded drivers** (as of Phase 124): `simplegad`, `simplemenu`, `menulayout`, `fontreq`, `simplegtgadget`, `talk2boopsi`, `gadtoolsgadgets`, `vim`, `sigma`, `blitzbasic2`, `finalwriter`. Any new driver taking >60 seconds should be sharded from the start — see §6.7.

### 6.18 Text Hook Architecture (Phase 130)

`_graphics_Text()` in `lxa_graphics.c` is the single choke point for all ROM text rendering. Once Phase 130 adds `lxa_set_text_hook()`, tests can assert text content semantically instead of pixel-counting.

**Pattern for new drivers** (after Phase 130 lands):
```cpp
// In SetUp():
lxa_set_text_hook([](const char *s, int n, int /*x*/, int /*y*/, void *ud) {
    ((std::vector<std::string>*)ud)->push_back(std::string(s, n));
}, &text_log_);

// In TearDown():
lxa_clear_text_hook();

// In tests:
EXPECT_TRUE(std::any_of(text_log_.begin(), text_log_.end(),
    [](const auto &s){ return s.find("EDITOR") != std::string::npos; }));
```

Until Phase 130 lands, use pixel-region counting (the existing approach) or DPRINTF log scanning as a proxy. Do not attempt to implement text extraction via ad-hoc DPRINTF log parsing — wait for the hook.

### 6.19 RTG / Picasso96 Architecture (Phases 137–139)

lxa's display strategy is RTG-first via Picasso96 (`Picasso96API.library`). Agents working on Phases 137–139 need these orientation points:

**Chunky BitMap flag**: Phase 137 introduces `BMF_RTG` (or an equivalent internal flag) in `BitMap.Flags` to distinguish chunky RTG bitmaps from classic planar bitmaps. Before touching any graphics code that iterates `bm->Planes[]`, check this flag — RTG bitmaps store all pixels in `bm->Planes[0]` as a contiguous RGBA buffer.

**SDL2 RTG path**: `display_update_rtg()` in `display.c` accepts a raw 32-bit RGBA buffer and uploads it to an `SDL_PIXELFORMAT_RGBA32` texture. The planar path (`display_update_planar()`) is unchanged. Do not mix the two paths for the same display handle.

**P96 disk library pattern**: `Picasso96API.library` is a disk library (`src/rom/lxa_p96.c`, compiled via `add_disk_library()` in `sys/CMakeLists.txt`). It follows the same pattern as `lxa_amigaguide.c` — init function, function table, stub returns for unimplemented functions. The `RenderInfo` struct (`Memory`, `BytesPerRow`, `RGBFormat`) is the primary pixel-buffer descriptor used by all P96 pixel-transfer functions.

**cybergraphics shim**: Many apps probe `cybergraphics.library` before `Picasso96API.library`. The CGX shim is a thin disk library that maps CGX function names (`GetCyberMapAttr`, `LockBitMapTags`, `WriteLUTPixelArray`, etc.) to their P96 equivalents. Implement it as a separate `add_disk_library()` target — do not merge it into `lxa_p96.c`.

**RTG display IDs**: P96 mode IDs live in a different monitor-ID namespace from ECS IDs. Add them to `g_known_display_ids[]` in `lxa_graphics.c` and ensure `GetDisplayInfoData(DTAG_DIMS)` returns host-resolution ranges (up to 1920×1200) for them. Do not set `DIPF_IS_ECS` or `DIPF_IS_HAM` flags on RTG mode descriptors.

**Symptom of missing P96**: App opens and immediately exits; `lxa.log` shows `OpenLibrary("Picasso96API.library", ...)` returning NULL followed by an exit. Check that the library is installed in `share/lxa/System/Libs/` and that `add_disk_library()` is in `sys/CMakeLists.txt`.

### 6.20 Disassembling Amiga App Binaries (Phase 151)

When an app misbehaves in lxa but its source is unavailable, **disassemble the binary** instead of guessing. This often pinpoints the exact ROM API call sequence the app expects, saving hours of speculation. Load the `develop-amiga` skill §13 for the full workflow; the short version:

```bash
export PATH=/opt/amiga/bin:$PATH
m68k-amigaos-objdump -D /path/to/AppBinary > /tmp/app.dis     # full disasm
m68k-amigaos-strings -t x /path/to/AppBinary > /tmp/app.str    # all strings + offsets
m68k-amigaos-nm /path/to/AppBinary                              # symbols if present
```

`m68k-amigaos-objdump` works directly on AmigaOS hunk-format executables (no need to extract sections first). Strings and string xrefs are the fastest way to locate the routine that opens a specific dialog, requester, or window.

**When to disassemble** (in priority order):
1. App opens a window but the body is blank → find what the app does immediately after `OpenWindow*` returns (it usually calls `GT_RefreshWindow`, walks `GfxBase->MonitorList`, enumerates `NextDisplayInfo`, etc.). The empty body almost always traces back to one of these enumerator calls returning empty in lxa.
2. App exits silently with rv ≥ 26 → find the OpenLibrary chain and the first probe-and-bail check.
3. App calls a documented ROM function with unexpected arguments → confirm the calling convention by reading the surrounding asm.

**For full-task disassembly** (sweeping a 600+ KB binary for a specific feature), launch a `general` subagent with a focused prompt — the disassembly output is too large to inspect in the main context. Phase 151 used this pattern successfully to locate DPaint's mode-list populator at `0x16aac` from a 2-line clue ("Screen Format dialog body is blank").

### 6.21 "Stub Returning NULL" is the Single Most Common Root Cause

When an Amiga system library function in `src/rom/lxa_<libname>.c` is implemented as a **stub returning NULL** (often with a comment like *"apps should handle NULL gracefully"*), it almost certainly breaks one or more apps that the comment author did not test against. Real AmigaOS never returns NULL from these functions for documented well-formed inputs. Examples found in the wild:

- `OpenMonitor(NULL, 0)` returning NULL → DPaint Screen Format dialog mode-list panel is blank (Phase 151).
- (Add future findings here as they surface.)

**Audit pattern**: Before adding new functionality, grep the relevant ROM file for `returning NULL`, `not available`, `not implemented`, or `apps should handle` comments. Each hit is a latent compatibility bug. Per AGENTS.md §1, system libraries must be fully implemented — these are not acceptable end states.

**Detection from outside**: A symptom that *looks* like an Intuition rendering bug (blank panels, missing widgets) is frequently a graphics/exec stub returning NULL one layer down. Before instrumenting Intuition rendering paths, list every system call the app makes between `OpenWindow` and the first `WaitPort`/`Wait` and check each one for stub status.

### 6.22 LPRINTF Stderr Interleaving — Diagnostic Logging Pitfall

When ROM code calls `LPRINTF()`/`DPRINTF()` from inside a high-frequency hook (e.g., the VBlank IRQ handler `VBlankInputHook`, the per-pixel `WritePixel`, the inner mouse-event loop), the host stderr stream interleaves at byte boundaries because Musashi may dispatch the next CPU cycle that triggers another `EMU_CALL_LPUTS` mid-line. Multi-line dumps from a single function become **shredded** in `lxa.log`, with `_intuition: VBlankInputHook…` snippets injected mid-`%08lx`.

**Symptoms**: `grep "MyLog: bounds=(0,0)-"` finds the prefix but the rest of the line is garbage; `grep -c MyLog` returns 1 when you expected 16.

**Workarounds** (in order of preference):
1. **Make each LPRINTF a single short line** with a unique stable prefix (e.g., `P151_GAD %d t=%x f=%x p=%d,%d s=%dx%d R=%lx T=%lx S=%lx`). Then `grep -aoE "P151_GAD [^_]+"` extracts only well-formed lines.
2. **Suppress the interleaving source temporarily**: lower the offending high-frequency LPRINTF (e.g., `VBlankInputHook calling PIE`) to `DPRINTF(LOG_DEBUG, …)` only for the duration of the diagnostic session — but **revert before commit** (see §6.3).
3. **Do NOT try to write to a host file from ROM via `fopen`/`fprintf`**: those symbols don't exist in the m68k ROM environment (linker error: `undefined reference to fopen`). The only viable host-bridge from ROM is `lputc`/`lputs`/`lprintf` via the `EMU_CALL_LPUTS` illegal-instruction trap.
4. **Removing a frequently-called LPRINTF can change test timing**: Phase 151 observed that lowering `VBlankInputHook calling PIE` from `LPRINTF(LOG_INFO)` to `DPRINTF(LOG_DEBUG)` caused a previously-passing dismiss flow in `dpaint_gtest` to fail. The host-side `lputs` syscall implicitly burns wall-clock time and acts as a soft throttle. Either keep the LPRINTF (and use workaround #1) or compensate with explicit settling cycles in the test.

**Rule for new instrumentation**: Always design the LPRINTF format to be **one line, ≤120 chars, with a unique prefix grep can anchor on**. Multi-line dumps via consecutive LPRINTFs are unsafe in any code path that runs more than ~10 times per second.

### 6.23 ROM Code Cannot Use Host stdio

ROM code (`src/rom/`) is cross-compiled to m68k and linked against a tiny ROM-only runtime. **Standard C library headers are not usable**:

- `<stdio.h>`: declarations conflict with `src/rom/util.h` (both declare `strlen`, `memcpy`, etc. with slightly different signatures). Including it triggers `error: conflicting types for 'strlen'`.
- `fopen`, `fprintf`, `fclose`, `printf`, `puts`, etc.: not provided by the ROM linker. Forward-declaring them manually compiles but fails at link time with `undefined reference to fopen`.
- `malloc`/`free`: do not exist in ROM. Use `AllocVec`/`FreeVec` or `AllocMem`/`FreeMem`.

**The only host-side I/O bridge available from ROM is the `lputc`/`lputs`/`lprintf` family** in `src/rom/util.c`, which uses `EMU_CALL_LPUTC`/`EMU_CALL_LPUTS` illegal-instruction traps to call back into the host emulator. All ROM diagnostic output must go through these. To send arbitrary binary data to the host, add a new `EMU_CALL_*` opcode and a host-side handler in `src/lxa/emu.c`.

## 7. Quick Start
1. Check `roadmap.md`.
2. Load `lxa-workflow` to understand the process.
3. Load `develop-amiga` for coding standards.
4. Load `lxa-testing` to plan your tests.
5. For UI tests, load `graphics-testing` for host-side driver patterns.
