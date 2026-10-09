# lxa Development Guide for Agents

This document is the entry point for AI agents working on the `lxa` codebase. Treat it as the short operational checklist, then load the relevant skills for the task at hand.

## 1. Core Mandates
- **Stability**: Zero tolerance for crashes.
- **Coverage (measurable since Phase 202)**: every public LVO of every system library has at least one test that executes it, and line coverage (ROM + host) **never decreases phase over phase**. Measure with `cmake -B build-cov -DLXA_COVERAGE=ON && make -C build-cov coverage`; it writes `build-cov/coverage/` (lcov tracefiles + `index.html`) and regenerates `doc/coverage/lvo-coverage.md`, which is checked in. Record the new numbers in the roadmap scoreboard when a phase closes.
- **Zero failing tests at phase completion**: The full test suite (`ctest --test-dir build -j16 --timeout 180`) must report **zero failures and zero timeouts** before any phase is declared done. Failures inherited from earlier phases are NOT acceptable as "pre-existing" — fix them, or formally quarantine them with a `DISABLED_` GTest prefix AND a roadmap entry justifying the deferral. Hand-waving "5 pre-existing failures, unrelated to my phase" is forbidden.
- **Reference**: ALWAYS consult RKRM and NDK.
- **System Libraries Must Be Fully Implemented**: AmigaOS system libraries (dos.library, exec.library, graphics.library, intuition.library, datatypes.library, etc.) must have complete, correct implementations — not stubs. Stub implementations of system libraries are never acceptable; they mask bugs and break app compatibility. If a system library function is missing or incomplete, implement it properly.
- **Every Remaining Gap Is Visible (Phase 203)**: until a function is complete, it must call `LXA_UNIMPLEMENTED(lib, fn, "stub: …" | "partial: …")` as its first statement (`src/rom/util.h`). Never add a silent stub or a "simplified" path without it. Regenerate `doc/stub-inventory.md` with `tools/stub_inventory.py` (ctest `stub_inventory_check` enforces it). Runs record every hit (`lxa_get_unimplemented_log()`, summary in `lxa.log`); `lxa --strict-unimplemented` / `lxa_config_t.strict_unimplemented` stops at the first stub (exit 125). An entry leaves the inventory only by implementing the function.
- **Third-Party Libraries: STOP and Notify the User**: Do NOT implement or stub third-party (non-Commodore-OS) libraries (e.g., req.library, reqtools.library, powerpacker.library, bgui.library, MUI, etc.). These must be supplied as real binaries on disk. If a required third-party library binary is missing, **STOP immediately and notify the user** — do not write a stub and do not proceed. The user must provide the real library binary.
- **Complete Phases if possible**: When working on a phase of the roadmap, plan ahead and make sure you create all the TODO items needed to **successfully complete** the phase. Do not stop early, but strive towards reaching the goal of finishing a phase.
- **Keep the roadmap.md file updated**: Whenever you complete, defer, or re-scope work, update `roadmap.md` so it stays clean, current, and future-focused. If you intentionally park unfinished work, document that decision explicitly instead of leaving ambiguous unchecked items behind.
- **Host-Side Test Drivers for UI Tests**: All interactive UI tests (gadget clicks, menu selection, keyboard input) MUST use the host-side driver infrastructure (`tests/drivers/` with liblxa) and Google Test. The legacy `test_inject.h` approach has been removed. All tests are integrated into the unified GTest suite.
- **Priority Order**: Schedule work strictly as **1) Quality → 2) Amiga compatibility → 3) Performance → 4) New features**. A lower-priority phase may not be promoted ahead of a higher-priority phase that is ready to start. RTG, MUI, ReAction, and external-process emulation are all "new features" and yield to any open quality or compatibility item.
- **No Pooling Sections in `roadmap.md`**: The roadmap must not contain catch-all lists like "Deferred Test Failures", "Known Limitations", "TODO", or "Backlog". Every issue is either a TODO bullet inside an existing scheduled phase (same root cause) **or** promoted into its own numbered phase with explicit objectives, sub-problems, TODO checkboxes, and a test gate. The only retrospective section permitted is the `## Completed Phases (Summary)` table.
- **Every `DISABLED_` GTest Must Be the Objective of a Scheduled Phase**: When you quarantine a test with the `DISABLED_` prefix, you must in the same commit create (or extend) a numbered phase whose explicit objective is to fix the root cause and re-enable the test. A `DISABLED_` test without an owning phase is forbidden.

## 1a. Reference-Driven Development (RDD)

Since v0.11 lxa is developed against a **real AmigaOS 3.1 reference machine** (FS-UAE, see `roadmap.md` "Canonical Reference Configuration" and the `rdd-reference` skill). These rules are mandatory:

1. **Real AmigaOS 3.1 is the oracle.** The RKRM and NDK explain *why*; the reference machine settles *what*. Expected values (goldens, `expected.ref.out`, probe outputs) come from the reference backend, never from lxa and never from a hand-written guess.
2. **Structure first, then pixels, then vision.** Compare Intuition trees, then pen-index pixels; use Claude vision only for triage and explanation. Every vision finding becomes a deterministic tree or pixel assertion before the phase closes (`rdd-review` skill).
3. **Breadth before depth.** Sweep many apps shallowly, cluster divergences by root cause, fix in order of frequency (`compat-sweep` skill).
4. **Goldens are never edited to match lxa.** A golden comes only from the reference backend. If you believe a golden is wrong, re-capture it on the reference.
   Goldens live in `tests/golden/<app>/<scenario>/` (`python3 -m rdd promote <scenario.yaml> --phase N` from `tools/`; CTest label `golden`, one test per golden). Each golden is a **ratchet**: the lxa divergence at promotion time (known tree diffs, pixel budget) is recorded with the phase that owns it; any new divergence fails, any improvement prints `tighten` — then re-promote. A golden may only be quarantined (`"disabled": {"phase": N}` in `golden.json`) by a phase whose objective is reaching equivalence; `golden_lint` enforces that every phase named exists.
5. **Clean-room.** Observe AmigaOS only as a black box: run it, probe it, trace it. **Never consult leaked AmigaOS source or Kickstart disassembly** (material under `~/media/sys/amiga/os/` is off limits for implementation). Disassembling *applications* remains allowed (§6.20). Porting **AROS** code is allowed (it is a clean-room reimplementation); keep its licence headers and record it in `doc/third-party-code.md`.

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

When a GTest driver captures a failure artifact via `lxa_capture_window()` or `lxa_capture_screen()` and the pixel-level assertion message alone does not explain the visual defect, open the captured PNG with the `Read` tool — Claude Code reads images natively, no external tool or API key is required. Describe layout, clipping, text rendering, or menu artifacts, then translate the findings into a deterministic tree/pixel/geometry assertion so the defect is caught automatically in future runs (RDD principle 2 below).

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

- **`rdd-reference`** (`.claude/skills/rdd-reference`): operating the FS-UAE reference system (`tools/refsys/`), the `lxaprobe` guest agent, the twin runner (`tools/rdd/`) and golden promotion.

- **`rdd-review`** (`.claude/skills/rdd-review`): protocol for comparing lxa vs reference snapshot bundles (tree diff → pen-index diff → vision triage) and writing `findings.yaml`.

- **`compat-sweep`** (`.claude/skills/compat-sweep`): `python3 -m rdd loop`, parallel per-app triage (one subagent per app), clustering divergences into phases, maintaining `apps/compat.yaml`.

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
  work for these apps. Trigger their menu items by RMB drag with
  `lxa_select_menu_path()` (§6.5).
- **Detection**: Check whether the menu item's `Flags` field has `COMMSEQ` set.
  If not, do not attempt CommKey-based activation.

### 6.5 Menu Interaction (Phase 204 API)

Never hard-code menu coordinates. Use the live MenuStrip:

- `lxa_select_menu_path(win, "Project/Save As...")` (or `lxa_select_menu(win, m, i, sub)`)
  selects an entry by RMB drag: press on the title, move to the item (and
  sub-item), release. Matching is case-insensitive; a trailing `...` is optional.
- `lxa_find_menu_path()` resolves a path to indices; `lxa_get_menu_rect()`
  returns the screen rectangle of a title/item/sub-item, computed exactly as
  Intuition lays out the drop-down.
- `lxa_inject_drag_begin()` / `_step()` / `_end()` split a drag so tests can
  inspect state mid-drag (see `MenuPixelStateDuringDrag`). Release on the menu
  bar to cancel without selecting.
- `lxa_get_qualifier_state()` returns the qualifier of the last delivered
  event (e.g. `IEQUALIFIER_RBUTTON` while the menu button is held).

Each drag phase waits until rendering has finished (no task ready, no
interrupt in progress). Opening a large drop-down currently costs several
frames of emulated time (pixel-based ROM rendering, Phase 274).

### 6.6 Test Timing: the Deterministic Virtual Clock (Phase 201)

liblxa (all GTest drivers) runs on a **deterministic virtual clock** by default
(`src/lxa/lxa_vclock.c`). Interactive `lxa` keeps the wall clock unless started
with `--deterministic`.

- **Time = emulated cycles.** The virtual CPU runs at 25 MHz (A4000-class
  reference machine); one 50 Hz PAL frame is 500 000 cycles. VBlank,
  timer.device, `DateStamp()`, `GetSysTime()` and `ReadEClock()` all derive from
  the cycle counter; the boot date is 2024-01-01 (`epoch_secs` overrides).
- **Idle time is skipped**, not slept: when no task is ready the clock jumps to
  the end of the current slice. Waiting for a 5 s `Delay()` costs ~nothing.
- **Same scenario, same bytes.** Two runs produce identical captures, output,
  cycle and frame counts (`determinism_gtest`). If a test is flaky, it is a bug
  in lxa or in the test, never "timing noise".
- **Timeouts are emulated time.** `lxa_wait_*()`, `lxa_run_until_exit()` and the
  `EmuDeadline` fixture helper measure their timeout in emulated ms; a
  wall-clock guard (10x, ≥60 s) only catches real hangs. Never add
  `steady_clock`/`gettimeofday` waits to tests.
- **Wait in frames, wait for state.** Prefer `RunFrames(n)`,
  `WaitForStableContent()`, `WaitForWindows()` and text-hook/output conditions
  over fixed `RunCyclesWithVBlank()` loops. A program doing `Delay(50)` needs
  50 frames, whatever the cycle budget.
- **Emulated time exposes ROM cost.** Every ROM cycle is now visible time; a
  slow ROM routine (e.g. per-pixel `Text()`/`RectFill()`) makes apps slow in
  emulated seconds. `lxa_get_idle_cycles()`, `lxa_get_pc()` and
  `tools/rom_symbolize.py` find the hot spot.
- **A/B debugging**: `LXA_REALTIME_CLOCK=1` forces the old wall-clock mode for
  one run. Phase 201 found several tests that only passed because a slow
  wall-clock paint was still in progress — check such "passes" with captures.
- **Stress gate**: `tools/stress_suite.sh -n 20 -l 16` runs the suite 20x under
  load and backtraces any driver alive >60 s.

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

**RTG supersedes ECS for modern apps**: PPaint, FinalWriter, and other productivity apps probe for P96 display IDs, not ECS modes. Their ECS-path failures are not investigated further. These apps are validation targets for Phase 243 (RTG app validation) after Phases 241–242 deliver the RTG infrastructure.

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

### 6.19 RTG / Picasso96 Architecture (Phases 241–243)

lxa's display strategy is RTG-first via Picasso96 (`Picasso96API.library`). Agents working on Phases 241–243 need these orientation points:

**Chunky BitMap flag**: Phase 241 introduces `BMF_RTG` (or an equivalent internal flag) in `BitMap.Flags` to distinguish chunky RTG bitmaps from classic planar bitmaps. Before touching any graphics code that iterates `bm->Planes[]`, check this flag — RTG bitmaps store all pixels in `bm->Planes[0]` as a contiguous RGBA buffer.

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

**Audit pattern**: `doc/stub-inventory.md` lists every known stub/partial (Phase 203). Run the app with `--strict-unimplemented` or inspect the `lxa.log` summary to see which ones it actually hits. Comments such as `returning NULL`, `not implemented` or `apps should handle` without an `LXA_UNIMPLEMENTED` call are bugs in the inventory: add the macro.

**Detection from outside**: A symptom that *looks* like an Intuition rendering bug (blank panels, missing widgets) is frequently a graphics/exec stub returning NULL one layer down. Before instrumenting Intuition rendering paths, list every system call the app makes between `OpenWindow` and the first `WaitPort`/`Wait` and check each one for stub status.

### 6.22 LPRINTF Stderr Interleaving — Diagnostic Logging Pitfall

When ROM code calls `LPRINTF()`/`DPRINTF()` from inside a high-frequency hook (e.g., the VBlank IRQ handler `VBlankInputHook`, the per-pixel `WritePixel`, the inner mouse-event loop), the host stderr stream interleaves at byte boundaries because Musashi may dispatch the next CPU cycle that triggers another `EMU_CALL_LPUTS` mid-line. Multi-line dumps from a single function become **shredded** in `lxa.log`, with `_intuition: VBlankInputHook…` snippets injected mid-`%08lx`.

**Symptoms**: `grep "MyLog: bounds=(0,0)-"` finds the prefix but the rest of the line is garbage; `grep -c MyLog` returns 1 when you expected 16.

**Workarounds** (in order of preference):
1. **Make each LPRINTF a single short line** with a unique stable prefix (e.g., `P151_GAD %d t=%x f=%x p=%d,%d s=%dx%d R=%lx T=%lx S=%lx`). Then `grep -aoE "P151_GAD [^_]+"` extracts only well-formed lines.
2. **Suppress the interleaving source temporarily**: lower the offending high-frequency LPRINTF (e.g., `VBlankInputHook calling PIE`) to `DPRINTF(LOG_DEBUG, …)` only for the duration of the diagnostic session — but **revert before commit** (see §6.3).
3. **Do NOT try to write to a host file from ROM via `fopen`/`fprintf`**: those symbols don't exist in the m68k ROM environment (linker error: `undefined reference to fopen`). The only viable host-bridge from ROM is `lputc`/`lputs`/`lprintf` via the `EMU_CALL_LPUTS` illegal-instruction trap.
4. **LPRINTF costs emulated time**: formatting runs in m68k code, so every LPRINTF burns thousands of emulated cycles. Phase 201 demoted the per-VBlank/per-Signal INFO lines (VBlank overhead dropped from 11 % to 2.5 % of a frame). Never leave INFO-level LPRINTFs in hot paths (VBlank, Signal/Wait, input, per-item rendering).

**Rule for new instrumentation**: Always design the LPRINTF format to be **one line, ≤120 chars, with a unique prefix grep can anchor on**. Multi-line dumps via consecutive LPRINTFs are unsafe in any code path that runs more than ~10 times per second.

### 6.23 ROM Code Cannot Use Host stdio

ROM code (`src/rom/`) is cross-compiled to m68k and linked against a tiny ROM-only runtime. **Standard C library headers are not usable**:

- `<stdio.h>`: declarations conflict with `src/rom/util.h` (both declare `strlen`, `memcpy`, etc. with slightly different signatures). Including it triggers `error: conflicting types for 'strlen'`.
- `fopen`, `fprintf`, `fclose`, `printf`, `puts`, etc.: not provided by the ROM linker. Forward-declaring them manually compiles but fails at link time with `undefined reference to fopen`.
- `malloc`/`free`: do not exist in ROM. Use `AllocVec`/`FreeVec` or `AllocMem`/`FreeMem`.

**The only host-side I/O bridge available from ROM is the `lputc`/`lputs`/`lprintf` family** in `src/rom/util.c`, which uses `EMU_CALL_LPUTC`/`EMU_CALL_LPUTS` illegal-instruction traps to call back into the host emulator. All ROM diagnostic output must go through these. To send arbitrary binary data to the host, add a new `EMU_CALL_*` opcode and a host-side handler in `src/lxa/emu.c`.

### 6.24 FS-UAE Reference Captures vs Canonical Amiga Geometry

Reference screenshots taken from FS-UAE (or any other emulator) often include **non-default overscan** that does not match real Amiga canonical geometry. Before using a reference image as ground truth for a pixel/layout test, verify what overscan and screenmode were active when the capture was taken. The canonical PAL Workbench is **640×256** (RKRM, `GfxBase->NormalDisplayRows == 256`); the canonical NTSC Workbench is **640×200**. OSCAN_STANDARD/MAX captures (e.g. 704×284, 720×284, 640×290) are *valid* Amiga configurations but are **not** what apps see by default on a freshly-booted system.

**Symptom**: A test asserts `image.height == reference.height ± 2`, the lxa capture is 247 px tall, the reference is 290 px tall, and the difference of 43 px tempts you to "fix" lxa's Workbench height. **Don't.** First check:

1. Does `lxa.log` show `OpenWorkBench` opening at 640×256? If yes, lxa is correct per RKRM.
2. Does `GfxBase->NormalDisplayRows` return 256? If yes, lxa is correct.
3. Was the FS-UAE reference taken with `Settings → Hardware → Amiga Model → Overscan` set to anything other than "Standard"? If yes, the reference is from an overscan configuration.

**Phase 158b/c (DPaint Screen Format)** was originally framed as a "coordinate origin bug" and a "PAL height clipping bug" because the lxa dialog (640×247) was visibly smaller than the FS-UAE reference (639×290). Disassembly proved DPaint sizes its dialog from `Screen.Height − 13`, so the smaller canonical Workbench yields a proportionally smaller dialog and GadTools auto-layout places panel headings further down within the upper framed panels — **this is correct rendering of correct geometry**. The phases were re-framed (no code change) and the test (`ScreenFormatLayoutOnCanonicalPALWorkbench`) re-enabled with assertions for canonical-PAL geometry instead of FS-UAE-overscan geometry.

**Rule**: Re-capture references on lxa (or on a real Amiga at the documented default mode) before asserting pixel-accurate equality. Do not change Workbench/screen geometry to match an overscan-captured reference image.

### 6.25 Relay Trace and `tracediff` (Phase 233)

When an app behaves differently on lxa and the cause is unclear, compare its **library calls** with real AmigaOS before forming hypotheses (the WINE `+relay` workflow):

```yaml
# tests/scenarios/<x>.yaml
steps:
  - trace: "dos.library:Open,Seek,Close;graphics.library:Text"   # NDK names or LVOs; lib:* = all
  - launch
  ...
```
```bash
cd tools && python3 -m rdd run ../tests/scenarios/<x>.yaml --out ../build/rdd-trace
python3 -m rdd tracediff ../build/rdd-trace/<x>        # first divergence: call order or return value
```

- lxa side: `LXA_TRACE=<spec>` / `LXA_TRACE_FILE` or `lxa_trace_start()` (pylxa `trace_start`); reference side: lxaprobe `TRACE`. Same JSON-lines format.
- Only top-level calls of the application are compared: calls from lxa's ROM code and calls nested inside another traced call are library-internal. Arguments and return values are typed by the NDK clib prototypes (pointers/BPTRs compare as NULL/non-NULL; VOID returns are ignored).
- `LXA_TRACE_INJECT="lib:lvo:delta"` offsets a function's return values — a deliberate bug for testing the tool (`tests/rdd/test_tracediff.py`).
- First catches: `Close()` returned 1 instead of DOSTRUE; `Seek()` returned the new instead of the old position (and allowed seeking past EOF).

### 6.26 Library Override Mode (Phase 235, diagnostic only)

To bisect whether a divergence lives in one of lxa's libraries or below it, run with the user's own AmigaOS 3.1 binary of that library:

```bash
LXA_OVERRIDE=iffparse,asl lxa ...        # or pylxa / GTest drivers (read at lxa_init)
LXA_OVERRIDE_DIR=/path/to/Libs           # default: ~/.cache/lxa/refsys/SYS-aga/Libs
```

The built-in library stays private to lxa's ROM and `OpenLibrary()` loads `LIBS:<name>` from that directory. Only hardware-independent libraries that 3.1 itself loads from disk are accepted (asl, iffparse, diskfont, locale, commodities, datatypes, amigaguide, math*, rexxsyslib, ...). If the app behaves like on the reference with the override, the bug is in lxa's library; if not, it is below (exec/graphics/intuition/dos). Never commit or ship those binaries; lxa's own implementations stay mandatory (§1). `tests/exec/libident` prints which implementation answered.

### 6.27 Shell and Command Execution (Phase 221)

- C: commands run **inside the shell's process** via `RunCommand()` (as on AmigaOS): a command's stack overflow corrupts the shell. lxa gives commands at least 16 KB of stack because ROM routines need more than 4 KB.
- `System()`/`Execute()` run their command line through the shell (`sys/System/Shell.c`), so variables, aliases, redirection and return codes behave like a 3.1 CLI; `C:Execute` runs command files (`.key/.def/.bra/.ket`).
- `NameFromLock()` returns volume-based names (never assign names); protection bits live in a host extended attribute.
- Output parity is checked by `tests/shell_parity/*.script` against WB 3.1's own commands; add a script line rather than a hand-written expectation when you change a command.

### 6.28 Window Chrome Facts (Phase 223, reference-verified)

- System gadgets come **first** in a window's gadget list and have GadgetID 0: identify them by `GadgetType & GTYP_SYSTYPEMASK`, never by position or ID.
- A window opened by tags without `WA_MaxWidth/Height` gets its initial size as maximum; without `WA_Top` it opens just below the screen title bar.
- The Workbench screen: `WBorTop` 2, `MenuHBorder`/`MenuVBorder` 4/2, pen 3 = 102,136,187, display ID = the requested mode.
- `python3 -m rdd gallery` renders every gallery golden next to the reference; text differences there are the ROM font (Phase 225), not layout.

### 6.29 Register Contracts, Volumes and the Harness Workbench (Phase 238)

- **"Preserves all registers" is ABI.** Disable/Enable/Forbid/Permit and ObtainSemaphore/ObtainSemaphoreShared/ReleaseSemaphore go through `EXEC_PRESERVE_ALL` wrappers (`exec.c`); GCC's C code may clobber D0/D1/A0/A1. AmiBlitz3 kept a list pointer in D0 across `Disable()` - the symptom was an `AllocPooled(NULL, …)` thousands of calls later. Relay traces carry `"ra"` (caller's return address): search the app's binaries for the bytes before it to find the call site.
- **Volumes**: the boot volume is "System", RAM: is "Ram Disk" (`vfs_set_volume_name()`); NameFromLock/Examine use volume names and they work as path prefixes. Never assign a name that is also a volume name (the old `System:` assign). pylxa builds a private SYS: root laid out like the WB 3.1 partition; T:/ENV: live in RAM:.
- **`LXA_WB_WINDOW=1`** (suite-ref and mass runs) opens LoadWB's backdrop window on the Workbench screen before the program starts, as on the reference; it is `app: false` in tree dumps and not in `lxa_get_window_count()`.

## 7. Quick Start
1. Check `roadmap.md`.
2. Load `lxa-workflow` to understand the process.
3. Load `develop-amiga` for coding standards.
4. Load `lxa-testing` to plan your tests.
5. For UI tests, load `graphics-testing` for host-side driver patterns.
