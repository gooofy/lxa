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
| **M3** | Breadth: app corpus, compat DB, sweeps, trace diff | Compatibility | 230–239, 244 |
| **M4** | RTG / Picasso96 | Compatibility (reclassified, see M4) | 240–243 |
| **M5** | Close the facades: system-library completeness (AROS ports where possible) | Compatibility | 250–258 |
| **M6** | WINE-like desktop integration | New features | 260–265 |
| **M7** | Dev platform, toolkits, performance | New features / Performance | 270–276 |

M0 and M1 are on the critical path: nothing in M2 and later is efficient without them. From M3 on, the milestones form a feedback loop. M3 sweeps show which gaps actually matter, M4 and M5 fix them, and the fixes are verified by re-running the M3 sweeps. M4 and M5 may run in parallel in separate worktrees.

---

## Next Phase

> M3: **237b** (agent running), then **244**.

---

## M0 — Foundations

Complete (Phases 200–204, v0.11.3); see the summary table.

---

## M1 — The Reference Oracle

Complete (Phases 210–216, v0.11.9); see the summary table.

---

## M2 — Ground-Truth the Existing Suite

Complete (Phases 220–225, v0.11.28); see the summary table.

---

## M3 — Breadth: Corpus, Compat DB, Sweeps

### Phase 237b — Full Fred Fish mass re-run
**Class**: Compatibility. Phase 237 closed the Phase 232 crash list (`doc/sweeps/2026-10-09-fish-crashlist.md`: 9 lxa-only crashes left of 299, each owned below); the other ~9600 programs were not re-run since Phase 232.
- [ ] Re-run the whole mass run (all 9943 programs, both sides; `LXA_REF_POOL` picks free reference instances) with the calibrated harness (`LXA_PROGDIR_ASSIGNS=0`, WB 3.1 fonts) and publish the report; cluster new lxa-only crash classes into phases.
- [ ] Triage the hangs and missing windows it lists, starting with Fish SetMouse (lxa keeps running without its window), DW and VChess (lxa: window/exit, 3.1: exit/hang), Scrambler (emulator hang where 3.1 crashes).
- [ ] Harness: lxa's `running` is the emulator, the reference's is the launched program (WAIT_EXIT) - Fish ISAM's detached server keeps its window, so lxa reports `window` where 3.1 reports `exit` (no crash since Phase 238's harness Workbench window); compare both on the launched program.
- [ ] Harness: programs that sleep exactly the run time (Fish DumpWB `Delay(300)`) end as `hang` on lxa because loading takes emulated time - give the lxa side the reference's load-time margin or compare them as equal.

**Test gate**: the full mass-run report shows no lxa-only crash class with >= 5 programs.

### Phase 244 — Corpus residue (from the Phase 238 sweep)
**Class**: Compatibility. `doc/sweeps/2026-10-09-sweep.md` / `apps/compat.yaml` (v0.11.32): 2 platinum, 15 gold, 19 silver, no bronze/garbage/untested. Raise the silver apps, worst first (use the compat-sweep skill's parallel triage; tracediff before hypotheses).
- [ ] Per-app divergences owned by this phase in `doc/sweeps/owners.yaml` (`Text() strings missing in lxa`, `stdout differs`): triage ADPro (477 tree diffs), AmiBlitz3 (editor area gray where 3.1 draws it white, missing editor scroller; 494602 px), SnoopDos, MaxonBASIC, DPaintV, ProWrite, Scout, SIGMAth/SIGMAth2, FontView, ReSource, FinalWriter_D, BTII, GFABasic, MaxonCPP into root causes and fix or re-own them.
- [ ] Cluster2 golden (`cluster2-exit`): the editor's cursor blink phase depends on how long the launch takes on each system (ignore region) - start both runs at the same IntuiTicks phase or compare the cursor after a fixed number of ticks.
- [ ] Re-run `python3 -m rdd sweep run` after each fix; ratings in `apps/compat.yaml` must not drop (`rdd compat --fail-on-drop`).

**Test gate**: no corpus app below gold for a reason inside lxa or the scenarios.

---

## M4 — RTG / Picasso96 (was legacy 164–166)

**Reclassified from "new feature" to "compatibility"**: PPaint and FinalWriter fail without it, and so does DPaint's "Choose Display Mode" list. The reference profile `rtg` provides the ground truth.

### Phase 240 — Machine identity: 68040 + AGA + RTG mode database
- [ ] Present a 68040. The FPU is done (Phase 237: 68881/68882 reported in `AttnFlags`, the coprocessor core completed, FPU context saved on task switches, probe `exec/fpu`). Still to do: switch the Musashi CPU type, report AFF_68040/AFF_FPU40, and ship a `68040.library`-compatible `SYS:Libs` entry, since apps probe for it. The reference's 68040.library returns 0x80000000 from FINT for |x| >= 2^31 (not mimicked).
- [ ] (from Phase 237) Fish WO 1.0d reads `IntuitionBase->ActiveScreen` before opening intuition.library, i.e. Kickstart ROM code through the vector table (`exec/vectors`: every vector points into ROM on both systems); its outcome depends on the ROM's bytes. Classify such programs as memory-dependent in `massrun report`.
- [ ] Report AGA everywhere (`ChipRevBits0`, `GetChipRev`-style code at `lxa_graphics.c:9314`, `exec.c:5765`). Add AGA display modes, 256-colour planar screens and `LoadRGB32`/`SetRGB32` 24-bit palettes.
- [ ] Cross-check GfxBase, ExecBase and display-info fields one by one against a reference `aga` dump.
- [x] Memory layout of the reference: 2 MB chip RAM plus fast RAM (done in Phase 238, v0.11.30).

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
- [ ] Old-look (pre-V39 / non-NewLook) window images and sysiclass images scaled for larger screen fonts (gallery-screen-oldlook, gallery-menus-oldlook pages, Phase 223).
- [ ] colorwheel.gadget and gradientslider.gadget as disk classes (GadToolsBox3 needs colorwheel.gadget, Phase 230 compat run).
- [ ] Gate: gallery pages (Phase 223) for each class match the reference.

### Phase 253 — datatypes: real class dispatch + WB 3.1 classes
- [ ] C:AddDataTypes (needs the descriptor list; deferred from Phase 221).
- [ ] Replace the private-struct guessing (`lxa_datatypes.c:385`) with real class dispatch and DataTypes descriptors (`DEVS:DataTypes`).
- [ ] Implement picture, ilbm, text, ascii, sound and 8svx classes, and the `AddDTObject`/`RefreshDTObjectA`/`RemoveDTObject`/`DoAsyncLayout`/`PrintDTObjectA` stubs.
- [ ] Gate: MultiView-equivalent scenarios (ILBM, text) match the reference.

### Phase 254 — amigaguide.library + amigaguide.datatype
- [ ] Open, display and navigate guides; synchronous and asynchronous APIs. Evaluate the AROS port first.
- [ ] Gate: an app's Help menu opens its guide, matching the reference.

### Phase 255 — Missing libraries, resources & exec/dos slots
- [ ] C:Mount and DOS handler loading (deferred from Phase 221).
- [ ] Libraries: mathieeesingtrans, lowlevel, nonvolatile.
- [ ] Resources: battclock, battmem, misc, potgo, disk, FileSystem. SysInfo needs these (Phase 234).
- [ ] exec: `RawIOInit`/`RawMayGetChar`/`RawPutChar` (routed to `lxa.log`), `Child*`, `ObtainQuickVector`, plus `Alert` (logged guru plus host dialog) and the `Cache*` functions (flush the CPU core).
- [ ] dos: `FindSegment`, `AbortPkt`, the remaining `AllocDosObject` types, and a real `LockDosList`.
- [ ] (from Phase 237) dos locks as real `struct FileLock`s (fl_Key, fl_Access, fl_Task, fl_Volume -> a DLT_VOLUME DosList node with its BSTR name): Modula-2 programs read the volume name through `fl_Volume` (Fish calendarfactory, AmigaPunt, Pennywise: CHK; SafeBoot copies a garbage BSTR over itself). lxa returns host ids as BPTRs. Probe `dos/lockfields` first.
- [ ] (from Phase 237) Input() handles with a real `fh_Buf` buffer that Read/FGetC/FGets consume first: Manx `fexecv` copies the command line into `BADDR(fh_Buf)` (Fish Pdc make wrote it over address 0). Probe `dos/inputbuf` first.
- [ ] (from Phase 237) A `workbench.task` resident (FindResident(); GoWB starts it with `CreateProc(.., (rt+42)>>2, ..)`, a seglist at RomTag+42 on 3.1). Probe `exec/residents` first.
- [ ] (from Phase 237) 1.3 BCPL `Run` copies lose the rest of their command line: the background CLI reads only "\n" (Fish YachtC3's `loadproc SOUNDPROC` never starts SOUNDPROC; on 3.1 `loadproc Echo >file hello` writes hello). `lxa_bcpl_cli_start`/`CliInitRun` current input.
- [ ] bullet.library (outline fonts) as a separately scoped sub-phase.

- [ ] (from Phase 222) CON:/RAW: options AUTO, WAIT and SCREEN are ignored (`LXA_UNIMPLEMENTED`).
- [ ] (from Phase 234) Re-enable `SysInfoTest.DISABLED_{Memory,Boards,Libraries,Speed}Gadget…` (`sysinfo_gtest.cpp`): SysInfo busy-waits for 60 ticks of CIA-A's time-of-day counter ($BFE801-$BFEA01) and its DRIVES scan needs a non-empty DOS device list (real `LockDosList`); battclock.resource is missing. Golden `SysInfo/sysinfo-gadgets` records the divergence.
### Phase 256 — intuition / graphics / gadtools remaining stubs
- [ ] `DisplayBeep` (screen flash plus optional host bell), `MoveScreen`/`ScreenPosition`, `ModifyProp` refresh, `RethinkDisplay`, `GT_FilterIMsg`/`GT_PostFilterIMsg`, `LoadView`, `RPTAG_DrawBounds`, layers SuperBitMap sync.
- [ ] Whatever else is in the stub inventory (Phase 203) for these libraries.
- [ ] Gate: the stub inventory for intuition, graphics, gadtools and layers is empty.

- [ ] (from Phase 222) residual divergences recorded in goldens and sweep clusters (`doc/sweeps/owners.yaml`): SimpleGad window title, DevPac Settings gadget texts, AutoRequest `WFLG_VISITOR`, Typeface's BGUI scroller knob, DirectoryOpus pixels; re-promote as they are fixed.
- [ ] (from Phase 222) `BitMapScale()` leaves its own DDA values in `bsa_XDDA`/`bsa_YDDA` on 3.1 (X 1:2 -> -3, 3:5 -> -5, 300:301 -> -342; Y 3:1 -> -5, 7:4 -> -8); another window moving off a GimmeZeroZero window's border redraws all its gadgets on 3.1, not on lxa.
### Phase 257 — Locale catalogs & non-English apps
- [ ] Real `OpenCatalog` with `LOCALE:` catalogs and language prefs, verified with German-language apps (MaxonBASIC, FinalWriter_D) on a reference with a German locale.


### Phase 258 — asl.library file and font requesters as on 3.1
**Class**: Compatibility. Phase 238 made the screen-mode requester identical to 3.1 (goldens `Asl/gallery-asl-screenmode`, `-info`); the file and font requesters are still lxa's own simplified windows (`LXA_UNIMPLEMENTED` partial in `AslRequest`).
- [ ] File requester: 3.1's GadTools layout, Control menu, volume list, pattern gadget, unlimited entries, all ASLFR_* tags and results - verified with a gallery scenario and probes on the reference.
- [ ] Font requester: size list, style/pen/draw-mode gadgets, sample text, Control menu, all ASLFO_* tags and results.
- [ ] Gate: gallery goldens for both requesters; the stub inventory has no asl entry.
---

## M6 — WINE-like Desktop Integration (New Features)

### Phase 260 — Launch UX & app prefixes
- [ ] Register `binfmt_misc` for hunk executables (magic `0x000003F3`), so `./MyAmigaApp` just runs.
- [ ] `.desktop` launcher generation.
- [ ] Per-app "prefixes" (the counterpart of `WINEPREFIX`) built from manifests: assigns, libs and prefs overlays.
- [ ] Volume model: every host path must belong to a volume. Today `ParentDir()` from an assign that lies outside all drives walks up to a bogus `home:` volume (FinalWriter builds its font path this way), so `HOME:` cannot simply be pointed elsewhere. Add a fallback root volume and isolate `HOME:` in test prefixes.

### Phase 261 — Host clipboard bridge
- [ ] C:ConClip (console copy/paste through clipboard.device), ruled out of Phase 221 until the console has clipboard support.
- [ ] clipboard.device unit 0 ↔ host clipboard (IFF FTXT ↔ UTF-8, with Latin-1 mapping).

### Phase 262 — Disk images
- [ ] Mount ADF/HDF read-only as DOS volumes on the host side, then trackdisk.device on top of ADF images (it does no I/O today).

### Phase 263 — Rootless polish
- [ ] Host window decorations policy, HiDPI integer scaling, AppWindow drag-and-drop from the host file manager, AppIcon → host tray.

### Phase 264 — Workbench
- [ ] C:LoadWB (deferred from Phase 221). The harnesses already open LoadWB's backdrop window (`LXA_WB_WINDOW`, Phase 238); its desktop icons are missing.
- [ ] Drawer icons (`.info`) for lxa's system partition: the reference's SYS: root holds 8 (DOpus lists them; `dopus-startup`/`dopus-select` scroller-knob ignore regions).
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

- [ ] (from Phase 222) diskfont, locale, icon and workbench report V44-V47 where 3.1 has V40; 3.1 ships older mathtrans/mathieeedoub*/rexxsyslib (V36-V38): decide per API level.
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
| 221 | C: commands & Shell parity: 11 shell scripts (`tests/shell_parity/`, runner `SYS:Tests/ShellParity/Run`) produce output identical to WB 3.1's commands (`.ref.out` from the reference, checked by `ref_expected_outputs`). Shell rewritten after the 3.1 shell (commands run in-process via `RunCommand`, vars/backticks/aliases/redirection, internal commands, If/Skip/Lab/FailAt); dos `ReadArgs` rewrite, in-process `RunCommand`/`System`, `MatchFirst` recursion, date strings, 3.1 fault texts, `NameFromLock` volume names, protection bits in xattrs; List/Dir/Type/Copy/Delete/Rename/Protect/Filenote/MakeDir/Join/Search/Sort/Eval/Wait/Date/Version/Status/Avail/Info/Assign/Execute rewritten to 3.1 output; new Which, SetDate, AddBuffers, CPU, DiskChange, IconX, LoadResource, Lock, Relabel, RequestFile, SetFont, SetKeyboard, SetPatch, version.library 40.42. Decision table `doc/c-commands.md` (Ed/Edit/hardware tools out of scope; ConClip/AddDataTypes/Mount/IPrefs/LoadWB owned by Phases 261/253/255/236/264). | v0.11.14 |
| 222 | API conformance probes ("WINE tests"): ~60 reference probes over exec, dos, utility, graphics, layers, intuition, gadtools, locale, keymap, iffparse, icon, diskfont, clipboard (ReadArgs, dos buffering, GELs/copper, PaletteExtra, VideoControl, Text/areas/blits/flood, ClipRects/damage/backfill, window/screen/requester geometry, every GadTools kind, CON: options, InitResident, library versions), interactive programs validated by scenario; `rdd suite-ref` binary-safe. Leftovers owned by 238/255/256/273. | v0.11.28 |
| 223 | Rendering conformance gallery: starter cluster fixed (Workbench `WBorTop` 2, pen 3 = 102,136,187, FILLPEN title bars with full borders, display ID = requested mode, MenuH/VBorder 4/2, system gadget types/order/geometry/imagery incl. drag gadget, menu drop-down position and rendering, EasyRequest layout, sysiclass/frameiclass/DrawBevelBox imagery, GadTools 3.1 geometry/imagery per kind, NewScreen/NewWindow extension tags, public-screen and visitor rules, default window placement); 7 gallery samples (`tests/gallery/`), 15 gallery goldens, `python3 -m rdd gallery`; existing goldens re-promoted (DirectoryOpus 64157→25245 px with an owned ignore region, SimpleGad 10843→1057). Remaining text-only diffs owned by Phase 225, internals by 222d/e, old-look images by 252. | v0.11.15 |
| 224 | Legacy app tests reference-validated: goldens for DPaint (startup after its 4 s ownership splash, Screen Format), Devpac (startup, typing, Settings), DOpus, Typeface (main + Preview); five pixel-count heuristic tests retired; interactive test programs replayed on AmigaOS 3.1 through lxaprobe input (console key/raw-event input incl. new `idcmp_console`, IDCMP Menu/SizeVerify) - findings fixed: `pr_SegList` segment array (DOpus detach), console.device no longer takes over the window's IDCMP; remaining findings moved to 222d/222e/222f/238/240. Suite: 121 tests, 24 s wall at -j16. | v0.11.20 |
| 225 | ROM font fidelity: topaz 8 (all 224 glyphs, strip layout, TextFont fields) and a new ROM topaz 9 generated from a reference dump (`tests/probes/graphics/romfont.c` → `tools/gen_romfont.py` → `src/rom/romfont_data.h`, ctest `romfont_data_check`, `doc/rom-fonts.md`); OpenFont nearest-size rules; soft styles and Workbench disk fonts identical (`diskfont/wbfonts`); layout offsets the old glyphs hid fixed (window titles, GadTools labels, menu shortcut and ghost pattern). 15 goldens now at 0 diffs; remaining owners moved to 222/252. | v0.11.24 |
| 230 | App corpus & compat DB: 36 apps catalogued (`apps/<App>.json`; 20 added from the user's FS-UAE disk, with provenance and their real third-party libraries; DOPUS/SYSINFO/KP2 are `alias_of` symlinks), `tools/rdd/corpus.py --check` (CTest `corpus_check`), one launch scenario per app (`tests/scenarios/apps/`), `python3 -m rdd compat` derives ratings from a twin run into `apps/compat.yaml` (no free-text issues; divergences linked to phases), `rdd dashboard`, `rdd loop` fails when a rating drops. First run: 1 platinum, 22 silver, 2 bronze, 6 garbage, 5 untested (3 crash the reference too). | v0.11.11 |
| 231 | Shallow sweep & divergence ranking: scenario step `menus` (open every menu, snapshot, cancel) on both backends, `python3 -m rdd sweep gen|run|rank` (36 app sweep scenarios generated from the Phase 230 launch scenarios; ranking crash > no window > tree diff > pixel diff; clustering + phase stubs), explicit cluster ownership map `doc/sweeps/owners.yaml` (checked by golden_lint). Report `doc/sweeps/2026-10-03-sweep.md`: 31 clusters in >= 2 apps, all owned; per-app failures moved to Phase 238. | v0.11.17 |
| 232 | Fred Fish mass run: `python3 -m rdd massrun crawl|lxa|ref|report` (9932 executables from 993 disks; lxa side in a private system copy; reference batched per instance with crash/hang detection). Emulator-level failures (host segfaults, Musashi `exit()`, wall-clock stalls) driven to zero: 32-bit address wrap in RAM/ROM fast paths, copper self-jump recursion, FPU/PMMU fatal errors, Read/Write buffer clamping, `WaitForChar` real-time select, runaway PCs (now a bus error), AmigaOS trap convention with held tasks / Guru for faulting interrupt code, sane stack for held tasks, bounded fault logging, console handles never close host fds; `Open("*")` and the CLI stack-size entry convention fixed. Report `doc/sweeps/2026-10-03-fish-massrun.md`; remaining lxa-only crash classes owned by Phase 237. | v0.11.16 |
| 233 | Relay-trace differential: `src/lxa/lxa_relay.c` traces calls through library jump tables (`LXA_TRACE`, `lxa_trace_start`, return detection by return address + SP, library list refreshed after `OpenLibrary`, ROM-internal calls skipped, `LXA_TRACE_INJECT` fault injection); scenario step `trace:`; reference side via lxaprobe `TRACE`; `python3 -m rdd tracediff` (NDK .fd names, clib-prototype typing, top-level calls only, first divergence in order or return value). Gate: an injected `Seek` return bug is located (`tests/rdd/test_tracediff.py`). Found and fixed: `Close()` returned 1 instead of DOSTRUE, `Seek()` returned the new position and allowed seeking past EOF; `ExecBase->ChkBase` was never set. | v0.11.10 |
| 234 | Legacy app items re-validated with reference goldens: BlitzBasic 2 ted (prop gadgets from a `propgad` probe, old-look screen pens, window re-activation), Cluster2 EXIT (screen MouseX/Y on every input path; blitter masks/shift/descending fixed), DOpus (`DirectoryOpusCopiesFile` re-enabled, PaletteExtra for every screen, string gadget Amiga-X/Q, `.uaem` hidden), SysInfo (beam position registers, address 0); goldens `blitzbasic2-ted`, `cluster2-exit`, `dopus-select`, `sysinfo-gadgets`. SysInfo page gadgets moved to Phase 255. | v0.11.29 |
| 235 | Library override mode: `LXA_OVERRIDE=asl,iffparse` (+ `LXA_OVERRIDE_DIR`, default the reference system's `Libs/`) keeps the built-in library private to the ROM and maps `LIBS:<name>` to the user's AmigaOS 3.1 binary (`src/lxa/lxa_override.c`, `EMU_CALL_LIB_OVERRIDDEN`); only 3.1 disk libraries accepted; `tests/exec/libident`; gate `tests/rdd/test_override.py` (Tests/IffParse/Basic runs on the real iffparse 40.1). Debug-only workflow: AGENTS §6.26. | v0.11.12 |
| 236 | Prefs fidelity: `C:IPrefs` (input, locale, overscan, font, palette, pointer, icontrol, screenmode from `ENV:Sys/*.prefs`, run at boot), `OpenLocale()` reads locale prefs, 3.1 defaults (ColorMap, overscan, Preferences, Workbench/pointer colours), fonts/palette/pens applied as on 3.1; goldens `Prefs/gallery-prefs-{default,fontpal,sys}` (scenario keys `prefs:`, `keymaps: wb31`). Open: wbpattern and icon font (264), live prefs changes, pointer image drawing. | v0.11.27 |
| 237 | Fred Fish crash classes: of the 299 Phase 232 lxa-only crashes 9 remain (owned by 237b/238/240/255; `doc/sweeps/2026-10-09-fish-crashlist.md`). Fixed, each with a 3.1 probe: entry registers, any-a6 dos, signed OpenLibrary versions, exec I/O returns and scratch registers, `pr_ReturnAddr`, ParentDir at assign roots (v0.11.21-26); address 0 = 0 and a full vector table (`exec/lowmem`, `exec/vectors`), CreateNewProc runs only higher-priority children first (`dos/newprocrun`), `dl_UtilityBase`/`dl_IntuitionBase`, OpenLibrary from the current directory (`exec/libpath`), FPU reported and the coprocessor core completed with task-switch context (`exec/fpu`), CIA-A buttons (`exec/buttons`), WaitBlit/OwnBlitter keep registers (`graphics/blitregs`), custom gadgets via the MutualExclude hook (`intuition/customhook`), PRT: (`dos/prt`), SuperState/UserState (`exec/superstate`), DisplayAlert waits for a button, ViewAddress' ViewPort list (`intuition/viewaddress`), console unit font (`console/conunit`), input handlers keep the writer's registers (`input/handlerregs`), InitResident of any type (`exec/initresident`); harness: `LXA_PROGDIR_ASSIGNS=0`, WB 3.1 fonts, `LXA_REF_POOL`, bounded invalid-write logging. | v0.11.31 |
| 238 | Corpus app failures: every corpus app reaches its window, none `garbage`/`untested` (`doc/sweeps/2026-10-09-sweep.md`, compat 2 platinum / 15 gold / 19 silver, was 6 garbage + 5 untested). Scout, Oberon, ADPro, Asm-One, AQB, BTII, SIGMAth(2) (v0.11.30: memory handlers, pools, 2 MB chip + fast RAM, AROS colorwheel/gradientslider/tapedeck, scenario `stack:`/`writable:`); AmiBlitz3: Disable/Enable/Forbid/Permit and the semaphore calls preserve all registers (`exec/preserveregs`; relay traces record the caller's `ra`); asl screen-mode requester identical to 3.1 (full display database, Control menu, Mode Properties, all ASLSM tags; goldens + probes `asl/smalloc`, `graphics/modedb`); volume names: boot volume "System", RAM: = "Ram Disk" with T:/ENV: (`dos/volnames`), twin-runner SYS: laid out like the 3.1 partition; harness Workbench backdrop window `LXA_WB_WINDOW` (`intuition/wbwindow`, Fish ISAM no longer crashes); ExAll no longer overruns small buffers; lxaprobe `MENU` reaches sub-items. Residue owned by 244/258/264. | v0.11.32 |
| 239 | BCPL programs: AROS m68k BCPL support ported (`src/rom/bcpl/`, `doc/third-party-code.md`); every command gets a2 = global vector, a5/a6 = BCPL call/return, a1 = BCPL stack (as 3.1, verified with `dos/entryregs`); ROM console segment for 1.3 Run/NewCLI; probe `dos/bcpl`. All six BCPL programs of the Fish corpus exit like on 3.1. Remaining BCPL stubs (coroutines, longjump, requesters) are in the stub inventory. | v0.11.27 |
