# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, and roadmap updates on every completed phase.

## Current Phases

- No active phase; next work is Phase 106 (test performance quick wins).

## Future Phases

### Near-term: Test Performance Quick Wins (before more app phases)

The test suite currently takes ~210 seconds wall-time. Most of that time is
spent on over-provisioned cycle budgets and redundant per-test app startup,
not on actual emulation work. These phases address the low-hanging fruit before
adding more app tests that would compound the slowness.

- Phase 106: Test performance — headless display skip and reduced cycle budgets
	- Skip `display_update_planar()` and `display_refresh_all()` in headless mode unless `lxa_flush_display()` or `lxa_capture_window()` is explicitly called. This avoids ~150 full planar-to-chunky conversions per test SetUp that produce pixels nobody looks at.
	- Raise `AUTO_VBLANK_CYCLES` from 100K to 500K to reduce spurious display refreshes during large cycle runs.
	- Add an `lxa_is_idle()` / task-blocked detection: check whether all Exec tasks are in `TS_WAIT` state (blocked on `WaitPort()`). Expose this to the host so `WaitForEventLoop()` and `RunCyclesWithVBlank()` can return early when the app is idle instead of burning millions of cycles pointlessly.
	- Once idle detection is proven reliable, reduce `lxa_inject_string()` per-character budget from 1M to ~100K cycles, `lxa_inject_drag()` per-step budget from 500K to ~100K, and `lxa_inject_mouse_click()` settle iterations from 11 to 3-5.
	- Validate: all 67 existing tests must still pass. Measure wall-time improvement. Target: full suite in <90 seconds (down from ~210).

- Phase 107: Test performance — persistent fixtures and SetUp deduplication
	- Convert multi-test-case app drivers (maxonbasic, devpac, cluster2, sigma, blitzbasic2, asm_one, vim) from per-test `SetUp()` to `SetUpTestSuite()` / `TearDownTestSuite()` (GTest static fixture). Load the app once per binary, run all test cases against the already-initialized instance.
	- This requires careful test ordering: tests that modify app state (typing, menu selection) must either restore state afterward or run in a defined sequence. Use `GTEST_SKIP()` if a prior interaction left the app in an unrecoverable state.
	- Address the GTest limitation that `SetUpTestSuite()` cannot use `ASSERT_*` directly — use a static bool to track initialization success and skip all tests if setup failed.
	- Validate: same test coverage, same pass rate, dramatically reduced wall-time for multi-case drivers. Target: maxonbasic_input from ~155s to <30s.

### App Testing Phases (continuing the integration test sweep)

For each app below the agent should follow a test-driven workflow: (1) run or
create a reliable startup host-side driver test to exercise launch and
reachability; (2) if the driver reveals failures, fix them and keep the test
green; (3) capture screenshots on failure and use `tools/screenshot_review.py`
to identify UI/rendering issues and hypotheses; (4) author additional automated
pixel or interaction tests that assert the UI issues are fixed; (5) implement
fixes and iterate until tests are green.

- Phase 108: Cluster2
	- Create or extend a host-side driver to launch Cluster2 and assert its main UI appears.
	- Fix any startup or resource-load issues discovered by the driver.
	- Use `lxa_capture_window()` and `tools/screenshot_review.py` for visual triage; convert findings into automated pixel or geometry assertions.
	- Add tests for About dialog, basic file open, and a simple action (e.g., open a project file and verify expected UI change).
	- Update launch.json for all apps, make sure assigns and other env settings match those of the test drivers so manual testing runs them using the same conditions as the automated test.

- Phase 109: DirectoryOpus (follow-up)
	- Ensure existing DirectoryOpus startup test reaches the main rootless window; fix regressions if any.
	- Use screenshot review for menu/layout quirks and add pixel assertions for key UI regions (toolbar, main list area).
	- Add interaction tests: open About, trigger Preferences/requesters, and verify closing behavior.

- Phase 110: DPaintV
	- Create a startup test that launches DPaintV and verifies the canvas window appears.
	- Fix crashes, missing palettes, or widget rendering failures revealed by the driver.
	- Use screenshot analysis for palette/menu/brush-rendering issues and add pixel checks for a few representative operations (draw a short line, open palette requester).
	- Add tests for About dialog, Open/Save requester, and simple keyboard shortcuts.

- Phase 111: kickpascal2
	- Add a host-side driver that launches KickPascal2 and asserts the IDE window and menus open.
	- Resolve any rootless-geometry, menu, or keyboard focus problems the driver reveals.
	- Run `tools/screenshot_review.py` on captures to localize rendering faults and add automated UI tests: open Help/About, type into the editor, open file requester.

- Phase 112: MaxonBASIC (follow-up)
	- Extend existing MaxonBASIC test coverage beyond startup.
	- Add interaction tests: About dialog, run a short BASIC program and assert expected output in console.
	- Use screenshot-review to identify rendering issues (font/line wrap/menu clipping) and add pixel/geometry assertions.

- Phase 113: DevPac (follow-up)
	- Extend existing DevPac test coverage beyond startup/visual/input.
	- Use `tools/screenshot_review.py` for visual triage and add tests for About dialog, open-file requester, and assemble/run basic example.

- Phase 114: Typeface
	- Add a startup test that opens Typeface and verifies the font preview/render area is visible.
	- Fix missing-font, scaling, or render bugs uncovered by the driver.
	- Use screenshot-review for glyph rendering issues and add tests for opening font requesters, About, and preview zoom/close.

### Mid-term: Architecture and Performance

These phases should be tackled after the current round of app integration tests
is complete (post-Phase 114) or when test suite wall-time becomes a significant
development bottleneck — whichever comes first.

- Phase 115: Profiling infrastructure
	- Add `--profile` flag to lxa that emits per-ROM-function call counts and cumulative cycle costs.
	- Instrument the `op_illg()` dispatch in `lxa.c` to count EMU_CALL invocations per function.
	- Add a `perf`-friendly build configuration (Release with frame pointers) so `perf record` / `perf report` gives useful host-side profiles.
	- Add a test-suite timing summary mode that reports per-test breakdown of: total cycles, VBlank count, display refresh count, idle cycles (if idle detection is available).
	- Use profiling data to identify the top-10 hottest ROM functions and host-side bottlenecks.

- Phase 116: Memory access fast paths
	- Replace byte-at-a-time `mread8()` calls in `m68k_read_memory_16/32` and `m68k_write_memory_16/32` with direct word/long-aligned loads for RAM/ROM address ranges. Currently every 32-bit read does 4 separate `if/else if` address-range checks.
	- Add `__builtin_expect()` hints to the hot paths (RAM reads are by far the most common).
	- Benchmark before/after with a representative app startup. Target: 2× reduction in per-instruction host overhead.

- Phase 117: Display pipeline optimization
	- Add dirty-region tracking to the display system: only run planar-to-chunky conversion for regions where the Amiga bitmap was actually written (via `WritePixel`, `BltBitMap`, `RectFill`, etc.).
	- Consider SIMD (SSE2/AVX2) for planar-to-chunky conversion of dirty regions.
	- Profile the display refresh path and eliminate redundant SDL texture uploads when the framebuffer hasn't changed.

- Phase 118: `lxa.c` decomposition
	- `lxa.c` is currently 9,386 lines — a monolith containing memory map, blitter, custom chip handler, emucall dispatch, and DOS operations all in one file.
	- Split into focused modules: `lxa_memory.c` (memory map + access), `lxa_custom.c` (custom chip / blitter / copper emulation), `lxa_dispatch.c` (EMU_CALL dispatch table), `lxa_dos_host.c` (hosted DOS operations).
	- This is a refactoring-only phase: no behavioral changes, all tests must pass before and after.

### Long-term: CPU Emulation Evolution

#### Assessment (as of Phase 105)
Musashi (MIT, C89, interpreter) is adequate for correctness testing but limits
performance. The 68k emulation ecosystem has a notable gap: **there is no
clean, permissively-licensed, high-performance 68k JIT library for x86-64**.

Options evaluated:
- **Moira** (MIT, C++20, vAmiga): Faster interpreter with bus-cycle-exact timing.
  Natural Musashi successor. Blocker: only supports up to 68020 (lxa uses 68030).
  Worth monitoring for 68030 support.
- **UAE/WinUAE JIT** (GPL v2): The only proven x86-64 68k JIT, but deeply
  entangled with the UAE framework. Not extractable as a library.
- **Emu68** (MPL-2.0, PiStorm): Exceptional JIT performance (~2000 MIPS on
  ARM). ARM-only, no x86-64 backend. Writing one would be months of work.
- **Unicorn/QEMU TCG** (GPL v2): JIT via QEMU's code generator. ~2-5× speedup
  over Musashi, but GPL, no cycle awareness, per-call overhead.

**Recommendation**: Stay on Musashi for now. Invest in reducing host-side
overhead first (Phases 106-107, 115-117) — that's where 95% of current wall-time
goes. Revisit CPU core migration when:
1. Moira adds 68030 support (then migrate for accuracy + speed), OR
2. Host-side optimizations are exhausted and CPU interpretation is the proven
   bottleneck (then consider a custom JIT or Emu68 x86-64 port).

- Phase 119 (tentative): CPU core evaluation
	- Re-evaluate Moira's CPU model support (check if 68030 has been added).
	- If Moira supports 68030: prototype integration behind a build flag, benchmark against Musashi, and migrate if faster.
	- If not: profile the Musashi hot loop, consider targeted assembly optimizations for the top-20 opcodes, and document remaining bottlenecks.
	- Evaluate whether lxa actually *needs* 68030 features or whether 68020 mode would suffice for all tested apps. If 68020 is enough, Moira becomes immediately viable.

- Phase 120 (tentative): Production readiness assessment
	- Define what "production use" means for lxa: interactive desktop use? batch processing? CI testing?
	- Set performance targets: target frames per second for interactive mode, target seconds per test for CI mode.
	- Create a benchmark suite of representative workloads (app startup, text rendering, menu interaction, file I/O).
	- Decide whether to invest in a custom JIT, port Emu68 to x86-64, or accept interpreter performance with optimized host overhead.

Each phase must include updated GTest host-side drivers under `tests/drivers/`,
regression assertions, and saved failure artifacts used to justify added tests.
Use `tools/screenshot_review.py` interactively during development, then
translate findings into deterministic pixel/geometry assertions so CI can detect
regressions without the vision model.
