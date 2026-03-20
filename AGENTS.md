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
~160 second range on this machine class. Higher parallelism is safe, but usually
does not improve wall time meaningfully.

See `lxa-testing` skill for detailed test execution docs and
`doc/test-reliability-report.md` for parallelism benchmarks.

## 6. Lessons Learned (Accumulated Knowledge)

These are hard-won debugging insights from Phases 98–105 that future agents
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

## 7. Quick Start
1. Check `roadmap.md`.
2. Load `lxa-workflow` to understand the process.
3. Load `develop-amiga` for coding standards.
4. Load `lxa-testing` to plan your tests.
5. For UI tests, load `graphics-testing` for host-side driver patterns.
