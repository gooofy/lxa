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

| Property | Reference (FS-UAE) | lxa (Phase 240 done) |
|---|---|---|
| Model / CPU | A4000, 68040 + FPU, Kickstart 3.1 r40.70 (A4000) | 68040 with FPU; AttnFlags `AFF_68040\|AFF_FPU40\|AFF_68881\|AFF_68882`, lxa's own `68040.library`; no MMU (Phase 275) |
| Chipset / memory | AGA, 2 MB chip, 8 MB fast at $07800000, 16 MB Zorro III fast | the same chipset and memory map |
| RTG | `uaegfx` (card driver built into FS-UAE) + Picasso96, 4 MB VRAM | own `Picasso96API.library` (Phases 241–242) |
| OS | **Fresh** Workbench 3.1 (40.42) built from the ADFs in `~/media/sys/amiga/os/3.1/` + `68040.library` + Picasso96 | lxa ROM + `share/lxa/System` |
| Profile `aga` | Workbench on PAL Hires 640×256, 4 colours, OSCAN_TEXT, topaz 8, WB 3.1 default palette and prefs | identical defaults |
| Profile `rtg` | Workbench on a uaegfx mode (e.g. 800×600×8; final mode fixed in Phase 210) | identical defaults once M4 lands |
| Drives | `SYS:` refsys · `APPS:` `../lxa-apps` (read-only) · `LXAREF:` exchange dir | `APPS:` same layout |
| Control | `serial_port = tcp://127.0.0.1:<port>`, Xvfb, warp mode | — |
| Time | real time (warp only skips idle) | deterministic virtual clock, 25 MHz virtual CPU (Phase 201) |

The daily-driver system at `~/media/emu/amiga/FS-UAE/hdd/system` is **not** the reference. Its patches (MuForce, KingCON, SetPatch variants, MUI prefs) would contaminate the results. It stays useful for manual exploration.

**Picasso96 source for the reference**: `/mnt/dagobert-home/media/sys/amiga/apps/Picasso96 Install v1.33 (1997)(T.Abt A.Kneer).adf`. It provides `rtg.library`, `Picasso96API.library`, the `Picasso96` monitor driver and `picture.datatype`. Phase 210 verifies that this version works with FS-UAE's uaegfx; if it does not, the user supplies a newer Picasso96 (2.x).

### AROS code is a reference only (decided 2026-10-10, replaces the 2026-10-02 porting decision)

AROS code must **not** be copied or ported into lxa: its licence (AROS Public License 1.1, MPL-based file-level copyleft) does not fit lxa's MIT licence. AROS may be read to understand a component, but every lxa implementation is lxa's own code, written from the RKRM/NDK and verified against the reference with probes. The earlier ports (BCPL support from Phase 239, the colorwheel/gradientslider/tapedeck classes from Phase 238) are replaced by Phase 245.
- **LGPL code is allowed** (decided 2026-10-10; e.g. the vendored BGUI in `src/bgui`, Regina for ARexx). It keeps its licence headers in its own directory, and `doc/third-party-code.md` plus the README's License section must name every such part as not MIT licensed.
- **Source**: the local snapshot `others/AROS-20231016-source` (`rom/`, `arch/`, `compiler/`) is the reading copy.

---

## Milestone Overview

| Milestone | Theme | Priority class | Phases |
|---|---|---|---|
| **M0** | Foundations: determinism, measurability, docs | Quality | 200–204 |
| **M1** | The Reference Oracle: FS-UAE twin harness | Quality (infrastructure) | 210–216 |
| **M2** | Ground-truth the existing suite + conformance gallery | Quality → Compatibility | 220–224 |
| **M3** | Breadth: app corpus, compat DB, sweeps, trace diff | Compatibility | 230–239, 244 |
| **M4** | RTG / Picasso96 | Compatibility (reclassified, see M4) | 240–243, 246 |
| **M5** | Close the facades: system-library completeness (own implementations; AROS as a reference only) | Compatibility | 250–258 |
| **M6** | WINE-like desktop integration | New features | 260–265 |
| **M7** | Dev platform, toolkits, performance | New features / Performance | 270–276 |

M0 and M1 are on the critical path: nothing in M2 and later is efficient without them. From M3 on, the milestones form a feedback loop. M3 sweeps show which gaps actually matter, M4 and M5 fix them, and the fixes are verified by re-running the M3 sweeps. M4 and M5 may run in parallel in separate worktrees.

---

## Next Phase

> M4 (**246**, the last M4 phase) and M5 (**251**, **252**, **255**, **256**) may run in parallel worktrees; M3 is complete, Phases 240–243 (68040 + AGA machine identity, RTG display, Picasso96API/cybergraphics, RTG apps and the `rtg` Workbench) are done.

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

Complete (Phases 230–239, 244, v0.11.88); see the summary table.  The Fred Fish residue (`doc/sweeps/2026-10-11-fish-residue3.md`) is owned by Phases 251, 255, 256 and 264.

---

## M4 — RTG / Picasso96 (was legacy 164–166)

**Reclassified from "new feature" to "compatibility"**: PPaint and FinalWriter fail without it, and so does DPaint's "Choose Display Mode" list. The reference profile `rtg` provides the ground truth.

**API reference: PrismRTG** (https://github.com/lainejones/PrismRTG, GPL-3.0-only). It is an independent, from-scratch RTG stack for AmigaOS 3.x with its own `Picasso96API.library` (v2: bitmaps, mode lists, screens, pixel arrays, board data) and `cybergraphics.library` (v41). Its README states that it reproduces P96 function and tag numbers for compatibility, while the implementations are its own. Rules for using it (same as AROS, AGENTS.md §1a rule 5):
- **Read it as documentation only**: LVO layout, tag IDs, structure layouts (`RenderInfo`, RGBFTYPE), and which functions and tags real apps use (MultiView, AWeb). **Never copy or port its code**, because GPL-3.0 does not fit lxa's MIT licence. Every implementation is lxa's own.
- **It is not an oracle.** Where PrismRTG and Picasso96 2.0 on the `rtg` reference profile disagree, the reference wins (rule 1). PrismRTG's documented gaps, such as no picture-in-picture, are not hints about P96 behaviour.
- Its `src/p96sdk` card-driver headers (Individual Computers et al., CC-BY) describe the `.card` and board interface. lxa does not need that interface, because lxa's display backend is the host.

Phases 240 (the A4000/040 with AGA as the reference has it), 241 (RTG display), 242 (Picasso96API.library and cybergraphics.library) and 243 (RTG apps and the `rtg` Workbench) are complete; see the summary table.  RTG is enabled per run (`lxa --rtg`, `LXA_RTG=1`, `lxa_config_t.rtg`); probes marked `rdd: profile rtg` (`tests/probes/rtg/`) run on the `rtg` reference profile.

### Phase 246 — Picasso96 / CyberGraphX residue
Values the Phase 242 probes leave out because lxa does not reproduce them yet, and the two requesters.  Each item ends with the probe line put back (re-captured on the `rtg` reference).
- [ ] `p96BestModeIDTagList`: lxa's size/depth scoring matches 610 of 667 reference queries (`src/lxa/lxa_rtg.c`, `p96_best_mode`); capture the misses as a probe and make all of them equal.
- [ ] `p96RequestModeIDTagList` and `CModeRequestTagList` show asl.library's screen-mode requester instead of Picasso96's / CyberGraphX's own window (`LXA_UNIMPLEMENTED` partials): build the reference windows (tree golden of each).
- [ ] Undocumented colours: the clear colour of `BMF_CLEAR` true-colour bitmaps, CLUT/`RECTFMT_LUT8` arrays written to true-colour bitmaps (default colour table), `p96ReadPixelArray` from CLUT bitmaps and from R5G6B5PC into B8G8R8A8 (green of the first pixel), `ScalePixelArray` beyond its first pixel, R5G5B5 alpha-bit handling (probes `rtg/p96pixels`, `rtg/p96screen`, `rtg/cgx`).
- [ ] RTG screen internals that Picasso96 sets: `ViewPort.Modes` (VP_HIDE, SPRITES), `ViewPort.DHeight` 0 and the private `Screen.Flags` bit 0x2000 (`rtg/p96screen`).
- Test gate: the omitted lines are back in the probes and `ref_expected_outputs` is green.
---

## M5 — Close the Facades (System-Library Completeness)

The order below is provisional. After Phase 231/232 it is re-sorted by how often corpus programs hit each item. AROS may be read as a reference for every item, but the code is lxa's own (see "AROS code is a reference only"). Every phase is verified with conformance probes against the reference (the Phase 222 pattern).

### Phase 250 — rexxsyslib.library + ARexx
- [ ] Implement rexxsyslib.library as an lxa disk library (own code; AROS `workbench/libs/rexxsyslib` as a reference only), replacing the 17-function facade in `lxa_rexxsyslib.c`.
- [ ] Vendor Regina (LGPL, allowed) as the ARexx interpreter (the RexxMast and `rx` equivalents), installed under `share/lxa/System`; list it in `doc/third-party-code.md` and the README's License section as LGPL.
- [ ] Gate: an app's ARexx port responds to a scripted `rx` message on both backends; rexxsyslib probe goldens pass.

### Phase 251 — commodities.library: Exchange and the last private slot
commodities.library is lxa's own complete implementation since Phase 237c (every object type, brokers, ParseIX/MatchIX, routing, input handler at priority 53, IECLASS_TIMER events from input.device, Exchange's private CopyBrokerList/FreeBrokerList/BrokerCommand; probes `commodities/*`).
- [ ] The reserved slot `Private1` (LVO -108) still calls `LXA_UNIMPLEMENTED`: find out what 3.1 does there (probe).
- [ ] Input handlers (and so commodities' custom objects) run in the VBlank interrupt on lxa, not in the input.device task: AllocMem and app callbacks from interrupt context. Move handler dispatch into the input.device task as on 3.1.
- [ ] Swallowed mouse events: a handler that drops or replaces a mouse event still lets Intuition see it (keys are handled since 237c).
- [ ] (from Phase 237e) Fish KCommodity hangs on lxa and opens its window on the reference: find the divergence (relay trace against the reference).
- [ ] Gate: the WB 3.1 `Exchange` commodity lists and controls a broker; probe goldens pass.

### Phase 252 — BOOPSI class completeness
- [ ] itexticlass, frbuttonclass, groupgclass, pointerclass; a complete sysiclass (frameiclass exists; fillrectclass done in Phase 244, probe `intuition/fillrect`).
- [ ] (from Phase 244) buttongclass and propgclass objects are GTYP_CUSTOMGADGET on 3.1 with the GA_* flags applied and a default box of 80x40 (propgclass: GFLG_GADGIMAGE with a knob image, Activation 0 unless asked); lxa's OM_NEW overwrites them with BOOLGADGET/PROPGADGET, GADGHCOMP and RELVERIFY. Needs real GM_RENDER/GM_GOACTIVE/GM_HANDLEINPUT (prop: drive the built-in prop code). Reference values: write probe `intuition/gclassnew` (NewObject with GA_RelRight/GA_RelBottom/GA_Image/GA_*Border/GA_Highlight/PGA_* - captured in Phase 244: e.g. arrow button type 0x0005 flags 0x801c act 0x0083). Apps: SnoopDos (border arrows/scrollers), AmiBlitz3 (PED window), the `gallery-autorequest`/`gallery-easyrequest` goldens.
- [ ] (from Phase 244) System requesters (BuildSysRequest/BuildEasyRequestArgs) use BOOPSI button gadgets with the caller's IntuiTexts (`GadgetText` = the pos/neg IntuiText, type 0x0005, flags 0x8004) plus a trailing empty gadget (type 0, flags 0x0007); 3.1 measures the body with the IntuiText's ITextFont (topaz 9 body: 330x45). ACE's AIDE alert. Write probe `intuition/sysreq` (captured in Phase 244).
- [ ] (from Phase 245) Intuition never sends GM_GOACTIVE/GM_HANDLEINPUT to BOOPSI custom gadgets, so colorwheel, gradientslider and tapedeck objects ignore the mouse (their methods work when called directly); dispatch input to GTYP_CUSTOMGADGET objects as 3.1 does and add an interactive test.
- [ ] (from Phase 244) groupgclass: MaxonCPP's about window ("Konnte Gadget nicht kreieren", NewObject("groupgclass") returns NULL in lxa; 3.1: 462x204 with 9 gadgets).
- [ ] Old-look (pre-V39 / non-NewLook) window images and sysiclass images scaled for larger screen fonts (gallery-screen-oldlook, gallery-menus-oldlook pages, Phase 223).
- [ ] Gate: gallery pages (Phase 223) for each class match the reference.

### Phase 253 — datatypes: real class dispatch + WB 3.1 classes
- [ ] C:AddDataTypes (needs the descriptor list; deferred from Phase 221).
- [ ] Replace the private-struct guessing (`lxa_datatypes.c:385`) with real class dispatch and DataTypes descriptors (`DEVS:DataTypes`).
- [ ] Implement picture, ilbm, text, ascii, sound and 8svx classes, and the `AddDTObject`/`RefreshDTObjectA`/`RemoveDTObject`/`DoAsyncLayout`/`PrintDTObjectA` stubs.
- [ ] Gate: MultiView-equivalent scenarios (ILBM, text) match the reference.

### Phase 254 — amigaguide.library + amigaguide.datatype
- [ ] Open, display and navigate guides; synchronous and asynchronous APIs. AROS as a reference only, own code.
- [ ] Gate: an app's Help menu opens its guide, matching the reference.

### Phase 255 — Missing libraries, resources & exec/dos slots
- [ ] C:Mount and DOS handler loading (deferred from Phase 221).
- [ ] Libraries: mathieeesingtrans, lowlevel, nonvolatile.
- [ ] Resources: battclock, battmem, FileSystem (misc.resource since 237b, disk.resource since 237c, potgo.resource since 237d). SysInfo needs these (Phase 234).
- [ ] (from Phase 237d) cia.resource timers and ICR interrupts with running CIA timers (Fish GurusGuide cia waits for 10 CIA-B timer A interrupts).
- [ ] exec: `RawIOInit`/`RawMayGetChar`/`RawPutChar` (routed to `lxa.log`), `Child*`, `ObtainQuickVector`, plus `Alert` (logged guru plus host dialog) and the `Cache*` functions (flush the CPU core).
- [ ] dos: `FindSegment`, `AbortPkt`, the remaining `AllocDosObject` types (the DosList and real FileLocks exist since 237c).
- [ ] (from Phase 237) Input() handles with a real `fh_Buf` buffer that Read/FGetC/FGets consume first: Manx `fexecv` copies the command line into `BADDR(fh_Buf)` (Fish Pdc make wrote it over address 0). Probe `dos/inputbuf` first.
- [ ] (from Phase 237) A `workbench.task` resident (FindResident(); GoWB starts it with `CreateProc(.., (rt+42)>>2, ..)`, a seglist at RomTag+42 on 3.1). Probe `exec/residents` first.
- [ ] (from Phase 237) 1.3 BCPL `Run` copies lose the rest of their command line: the background CLI reads only "\n" (Fish YachtC3's `loadproc SOUNDPROC` never starts SOUNDPROC; on 3.1 `loadproc Echo >file hello` writes hello). `lxa_bcpl_cli_start`/`CliInitRun` current input.
- [ ] bullet.library (outline fonts) as a separately scoped sub-phase. (from Phase 237e) Fish NewPop opens CGTriumvirate.font 15 (an outline font of WB 3.1) with OpenDiskFont() and quits without it.
- [ ] (from Phase 237e) Open("*") goes to the console task (`pr_ConsoleTask`, a FINDINPUT/FINDOUTPUT packet with the name "*"): with `RUN >file` the console task is the file system, and Open("*") fails (Fish Sushi's SAS/C startup exits with rc 20). Under `LXA_STDIO_FILE` the output handle's fh_Type must then be the file system port. Probe `dos/openstar` first.

- [ ] (from Phase 222) CON:/RAW: options AUTO, WAIT and SCREEN are ignored (`LXA_UNIMPLEMENTED`).
- [ ] (from Phase 244) vim-5.3: check whether the CON:/RAW: handler ports of 237d (`dos/conpkt`) let it find its window (`SetWindowTitles("VIM -")`); Open("*") must share the window.
- [ ] (from Phase 240) SysInfo shows "Ramsey rev $F" and "Gary rev $0" on the A4000 reference, "N/A" on lxa: the A4000 gate-array registers are not emulated. Find the registers SysInfo reads with a relay/bus trace of lxa (clean-room: observe, never read Kickstart code), then probe their values on the reference.
- [ ] (from Phase 234) Re-enable `SysInfoTest.DISABLED_{Memory,Boards,Libraries,Speed}Gadget…` (`sysinfo_gtest.cpp`): SysInfo busy-waits for 60 ticks of CIA-A's time-of-day counter ($BFE801-$BFEA01) and its DRIVES scan walks the DOS device list (exists since 237c - re-check); battclock.resource is missing. Golden `SysInfo/sysinfo-gadgets` records the divergence.
### Phase 256 — intuition / graphics / gadtools remaining stubs
- [ ] (from Phase 237d) Real copper lists: GfxBase->copinit, ActiView and the cprlists MakeVPort()/MrgCop() build (Fish CopDis, GurusGuide copper list them; RadBoogie patches the BPL2PTL/COLOR01 moves of its LOFCprList).
- [ ] `DisplayBeep` (screen flash plus optional host bell), `MoveScreen`/`ScreenPosition`, `ModifyProp` refresh, `RethinkDisplay`, `GT_FilterIMsg`/`GT_PostFilterIMsg`, `LoadView`, `RPTAG_DrawBounds`, layers SuperBitMap sync.
- [ ] Whatever else is in the stub inventory (Phase 203) for these libraries.
- [ ] (from Phase 237e) Fish AmigaCOMAL (demo and Install) quits after its first OpenWindow() on lxa (BltClear()'s registers are 3.1's since 237e): find the next call whose registers or flags it tests (relay traces change the flags - compare with a probe).
- [ ] (from Phase 237e) Fish vt100 2.6: on the reference it stays with its windows after OpenDiskFont("j132.font") fails, apparently blocked in CloseWindow() after RemTask() of its iconify task; on lxa it closes and exits. Find out what 3.1 does there (probe RemTask() of a task waiting on a window's port, then CloseWindow()).
- [ ] (from Phase 240) GTLV_ShowSelected: the string gadget below a listview stays empty on lxa; on 3.1 it shows the selected label (DPaintV Screen Format shows "PAL:Standard+ 640x256", golden `dpaintv-startup`). Set the string from GTLV_Selected at creation, GT_SetGadgetAttrs() and clicks, and refresh it.
- [ ] (from Phase 240) A screen's `RastPort.BitMap` is not `&screen->BitMap` on 3.1 (probe `graphics/bmattr` prints neither; lxa uses the screen's BitMap). Probe what the RastPort's bitmap is (a copy? the same planes?) and match it.
- [ ] (from Phase 240) `palette pen` sweep diffs (`doc/sweeps/owners.yaml`): re-run the sweep after Phase 240's AGA palette and 256-colour pen sharing and fix what remains, per app (ObtainPen/ObtainBestPen order, LoadRGB32 on custom screens).
- [ ] Gate: the stub inventory for intuition, graphics, gadtools and layers is empty.

- [ ] (from Phase 244) DPaintV's Settings/History sub-items ("#?") are 37 wide on 3.1, 21 on lxa (LayoutMenusA with the same labels and font; the reference measures 16 more pixels - find the input with a probe that copies DPaint's NewMenu flags/command strings).
- [ ] (from Phase 244) FinalWriter_D's buttons carry GFLG_IMAGEDISABLE on 3.1 (`gadget.flags` 6 vs 0x806); Intuition does not set it (probe `intuition/imagedisable`) - trace which call makes FinalWriter set it.
- [ ] (from Phase 244) SnoopDos: Hotkey title `<invalid>` (commodities `ParseIX`, Phase 251), window 0/256 vs 12/244, the checked "Task priority" item, sub-item width 36 vs 37; AmiBlitz3's window keeps WFLG_WINDOWTICKED at the snapshot.
- [ ] (from Phase 244) A system requester measures its body with the screen RastPort's current font on 3.1 (probe `intuition/reqfont`); with a font other than the screen font its height differs on lxa (+3/+5 instead of +2 for topaz 9) - the probe prints widths only for that case.
- [ ] (from Phase 222) residual divergences recorded in goldens and sweep clusters (`doc/sweeps/owners.yaml`): SimpleGad window title, DevPac Settings gadget texts, AutoRequest `WFLG_VISITOR`, Typeface's BGUI scroller knob, DirectoryOpus pixels; re-promote as they are fixed.
- [ ] (from Phase 222) `BitMapScale()` leaves its own DDA values in `bsa_XDDA`/`bsa_YDDA` on 3.1 (X 1:2 -> -3, 3:5 -> -5, 300:301 -> -342; Y 3:1 -> -5, 7:4 -> -8); another window moving off a GimmeZeroZero window's border redraws all its gadgets on 3.1, not on lxa.
- [ ] (from Phase 243) ProWrite 3.3 (goldens `ProWrite/prowrite-rtg`, aga `apps/prowrite`, ~13 800 px): on lxa its screen title bar and the "Untitled #1" window frame and scroll gadgets are painted over. The relay trace (`graphics:RectFill,SetRast,...`) shows lxa running a different drawing path: SetRast()/RectFill() on a RastPort that later belongs to the window, where 3.1 draws small gadget images into a 20x9 offscreen RastPort. Find the call whose result sends ProWrite down the other path.
- [ ] (from Phase 243) PPaint (golden `ppaint/ppaint-rtg`): its WDRAGGING gadget has GACT_BORDERSNIFF (0x8000) on 3.1, and its paint backdrop window has WFLG_RMBTRAP; 3.1 returns 7 for ReadPixel(0,0) on PPaint's private RastPort (the 1 px brush-cursor diff).
- [ ] (from Phase 243) System requesters (EasyRequest/AutoRequest) on the 1:1 800x600 RTG Workbench are 8 px wider and 9 px higher on 3.1 (goldens `Req/gallery-rtg-*`); asl's screen-mode info window menu (`Asl/gallery-rtg-asl-screenmode-info`, 1159 px vs 583 on aga).
- [ ] (from Phase 243) FinalWriter's toolbar images use other pens on the RTG Workbench (golden `FinalWriter_D/finalwriter-rtg` editor, ~36 000 px), plus the menu item widths (+4) and sub-menu lefts (+1/+2) seen on aga.
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
- [ ] C:LoadWB (deferred from Phase 221). The harnesses already open LoadWB's backdrop window (`LXA_WB_WINDOW`, Phase 238); its desktop icons are missing. (from Phase 237e) Fish Dk x2 scan the Workbench screen's pixels for the disk icons and quit without them.
- [ ] Drawer icons (`.info`) for lxa's system partition: the reference's SYS: root holds 8 (DOpus lists them; `dopus-startup`/`dopus-select` scroller-knob ignore regions).
- [ ] (from Phase 240) DOpus' status line: the free CHIP/FAST/TOTAL amounts and the clock differ by what the reference's Workbench and lxaprobe hold and by its boot time (`dopus-startup`/`dopus-select` status-line ignore regions). Decide with the harness Workbench whether these become comparable or stay ignored with this phase as owner.
- [ ] Decide the scope: a hosted `LoadWB` desktop versus relying on the host desktop. If a desktop is in scope, write lxa's own (AROS Wanderer as a reference only).

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
- [ ] (from Phase 240) The 68040 MMU: on the reference 68040.library/SetPatch enable it (TC = $8000, SysInfo "68040 (IN USE)", VBR at $07820CA4 allocated then); lxa reports TC 0 and keeps its VBR at the start of fast RAM. Musashi has no 68040 MMU; the probe `exec/machine` leaves TC out until a core has one.

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
| Line coverage (host + ROM) | ROM 82.3 % / host 40.5 % (v0.12.11; v0.11.0: 77.3 % / 32.6 %) | measured | never decreasing |
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
| 237b | Full Fred Fish mass re-run (`doc/sweeps/2026-10-10-fish-massrun.md`, first complete reference side: 9943 programs, instances 0-7): lxa-only crashes 299 -> 13, largest class 4 (gate < 5). Harness: lxa budget from program load + WAIT_EXIT margin, "running" = program not returned on both sides, reference retries, memory-dependent list. Fixed with 3.1 probes: VBR in fast RAM (`exec/superstate`, `exec/vectors`), misc.resource (`exec/miscres`), registers saved around foreign library/device vectors and InitSemaphore (`exec/initregs`, `exec/preserveregs`), GetCC from user mode (`exec/getcc`), unknown ILLEGAL = illegal instruction (`exec/illegal`), SPCmp/IEEE Tst condition codes (`math/cmpccr`), BltBitMap() call cost + byte-wise host blits (`graphics/blittime`), WaitForChar host SIGBUS. Residue owned by 237c/255. | v0.11.34 |
| 237c | Fred Fish residue (`doc/sweeps/2026-10-10-fish-residue.md`): lxa-only hangs and missing windows 194 -> 68, lxa-only crashes 13 -> 6 (largest class 2, was CHK 4). With 3.1 probes: overlaid executables and HUNK_NAME/DREL32/RELOC32SHORT/RELRELOC32 (`dos/overlay`, `dos/hunks`), dos results in D1 (`dos/retd1`), the DosList at boot with LockDosList/NextDosEntry/FindDosEntry, real FileLocks and a file system port (`dos/devlist`, `dos/lockfields`), disk.resource (`exec/diskres`), WBENCHSCREEN OpenScreen (`intuition/wbtype`), AllocTrap (`exec/alloctrap`), WaitPort/GetMsg(NULL) (`exec/waitport`, `exec/getmsgnull`), TextFit & co. keep A0 (`graphics/textregs`), Paula audio interrupt requests (`exec/audioirq`), BltClear pattern/words on the host (`graphics/bltclear`), D0 after PrintIText/DrawBorder/DrawImage (`intuition/drawd0`), disk libraries found by name after init (`exec/libinit`), OpenWindow ignores NewWindow.Screen for WBENCHSCREEN (`intuition/nwscreen`); DEVS: drawers of WB 3.1; commodities.library written as lxa's own complete implementation (7 probes; input.device timer events). Mass-run lxa side without the third-party LIBS: the reference lacks. Residue owned by 237d/251/255/262. | v0.11.64 |
| 237d | Fred Fish residue II and twin-runner timing (`doc/sweeps/2026-10-10-fish-residue2.md`): lxa-only hangs and missing windows 68 -> 32, lxa-only crash classes at most 1 program (was 2), `cluster2-exit` without ignore region (blink-safe lxaprobe `WAIT_PIXEL ... SNAP`). With 3.1 probes: Delay() tick rounding (`dos/delayticks`), file-operation times (`dos/iotime`) during which the task sleeps and others run, System() sleeping until its child ends, Execute() with one shell, internal Run (`dos/runload`), AllocMem() registers and flags (`exec/allocregs`, `exec/allocflags`), file and CON:/RAW: handler ports (`dos/filepkt`, `dos/conpkt`, `dos/nilread`), Info() volume node (`dos/infovol`), asynchronous System() through the shell (`dos/sysasync`), potgo.resource (`exec/potgores`), ReadArgs() defaults (`dos/readargsdef`), AllocRemember() with a NULL key (`intuition/remnull`), two floppy drives with trackdisk tasks and DF0:/DF1: nodes (`exec/tdunits`), RawDoFmt() registers (`exec/rdfregs`), SetTaskPri() byte (`exec/taskprilow`), shell variables of a command (`dos/procvar`). Mass runs: the reference's 12 s run time, its batches (fresh system directory per 25 programs) and names (`LXAREF:p/<id>/<name>`). Residue owned by 237e/240/251/255/256. | v0.11.77 |
| 237e | Fred Fish residue III (`doc/sweeps/2026-10-11-fish-residue3.md`): lxa-only hangs and missing windows 32 -> 14, lxa-only crash classes at most 1 program; the suite flakes found: a write to an unmapped address started the interactive debugger, which read the shared stdin (`tests/rdd/test_stdin.py`), and a second emulator run in one process inherited CPU registers, FPU state, host fds and host tables from the first. With 3.1 probes: RastPort blit times (`graphics/rptime`), the "Please insert volume" requester and AutoRequest() as BuildSysRequest() (`dos/volreq`), Lock(NULL) and Lock(":") (`dos/locknull`, `dos/lockcolon`), WA_PubScreenName NULL (`intuition/pubnull`), serial mn_Length (`exec/serlen`), ExNext() after the end (`dos/exnextend`), pr_WindowPtr 0 (`dos/winptr`), BltClear() registers (`graphics/bltclearcc`), fh_Port as the interactive flag and the CLI's standard handles (`dos/fhport`), DF0:/DF1: handlers (`dos/dfhandler`), DoIO() D0 to BeginIO and gameport standard commands (`exec/gpstd`), trackdisk status without a disk (`exec/tdstatus`). | v0.11.88 |
| 238 | Corpus app failures: every corpus app reaches its window, none `garbage`/`untested` (`doc/sweeps/2026-10-09-sweep.md`, compat 2 platinum / 15 gold / 19 silver, was 6 garbage + 5 untested). Scout, Oberon, ADPro, Asm-One, AQB, BTII, SIGMAth(2) (v0.11.30: memory handlers, pools, 2 MB chip + fast RAM, AROS colorwheel/gradientslider/tapedeck, scenario `stack:`/`writable:`); AmiBlitz3: Disable/Enable/Forbid/Permit and the semaphore calls preserve all registers (`exec/preserveregs`; relay traces record the caller's `ra`); asl screen-mode requester identical to 3.1 (full display database, Control menu, Mode Properties, all ASLSM tags; goldens + probes `asl/smalloc`, `graphics/modedb`); volume names: boot volume "System", RAM: = "Ram Disk" with T:/ENV: (`dos/volnames`), twin-runner SYS: laid out like the 3.1 partition; harness Workbench backdrop window `LXA_WB_WINDOW` (`intuition/wbwindow`, Fish ISAM no longer crashes); ExAll no longer overruns small buffers; lxaprobe `MENU` reaches sub-items. Residue owned by 244/258/264. | v0.11.32 |
| 244 | Corpus residue (`doc/sweeps/2026-10-10-sweep.md`): compat 2 platinum / 25 gold / 9 silver (was 15 gold / 19 silver), sweep tree-diff apps 19 -> 9. OpenWindow widens borders around GACT_*BORDER gadgets, GACT_BORDERSNIFF rules, AddGadget = AddGList (`intuition/bordersniff`; SIGMAth(2), BTII); LayoutMenusA moves items/sub-menus inside the screen and continues long menus in columns (`gadtools/menufit`; ADPro, MaxonBASIC, Scout); GT_Underscore underline IntuiTexts (`gadtools/underscore`); tag windows take the screen pens (`intuition/scrpens`; ReSource); "input.device"/"ramlib" system tasks (`exec/systasks`); IntuiTextLength/PrintIText use ITextFont (`intuition/itextfont`; FinalWriter); BuildSysRequest opens the Workbench before measuring, ownerless requesters are visitors (`intuition/reqfont`; ACE); fillrectclass (`intuition/fillrect`; reqtools/FontView); GetScreenData ignores the screen for WBENCHSCREEN, negative screen sizes fail (`intuition/screenzero`; GFA-BASIC); BOOL entry points return clean 32-bit D0 (`dos/boolret`; AmiBlitz3); scenarios: BTII waits for its menus, MaxonBASIC/ProWrite use WB 3.1 fonts; tracediff maps renamed app tasks. Residue owned by 237c/243/252/255/256 (`doc/sweeps/owners.yaml`). | v0.11.45 |
| 239 | BCPL programs: AROS m68k BCPL support ported (`src/rom/bcpl/`, `doc/third-party-code.md`); every command gets a2 = global vector, a5/a6 = BCPL call/return, a1 = BCPL stack (as 3.1, verified with `dos/entryregs`); ROM console segment for 1.3 Run/NewCLI; probe `dos/bcpl`. All six BCPL programs of the Fish corpus exit like on 3.1. Remaining BCPL stubs (coroutines, longjump, requesters) are in the stub inventory. | v0.11.27 |
| 240 | Machine identity of the A4000/040 reference: Musashi 68040 with AFF_68040/FPU40, CACR/VBR as after boot, CINV/CPUSH, FPU starting in its null state (FSAVE frames as a 68040, `exec/fpu`; SIGMAth2 fine), lxa's own `68040.library` open from boot (`exec/machine` field dump); AGA GfxBase/ChipRevBits/MonitorInfo/View (`graphics/mntrdb`, `graphics/modedb` incl. the 24-bit palette); interleaved bitmaps (AllocBitMap, Workbench, SA_Interleaved/SA_LikeWorkbench, `graphics/bmattr`); 256-colour screens sharing pens as on 3.1 (`graphics/rgb32`); the memory map (2 MB chip, 8 MB fast at $07800000, 16 MB Zorro III, MemHeaders in their regions; programs and stacks above the ROM). GoldED Recover timing disproved (the reference is slower: a fixed ReadEClock-timed loop takes 4.3-6.3 s there, 0.7-0.8 s on lxa). Residue owned by 255 (Ramsey/Gary), 256 (ShowSelected, screen RastPort), 264 (DOpus status line), 275 (MMU). | v0.11.97 |
| 241 | RTG display: with `--rtg`/`LXA_RTG=1`/`lxa_config_t.rtg` the display database lists the 57 uaegfx modes of the `rtg` profile after the native ones (DisplayInfo/DimensionInfo/MonitorInfo/NameInfo, MonitorList, BestModeIDA; `rtg/modes`); chunky RTG bitmaps (Picasso96 layout, pad tag + host descriptor) for AllocBitMap with an RTG friend or BMF_SPECIALFMT, rendered by the host (`src/lxa/lxa_rtg.c`); screens of RTG modes open at their depth and `display_update_rtg()` shows them (`rtg_gtest`); layers keep backing store in the screen format, backfill pen 0 in the ColorMap colour, ClipBlit reads obscured SMART_REFRESH parts from backing store (`rtg/rtglayers`). Residue: Phase 246. | v0.12.6 |
| 242 | `Picasso96API.library` (v2) and `cybergraphics.library` (v41) as lxa's own disk libraries (`src/rom/lxa_p96.c`, `lxa_cgx.c`, shared `lxa_rtglib.c`; open only with RTG): bitmaps, locks, pixel arrays in every RGBFTYPE/RECTFMT, TrueColorData, RectFill/WritePixel/ReadPixel with the reference's CLUT/planar semantics, mode lists and BestModeID, screens (p96OpenScreenTagList), PIP (none on uaegfx), board data, CGX mode queries, GetCyberMapAttr, LockBitMapTagList, DoCDrawMethod, Fill/Invert/Move/ScalePixelArray; probes `rtg/p96lib`, `rtg/p96modes`, `rtg/p96pixels`, `rtg/p96screen`, `rtg/cgx` from the `rtg` reference. Requesters (asl-based) and undocumented colours: Phase 246. | v0.12.4 |
| 243 | RTG apps and the `rtg` Workbench: OpenWorkBench takes the screenmode prefs (an RTG Workbench is HIRES, has View offsets 0, GfxBase MaxDisplayRow/Column of the board, a DisplayClip of the prefs mode; probe `rtg/wbscreen`); SA_LikeWorkbench copies the prefs mode/size/depth; the screen bar is a layer in front of backdrop windows, ShowTitle moves it, window MouseX/Y follow the pointer (`intuition/showtitle`, `mousepos`); GetDisplayInfoData prefers the handle over the ID; RDD indexed snapshots read RTG bitmaps. FinalWriter reaches its editor on the 800x600 Workbench (`FinalWriterRtgTest.AcceptDialogOpensEditorWindow` replaces the DISABLED test; swshell.library centres dialogs in the DisplayClip); goldens ppaint-rtg, dpaintv-rtg (uaegfx modes in Screen Format), finalwriter-rtg, prowrite-rtg (Wordsworth/Amiga Writer are not in the corpus) and 19 `gallery-rtg-*`; residue owned by 246/256. | v0.12.11 |
| 245 | AROS code replaced by lxa's own (licensing: AROS is a reference only): BCPL support (`doc/bcpl.md`) and colorwheel/gradientslider/tapedeck gadget classes (`doc/gadget-classes.md`) rewritten, both APL LICENSE files and third-party rows gone; audit of every "based on AROS" site (`doc/aros-audit.md`): 10 rewritten (area vector collection, AddFont/RemFont, FontExtent, InitBitMap, DrawBorder, DoubleClick, MakeFunctions + dead `__AROS_*` macros, ReleaseSemaphore hand-over, Exception, WaitIO), 13 comments reworded to cite NDK/RKRM/3.1, 2 interface facts kept (`LA_*` tag values, SetIPrefs codes). Suite and `suite-ref --lxa-check` (338 programs) green. | v0.11.62 |
