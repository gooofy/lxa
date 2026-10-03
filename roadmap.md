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
| Time | real time (warp only skips idle) | deterministic virtual clock, 25 MHz virtual CPU (Phase 201) |

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

> **Phase 221 — C: commands & Shell parity** (with Phase 223 in parallel).

---

## M0 — Foundations

Complete (Phases 200–204, v0.11.3); see the summary table.

---

## M1 — The Reference Oracle

Complete (Phases 210–216, v0.11.9); see the summary table.

---

## M2 — Ground-Truth the Existing Suite

### Phase 221 — C: commands & Shell parity
**Class**: Compatibility. Scripts break when output formats differ.
- [ ] Run identical scripts on the reference (WB 3.1 `C:`) and on lxa (`sys/C`). Cover `Dir`, `List` (LFORMAT), `Info`, `Version`, `Assign`, `Echo`, `If`/`Skip`, `ReadArgs` error messages and return codes.
- [ ] List the WB 3.1 `C:` commands that lxa lacks. Each one is either implemented or explicitly ruled out of scope (decided in this phase).

**Test gate**: golden output parity for the script set.

### Phase 222 — API conformance probes ("WINE tests")
**Class**: Compatibility. Small generated probe programs call each function with normal and edge-case inputs and print the results. The reference output is the golden. Sub-phases are prioritised by stub telemetry and app traces:
- [ ] **222a** utility, exec (lists, memory, semaphores, signals, ports), dos (paths, `ReadArgs`, pattern matching, locks, `ExAll`, `SetVBuf`, error codes).
  - Open from the Phase 220 dos triage: `ReadArgs()` must read the command line from `Input()`'s buffer, not `pr_Arguments`; `Output()` buffering differs from AmigaOS (visible when stdio and dos output are mixed); `NameFromLock()` returns `SYS:` instead of the volume name; RawDoFmt's stray NUL after zero-padded negative numbers (`%05ld`) is not emulated.
- [ ] **222b** graphics on off-screen bitmaps (lines, areas, flood, blits, `Text`/`TextExtent`/`TextFit`, algorithmic styles), compared as bitmap hashes.
  - From the Phase 220 triage: `tests/exec/library_lxa` sections that still differ from 3.1 (GEL animation `AddAnimOb`/`RemIBob`, `CMove`, `CalcIVG` beyond 40 copper instructions, `SyncSBitMap`/`CopySBitMap`, sprite allocation, `VTAG_IMMEDIATE` and unset `VTAG_*_GET` values); PaletteExtra keeps 16-bit refcount/allocation arrays where 3.1 has 8-bit ones; `IEEESPMul`/`IEEESPDiv` never return on the reference (check FS-UAE's FPU emulation before trusting it: `Tests/Exec/MathIeeeSingBasMulDiv` is lxa_only until then).
- [ ] **222c** layers: clip rects, damage lists, backfill hooks, SMART/SIMPLE/SUPER refresh sequences.
- [ ] **222d** intuition geometry: border sizes per flag combination, `WA_*` tag effects, requester layout, `EasyRequest` layout, screen title bar. Includes the `OpenWindowTags` defaults seen in the `simplegad`/`simplegtgadget` goldens: untitled window `Title` is `""` not NULL, `MaxWidth`/`MaxHeight` default to the window size (not 65535) without a sizing gadget, `WFLG_VISITOR` on public screens, system gadget type bits (close gadget `0x8085`).
- [ ] **222e** gadtools: `CreateGadget` resulting geometry for every kind, font and flag combination; `CreateMenus`/`LayoutMenus` item geometry. Includes finding starter-4: GadTools gadgets lack `GTYP_GADTOOLS` (0x0100) and the zero-size context gadget.
- [ ] **222f** clipboard.device holds off writes while a read is unfinished (3.1) - lxa does not.
- [ ] **222f** locale, keymap (`MapRawKey`/`MapANSI` over all keys and qualifiers), iffparse, icon, diskfont. Includes re-enabling `ConsoleTest.DISABLED_KeymapUnit` (`console_gtest.cpp`): `keymap_unit` terminates silently (rc 0, no output) inside `CD_ASKDEFAULTKEYMAP` on a `CONU_LIBRARY` open.

**Test gate per sub-phase**: probe outputs equal the reference goldens.

### Phase 223 — Rendering conformance gallery
**Class**: Compatibility. This replaces the piecemeal fixes of 153–157.
- [ ] Gallery samples: every GadTools kind × states (normal / selected / disabled) × topaz 8 / 9 / a disk font. Window chrome for every flag combination (first finding, `simplegtgadget` scenario: lxa draws a black/white title bar and different system gadget imagery; WB 3.1 uses the blue/white active-window pens). Menus with checkmarks, sub-items, COMMSEQ and ghosted items. Requesters. BOOPSI sysiclass images. Bevel box styles.
- [ ] Pixel-exact (pen-index) goldens from the reference, plus a gallery HTML page.
- [ ] Fix the cluster from `doc/findings/2026-10-02-starter.yaml` (seen in every GUI scenario): `Screen.WBorTop` is 11 instead of 2 (shifts GadTools layouts by 9 px), Workbench pen 3 is `0,85,170` instead of `102,136,187`, the active title bar is black/white instead of FILLPEN/FILLTEXTPEN with missing right/bottom border lines, screen display ID `0x29000` instead of `0x8000`, `MenuVBorder`/`MenuHBorder` 0 instead of 2/4 (findings starter-1/2/3/5).

**Test gate**: every gallery page matches pixel for pixel, or has an owning phase.

### Phase 224 — Reference-validate the legacy app tests
**Class**: Quality.
- [ ] Re-express the strongest existing app assertions (DPaint, Devpac, DOpus, Typeface…) as scenarios with reference goldens.
- [ ] Retire pixel-count heuristics that the goldens make redundant.
- [ ] Re-express the `interactive` test programs in `tests/ref_suite.yaml` (console key input, IDCMP mouse/menu/size verify, requester clicks, keyboard.device) as reference scenarios that inject the same input through `lxaprobe`, so they are validated on AmigaOS 3.1 like the unattended programs (Phase 220).
- [ ] First twin-run findings (Phase 213 starter scenarios, `tests/scenarios/`):
  - `dopus-startup`: lxa shows "Directory not available" in both panes (the reference lists `SYS:` on the right), and lacks the "OK" message line and the CHIP/FAST/TOTAL/date status line.
  - `dpaintv-startup`: lxa opens the "Ownership Information" registration dialog; the reference goes straight to "Choose Display Mode" (DPaint reads its personalisation from its own executable via the CLI command name).

**Test gate**: no regression in coverage of app behaviour; suite wall time recorded.

---

## M3 — Breadth: Corpus, Compat DB, Sweeps

### Phase 231 — Shallow sweep & divergence ranking
**Class**: Compatibility.
- [ ] Sweep scenario for every app: launch, wait for idle, snapshot, dump the menu tree, open every menu (snapshot), Escape, quit.
- [ ] Run it on both backends and rank the divergences: crash > no window > tree diff > pixel diff.
- [ ] Cluster the divergences by suspected root cause across apps (Phase 216 procedure). The top clusters become phases in M4/M5 or new numbered phases. Input includes the 490 still-unowned divergences of the first compat run (`apps/compat.yaml`, Phase 230).
- [ ] First compat run, worst ratings first: lxa crashes in ADPro (jumps to PC=0xffffffff after loading `adpro.library`) and Oberon; Scout (MUI) and SnoopDos open no window; Asm-One and GadToolsBox3 never show the screen-mode requester the reference shows; vim-5.3's lxa snapshot fails ("indexed capture failed").
- [ ] Scenario DSL: stack size for the launched program (AQB needs 64 KB) and a writable copy of the app directory (AmiBlitz3 writes into its own folder; `APPS:` is read-only on the reference), so the two `untested` apps get ratings.
- [ ] liblxa's tracked window title does not follow `SetWindowTitles()` after the window opened (the runner works around it).

**Test gate**: the first full sweep report is checked in under `doc/sweeps/`; every cluster with ≥2 apps has an owning phase.

### Phase 232 — Fred Fish / PD mass run
**Class**: Compatibility (crash hunting at scale).
- [ ] Crawl the 1002 Fish disks for runnable CLI and GUI programs.
- [ ] Run each in lxa headless with a timeout (and `--strict-unimplemented` off), then on the reference.
- [ ] Classify: crash / guru (P0) · hang · unimplemented hit · output diff · OK.
- [ ] Aggregate "most-hit unimplemented functions" over the corpus. This feeds M5 prioritisation, the way WINE ranks missing functions by usage.

**Test gate**: a mass-run report is produced, and every lxa crash class either has an owning phase or is fixed.

### Phase 234 — Legacy app items, re-validated
**Class**: Compatibility. Carried over from the legacy roadmap; each one must now end with a reference golden.
- [ ] BlitzBasic 2 ted editor shows no text (legacy 160). Start with a tracediff and a reference capture, not with hypotheses.
- [ ] SysInfo hardware fields and the Cluster2 EXIT button (legacy 162). SysInfo needs the battclock/CIA resources from Phase 255.
- [ ] DOpus button pages beyond the default (Move/Rename), and re-enable `AppsMiscScreenTest.DISABLED_DirectoryOpusCopiesFile` (`apps_misc_gtest.cpp`: the copy never lands on the host).
- [ ] SysInfo gadgets: re-enable `SysInfoTest.DISABLED_{Memory,Boards,Libraries,Speed}Gadget…` (`sysinfo_gtest.cpp`). Phase 201 showed they only passed while the slow wall-clock startup paint was still running; after full startup a click on MEMORY/BOARDS/LIBRARIES/SPEED repaints nothing. Start with a reference capture of the same clicks.

**Test gate**: each item has a passing golden scenario.

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
- [ ] colorwheel.gadget and gradientslider.gadget as disk classes (GadToolsBox3 needs colorwheel.gadget, Phase 230 compat run).
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
- [ ] Volume model: every host path must belong to a volume. Today `ParentDir()` from an assign that lies outside all drives walks up to a bogus `home:` volume (FinalWriter builds its font path this way), so `HOME:` cannot simply be pointed elsewhere. Add a fallback root volume and isolate `HOME:` in test prefixes.

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
- [ ] Decide one rule for post-3.1 (V44-V47) extensions lxa already has: Phase 220 removed input.device `IND_ADDEVENT` (V47) to match 3.1, but kept console scrollback, icon/workbench V44, layers V45 and diskfont V45 calls as `lxa_only` tests. Either all stay (gated by version) or all go.
- [ ] Per-version reference profiles. Decide which V44+/V47 functions lxa exposes; the gadtools V47 stubs are an early instance.

### Phase 274 — Performance (was legacy 163)
- [ ] With deterministic time, wall time becomes measurable per scenario. Profile host overhead with `--profile` and set budgets per scenario.
- [ ] **Emulated-cycle cost of ROM rendering** (found in Phase 201; every ROM cycle is now emulated time): an open menu re-renders all items on every VBlank (~80–500 K cycles per item); `RectFill`/`SetPixelDirect` and `Text()` still work pixel by pixel (~37 K cycles per character including menu overhead). Make rendering row/word based and repaint only the changed highlight. Measure with `lxa_get_idle_cycles()` and PC sampling (`tools/rom_symbolize.py`).

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
| Stubbed system-library LVOs (Phase 203 inventory) | 85 stub + 39 partial + 117 without implementation (v0.11.1) | measured | 0 used by the corpus |
| Line coverage (host + ROM) | ROM 77.3 % / host 32.6 % (v0.11.0) | measured | never decreasing |
| Flaky-test rate (20× runs) | 0 / 20 under load (v0.11.0) | 0 | 0 |
| Full-suite wall time `-j16` | 22 s (v0.11.0; was 110 s) | ≤145 s | ≤120 s |

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
| 201 | Deterministic virtual time: cycle-derived VBlank/timer/DateStamp (`lxa_vclock.c`, 25 MHz virtual CPU, idle skipping), emulated-time timeouts (`EmuDeadline`), real `WaitTOF`, ReadEClock overflow fix, host stdin detached under liblxa, 38 hot-path LPRINTFs demoted, ROM built for 68020, faster `Text`/`memset`/`CopyMem`. Suite 110 s → 22 s, 20/20 runs under load green. | v0.11.0 |
| 202 | Measurable coverage: ROM PC-bitmap coverage + disk-library vector coverage (`LXA_ROM_COVERAGE`), host gcov (`-DLXA_COVERAGE=ON`), `make coverage` → merged lcov + HTML, per-LVO table `doc/coverage/lvo-coverage.md`. Baseline: ROM 77.3 %, host 32.6 %, 787 LVOs tested. | v0.11.0 |
| 203 | Stub telemetry: `LXA_UNIMPLEMENTED` → EMU_CALL_UNIMPLEMENTED for every stub/partial/private slot/empty exec vector (173 sites), `lxa_get_unimplemented_log()`, `lxa.log` summary, `--strict-unimplemented` (exit 125, stubs only), `tools/stub_inventory.py` → `doc/stub-inventory.md` (+117 LVOs without implementation); empty vectors/private slots no longer halt the emulator; console unknown commands return IOERR_NOCMD. | v0.11.1 |
| 204 | Menu/input scripting API: `lxa_select_menu_path()`, `lxa_find_menu_path()`, `lxa_get_menu_rect()` (live MenuStrip geometry), non-atomic `lxa_inject_drag_begin/_step/_end`, `lxa_get_qualifier_state()`; Intuition now opens menus on a menu-button press over the screen bar with no window under the pointer; MaxonBASIC/KickPascal/ASM-One menu tests migrated; `MenuPixelStateDuringDrag`. | v0.11.3 |
| 210 | Reference system builder: `tools/refsys/` (`build_refsys.sh` fresh WB 3.1 + 68040.library, profiles `aga`/`rtg` with Picasso96 2.0 + uaegfx; `refctl` boot/ping/shot/stop with per-instance Xvfb, TCP serial, fresh SYS copy). Boot to Workbench 1.6 s, 10/10 boot+ping, 8 parallel instances. | v0.11.2 |
| 211 | `lxaprobe` guest agent (`tools/refsys/agent/`): serial line protocol (PING, RUN, WAIT_WINDOW, WAIT_IDLE, DUMP_TREE lxa-tree/1, SNAP pen-index, MOVE/CLICK/PRESS/RELEASE/KEY/TYPE/MENU via input.device, Text() hook, SetFunction TRACE relay, MODES, QUIT with survivor/memory check, SETCLOCK); `rtg` profile Workbench on uaegfx 800×600×8; `refsys_selftest` ctest (Clock scenario). | v0.11.4 |
| 212 | Snapshot bundle schema & lxa parity: `doc/rdd-snapshot.md` (lxa-bundle/1, lxa-tree/1, LXASNAP1) with JSON Schemas; liblxa `lxa_dump_tree_json`, `lxa_capture_{screen,window}_indexed`, `lxa_text_log_*` (NDK offsets generated by `tools/gen_ndk_offsets.py`); `liblxa.so` + `tools/rdd/pylxa.py`; `tools/rdd/bundle.py`; host objects crossed into emulated memory as 32-bit handles (fixes PIE processes); ctest `rdd_python`. | v0.11.5 |
| 213 | Scenario DSL & twin runner: `tools/rdd` (`python3 -m rdd run … --backend lxa\|ref\|both`), YAML steps launch/wait_window/wait_idle/frames/click/menu/type/key/snapshot/quit, gadget clicks resolved from each backend's own tree, reference instance pool + result cache, manifests moved into `apps/<App>.json`; starter scenarios cli-helloworld, simplegtgadget, dpaintv-startup, dopus-startup on both backends. Fixes found on the way: lxa idle detection ignored the running task, `ActiveScreen` not updated by `ActivateWindow`, agent RUN command name, TRACE string arguments. | v0.11.6 |
| 214 | Comparator, report & vision review: `tools/rdd/compare.py` (tree diff with ignore list and GadgetID pairing, pen-index pixel diff with 16 px heat-map regions, palette/Text()/stdout), `python3 -m rdd report` (composite lxa \| reference \| diff ×2 with grid, HTML, `compare-summary.json`), `findings.yaml` format + validator, skill `rdd-review`; dry run `doc/findings/2026-10-02-starter.yaml` (7 findings, routed to Phases 222–224). | v0.11.7 |
| 215 | Golden promotion: `python3 -m rdd promote <scenario> --phase N` copies reference bundles to `tests/golden/<app>/<scenario>/` with a ratchet (known tree diffs + pixel budget + known size, each owned by a phase); `rdd golden` replays on lxa (fail on new divergence, `tighten` on improvement); one CTest per golden (label `golden`, DISABLED only via `golden.json`, `golden_lint` checks owning phases). 6 goldens (HelloWorld, SimpleGad, SimpleGTGadget, GadToolsGadgets, DOpus, DPaint V); an injected 1 px GadTools label shift fails `gadtoolsgadgets`. Python goldens instead of generated GTests: the scenario replay lives in pylxa. | v0.11.8 |
| 216 | RDD skills & loop: skills `rdd-reference`, `compat-sweep` (parallel per-app triage in worktrees, coordinator clustering); `python3 -m rdd cluster` (mechanical tree-diff clusters with bit-level flag signatures and folded geometry + reviewed findings; owners from golden ratchets, findings and roadmap text; phase stubs for unowned clusters in ≥ 2 scenarios); `python3 -m rdd loop` (run → report → goldens → cluster → sweep report, non-zero on golden regression or unowned cluster). First loop: `doc/sweeps/2026-10-02-loop.md`, 6 scenarios, 6/6 goldens pass, 0 unowned multi-app clusters (no stubs). Compat-DB refresh moved to Phase 230. | v0.11.9 |
| 220 | All m68k test programs run on real AmigaOS 3.1 (`python3 -m rdd suite-ref`: batched reference runs with `WAIT_EXIT`, `SYS:Tests` overlay, crash/hang detection, per-binary cache): 236 programs → 197 identical on both, 26 `lxa_only` and 13 `interactive` (owned by Phase 224) in `tests/ref_suite.yaml`; 196 `expected.ref.out` captured from the reference and checked by CTest `ref_expected_outputs_*`. Six parallel triage agents fixed the tests that encoded lxa guesses and the lxa behaviour they hid (dos: error codes, locks, Seek/Close, ParsePattern, ReadArgs, IoErr rules, Execute/C:Execute, AddSegment; exec: signals, RawDoFmt, vblank servers, CIA, trap convention + held tasks; graphics: InitBitMap/RastPort flags, draw modes, regions, sprites/GELs, display database; intuition/gadtools/BOOPSI/icon/iffparse/datatypes; devices: timer, clipboard, gameport, ramdrive, parallel/printer, console CSI/SGR, layers scrolling/damage). | v0.11.13 |
| 230 | App corpus & compat DB: 36 apps catalogued (`apps/<App>.json`; 20 added from the user's FS-UAE disk, with provenance and their real third-party libraries; DOPUS/SYSINFO/KP2 are `alias_of` symlinks), `tools/rdd/corpus.py --check` (CTest `corpus_check`), one launch scenario per app (`tests/scenarios/apps/`), `python3 -m rdd compat` derives ratings from a twin run into `apps/compat.yaml` (no free-text issues; divergences linked to phases), `rdd dashboard`, `rdd loop` fails when a rating drops. First run: 1 platinum, 22 silver, 2 bronze, 6 garbage, 5 untested (3 crash the reference too). | v0.11.11 |
| 233 | Relay-trace differential: `src/lxa/lxa_relay.c` traces calls through library jump tables (`LXA_TRACE`, `lxa_trace_start`, return detection by return address + SP, library list refreshed after `OpenLibrary`, ROM-internal calls skipped, `LXA_TRACE_INJECT` fault injection); scenario step `trace:`; reference side via lxaprobe `TRACE`; `python3 -m rdd tracediff` (NDK .fd names, clib-prototype typing, top-level calls only, first divergence in order or return value). Gate: an injected `Seek` return bug is located (`tests/rdd/test_tracediff.py`). Found and fixed: `Close()` returned 1 instead of DOSTRUE, `Seek()` returned the new position and allowed seeking past EOF; `ExecBase->ChkBase` was never set. | v0.11.10 |
| 235 | Library override mode: `LXA_OVERRIDE=asl,iffparse` (+ `LXA_OVERRIDE_DIR`, default the reference system's `Libs/`) keeps the built-in library private to the ROM and maps `LIBS:<name>` to the user's AmigaOS 3.1 binary (`src/lxa/lxa_override.c`, `EMU_CALL_LIB_OVERRIDDEN`); only 3.1 disk libraries accepted; `tests/exec/libident`; gate `tests/rdd/test_override.py` (Tests/IffParse/Basic runs on the real iffparse 40.1). Debug-only workflow: AGENTS §6.26. | v0.11.12 |
