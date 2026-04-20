# lxa Development Guide for Agents

This document is the entry point for AI agents working on the `lxa` codebase. Treat it as the short operational checklist, then load the relevant skills for the task at hand.

## 1. Core Mandates
- **Stability**: Zero tolerance for crashes.
- **Coverage**: 100% test coverage required.
- **Reference**: ALWAYS consult RKRM and NDK.
- **Third-Party Libraries**: Do not re-implement third-party libraries in ROM. They must be supplied on disk and opened through the normal Amiga library search path.
- **Complete Phases if possible**: When working on a phase of the roadmap, plan ahead and make sure you create all the TODO items needed to **successfully complete** the phase. Do not stop early, but strive towards reaching the goal of finishing a phase.
- **Keep the roadmap.md file updated**: Whenever you complete, defer, or re-scope work, update `roadmap.md` so it stays clean, current, and future-focused. If you intentionally park unfinished work, document that decision explicitly instead of leaving ambiguous unchecked items behind.
- **Host-Side Test Drivers for UI Tests**: All interactive UI tests (gadget clicks, menu selection, keyboard input) MUST use the host-side driver infrastructure (`tests/drivers/` with liblxa) and Google Test. The legacy `test_inject.h` approach has been removed. All tests are integrated into the unified GTest suite.

## 2. Standard Workflow
1. Read `roadmap.md` and identify the active phase or the follow-up you are explicitly parking.
2. Load the relevant skills before editing code or documentation.
3. Preserve the boundary between ROM functionality and disk-provided third-party components.
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

### Visual Investigation with `tools/screenshot_review.py`

When a captured screenshot shows an unexpected visual result and the root cause is not obvious from code inspection alone, use the OpenRouter-backed screenshot review helper to get a structured visual analysis:

```bash
# Review a single capture
python tools/screenshot_review.py path/to/capture.png

# Compare multiple shots (before/after, or lxa vs reference)
python tools/screenshot_review.py before.png after.png

# Override the model when a specific vision model is preferred
python tools/screenshot_review.py --model anthropic/claude-sonnet-4.6 shot.png

# Supply a focused prompt when investigating a specific known issue
python tools/screenshot_review.py --prompt "Describe the menu separator rendering" shot.png

# Machine-readable output for scripting
python tools/screenshot_review.py --output json shot.png
```

**Requirements**: set `OPENROUTER_API_KEY` in the environment before calling the tool.

**When to use it**:
- A GTest driver captures a failure artifact via `lxa_capture_window()` and the pixel-level assertion message alone does not explain the visual defect.
- You need to triage a UI regression that involves layout, clipping, text rendering, or menu drawing without a real Amiga for reference comparison.
- You want a quick second opinion on whether a rendered window looks correct before writing a pixel-level regression test.

**When NOT to use it**:
- The bug is purely algorithmic (wrong calculation, wrong state) — read the code instead.
- You have not yet captured a screenshot — run the GTest driver first to produce artifacts.
- Automated CI paths — the tool requires a live OpenRouter API key and is intended for developer investigations only.

See the `graphics-testing` skill for the full workflow and how to integrate capture + review into a debugging session.

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

- **Current state**: Backing store is not fully implemented. The
  `DamageExposedAreas()` skip for SMART_REFRESH layers prevents spurious
  REFRESHWINDOW messages (which caused "dialog stamping" artifacts), but
  obscured content is still lost when covered by another layer.
- **Symptom of incomplete backing store**: When a dialog is opened over a
  window and then closed, the area behind it shows garbage or the dialog's
  content is "stamped" into the window.
- **Detection macro**: `IS_SMARTREFRESH(layer)` in `lxa_layers.c` line ~53.
- **Previous attempt**: A full backing store implementation was written,
  tested, and reverted because it caused regressions (SIGMAth2 Analysis
  window, requester test timeouts). The incremental approach in Phase 111
  addresses this.

### 6.9 Disk Library Pattern

Third-party libraries must NOT be in ROM. They are compiled as disk libraries
using `add_disk_library()` in `sys/CMakeLists.txt` and installed to
`share/lxa/System/Libs/`.

**How to create a new stub library**:
1. Create source in `src/rom/lxa_<name>.c` (the source lives in `src/rom/`
   but is compiled separately, not linked into ROM).
2. Follow the pattern in `lxa_amigaguide.c` or `lxa_rexxsyslib.c`:
   - Init function that sets up the Library node.
   - Function table with all public vectors (use NDK Autodocs for the list).
   - Each function returns a safe failure value (NULL, 0, FALSE).
3. Add `add_disk_library(<target> lxa_<name>.c <name>.library <version>)`
   to `sys/CMakeLists.txt`.
4. Add a startup test that verifies `OpenLibrary("<name>.library", 0)` succeeds
   and key functions return clean failure values.

**Key principle**: Stubs exist so apps get a clean "library not available" code
path rather than crashing on OpenLibrary failure. If an app truly needs the
library's functionality, that's a deeper problem.

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

Many apps probe available display modes at startup using `GetDisplayInfoData()`, `BestModeIDA()`, and `NextDisplayInfo()`. lxa's display mode database is intentionally minimal (ECS LORES/HIRES + PAL/NTSC variants only). Apps like PPaint's CloantoScreenManager and FinalWriter exit early when the probe yields unexpected results.

**Key constraints**:
- `g_known_display_ids[]` in `lxa_graphics.c`: the authoritative list of what lxa will claim to support. Adding a mode ID here alone is not enough — `GetDisplayInfoData(DTAG_DIMS)` must also return plausible `MinRaster`/`MaxRaster` ranges for it.
- `GetDisplayInfoData(DTAG_DIMS)` currently returns `MinRasterWidth == MaxRasterWidth` (pinned values). Apps that check whether a mode can render at a target size will reject all modes. The fix is to return realistic ranges (e.g., 320–1280 for HIRES).
- `BestModeIDA()` currently ignores `BIDTAG_DIPFMustHave`/`MustNotHave` flags. Apps that require `DIPF_IS_WB` or exclude `DIPF_IS_LACE` get wrong results.
- The `FindDisplayInfo()` → `NextDisplayInfo()` iteration returns a fake handle sequence. Iteration-based probes (like CloantoScreenManager) may behave differently from single-query probes.

**Symptom pattern**: App opens and immediately exits with a return code ≥ 26, or opens probe windows briefly then terminates. Often preceded by a flood of `FindDisplayInfo()`/`GetDisplayInfoData()` log entries.

**Fix philosophy**: Accept and virtualize any mode request that maps to our physical LORES/HIRES display — same as Wine accepting DirectX surface requests regardless of the exact format requested.

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

## 7. Quick Start
1. Check `roadmap.md`.
2. Load `lxa-workflow` to understand the process.
3. Load `develop-amiga` for coding standards.
4. Load `lxa-testing` to plan your tests.
5. For UI tests, load `graphics-testing` for host-side driver patterns.
