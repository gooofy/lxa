# lxa Roadmap — Reference-Driven Era

`lxa` follows `AGENTS.md`: no crashes, system libraries fully implemented, third-party libraries never stubbed, **zero failing tests at phase completion**, and a roadmap update on every completed or deferred phase. This roadmap replaces the v0.10 roadmap (Phases 1–159c). That roadmap is archived in `doc/roadmap-legacy.md`; the git history has the full details.

---

## North Star

> **Run unmodified, OS-friendly AmigaOS 3.x applications on Linux so they behave the same as on a real Amiga — what WINE does for Windows.**

"The same" must be measurable, not a matter of opinion:

| Level | Definition |
|---|---|
| **Structural equivalence** | For a scripted scenario, the Intuition tree lxa produces matches real AmigaOS 3.1. The tree covers screens, windows, geometry, flags, gadgets, menus, IDCMP masks and fonts. |
| **Visual equivalence** | Window-relative pixels match the real OS by **pen index** (not RGB), with a per-scenario tolerance budget. |
| **Behavioural equivalence** | stdout, exit codes, files written and the library call/return trace match the real OS. |
| **App rating** | Each app gets a WINE-AppDB-style rating (Platinum / Gold / Silver / Bronze / Garbage), computed from the three levels above over its scenarios. |

**1.0 release criterion**: 25 real applications rated Gold or better, every rating backed by checked-in reference goldens, and no stubbed system-library LVO left that any corpus app calls.

---

## Why a New Roadmap — Audit Findings (2026-10)

lxa has a strong core: a 68030 CPU, about 150k lines of library code, a host-side test API with window, gadget and menu introspection plus a text hook, 633 GTest cases and a green suite. The way it was being developed, however, had hit a ceiling:

1. **The expected results never came from a real Amiga.** All 225 m68k test programs check themselves (`PASS:`/`FAIL:`), and the same agent wrote both the expectations and the implementation. A misreading of the RKRM therefore ends up in both, and the tests pass anyway. The few FS-UAE screenshots were taken by hand and inconsistently. One of them (taken with overscan) sent Phases 158b/c in the wrong direction.
2. **The clock is non-deterministic.** VBlank comes from `SIGALRM` (`lxa.c:226`), and timer.device and DateStamp read `gettimeofday`. That is the root cause of flaky tests, of the very generous cycle budgets (AGENTS §6.6), and of goldens that cannot be pixel-exact.
3. **Depth-first and symptom-driven.** A single DPaint dialog produced eight phases (153–158). The rendering defects behind it, such as gadget imagery, bevels and ghosting, should have been found all at once by a systematic gallery compared against the real OS.
4. **Facades break the "no stubs" mandate, and nothing tracks them.**
   - commodities.library, rexxsyslib.library and amigaguide.library are entirely stubbed.
   - There are no datatype classes, and the BOOPSI class set is incomplete (frameiclass, itexticlass, frbuttonclass, groupgclass, fillrectclass, pointerclass are missing).
   - These are absent: mathieeesingtrans, lowlevel, nonvolatile and bullet libraries; battclock, misc, potgo and disk resources; and trackdisk I/O.
   - `AttnFlags` hides the FPU, and the chip revision is reported inconsistently (`exec.c:5765` says ECS, `lxa_graphics.c:9314` says AA).
5. **The "100% coverage" mandate cannot be checked.** No gcov, lcov or ROM coverage tooling exists.
6. **The app corpus is thin.** There are 19 apps and only 2 manifests. Meanwhile the reference machine already has about 40 apps installed, MUI among them, plus 1002 unpacked Fred Fish disks at `~/media/sys/amiga/pd/unpacked/fish`.

---

## The Method: Reference-Driven Development (RDD)

```
            ┌──────────── scenario.yaml (one script, two backends) ────────────┐
            ▼                                                                  ▼
   lxa backend (liblxa / pylxa)                         reference backend (FS-UAE + AmigaOS 3.1
   deterministic virtual clock                          + lxaprobe guest agent over serial/TCP)
            │                                                                  │
            └──────────────► snapshot bundle (same schema) ◄───────────────────┘
                 screen.png (pen-indexed) · palette.json · tree.json
                 text.jsonl (Text() hook) · stdout.txt · trace.jsonl
                                    │
                                    ▼
         comparator: 1) tree diff → 2) pen-index pixel diff → 3) Claude vision review
                                    │
                 ┌──────────────────┴───────────────────┐
                 ▼                                      ▼
      finding → root-cause cluster →          reference bundle → checked-in golden
      phase TODO / fix                        + generated GTest regression
```

**Principles** (they become AGENTS.md rules in Phase 200):

1. **Real AmigaOS 3.1 is the oracle.** The RKRM and NDK explain *why* something behaves as it does; the reference machine settles *what* it does. Expected values come from the reference backend.
2. **Compare structure first, then pixels, then use vision.** Vision (attaching captures to Claude) is for triage and for explaining a diff. Every vision finding must be turned into a deterministic tree or pixel assertion before the phase closes.
3. **Breadth before depth.** Sweep many apps shallowly, cluster the divergences by root cause, then fix them in order of how often they occur, not in order of which app was looked at most recently.
4. **Goldens are never edited to match lxa.** A golden comes only from the reference backend. If you believe a golden is wrong, re-capture it on the reference.
5. **Clean-room.** Observe AmigaOS only as a black box: run it, probe it, trace it. Never consult leaked AmigaOS source or Kickstart disassembly for implementation (`~/media/sys/amiga/os/` contains such material; it is off limits). Disassembling *applications* to understand what they call remains allowed (AGENTS §6.20).

### Canonical Reference Configuration

lxa and the reference must present the same machine. Otherwise every diff is just noise (see the 158b/c overscan lesson). The reference is a **high-end machine (decided 2026-10-02)**: an A4000/040 with RTG. There are two display profiles, and every scenario declares which one it uses.

| Property | Reference (FS-UAE) | lxa (target state, Phase 240) |
|---|---|---|
| Model / CPU | A4000, 68040 + FPU, Kickstart 3.1 r40.70 (A4000) | 68040 with FPU; AttnFlags `AFF_68040\|AFF_FPU40\|AFF_68881\|AFF_68882` (today: 68030, no FPU bits) |
| Chipset / memory | AGA, 2 MB chip, 16 MB Zorro III fast | AGA, reported consistently (today: mixed ECS/AA) |
| RTG | `uaegfx` (card driver built into FS-UAE) + Picasso96, 4 MB VRAM | own `Picasso96API.library` (Phases 240–242) |
| OS | **Fresh** Workbench 3.1 (40.42) built from the ADFs in `~/media/sys/amiga/os/3.1/` + `68040.library` + Picasso96 | lxa ROM + `share/lxa/System` |
| Profile `aga` | Workbench on PAL Hires 640×256, 4 colours, OSCAN_TEXT, topaz 8, WB 3.1 default palette and prefs | identical defaults |
| Profile `rtg` | Workbench on a uaegfx mode (e.g. 800×600×8; final mode fixed in Phase 210) | identical defaults once M4 lands |
| Drives | `SYS:` refsys · `APPS:` `../lxa-apps` (read-only) · `LXAREF:` exchange dir | `APPS:` same layout |
| Control | `serial_port = tcp://127.0.0.1:<port>`, Xvfb, warp mode | — |

The daily-driver system at `~/media/emu/amiga/FS-UAE/hdd/system` is **not** the reference. Its patches (MuForce, KingCON, SetPatch variants, MUI prefs) would contaminate the results. It stays useful for manual exploration.

**Picasso96 source for the reference**: `/mnt/dagobert-home/media/sys/amiga/apps/Picasso96 Install v1.33 (1997)(T.Abt A.Kneer).adf`. It provides `rtg.library`, `Picasso96API.library`, the `Picasso96` monitor driver and `picture.datatype`. Phase 210 verifies that this version works with FS-UAE's uaegfx; if it does not, the user supplies a newer Picasso96 (2.x).

### Reusing AROS code (decided 2026-10-02)

AROS is a clean-room, open-source AmigaOS reimplementation, so porting its code does not conflict with the clean-room rule. Where AROS already implements a component lxa lacks, **port it instead of writing it from scratch**, then verify the port against the reference like any other code. Candidates: rexxsyslib.library plus Regina as the ARexx interpreter, commodities, datatype classes, amigaguide, BOOPSI image and gadget classes, lowlevel, nonvolatile and mathieeesingtrans.
- **Licensing**: AROS code is under the AROS Public License 1.1 (MPL-based, file-level copyleft); Regina is LGPL. Ported files keep their original licence headers in their own directories (the same precedent as the vendored LGPL BGUI in `src/bgui`). lxa's own code stays MIT. Record every ported component in `doc/third-party-code.md`.
- **Source**: the local snapshot `others/AROS-20231016-source` has only `rom/`, `arch/` and `compiler/`. `workbench/` (libraries, classes, commodities) and AROS-Contrib (Regina) must be fetched from the AROS GitHub repositories.
- **Status**: ported AROS components count as *system implementations*, not third-party libraries. They are built from source into lxa, so the STOP-and-notify rule does not apply.

---

## Milestone Overview

| Milestone | Theme | Priority class | Phases |
|---|---|---|---|
| **M0** | Foundations: determinism, measurability, docs | Quality | 200–204 |
| **M1** | The Reference Oracle: FS-UAE twin harness | Quality (infrastructure) | 210–216 |
| **M2** | Ground-truth the existing suite + conformance gallery | Quality → Compatibility | 220–224 |
| **M3** | Breadth: app corpus, compat DB, sweeps, trace diff | Compatibility | 230–236 |
| **M4** | RTG / Picasso96 | Compatibility (reclassified, see M4) | 240–243 |
| **M5** | Close the facades: system-library completeness (AROS ports where possible) | Compatibility | 250–257 |
| **M6** | WINE-like desktop integration | New features | 260–265 |
| **M7** | Dev platform, toolkits, performance | New features / Performance | 270–276 |

M0 and M1 are on the critical path: nothing in M2 and later is efficient without them. From M3 on, the milestones form a feedback loop. M3 sweeps show which gaps actually matter, M4 and M5 fix them, and the fixes are verified by re-running the M3 sweeps. M4 and M5 may run in parallel in separate worktrees.

---

## Next Phase

> **Phase 201 — Deterministic virtual time**. Phase 210 (reference system builder) has no code dependency on 201 and may run in parallel in a separate worktree.

---

## M0 — Foundations

### Phase 201 — Deterministic virtual time
**Class**: Quality (root cause of flakiness).
- [ ] Add a `deterministic` mode (config + `lxa_init` flag). VBlank is derived only from emulated cycles, with a configurable cycles-per-frame (default about a 7.09 MHz PAL frame equivalent). No `SIGALRM` / `setitimer`.
- [ ] timer.device (`lxa_dos_host.c:28`), DateStamp (`lxa_dispatch.c:601`) and every `gettimeofday` in emulation paths read a virtual clock: a seeded epoch plus elapsed emulated time. The `lxa_api.c` test-wait timeouts may keep using wall time, but only as a hang guard.
- [ ] Make deterministic mode the default for GTest drivers. Interactive `lxa` keeps the real-time clock.
- [ ] Add `lxa_run_frames(n)` and switch fixtures from cycle-count loops to frame counts.
- [ ] Shrink the generous budgets in AGENTS §6.6 (`lxa_inject_string` 1M cycles/char, the 500K-cycle drag steps) once determinism is proven, and record the new wall time.

**Test gate**: running the same app scenario twice gives byte-identical captures and event logs. The full suite passes 20× in a row under `stress-ng -c 16` with zero flakes.

### Phase 202 — Coverage you can measure
**Class**: Quality (makes the coverage mandate checkable).
- [ ] Host side: a `-DLXA_COVERAGE=ON` build with gcov/lcov and a `make coverage` target.
- [ ] ROM side: an emulator PC-histogram mode that records executed ROM addresses and maps them to source lines with `m68k-amigaos-addr2line` against the ROM ELF. Output is lcov-compatible, so host and ROM merge into one report.
- [ ] Per-LVO table: for every public function of every system library, record whether a test executes it.
- [ ] Redefine the mandate in AGENTS.md as: *every public LVO has ≥1 test, and line coverage never decreases phase over phase*. Record the baseline here.

**Test gate**: `make coverage` produces the merged report; the per-LVO table is checked in under `doc/coverage/`.

### Phase 203 — Stub telemetry
**Class**: Quality (makes facades visible).
- [ ] Route every stub, partial implementation and unfilled LVO slot through one macro, `LXA_UNIMPLEMENTED(lib, fn, detail)`. Include the `_exec_unimplemented_call` slots (`exec.c:533`) and the `_dos_privateN` `assert(FALSE)` paths. The macro emits a structured `EMU_CALL` event.
- [ ] Host side: an `lxa_get_unimplemented_log()` API, a `--strict-unimplemented` mode that fails the run, and a per-run summary in `lxa.log`.
- [ ] Add `tools/stub_inventory.py`, which statically lists every macro site. Its output is the authoritative work list for M5.

**Test gate**: the inventory matches the audit (commodities, rexxsyslib and amigaguide fully listed). A GTest proves the strict mode trips.

### Phase 204 — Fix menu/input introspection for scripting (was legacy Phase 161)
**Class**: Quality (infrastructure). The M1 scenario DSL needs it.
- [ ] `lxa_select_menu_path("Project/Save As...")` and an index-based variant, computed from the live MenuStrip with no hard-coded coordinates.
- [ ] Non-atomic drag: `lxa_inject_drag_begin` / `_step` / `_end`.
- [ ] `lxa_get_qualifier_state()`.
- [ ] Migrate one MaxonBASIC, one KickPascal and one ASM-One test to the new API. Add `MenuPixelStateDuringDrag`.

**Test gate**: migrated tests pass; no menu-test regressions.

---

## M1 — The Reference Oracle

### Phase 210 — Reference system builder
**Class**: Quality (infrastructure).

**Feasibility spike (2026-10-02)**: a fresh Workbench 3.1 system was built by hand into `~/.cache/lxa/refsys/SYS` (5 MB, with config `~/.cache/lxa/refsys/lxa-ref.fs-uae`). It boots headless to Workbench under Xvfb in about 25 s with warp mode, on an A4000/040 with KS 40.70. Xvfb and amitools are now installed. A sample capture is at `screenshots/refsys-wb31-screenmode-prefs.png`. **RTG is not working yet**: with Picasso96 1.33 (`rtg.library` 40.2912), a `Devs/Monitors/uaegfx` driver and the `BOARDTYPE=uaegfx` tooltype, no uaegfx modes appear, and the FS-UAE log shows FindCard was never called. The most likely cause is that 1.33 predates the built-in uaegfx card ABI.

- [ ] Turn the spike into `tools/refsys/build_refsys.sh`.
  - Unpack the six Workbench 3.1 ADFs with `xdftool`.
  - Merge Workbench, Extras, Storage, Locale and Fonts, and add `68040.library` from the Install disk.
  - Write the HD Startup-Sequence (start `lxaprobe` when `LXAREF:` is mounted).
  - Install Picasso96 and generate the `uaegfx` monitor icon with tooltypes in Python. The spike's icon patch works.
  - The build is idempotent, its result checksummed, and it supports profiles `aga` and `rtg`.
- [ ] Get RTG working: try a Picasso96 2.x release (**the user supplies it**), then define the uaegfx resolutions (the P96 settings file / Picasso96Mode) non-interactively. Confirm that ScreenMode prefs list the uaegfx modes, then fix the Workbench mode for the `rtg` profile.
- [ ] `tools/refsys/lxa-ref.fs-uae.in`: the Canonical Reference Configuration, with port, drives and paths templated, `warp_mode = 1`, `sound_output = none`. Never modify the user's `Default.fs-uae`.
- [ ] Run headless with a private Xvfb display per instance; this works per the spike. Note that `pkill -f fs-uae…` also matches the calling shell, so track PIDs instead.
- [ ] Parallel instances: several FS-UAE processes with separate state dirs and ports. Measure boot-to-ready time in warp mode, with a target under 10 s.
- [ ] Evaluate savestate resume for faster startup. Directory-backed drives can conflict with savestates; fall back to a warp boot.

**Test gate**: `tools/refsys/refctl boot && refctl ping` succeeds headless, 10/10 times.

### Phase 211 — `lxaprobe` guest agent
**Class**: Quality (infrastructure). An m68k program, built with the existing cross toolchain, lives in `tools/refsys/agent/`. It talks a line-based protocol over `serial.device`; bulk data goes through `LXAREF:` files.
- [ ] `RUN <cmdline> [cwd]`: start an app asynchronously (`SystemTags`) and track its process.
- [ ] `WAIT_WINDOW <title-substr>` and `WAIT_IDLE`: idle means every task of the app is blocked in `Wait()` with an empty UserPort and input.device is drained. This mirrors `lxa_wait_idle`.
- [ ] `DUMP_TREE`: screens, windows, gadgets (type, geometry, flags, GadgetID, IntuiText labels), menus with item geometry, IDCMP flags and fonts, written as JSON in **the same schema as liblxa** (Phase 212).
- [ ] `SNAP [screen|window N]`: dump the bitmap as pen indices plus palette (read through `ReadPixelArray8`/`ReadPixel`, not an emulator screenshot), so overscan and scaling cannot distort it.
- [ ] Input: absolute mouse moves, clicks, keys and qualifiers through `input.device` `IND_WRITEEVENT`. Menu selection by path works the same way as Phase 204.
- [ ] Text hook: a `SetFunction` patch of `Text()` that logs strings with RastPort coordinates. Patching is fine on the test machine.
- [ ] `TRACE <lib> <lvo-list>`: a `SetFunction` relay that logs arguments and return values. This prepares Phase 233.
- [ ] `QUIT`: close the app's windows and send CTRL-C; report surviving tasks and leaked memory (an `AvailMem` delta).

**Test gate**: agent unit scenario on the reference: run `SYS:Utilities/Clock`, wait for its window, dump the tree, snap, quit. All succeed and the tree is plausible.

### Phase 212 — Snapshot bundle schema & lxa backend parity
**Class**: Quality (infrastructure).
- [ ] Specify the bundle schema in `doc/rdd-snapshot.md`, versioned.
- [ ] liblxa: `lxa_dump_tree_json()` and pen-index capture (`lxa_capture_window_indexed`). The text hook output goes to `text.jsonl`.
- [ ] `pylxa`: Python ctypes bindings for liblxa, so the scenario runner drives lxa and the reference from the same code.

**Test gate**: a pylxa unit test produces a bundle that validates against the schema.

### Phase 213 — Scenario DSL & twin runner
**Class**: Quality (infrastructure). Python package `tools/rdd/`.
- [ ] Scenario YAML with these fields: `app` (manifest ref), `steps` (`launch`, `wait_window`, `wait_idle`, `click {gadget_id|label|xy}`, `menu "A/B/C"`, `type`, `key`, `snapshot <name>`, `quit`).
- [ ] `rdd run <scenario> --backend lxa|ref|both`, which runs reference instances in parallel and caches reference bundles by (scenario hash, refsys checksum).
- [ ] Starter scenarios: a CLI program, the `simplegtgadget` sample, DPaint V startup, and DOpus startup.

**Test gate**: all four starter scenarios produce bundles on both backends.

### Phase 214 — Comparator, report & vision-review protocol
**Class**: Quality (infrastructure).
- [ ] Tree diff with a normalisation and ignore list (task addresses, timestamps). It reports geometry, flag, title, gadget-count and menu differences.
- [ ] Pen-index pixel diff, window-relative, with a per-region heat map. Use an SSIM fallback only for RTG.
- [ ] `rdd report` produces an HTML page with lxa | reference | diff side by side (nearest-neighbour ×2, coordinate grid), plus `summary.json` for agents.
- [ ] Skill `.claude/skills/rdd-review`: the protocol for Claude's vision triage.
  - Open the composite image.
  - Enumerate the differing regions with coordinates.
  - Classify each one (chrome / gadget imagery / text / layout / missing content / colour).
  - Name the suspected library and function.
  - Write the result to `findings.yaml`.
  - Turn each finding into a phase TODO or an assertion.

**Test gate**: the report renders for the four starter scenarios, and an `rdd-review` dry run yields `findings.yaml`.

### Phase 215 — Golden promotion & generated regressions
**Class**: Quality.
- [ ] `rdd promote <bundle>` copies a **reference** bundle into `tests/golden/<app>/<scenario>/` and generates a GTest. The GTest replays the scenario on lxa and asserts tree equality plus a pixel-diff budget.
- [ ] Add a CTest label `golden`, include it in the full suite, and auto-update the shard filters (AGENTS §6.17).
- [ ] Rule: a golden test may only be quarantined (`DISABLED_`) by a phase whose objective is to reach equivalence.

**Test gate**: at least 4 goldens checked in. They fail when an lxa regression is injected deliberately and pass otherwise.

### Phase 216 — RDD skills & autonomous loop
**Class**: Quality (process).
- [ ] Skills: `rdd-reference` (operating the refsys), `rdd-review` (from Phase 214), `compat-sweep` (Phase 231 procedure).
- [ ] Document parallel triage: one subagent per app, each in its own worktree, each returning `findings.yaml`. A coordinator clusters the findings by root cause before any phase is opened.
- [ ] Add a nightly or `/loop` sweep that refreshes the compat DB (Phase 230) and opens or updates phases whenever a rating drops.

**Test gate**: one end-to-end loop run: sweep, cluster, phase stub generated, reviewed by the user.

---

## M2 — Ground-Truth the Existing Suite

### Phase 220 — Run all m68k test programs on real AmigaOS
**Class**: Quality (validates 225 self-written oracles).
- [ ] `rdd suite-ref` runs every `tests/**/main.c` binary on the reference and captures stdout and the exit code.
- [ ] Triage each mismatch:
  - (a) FAIL on the real OS: the test's expectation is wrong. Fix the test first, then lxa.
  - (b) It crashes on the real OS: the test is buggy.
  - (c) It depends on lxa-only behaviour: tag it `lxa_only` and justify why.
- [ ] Bring back expected-output comparison. `tests/<area>/<t>/expected.ref.out` is captured **from the reference only**, and the drivers compare against it in addition to PASS/FAIL.

**Test gate**: every test program passes on the reference or carries an `lxa_only` justification, and lxa output matches `expected.ref.out` for all of them.

### Phase 221 — C: commands & Shell parity
**Class**: Compatibility. Scripts break when output formats differ.
- [ ] Run identical scripts on the reference (WB 3.1 `C:`) and on lxa (`sys/C`). Cover `Dir`, `List` (LFORMAT), `Info`, `Version`, `Assign`, `Echo`, `If`/`Skip`, `ReadArgs` error messages and return codes.
- [ ] List the WB 3.1 `C:` commands that lxa lacks. Each one is either implemented or explicitly ruled out of scope (decided in this phase).

**Test gate**: golden output parity for the script set.

### Phase 222 — API conformance probes ("WINE tests")
**Class**: Compatibility. Small generated probe programs call each function with normal and edge-case inputs and print the results. The reference output is the golden. Sub-phases are prioritised by stub telemetry and app traces:
- [ ] **222a** utility, exec (lists, memory, semaphores, signals, ports), dos (paths, `ReadArgs`, pattern matching, locks, `ExAll`, `SetVBuf`, error codes).
- [ ] **222b** graphics on off-screen bitmaps (lines, areas, flood, blits, `Text`/`TextExtent`/`TextFit`, algorithmic styles), compared as bitmap hashes.
- [ ] **222c** layers: clip rects, damage lists, backfill hooks, SMART/SIMPLE/SUPER refresh sequences.
- [ ] **222d** intuition geometry: border sizes per flag combination, `WA_*` tag effects, requester layout, `EasyRequest` layout, screen title bar.
- [ ] **222e** gadtools: `CreateGadget` resulting geometry for every kind, font and flag combination; `CreateMenus`/`LayoutMenus` item geometry.
- [ ] **222f** locale, keymap (`MapRawKey`/`MapANSI` over all keys and qualifiers), iffparse, icon, diskfont.

**Test gate per sub-phase**: probe outputs equal the reference goldens.

### Phase 223 — Rendering conformance gallery
**Class**: Compatibility. This replaces the piecemeal fixes of 153–157.
- [ ] Gallery samples: every GadTools kind × states (normal / selected / disabled) × topaz 8 / 9 / a disk font. Window chrome for every flag combination. Menus with checkmarks, sub-items, COMMSEQ and ghosted items. Requesters. BOOPSI sysiclass images. Bevel box styles.
- [ ] Pixel-exact (pen-index) goldens from the reference, plus a gallery HTML page.

**Test gate**: every gallery page matches pixel for pixel, or has an owning phase.

### Phase 224 — Reference-validate the legacy app tests
**Class**: Quality.
- [ ] Re-express the strongest existing app assertions (DPaint, Devpac, DOpus, Typeface…) as scenarios with reference goldens.
- [ ] Retire pixel-count heuristics that the goldens make redundant.

**Test gate**: no regression in coverage of app behaviour; suite wall time recorded.

---

## M3 — Breadth: Corpus, Compat DB, Sweeps

### Phase 230 — App corpus & compatibility database
**Class**: Compatibility.
- [ ] Write `app.json` manifests for all 19 apps in `../lxa-apps`, then add apps already present on the reference machine: ProWrite, EdWordPro, GadToolsBox3, Redit, FontView, AmigaBasic, GFABasic, Oberon, Scout, SnoopDos, MCPP, AmiBlitz3, BTII, Aztec C, ACE, AQB, and others.
- [ ] `apps/compat.yaml` records per app: rating, the scenarios behind it, the last tested lxa version, and open divergences, each linked to a phase number. There is no free-text "known issues" list.
- [ ] `rdd dashboard` renders the DB as HTML.

**Test gate**: ≥35 apps catalogued, each with a rating derived automatically from its scenarios.

### Phase 231 — Shallow sweep & divergence ranking
**Class**: Compatibility.
- [ ] Sweep scenario for every app: launch, wait for idle, snapshot, dump the menu tree, open every menu (snapshot), Escape, quit.
- [ ] Run it on both backends and rank the divergences: crash > no window > tree diff > pixel diff.
- [ ] Cluster the divergences by suspected root cause across apps (Phase 216 procedure). The top clusters become phases in M4/M5 or new numbered phases.

**Test gate**: the first full sweep report is checked in under `doc/sweeps/`; every cluster with ≥2 apps has an owning phase.

### Phase 232 — Fred Fish / PD mass run
**Class**: Compatibility (crash hunting at scale).
- [ ] Crawl the 1002 Fish disks for runnable CLI and GUI programs.
- [ ] Run each in lxa headless with a timeout (and `--strict-unimplemented` off), then on the reference.
- [ ] Classify: crash / guru (P0) · hang · unimplemented hit · output diff · OK.
- [ ] Aggregate "most-hit unimplemented functions" over the corpus. This feeds M5 prioritisation, the way WINE ranks missing functions by usage.

**Test gate**: a mass-run report is produced, and every lxa crash class either has an owning phase or is fixed.

### Phase 233 — Relay-trace differential
**Class**: Compatibility (debugging power tool, the counterpart of WINE's `+relay`).
- [ ] Trace in lxa: every library entry and exit, with arguments and return values, via ROM dispatch instrumentation. Filter with `LXA_TRACE=intuition,graphics:Text`.
- [ ] Compare with the reference trace from the `lxaprobe` `TRACE` command (Phase 211).
- [ ] `rdd tracediff` aligns both call sequences and reports the first divergence in a return value or in the call order.

**Test gate**: tracediff locates a deliberately injected return-value bug in a sample app at the correct call.

### Phase 234 — Legacy app items, re-validated
**Class**: Compatibility. Carried over from the legacy roadmap; each one must now end with a reference golden.
- [ ] BlitzBasic 2 ted editor shows no text (legacy 160). Start with a tracediff and a reference capture, not with hypotheses.
- [ ] SysInfo hardware fields and the Cluster2 EXIT button (legacy 162). SysInfo needs the battclock/CIA resources from Phase 255.
- [ ] DOpus button pages beyond the default (Move/Rename).

**Test gate**: each item has a passing golden scenario.

### Phase 235 — Library override mode (diagnostic, like `WINEDLLOVERRIDES`)
**Class**: Compatibility (diagnostic tooling, never shipped).
- [ ] An `LXA_OVERRIDE=asl,iffparse,…` mode that loads the **user's own** Workbench 3.1 disk-library binaries instead of lxa's versions. This applies only to hardware-independent disk libraries.
- [ ] Use: bisect whether a divergence lives in lxa's library or below it. The user's binaries are never committed and never distributed. lxa's own implementations stay mandatory (AGENTS §1).

**Test gate**: an app runs with one overridden library; documented as a debug-only workflow.

### Phase 236 — Prefs fidelity
**Class**: Compatibility.
- [ ] Read the `ENV:Sys/*.prefs` files the way IPrefs does: screenmode, font, palette, wbpattern, input, pointer, overscan, locale.
- [ ] lxa's defaults equal the reference defaults. Verify with goldens taken under non-default prefs, such as a different font or palette.

**Test gate**: goldens under two prefs configurations pass.

---

## M4 — RTG / Picasso96 (was legacy 164–166)

**Reclassified from "new feature" to "compatibility"**: PPaint and FinalWriter fail without it, and so does DPaint's "Choose Display Mode" list. The reference profile `rtg` provides the ground truth.

### Phase 240 — Machine identity: 68040 + AGA + RTG mode database
- [ ] Present a 68040 with FPU. Switch the Musashi CPU type, set `AttnFlags` (`exec.c:5898`), and ship a `68040.library`-compatible `SYS:Libs` entry, since apps probe for it.
- [ ] Report AGA everywhere (`ChipRevBits0`, `GetChipRev`-style code at `lxa_graphics.c:9314`, `exec.c:5765`). Add AGA display modes, 256-colour planar screens and `LoadRGB32`/`SetRGB32` 24-bit palettes.
- [ ] Cross-check GfxBase, ExecBase and display-info fields one by one against a reference `aga` dump.

**Test gate**: the field dump and the `aga` gallery (Phase 223) match the reference.

### Phase 241 — RTG display foundation
- [ ] Chunky `BMF_RTG` bitmaps for depth > 8, a `display_update_rtg()` RGBA path, P96 mode IDs in `g_known_display_ids[]`, and `OpenScreenTagList` with RTG depth.
- [ ] Make the mode IDs and `GetDisplayInfoData` results equal the reference uaegfx mode list (captured by `lxaprobe`).

### Phase 242 — Picasso96API.library + cybergraphics shim (disk libraries)
- [ ] Core P96 functions (alloc / free / lock / read / write pixel arrays, mode info, `BestModeID`, `RequestModeID`) and the CGX shim.
- [ ] Probe goldens from the `rtg` reference profile.

### Phase 243 — RTG app validation and the Workbench `rtg` profile
- [ ] PPaint, FinalWriter (re-enable `DISABLED_AcceptDialogOpensEditorWindow`), DPaint's "Choose Display Mode" list, and one more productivity app (Wordsworth / Amiga Writer). Each one ends with a golden.
- [ ] lxa boots Workbench in the `rtg` profile, and the gallery matches the reference.

---

## M5 — Close the Facades (System-Library Completeness)

The order below is provisional. After Phase 231/232 it is re-sorted by how often corpus programs hit each item. **Check AROS first** for every item, and port where it is viable (see "Reusing AROS code"). Every phase is verified with conformance probes against the reference (the Phase 222 pattern).

### Phase 250 — rexxsyslib.library + ARexx (AROS port)
- [ ] Port AROS `workbench/libs/rexxsyslib` (APL) as an lxa disk library, replacing the 17-function facade in `lxa_rexxsyslib.c`.
- [ ] Port Regina from AROS-Contrib (LGPL) as the ARexx interpreter (the RexxMast and `rx` equivalents), installed under `share/lxa/System`.
- [ ] Gate: an app's ARexx port responds to a scripted `rx` message on both backends; rexxsyslib probe goldens pass.

### Phase 251 — commodities.library (currently a ~25-function facade)
- [ ] Brokers, filters, senders, translators, `ParseIX`, `CxMsg*`, input-handler-chain integration, and hotkeys. Evaluate the AROS port first.
- [ ] Gate: the WB 3.1 `Exchange` commodity lists and controls a broker; probe goldens pass.

### Phase 252 — BOOPSI class completeness
- [ ] frameiclass, fillrectclass, itexticlass, frbuttonclass, groupgclass, pointerclass; a complete sysiclass.
- [ ] colorwheel.gadget and gradientslider.gadget as disk classes.
- [ ] Gate: gallery pages (Phase 223) for each class match the reference.

### Phase 253 — datatypes: real class dispatch + WB 3.1 classes
- [ ] Replace the private-struct guessing (`lxa_datatypes.c:385`) with real class dispatch and DataTypes descriptors (`DEVS:DataTypes`).
- [ ] Implement picture, ilbm, text, ascii, sound and 8svx classes, and the `AddDTObject`/`RefreshDTObjectA`/`RemoveDTObject`/`DoAsyncLayout`/`PrintDTObjectA` stubs.
- [ ] Gate: MultiView-equivalent scenarios (ILBM, text) match the reference.

### Phase 254 — amigaguide.library + amigaguide.datatype
- [ ] Open, display and navigate guides; synchronous and asynchronous APIs. Evaluate the AROS port first.
- [ ] Gate: an app's Help menu opens its guide, matching the reference.

### Phase 255 — Missing libraries, resources & exec/dos slots
- [ ] Libraries: mathieeesingtrans, lowlevel, nonvolatile.
- [ ] Resources: battclock, battmem, misc, potgo, disk, FileSystem. SysInfo needs these (Phase 234).
- [ ] exec: `RawIOInit`/`RawMayGetChar`/`RawPutChar` (routed to `lxa.log`), `Child*`, `ObtainQuickVector`, plus `Alert` (logged guru plus host dialog) and the `Cache*` functions (flush the CPU core).
- [ ] dos: `FindSegment`, `AbortPkt`, the remaining `AllocDosObject` types, and a real `LockDosList`.
- [ ] bullet.library (outline fonts) as a separately scoped sub-phase.

### Phase 256 — intuition / graphics / gadtools remaining stubs
- [ ] `DisplayBeep` (screen flash plus optional host bell), `MoveScreen`/`ScreenPosition`, `ModifyProp` refresh, `RethinkDisplay`, `GT_FilterIMsg`/`GT_PostFilterIMsg`, `LoadView`, `RPTAG_DrawBounds`, layers SuperBitMap sync.
- [ ] Whatever else is in the stub inventory (Phase 203) for these libraries.
- [ ] Gate: the stub inventory for intuition, graphics, gadtools and layers is empty.

### Phase 257 — Locale catalogs & non-English apps
- [ ] Real `OpenCatalog` with `LOCALE:` catalogs and language prefs, verified with German-language apps (MaxonBASIC, FinalWriter_D) on a reference with a German locale.

---

## M6 — WINE-like Desktop Integration (New Features)

### Phase 260 — Launch UX & app prefixes
- [ ] Register `binfmt_misc` for hunk executables (magic `0x000003F3`), so `./MyAmigaApp` just runs.
- [ ] `.desktop` launcher generation.
- [ ] Per-app "prefixes" (the counterpart of `WINEPREFIX`) built from manifests: assigns, libs and prefs overlays.

### Phase 261 — Host clipboard bridge
- [ ] clipboard.device unit 0 ↔ host clipboard (IFF FTXT ↔ UTF-8, with Latin-1 mapping).

### Phase 262 — Disk images
- [ ] Mount ADF/HDF read-only as DOS volumes on the host side, then trackdisk.device on top of ADF images (it does no I/O today).

### Phase 263 — Rootless polish
- [ ] Host window decorations policy, HiDPI integer scaling, AppWindow drag-and-drop from the host file manager, AppIcon → host tray.

### Phase 264 — Workbench
- [ ] Decide the scope: a hosted `LoadWB` desktop versus relying on the host desktop. If a desktop is in scope, evaluate porting AROS Wanderer or its WB 3.x-style workbench.library parts.

### Phase 265 — Audio validation (was legacy 169)
- [ ] Observable audio output (a captured sample buffer), checked against an audio-using app scenario on the reference.

---

## M7 — Dev Platform, Toolkits, Performance

### Phase 270 — External process emulation + DOS output capture (was legacy 167)
- [ ] Real m68k subprocesses for `CreateProc`/`Execute`/`SystemTags` with output capture, for the compile and run workflows of DevPac, ASM-One, BlitzBasic and KickPascal. Each workflow is validated against the reference.

### Phase 271 — MUI hosting (was legacy 170)
- [ ] The real `muimaster.library` (present on the reference machine; the user supplies the binary). Audit BOOPSI against MUI's needs. Gate: an MUI app's main window matches the reference tree.

### Phase 272 — ReAction / ClassAct hosting (was legacy 171)
- [ ] Needs an OS 3.9 reference profile (the user has AmigaOS 3.9 media). Add a second refsys profile.

### Phase 273 — OS 3.2 / 3.9 API levels
- [ ] Per-version reference profiles. Decide which V44+/V47 functions lxa exposes; the gadtools V47 stubs are an early instance.

### Phase 274 — Performance (was legacy 163)
- [ ] With deterministic time, wall time becomes measurable per scenario. Profile host overhead with `--profile` and set budgets per scenario.

### Phase 275 — CPU core evaluation (was legacy 172)
- [ ] Moira or JIT, considered only once host overhead is no longer dominant.

### Phase 276 — Developer experience
- [ ] Host-side debugging of m68k apps: a gdb stub on Musashi, and symbol loading from hunk debug info.
- [ ] Together these make lxa the "viable development platform" from the original vision.

---

## Scoreboard (updated at every milestone)

| Metric | Baseline (v0.10.13) | M1 target | 1.0 target |
|---|---|---|---|
| Apps catalogued / rated Gold+ | 19 / unrated | 19 / measured | 40 / 25 |
| Scenarios with reference goldens | 0 | 4 | 300+ |
| Test programs validated on real OS | 0 / 225 | — | 225 / 225 |
| Stubbed system-library LVOs (Phase 203 inventory) | ~90 (audit estimate) | measured | 0 used by the corpus |
| Line coverage (host + ROM) | unmeasured | measured | never decreasing |
| Flaky-test rate (20× runs) | unmeasured | 0 | 0 |
| Full-suite wall time `-j16` | ~145 s | ≤145 s | ≤120 s |

---

## Roadmap Policy (unchanged in spirit)

1. **Priority order**: Quality → Amiga compatibility → Performance → New features. RDD infrastructure counts as Quality.
2. **No pooling sections.** Every issue is either a TODO in an existing phase with the same root cause or a new numbered phase with a test gate. The compat DB links divergences to phase numbers; it is not a backlog.
3. **`DISABLED_` tests and failing goldens** must each be the objective of a scheduled phase.
4. **Completed phases** collapse to one row in the summary table below.
5. **Length budget**: about 600 lines. Put deep detail in skills and `doc/`.
6. **Decisions for the user** stay marked in their phase until the user decides.

---

## Completed Phases (Summary)

| Phase | Title | Version |
|---|---|---|
| 1–159c | Legacy era: core emulator, exec/dos/graphics/intuition/layers/gadtools/etc., host test API, 19-app driver suite. See `doc/roadmap-legacy.md` and git history. | ≤ v0.10.14 |
| 200 | Roadmap & agent-docs reset: AGENTS.md for Claude Code + RDD/clean-room rules, workflow skill de-pooled, stale stub comments removed, `apps/README.md` manifest rule. | v0.10.15 |
