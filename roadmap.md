# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, and roadmap updates on every completed phase.

## Phases

### Address issues uncovered by manual testing

Make sure to keep an eye on launch.json - make sure the launch configs in there match what is used in the test suite.

Capture screenshots on failure and use `tools/screenshot_review.py` to identify UI/rendering issues. Be thorough, do not assume anything on screen, check and check again!

- Phase 108-d: Addressing issues uncovered by manual testing (complete, v0.8.73)

	Fixed 13 issues across 7 apps; documented 7 known limitations that require
	deeper infrastructure (blitter line mode, copper lists, hardware detection,
	menu double-buffering, missing third-party libraries).

	**Fixes delivered:**
	- [x] ASM-One event queue overflow: enlarged queue 32→256, added mouse-move coalescing
	- [x] ASM-One MOUSEMOVE qualifier propagation: `display_inject_mouse()` sets qualifier bits from held buttons
	- [x] Devpac crash on startup: created `amigaguide.library` disk stub (35 vectors)
	- [x] DPaint V crash (double-free assertion): overlap detection in `_exec_Deallocate()` silently skips overlapping frees
	- [x] DPaint V Screen Format requester layout: uses actual `BorderTop`/`BorderBottom` instead of hardcoded `20`
	- [x] PPaint immediate close (rv=26): initialised `GfxBase->DisplayFlags = PAL|REALLY_PAL`, `VBlank = 50`, `ChipRevBits0 = SETCHIPREV_ECS`
	- [x] MaxonBASIC incomplete scroll bar: fully implemented `SCROLLER_KIND` with PropInfo/AUTOKNOB, PGA_Freedom, tag support
	- [x] MaxonBASIC About dialog stamping: `DamageExposedAreas()` skip for SMART_REFRESH layers
	- [x] MaxonBASIC Settings dialog layout: removed double-baseline offset in GadTools `gt_create_label()`
	- [x] GadTools double-baseline bug: removed `GT_FONT_BASELINE` from all TopEdge calculations (9 sites across label, slider, cycle)
	- [x] Cluster2 EMU_CALL_INT_GET_MOUSE_BTN: now returns `(qualifier << 8) | (button_code & 0xFF)`
	- [x] IDCMP_INTUITICKS implementation: fires every 10th VBlank (~5 Hz PAL) to all interested windows
	- [x] INTUITICKS qualifier propagation: `g_current_qualifier` tracked from all input events and passed to INTUITICKS messages

	**Known limitations (not addressable in this phase):**
	- Cluster2 EXIT button: MOUSEBUTTONS events are delivered but Cluster2's custom toolbar (no Intuition gadgets) doesn't respond — likely internal coordinate mapping mismatch
	- ASM-One flickery menus: architectural (needs double-buffered menu rendering)
	- ASM-One invalid memory read warnings (0xda00f8xx): cosmetic, addresses outside 24-bit range
	- MaxonBASIC flickery menus: same architectural issue as ASM-One
	- BlitzBasic2 empty windows: requires blitter line mode + copper list interpreter
	- KickPascal layout/menu/compile issues: most require missing third-party libraries (arp.library, req.library) or deep infrastructure
	- SysInfo empty hardware fields / no input: requires hardware detection features (BattClock, CIA timing)

### Infrastructure Gap Fixes

Phase 108-d's manual testing sweep uncovered systemic gaps that affect many apps.
Fixing these *before* the next app testing round avoids re-discovering the same
issues in every driver. Ordered by dependency and effort (low-hanging fruit first).

- Phase 109: Missing IDCMP types and GfxBase completeness (complete, v0.8.74)

	Implemented five missing IDCMP message types and completed GfxBase field
	audit. All new IDCMP types tested with dedicated m68k integration tests
	and host-side GTest drivers (interactive injection for SIZEVERIFY,
	MENUVERIFY, DELTAMOVE). 63/63 tests pass, zero regressions.

	- [x] IDCMP_REQSET/REQCLEAR: posted in Request()/EndRequest(), gated by window IDCMPFlags
	- [x] IDCMP_SIZEVERIFY: posted before sizing drag in both PIE and non-PIE input paths
	- [x] IDCMP_MENUVERIFY: MENUHOT to active window, MENUWAITING to other interested windows
	- [x] IDCMP_DELTAMOVE: modifier flag — MOUSEMOVE/MOUSEBUTTONS carry dx/dy via `_compute_idcmp_mouse_coords()`
	- [x] GfxBase audit: NormalDPMX/Y=22, MicrosPerLine=64, MinDisplayColumn=0x71, monitor_id=0, TopLine=0
	- [x] Integration tests: IDCMPReqSet, IDCMPDeltaMove, IDCMPSizeVerify, IDCMPMenuVerify, GfxBaseFields

- Phase 110: Third-party library stubs (complete, v0.8.75)

	Created four disk library stubs so apps that depend on third-party libraries
	get clean failure returns instead of crashing on OpenLibrary.

	- [x] req.library: 8 function vectors (FileRequester, FontRequester, SimpleRequest, etc.) — all return FALSE/NULL
	- [x] reqtools.library: 23 function vectors (rtFileRequest, rtFontRequest, rtEZRequest, etc.) — all return failure
	- [x] powerpacker.library: 11 function vectors (ppLoadData returns PP_OPENERR, etc.) — version 36.10
	- [x] arp.library: 106 function vectors (33 reserved + 73 public) — largest stub, version 40.1
	- [x] Integration test (disklib_stubs): opens all 4 libraries, verifies versions, calls key functions, tests refcounting, confirms not ROM-resident
	- [x] All 63 tests pass, zero regressions

- Phase 111: SMART_REFRESH full backing store (complete, v0.8.76)

	Implemented full backing store for SMART_REFRESH layers. Since
	`WFLG_SMART_REFRESH == 0`, this affects every window that does not
	explicitly request SIMPLE_REFRESH or SUPER_BITMAP — i.e., the vast
	majority of Amiga windows.

	**Core implementation** (in `lxa_layers.c`):
	- [x] `CreateObscuredClipRect()`: allocates a ClipRect with `cr->obscured=1` and a backing store `cr->BitMap` sized to the CR bounds
	- [x] `SaveToBackingStore()`: blits screen content into newly-obscured CRs' backing store bitmaps
	- [x] `RestoreBackingStore()`: blits old obscured CRs' backing store back to screen before recomputing
	- [x] `RebuildClipRects()` rewritten: saves old CR list → restores all backing store → frees old list → computes new visible+obscured split → saves screen content into new obscured CRs
	- [x] `SwapBitsRastPortClipRect()`: fully implemented 3-step swap using temp bitmap

	**Graphics function CR-awareness** (in `lxa_graphics.c`):
	- [x] `RectFill`: renders into `cr->BitMap` for obscured SMART_REFRESH CRs
	- [x] `WritePixel`: writes into backing store with CR-relative coordinates
	- [x] `Draw`: per-pixel Bresenham renders into backing store
	- [x] `ReadPixel`: reads from backing store for obscured CRs (bitplane inspection)
	- [x] `ClipBlit`: destination-side uses `cr->BitMap` with CR-relative offsets
	- [x] `BltBitMapRastPort`: uses `BltBitMapCore()` into backing store
	- [x] `BltMaskBitMapRastPort`: same pattern with mask parameter preserved

	**Tests**: 5 integration test cases (partial obscure, MoveLayer save/restore, SIMPLE_REFRESH negative, full obscure/uncover, drawing-while-obscured)

	**Deferred**: `Text()`, `BltTemplate()`, `ScrollRaster()` CR-awareness — these functions do not currently iterate ClipRects and are lower priority. They can be added incrementally if apps expose issues.

	62/63 tests pass. The 3 `sigma_interaction_gtest` failures (AnalysisWindowGadgetGridLayout, ButtonClickInteraction, ButtonAreaGadgetsAreConsistentlyPlaced) are pre-existing and unrelated to backing store.

- Phase 112: Menu double-buffering (complete, v0.8.77)
	- Replaced byte-aligned menu save/restore with pixel-accurate save/restore
	  using `AllocBitMap` + `BltBitMap`. This eliminates the 1–7 pixel edge
	  artifact that appeared whenever a menu opened at a non-byte-aligned X
	  coordinate.
	- To keep this affordable, `BltBitMapCore` is now dispatched to a native
	  host-side handler via `EMU_CALL_GFX_BLT_BITMAP` (2050). The interpreted
	  m68k bit-loop has been deleted. The host implementation handles all
	  minterms, overlap (bottom-up vs top-down), byte-unaligned source/dest
	  shifts, plane masks, pixel masks, and the `DrawImage` "planeonoff"
	  conventions where `Planes[p] == NULL` reads as all-zero and
	  `Planes[p] == (PLANEPTR)-1` reads as all-one.
	- Performance: gadtoolsmenu tests run in ~7.9 s (within Phase 111 baseline
	  of 7.2 s). The previous interpreted BltBitMap was ~50–100× slower and
	  would have made menu save/restore unusable.
	- Regression guards added to `tests/graphics/blt_bitmap`: non-byte-aligned
	  destination shifts (1..7), minterm 0x30 (used by `DrawImageState`
	  `IDS_SELECTED`), NULL source plane, and `(PLANEPTR)-1` source plane.
	- `RequesterBasicDriverTest.SystemRequesterCancelAndConfirm` was reworked:
	  the in-program self-`PutMsg` injection was removed; the test program now
	  emits `READY: requester1_negative` / `READY: requester2_positive` sync
	  markers and blocks in `SysReqHandler(..., TRUE)`. The host driver uses
	  `ClickGadget(1, 1)` and `ClickGadget(0, 1)` instead of hard-coded pixel
	  offsets. BuildSysRequest windows have no `WFLG_CLOSEGADGET`, so test 4
	  now verifies the negative gadget path and accepts losing the old
	  `IDCMP_CLOSEWINDOW` coverage for that requester.
	- Full suite: 63/63 passing. 30/30 reliability run on the requester test.

- Phase 113: Blitter line-draw mode (complete, v0.8.78)
	- Implemented Bresenham line-draw mode in `_blitter_execute()` for
	  BLTCON1 bit 0 (LINE). Branch is taken at the top of the function after
	  register decode, before the standard area-mode copy/fill path.
	- BLTCON1 bit decoding (per fs-uae/e-uae, authoritative — the original
	  roadmap text described the octant bits incorrectly):
	  - bit 0 (0x01): LINE
	  - bit 1 (0x02): SING (ONEDOT — at most one pixel per row)
	  - bit 2 (0x04): AUL (sign of the always-step on the dominant axis)
	  - bit 3 (0x08): SUL (sign of the conditional step on the minor axis)
	  - bit 4 (0x10): SUD (1 = X dominant, 0 = Y dominant)
	  - bit 6 (0x40): SIGN (initial Bresenham error sign)
	- Per-pixel: read C-word, compute `a_word = bltadat >> cur_ash`, replicate
	  BLTBDAT LSB to a 16-bit b_word, apply minterm via existing 256-entry
	  table, write D-word. Apps must set `BLTADAT = 0x8000` for a single-bit
	  pixel mask (matches HRM/e-uae convention).
	- Bresenham step matches e-uae's `blitter_line_proc()`:
	  - When `!sign`: BLTAPT += BLTAMOD (sign-extended s16) and step both
	    dominant and minor axes.
	  - When `sign`: BLTAPT += BLTBMOD (sign-extended s16) and step only the
	    dominant axis. New sign = (acc < 0).
	- Cross-word ASH wrapping: when stepping X, cur_ash wraps 0↔15 and the
	  CPT/DPT pointers advance/retreat by 2 bytes.
	- SING (ONEDOT) suppresses subsequent pixels in the same row by gating
	  the D-write until the dominant Y advances (or always, when X-dominant).
	- After completion, the final cpt/dpt/apt/sign/bshift values are written
	  back to `g_blitter` (per HRM, the line blit leaves these in a defined
	  post-line state).
	- 9 new sub-tests added to `tests/graphics/hw_blitter/main.c` (Tests
	  8–16) covering: horizontal, vertical, +X+Y diagonal, +X-dominant +Y
	  shallow, +X-Y, -X+Y, +Y-dominant steep, cross-word horizontal
	  (verifies ASH wrap), and patterned (BLTBDAT=0xAAAA) lines.
	- Full suite: 63/63 passing in 139 s wall time. No regressions.

- Phase 114: Copper list interpreter (basic)
	- Implement a minimal copper list interpreter for the subset of copper
	  operations that productivity apps use.
	- **Scope** (deliberately limited):
	  - MOVE: write a value to a custom chip register. This is 95% of what apps
	    need — they use copper to set color registers per scanline for gradient
	    backgrounds.
	  - WAIT: wait for a specific beam position. Needed for per-scanline color
	    changes.
	  - SKIP: conditional skip (lower priority, rarely used by apps).
	  - Execute copper list once per VBlank from `COP1LC`.
	- **Out of scope for this phase**: copper DMA timing, interlaced mode copper
	  lists, copper interrupts, cycle-exact beam position tracking. These are
	  needed for demos but not for productivity apps.
	- Prerequisite: Phase 118 (`lxa.c` decomposition) should ideally happen first
	  so copper code lives in `lxa_custom.c`. If not yet done, copper code can
	  go in a new `lxa_copper.c` module.
	- Test with a simple custom copper list that creates a color gradient, verify
	  with pixel assertions.
	- Test BlitzBasic2 to assess impact.

### App Testing Sweep (post-infrastructure)

With the infrastructure fixes in Phases 109-114 applied, these app tests should
produce much more useful results. Each app phase follows the standard workflow:
(1) run/create startup driver; (2) fix failures; (3) screenshot review;
(4) pixel/interaction assertions; (5) iterate.

- Phase 115: App testing sweep — batch 1 (existing drivers)
	- **DPaintV**: Extend existing `dpaint_gtest.cpp` — palette operations, brush
	  rendering, About dialog, Open/Save requester, keyboard shortcuts.
	- **MaxonBASIC**: Extend `maxonbasic_gtest.cpp` — verify About dialog no
	  longer stamps (SMART_REFRESH), scroll bar interaction, type and run a short
	  BASIC program.
	- **DevPac**: Extend `devpac_gtest.cpp` — About dialog, open-file requester,
	  assemble a trivial source file.
	- **ASM-One**: Extend `asm_one_gtest.cpp` — verify menu flicker is reduced
	  (double-buffering), test assemble/run workflow.

- Phase 116: App testing sweep — batch 2 (new drivers)
	- **DirectoryOpus**: Create/extend startup test, exercise file list
	  navigation, toolbar buttons, About dialog, Preferences.
	- **KickPascal2**: Create startup test, verify editor window and menus open
	  (depends on req.library stub from Phase 110), type into editor, open
	  Help/About.
	- **Typeface**: Create startup test, verify font preview area, test font
	  requester, About, preview zoom/close.
	- **FinalWriter**: Extend `finalwriter_gtest.cpp` beyond startup — document
	  editing, menu interaction, requester dialogs.

- Phase 117: App testing sweep — batch 3 (stress and edge cases)
	- **BlitzBasic2**: Re-test after blitter line mode + copper (Phases 113-114).
	  If "ted" editor now renders, add editor interaction tests.
	- **SysInfo**: Add tests for whatever new fields are populated by
	  infrastructure improvements. Document remaining gaps (hardware detection).
	- **Cluster2**: Re-test EXIT button after any MOUSEBUTTONS/coordinate fixes.
	  Add more navigation/toolbar tests.
	- **PPaint**: Extend beyond startup — drawing operations, palette, requesters.

### Mid-term: Architecture and Performance

- Phase 118: `lxa.c` decomposition
	- `lxa.c` is currently ~9,400 lines — a monolith containing memory map,
	  blitter, custom chip handler, emucall dispatch, and DOS operations.
	- Split into focused modules: `lxa_memory.c` (memory map + access),
	  `lxa_custom.c` (custom chip / blitter / copper emulation),
	  `lxa_dispatch.c` (EMU_CALL dispatch table), `lxa_dos_host.c` (hosted DOS).
	- This is a refactoring-only phase: no behavioral changes, all tests must
	  pass before and after.
	- **Note**: If copper implementation (Phase 114) happens before this, copper
	  code should be placed in a separate `lxa_copper.c` from the start to
	  avoid adding more code to the monolith.

- Phase 119: Profiling infrastructure
	- Add `--profile` flag to lxa that emits per-ROM-function call counts and
	  cumulative cycle costs.
	- Instrument the `op_illg()` dispatch to count EMU_CALL invocations per
	  function.
	- Add a `perf`-friendly build configuration (Release with frame pointers).
	- Add a test-suite timing summary mode.
	- Use profiling data to identify the top-10 hottest ROM functions and
	  host-side bottlenecks.

- Phase 120: Memory access fast paths
	- Replace byte-at-a-time `mread8()` calls in `m68k_read_memory_16/32` and
	  `m68k_write_memory_16/32` with direct word/long-aligned loads for RAM/ROM.
	- Add `__builtin_expect()` hints to hot paths.
	- Target: 2x reduction in per-instruction host overhead.

- Phase 121: Display pipeline optimization
	- Add dirty-region tracking to the display system.
	- Consider SIMD (SSE2/AVX2) for planar-to-chunky conversion.
	- Eliminate redundant SDL texture uploads when framebuffer hasn't changed.

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
- **Unicorn/QEMU TCG** (GPL v2): JIT via QEMU's code generator. ~2-5x speedup
  over Musashi, but GPL, no cycle awareness, per-call overhead.

**Recommendation**: Stay on Musashi for now. Invest in reducing host-side
overhead first (Phases 119-121) — that's where 95% of current wall-time
goes. Revisit CPU core migration when:
1. Moira adds 68030 support (then migrate for accuracy + speed), OR
2. Host-side optimizations are exhausted and CPU interpretation is the proven
   bottleneck (then consider a custom JIT or Emu68 x86-64 port).

- Phase 122 (tentative): CPU core evaluation
	- Re-evaluate Moira's CPU model support (check if 68030 has been added).
	- If Moira supports 68030: prototype integration behind a build flag,
	  benchmark against Musashi, and migrate if faster.
	- If not: profile the Musashi hot loop, consider targeted assembly
	  optimizations for the top-20 opcodes, and document remaining bottlenecks.
	- Evaluate whether lxa actually *needs* 68030 features or whether 68020 mode
	  would suffice for all tested apps. If 68020 is enough, Moira becomes
	  immediately viable.

- Phase 123 (tentative): Production readiness assessment
	- Define what "production use" means for lxa: interactive desktop use? batch
	  processing? CI testing?
	- Set performance targets.
	- Create a benchmark suite of representative workloads.
	- Decide whether to invest in a custom JIT, port Emu68 to x86-64, or accept
	  interpreter performance with optimized host overhead.

Each phase must include updated GTest host-side drivers under `tests/drivers/`,
regression assertions, and saved failure artifacts used to justify added tests.
Use `tools/screenshot_review.py` interactively during development, then
translate findings into deterministic pixel/geometry assertions so CI can detect
regressions without the vision model.

## Completed Phases (recent)

- Phase 108-c: Make SysInfo fully functional — **UNSUCCESSFUL** (v0.8.72)

	Resolved all three issues reported during manual testing:
	- Fixed ROM coldstart to initialise ExecBase fields (AttnFlags=68030, VBlankFrequency=50, MaxLocMem, EClockFrequency) — CPU now reports "68030"
	- Fixed VBlank IRQ pulse bug in **all four** `m68k_set_irq(3)` call sites (Musashi edge-trigger semantics): main loop, API `lxa_run_cycles()`, INTENA write handler, and `EMU_CALL_WAIT` handler — buttons now respond in SDL interactive mode
	- Updated launch.json for SysInfo and all other app entries to include FS-UAE third-party LIBS: path
	- Added `lxa_peek16()` API, ExecBase field verification tests, LIBRARIES interaction test, and screenshot capture test (16 tests total)
	- Expansion board list remains empty (acceptable — lxa doesn't emulate physical expansion hardware)

- Phase 108: SysInfo and Cluster2 app testing (complete, v0.8.71)
	- **Phase 108a — SysInfo**: Converted `sysinfo_gtest.cpp` to persistent `SetUpTestSuite()` fixture (10 tests, ~376ms, down from ~985ms). Added MEMORY and BOARDS gadget interaction tests with pixel-change verification. Reordered test execution so SPEED benchmark runs last (it monopolises the CPU and blocks subsequent gadget clicks).
	- **Phase 108b — Cluster2**: Extended `cluster2_gtest.cpp` from 7 to 10 tests. Added About-dialog toolbar click test, F6 file-requester open/dismiss test (pixel-based detection — Cluster2 uses custom screens so rootless window tracking doesn't detect its dialogs), and editor-content typing verification test. Discovered Cluster2 uses NO Intuition menus or gadgets (MenuStrip=NULL, gadget count=0); all UI is custom-rendered with WFLG_RMBTRAP.
	- launch.json entries for both apps verified consistent with test driver assigns.

- Phase 107: Test performance — persistent fixtures and SetUp deduplication (complete)
	- **Batch 1** (v0.8.69): Converted 7 multi-test-case drivers to `SetUpTestSuite()` / `TearDownTestSuite()`:
	  api_gtest (8 tests, 1 shard), cluster2_gtest (7 tests, 3→1 shard),
	  maxonbasic_gtest (5 tests, 2→1 shard), devpac_gtest (9 tests, 3→1 shard),
	  simplegad_gtest (8 tests, 2 fixtures/shards), simplemenu_gtest (5 tests, 2 fixtures/shards),
	  simplegtgadget_gtest (8 tests, 1→2 fixtures/shards).
	- **Batch 2** (v0.8.70): Converted 3 more drivers:
	  mousetest_gtest (5 tests, 1 shard), talk2boopsi_gtest (4 tests, 2 fixtures/shards — behavioral rootless + pixel non-rootless), blitzbasic2_gtest (5 tests, 2→1 shard, DragMenu cycle budget tuned).
	- Deferred apps_misc_gtest ProWrite conversion (5 fixture classes, ~62s but not on critical path).
	- Pattern: inherit `::testing::Test` directly, replicate `LxaTest` assigns in static `SetUpTestSuite()`, use `s_setup_ok` bool + `GTEST_SKIP()` for per-test guard, place destructive tests (CloseWindow, QuitViaMenu) last.
	- Two-fixture files (simplegad, simplemenu, simplegtgadget, talk2boopsi) keep 2 CTest shards since the behavioral (rootless) and pixel (non-rootless) fixtures require different emulator configs.
	- Removed redundant close+exit from simplegtgadget behavioral tests (ClickButton, ClickCheckbox, ClickCycle, NumberGadgetAcceptsKeyboardInput) so they coexist in a persistent fixture.
	- Result: 63 CTest shards, all passing, wall-time reduced from ~131s to ~108s (18% improvement on top of Phase 106's 38%).
	- Combined Phase 106+107 improvement: wall-time from ~210s to ~108s (49% total reduction).

- Phase 106: Test performance — headless display skip, idle detection, reduced cycle budgets (complete)
	- Skipped `display_update_planar()`/`display_refresh_all()` in headless VBlank; added `s_display_dirty` flag so `lxa_read_pixel()`/`lxa_capture_*()` auto-flush on demand.
	- Raised `AUTO_VBLANK_CYCLES` from 100K to 500K.
	- Added `lxa_is_idle()` (TaskReady empty check) and `lxa_run_until_idle()`.
	- Reduced `lxa_inject_mouse_click()` from 11 to 6 settle iterations via idle detection.
	- Reduced `lxa_inject_string()` per-character budget from 2×500K to 10×50K with idle early-return.
	- Reduced `lxa_inject_drag()` per-step budget from 500K to 3×200K (fixed VBlank-driven, not idle-detected, because Intuition renders in interrupt context).
	- Result: all 67 tests pass, wall-time reduced from ~210s to ~131s (38% improvement).
