# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, and roadmap updates on every completed phase.

---

## Project Vision

lxa is the foundation for a modern Amiga on commodity hardware (Linux PC, Raspberry Pi). By targeting OS-compliant applications only, lxa can bypass chip-register emulation entirely and go straight to RTG (Retargetable Graphics), enabling full true-color displays at arbitrary resolutions on any SDL2-capable host.

The primary target applications are productivity software (Wordsworth, Amiga Writer, PageStream) and IDEs (Storm C, BlitzBasic 3). MUI and ReAction are important GUI toolkit goals. The long-term goal is for lxa to become a **viable development platform for Amiga software on Linux** — enabling Amiga developers to write, test, and run their applications directly on commodity hardware without needing real Amiga hardware.

**RTG strategy**: Implement Picasso96 (`Picasso96API.library`) as the RTG system. This gives all P96-aware apps a clean, standard interface and the broadest compatibility with modern AmigaOS productivity software. PPaint is the flagship RTG validation target. A thin `cybergraphics.library` compatibility shim is included so apps that probe for CGX also work.

---

## Completed Phases (Summary)

| Phase | Description | Version |
|-------|-------------|---------|
| 106 | Test performance: headless display skip, idle detection, cycle budget cuts (−38% wall time) | v0.8.68 |
| 107 | Test performance: persistent fixtures, SetUp deduplication (−49% combined with 106) | v0.8.69–70 |
| 108 | App testing: SysInfo + Cluster2 drivers; Musashi IRQ pulse fix; ExecBase fields | v0.8.71–72 |
| 108-d | Manual testing sweep: 13 fixes across 7 apps; event queue, GfxBase, GadTools, SMART_REFRESH, IDCMP_INTUITICKS | v0.8.73 |
| 109 | Missing IDCMP types (REQSET/REQCLEAR, SIZEVERIFY, MENUVERIFY, DELTAMOVE); GfxBase audit | v0.8.74 |
| 110 | Third-party disk library stubs: req, reqtools, powerpacker, arp | v0.8.75 |
| 111 | SMART_REFRESH full backing store: CreateObscuredClipRect, Save/RestoreBackingStore, CR-aware graphics | v0.8.76 |
| 112 | Menu double-buffering: pixel-accurate save/restore; native BltBitMapCore via EMU_CALL | v0.8.77 |
| 113 | Blitter line-draw mode: Bresenham, all octants, SING/ONEDOT, ASH cross-word wrap | v0.8.78 |
| 114 | Copper list interpreter: MOVE/WAIT/SKIP, VBlank execution, CDANG, color register shadow | v0.8.79 |
| 115 | DPaint V driver expansion: Screen Format gadgets, depth-2→8 transition, main editor render | v0.8.80 |
| 116 | MaxonBASIC driver expansion: text rendering, RMB menu reveal, scrollbar gadgets, stress survival | v0.8.81 |
| 117 | DevPac driver expansion: Amiga-key shortcut survival, full menu bar sweep, req.library stub validation | v0.8.82 |
| 118 | ASM-One driver expansion: menu flicker guard, event-queue rapid-motion stress, qualifier-key propagation survival | v0.8.83 |
| 119 | BlitzBasic 2 driver expansion: editor-paint regression bounds, secondary-window tracking, multi-menu stability, editor keyboard survival | v0.8.84 |
| 120 | PPaint driver expansion: probe-window geometry bounds, GfxBase regression guards, null-BitMap probe guards, capture-pipeline survival; ROM path helper fix | v0.8.85 |
| 121 | DirectoryOpus driver creation: 10 tests covering startup, bundled-library resolution, geometry, content rendering, gadget enumeration safety, capture pipeline, menu-probe survival, idle-time stability; PPaint sharding bug fix (orphaned Z-tests) | v0.8.86 |
| 122 | KickPascal 2 driver expansion: default-workspace acceptance reaches EDITOR mode (status-bar pixel proof), RMB menu drag survival + geometry stability, splash content stability across idle, missing-library/PANIC log guards | v0.8.87 |
| 123 | Typeface deferred — bgui.library v39+ required; real binary now available at `others/bgui`, installed to `share/lxa/System/Libs/bgui.library`; full driver deferred to Phase 136 | — |
| 124 | FinalWriter driver expansion: dialog-state coverage (title regression, gadget count, OK/Cancel button discovery, display-options list discovery, missing-library/PANIC log guards, idle-time content stability); FinalWriter sharding bug fix (orphaned tests); broken `AcceptDialogOpensEditorWindow` quarantined as DISABLED_ | v0.8.88 |
| 0 | CMake shard coverage safety: audit all sharded drivers (4 orphaned GadTools tests fixed), `tools/check_shard_coverage.py` written and registered as `shard_coverage_check` CTest meta-test | v0.8.89 |
| 125 | `lxa.c` decomposition: split 9,960-line monolith into `lxa_custom.c`, `lxa_dispatch.c`, `lxa_dos_host.c`, `lxa_internal.h`, `lxa_memory.h`; `lxa.c` reduced to ~1,658 lines | v0.8.90 |
| 126 | Profiling infrastructure: `lxa_profile.c` with per-EMU_CALL counters + timing; `--profile <path>` flag on lxa binary; `tools/profile_report.py` analysis tool; `ZStartupLatency` baseline tests in SysInfo, DOpus, FinalWriter, SIGMAth2, Cluster2, BlitzBasic2, Vim drivers; profiling API unit tests in `api_gtest.cpp` | v0.9.0 |
| 127 | Memory access fast paths: direct bswap-based 16/32-bit RAM and ROM reads/writes in `m68k_read/write_memory_16/32`; `__builtin_expect` hints on hot paths in `mread8`/`mwrite8`; 4 byte-order correctness unit tests in `api_gtest.cpp` | v0.9.1 |
| 128 | Display pipeline optimization: dirty-region scanline tracking (min/max dirty row → partial SDL texture upload); SSE2 planar-to-chunky acceleration (8 pixels/iter via byte broadcast + mullo); coalesced VBlank uploads; all visual regression tests pass unchanged | v0.9.2 |
| 129 (partial) | Screen-mode emulation ROM improvements: `g_known_display_ids[]` expanded from 6 to 36 entries (DEFAULT/NTSC/PAL monitors × 12 ECS modes incl. LORESLACE, HIRESLACE, SUPER, HAM, HAMLACE, HIRESHAM, EHB, EHBLACE); `graphics_virtualize_display_id()` maps unknown IDs to closest physical mode (Wine strategy); `GetDisplayInfoData(DTAG_DIMS)` returns realistic raster ranges (HIRES 320–1280, LORES 160–640); `DIPF_IS_HAM`/`EXTRAHALFBRITE`/`LACE`/`ECS` advertised in `DTAG_DISP`; `BestModeIDA()` honors `BIDTAG_DIPFMustHave`/`MustNotHave`; `FindDisplayInfo(INVALID_ID)` correctly rejects; `LockIBase()` returns 0; `OpenScreen` accepts WBENCHSCREEN via CUSTOMSCREEN + flag; VPModeID set from SA_DisplayID; OpenFont loads screen font; `OpenWorkBench` uses CUSTOMSCREEN trick; diskfont registers fonts by basename; GfxBase DisplayFlags/VBlankFrequency/PowerSupplyFrequency populated; display_viewport test expanded with virtualization + HAM/EHB/LACE property-flag + BestModeIDA MustNotHave coverage. **PPaint launch-path still blocked** — see Known Limitations | v0.9.3 |
| 132 | SMART_REFRESH backing store validation + workaround removal: new `BackingStoreTest` sample + `backingstore_gtest.cpp` driver (4 tests) prove Phase 111's backing store correctly saves/restores SMART_REFRESH pixels through dialog-over-window obscure/uncover cycles; defensive `IS_SMARTREFRESH` skip in `DamageExposedAreas()` removed since backing store is no longer in danger of being overpainted; full suite 67/67 pass | v0.9.6 |
| 133 | Menu double-buffering: `_render_menu_bar()` and `_render_menu_items()` in `lxa_intuition.c` now render into a persistent off-screen compose BitMap and atomic-BltBitMap the completed frame to the screen, eliminating pixel flicker during menu drags on ASM-One and MaxonBASIC. `ZMenuFlickerCheck` upgraded to ±5% pixel-stability assertion with throwaway first drag; new `ZMenuDragPixelStability` test in `maxonbasic_gtest.cpp` guards the editor area against menu residue. Cycle budgets bumped in `maxonbasic`, `gadtoolsmenu`, `blitzbasic2` drivers to account for the compose→blit double traversal cost. `YEditorSurfaceShowsContentAfterMenuInteraction` rewritten: its lower bound relied on flicker residue bleeding into the editor area, which no longer happens (correct Amiga behaviour). Full suite 67/67 pass in ~163s | v0.9.7 |
| 134 | DirectoryOpus button bank rendering: diagnosed via Phase 130 text hook that DOpus renders its main button bank (Copy/Move/Rename/Makedir/etc.) via a VBlank-driven timer event that fires ~400 VBlanks after startup; the Phase 121 test settle budget (150 iters) was insufficient. Fixed by bumping the `DOpusTextHookTest` and `DOpusTest` settle loops to 600 VBlank iterations (30M cycles). Upgraded `TextHookCapturesKnownDOpusLabels` assertion to verify Copy/Move/Rename labels appear. Cleaned up all diagnostic LPRINTFs from the previous investigation session (exec.c, lxa_dos_host.c, lxa_graphics.c). Yield added to `_dos_CreateNewProc` (give new processes a timeslice, matches real Amiga scheduler behaviour) retained as a correctness improvement. | v0.9.8 |
| 135 | BlitzBasic 2 ted editor deep dive: original blitter/copper hypothesis invalidated (BB2 emits zero blitter/copper writes at startup). Real root cause: off-screen-clamped About requester blocks all editor activity via `WaitPort()`. Fixed `_intuition_OpenWindow` to recentre off-screen windows instead of clipping to edge; fixed RAWKEY routing to `ActiveWindow`. `StartupOpensVisibleIdeWindow` flipped from upper-bound to lower-bound assertion. All 10 BB2 tests pass. | v0.9.9 |
| 136 | bgui.library → Typeface: BGUI gadgets (`bgui_bar`, `bgui_layoutgroup`, `bgui_palette`, `bgui_popbutton`, `bgui_treeview`) copied to `share/lxa/System/Libs/gadgets/`. Typeface binary at `lxa-apps/Typeface/Typeface`; bundles its own `bgui.library` in `Libs/`. New `typeface_gtest.cpp` driver (5 tests): `StartupOpensWindow`, `NoPanicDuringStartup`, `TextHookCapturesSomeText`, `WindowGeometryIsPlausible`, `IdleTimeStability`, `ZStartupLatencyBaseline`. All 5 tests pass (83 s). Full suite: 63/68 pass (4 pre-existing failures unrelated to Phase 136). | v0.9.10 |
| 136-h | datatypes.library hotfix: `lxa_datatypes.c` existed in `src/rom/` but was never wired into the build, causing `OpenLibrary("datatypes.library",…)` to return NULL and any subsequent `jsr (-$f0,A6)` to crash at PC=0xFFFFFF10. Rewrote `lxa_datatypes.c` as a clean minimal stub (all public API functions return safe failure values); registered it via `add_disk_library(datatypes_disklib datatypes.library …)` in `sys/CMakeLists.txt`. Typeface now opens cleanly. | v0.9.11 |
| 136-h2 | commodities.library missing from ROM: `lxa_commodities.c` existed in `src/rom/` but was never added to the ROM `CMakeLists.txt` or registered in `exec.c`. bgui.library tries to open `commodities.library` at init time; with it absent, bgui failed to initialise and Typeface complained "missing bgui.library v39+". Fixed by adding `lxa_commodities.c` to the ROM build, declaring its `ROMTag` in `exec.c`, and registering it in `g_ResidentModules[]`. Added `FindGadgetsPath()` to `lxa_test.h` so `GADGETS:` is set up in all tests (needed for gadget-class loading). Added `GADGETS:` assign to the `App: Typeface` launch.json entry. | v0.9.12 |
| 136-b1 | bgui.library source build foundation: full BGUI 42.0 source tree vendored from `others/bgui/BGUI` to `src/bgui/` (49 .c files, 22 headers, 3 asm files, 336 files total / 3.8 MB). `bgui_compilerspecific.h` patched with a new `__GNUC__ && __mc68000__ && !__AROS__` branch for bebbo m68k-amigaos-gcc — adds INLINE/STDARGS/SAVEDS/CONSTCALL/etc. attribute defs, redefines `METHOD()`/`METHODPROTO()` with per-argument `__asm("a0/a2/a1/a4")` clauses, fixes the upstream broken `typedef IPTR ULONG` (reversed + missing semicolon), pulls in `<exec/types.h>` so ULONG is known regardless of include order, typedefs `RAWARG` to `IPTR *` for non-AROS varargs. `include/aros/class-protos.h` patched: dropped the conflicting `void sprintf(char *, char *, ...)` (collides with bebbo's standard `int sprintf`), updated `LibInit` proto to 3-arg form (D0 dummy + A0 seg + A6 sysbase per lib.c Phase 42.13), updated `BGUI_GetCatalogStr` proto to use `CONST_STRPTR`. Header shim directory `src/bgui/build_include/{bgui,libraries}/` provides `<bgui/...>` and `<libraries/bgui*.h>` resolution via symlinks to the vendored `include/*.h`. `o/bgui.library_rev.h` symlink wired up. `stkext.c` patched to include `<bgui/bgui_compilerspecific.h>` for IPTR. With these patches, **44 of 49 BGUI source files compile cleanly** with `m68k-amigaos-gcc -c -O2 -noixemul -fbaserel -m68000 -DLIBRARY_BUILD`. Remaining 5 (`classes.c`, `dgm.c`, `listclass.c`, `miscc.c`, `request.c`) fail with small individual issues to be addressed in 136-b2. CMake target and link step still pending. | v0.9.13 |
| 136-b2 | bgui.library source build: all 49 source files compile. `bgui_compilerspecific.h` extended with `getreg(REG_A4)`/`putreg()`/`geta4()` (a4 read via `register IPTR _v __asm("a4")`, used by listclass.c and dgm.c for the BGUI library global pointer that lives in A4 under `-fbaserel`), and with portable `AROS_SLOWSTACKTAGS_PRE[_AS]/ARG/POST` and `AROS_SLOWSTACKMETHODS_PRE[_AS]/ARG/POST` macros (varargs-as-TagItem-array trick — same convention as NDK `<inline/...>` stubs — used by classes.c/dgm.c/miscc.c). `classes.c` guards SAS/C `<dos.h>` for bebbo. `request.c` adds `#include <bgui/bgui_macros.h>`. `build_include/bgui/bgui_macros.h` symlink retargeted to `../../include/bgui_macros.h` (the prior libraries_bgui_macros.h target was circular). Compile sweep: **OK 49 / Fail 0**. CMake target and link step next. | v0.9.14 |
| 136-b3 | bgui.library source build COMPLETE: lxa-built bgui.library replaces the SAS/C 2000-vintage prebuilt in `share/lxa/System/Libs/`. New `src/bgui/lxa_shims.c` provides 5 categories of stubs needed to make BGUI link with bebbo m68k-amigaos-gcc instead of SAS/C: (1) `__restore_a4` — bare RTS shim, justified because the four `getreg(REG_A4)` sites (classes.c x3, listclass.c x1) treat A4 only as an opaque "BGUIGlobalData" cookie that is saved/restored/passed-as-arg but never dereferenced as a real data pointer, so leaving A4 alone preserves BGUI's invariants without needing libnix's `___a4_init` scaffolding; (2) `stch_l` / `stcu_d` — SAS/C string-to-number helpers re-implemented over `strtol` (blitter.c:223) and inline decimal conversion (labelclass.c:567); (3) `kprintf` no-op (one bare site at baseclass.c:1159 outside `WW(...)`); (4) `exit` no-op halt (transitively pulled in by libnix's raise.o/__initstdio.o, never legitimately reached in a library); (5) `AsmCreatePool`/`AsmDeletePool`/`AsmAllocPooled`/`AsmFreePooled` — thin V39 Exec wrappers replacing SAS/C-asm extern decls in task.c. New `src/bgui/CMakeLists.txt` builds 49 BGUI sources (48 vendored + lxa_shims.c) per-file with `m68k-amigaos-gcc -O2 -m68000 -noixemul -fbaserel -DLIBRARY_BUILD -Wno-int-conversion`, links with `-shared -nostartfiles -noixemul -fbaserel`, and stages the result into `share/lxa/System/Libs/bgui.library`. Wired into top-level `CMakeLists.txt` as `add_subdirectory(src/bgui target/bgui)`. Output binary: 198,880 bytes (vs. 140,692-byte SAS/C prebuilt — larger due to GCC O2 codegen but correct). Resident-tag symbols verified: `_ROMTag`, `_LibInit`, `_LibName`, `_LibID`, `_BGUI_end` all present. Typeface still launches and `typeface_gtest` passes (5/5, ~4.5s wall). Full suite: 63/68 — same 5 pre-existing failures as HEAD baseline (`gadtoolsmenu_gtest`, `gadtoolsgadgets_pixels_c_gtest`, `dos_gtest`, `misc_gtest`, `apps_misc_gtest`); zero regressions from the BGUI swap. Window-sizing instrumentation deferred to 136-b4. | v0.9.15 |

**Known open limitations** (not yet addressed):
- KickPascal layout/menus — depends on deeper arp/req library functionality
- SysInfo hardware fields (BattClock, CIA timing) — requires hardware detection
- Cluster2 EXIT button — coordinate mapping mismatch in custom toolbar
- DPaint V main editor defers menu bar / toolbox / palette rendering until first mouse interaction; canvas remains blank-pen until tool/palette state is exercised. Palette/pencil/fill interaction tests deferred — require either the deferred-paint trigger to be reverse-engineered or a new "force full redraw" capability.
- DPaint V Ctrl-P (Screen Format reopen) only works on the depth-2 startup dialog, not from the depth-8 main editor. About dialog and File→Open requester not yet exercised — depends on resolving deferred-paint to make menu bar interactable.
- MaxonBASIC About/Settings/Run interaction: menus are Intuition-managed but reachable only via RMB drag through hardcoded coordinates; the German menu titles ("Projekt", "Editieren", "Suchen") and lack of programmatic menu introspection make item-level activation brittle. Phase 116 verified menu reveal and editor text rendering; deeper menu-driven workflows defer until host-side menu introspection (Phase 131 gap #4) is implemented.
- DevPac File→Open requester: Phase 117 verified Amiga-O does not crash, but req.library stub returns failure cleanly so no requester appears. Real file requester behavior requires a non-stub req.library implementation (out of scope per AGENTS.md "do not re-implement third-party libraries in ROM"). Assemble/Run workflow not exercised — would require a loaded valid m68k source and external assembler invocation, both of which need external-process emulation (Phase 140+).
- BlitzBasic2 ted editor — copper/blitter improvements (Phase 113/114) reach the surface (~13% of editor body fills with paint after menu interaction) but ted still does not render real editable text content. Phase 119 captured vision-model analysis: menu bar and dropdowns render correctly (PROJECT/EDIT/SOURCE/SEARCH/COMPILER), but the editor body remains a flat surface with only menu-residue and status overlay paint. Root cause is deeper than line-mode blitter and copper colour cycling — likely involves blitter copy modes, sprite hardware, or specific copper waits that ted uses for its custom overlay (Phase 135).
- ASM-One Assemble/Run workflow not exercised (Phase 118) — same external-process constraint as DevPac; deferred until Phase 140+. Host-side observation of IDCMP qualifier bits is not currently possible; qualifier propagation is guarded via survival + subsequent-input responsiveness only.
- PPaint is an RTG application — its internal mode-scan loop targets P96 display IDs, not ECS modes. The ECS-path investigation (`$0x26d78`, `A4+$34f0` table) is abandoned. PPaint validation is deferred to Phase 139 (RTG app validation), where it is the flagship target.
- FinalWriter `DISABLED_AcceptDialogOpensEditorWindow` remains disabled pending Phase 139 RTG validation (FW probes screen modes in a way that requires RTG to succeed).
- Menu pixel introspection during a drag: `lxa_inject_drag` performs press → drag → release atomically, so we cannot capture screen state while a menu is open. Tests verify menu interaction via side effects (window count, app survival) instead. A future infrastructure improvement would be a non-releasing drag API or a "capture during drag" hook.
- DirectoryOpus 4 button bank now renders correctly (Phase 134). The remaining gaps: file-list navigation (requires directory reading in the lister panes), button-bank click workflows (require dopus.library command dispatch), Preferences panel interaction, and the title-bar version/memory string (rendered by DOpusRT, which is not launched in the current test setup). `commodities.library` and `inovamusic.library` are absent but tolerated by DOpus.
- CMake test sharding with hardcoded GTest filters can silently orphan newly added tests. Phase 0 audited all sharded drivers (fixing 4 orphaned GadTools tests) and added `tools/check_shard_coverage.py` as a `shard_coverage_check` CTest meta-test. **Any future test additions to a sharded driver MUST update the corresponding FILTER strings** — CI will now catch violations automatically.
- KickPascal 2 (Phase 122): editor body never clears the splash screen (HiS logo + "Pascal" graphic) so typed Pascal source is overlaid on stale pixels. Compile/run workflows are not exercised (same external-process constraint as DevPac/ASM-One; Phase 140+). Editor-body splash-clear deferred until KickPascal's deferred-paint trigger is reverse-engineered or a forced full redraw can be issued.
- Typeface (Phase 123, deferred Phase 136): font list interaction (clicking font names), preview pane rendering, and the text-entry preview widget (requires `gadgets/textfield.gadget` via `GADGETS:` assign) deferred. The startup + BGUI layout rendering is now validated (Phase 136). Deeper font-browser workflows depend on BGUI gadget class dispatch being fully functional.
- FinalWriter (Phase 124): clicking OK in the startup dialog causes FW to attempt opening a new screen mode; the two child processes spawn and immediately Exit(0), and FinalWriter terminates without ever opening the editor window. The `DISABLED_AcceptDialogOpensEditorWindow` test is preserved in source for the day Phase 129 screen-mode emulation makes the editor reachable. All editor-mode workflows deferred.

---

## Active Development

### Library Policy Cleanup (Phases 141–143)

These phases correct a longstanding policy violation: several third-party library
stubs (`req.library`, `reqtools.library`, `powerpacker.library`) were implemented
in lxa in Phase 110. This was wrong — these are not AmigaOS system libraries and
must never be implemented or emulated by lxa. Real binaries must be supplied by
the user. Similarly, `datatypes.library` is a genuine AmigaOS system library that
deserves a full, correct implementation rather than the minimal stub introduced
in Phase 136-h.

#### Phase 141: Remove third-party library stubs; wire in real binaries

**Goal**: Expunge `req.library`, `reqtools.library`, and `powerpacker.library`
stubs from the build entirely. Replace them with real binaries supplied by the
user (the same way `bgui.library` is already handled).

- Delete `src/rom/lxa_reqlib.c`, `src/rom/lxa_reqtools.c`, `src/rom/lxa_powerpacker.c`.
- Remove the corresponding `add_disk_library()` entries from `sys/CMakeLists.txt`.
- Locate or request real `req.library`, `reqtools.library`, `powerpacker.library`
  binaries and install them to `share/lxa/System/Libs/` (same pattern as bgui.library
  in `share/lxa/System/Libs/bgui.library`).
- Update all tests that previously validated "stub returns failure cleanly" to instead
  validate real library functionality (or skip if the binary is not present).
- Update `roadmap.md` Known Limitations entries that referenced stub behaviour.
- **Note**: `arp.library` is ambiguous — it straddles system and third-party territory.
  Treat it as third-party (CBM did not ship it with AmigaOS); apply the same removal
  pattern once the real binary is available.

**Test gate**: Full suite passes. No app that previously worked should regress. Apps
that depended on stub-returned failures (DevPac File→Open) should still not crash —
the real library handles the "no file selected" path natively.

#### Phase 142: datatypes.library — full implementation

**Goal**: Replace the Phase 136-h stub with a complete, RKRM-correct implementation
of `datatypes.library` per the AmigaOS 3.x NDK and RKRM: Libraries.

Key areas to implement:
- `ObtainDataType()` / `ReleaseDataType()` — type detection via magic bytes
- `NewDTObject()` / `DisposeDTObject()` — DataTypes object lifecycle
- `GetDTAttrs()` / `SetDTAttrs()` — attribute access on DT objects
- `DoDTMethod()` — method dispatch to DataTypes classes
- `DrawDTObject()` — render a DataTypes object into a RastPort
- `FindToolNode()` / `AddDTObject()` / `RemoveDTObject()` — gadget management
- `GetDTMethods()` / `GetDTTriggerMethods()` — method introspection
- DataTypes class loading: scan `SYS:Classes/DataTypes/` for `.datatype` descriptors
- Built-in type detection for common IFF formats (ILBM, 8SVX, FTXT, ANIM)

**Test gate**: Apps that use `datatypes.library` (picture viewers, sound players,
Workbench icon loading) must open DataTypes objects without crashing. Add a
`datatypes_gtest.cpp` driver covering the core lifecycle and at least one IFF type.

#### Phase 143: Startup smoke test suite

**Goal**: Catch "app crashes on startup" regressions automatically, before a
dedicated driver is written. The Typeface crash (datatypes.library NULL pointer)
went undetected because no test existed yet for Typeface.

- Add `smoke_gtest.cpp` in `tests/drivers/`: a parameterised test that, for each
  known app binary in `lxa-apps/`, loads the app, runs 100 VBlank settle iterations,
  and asserts:
  1. lxa did not abort/SIGABRT.
  2. No `PANIC` string appears in the output.
  3. No `rv=26` (immediate exit with error code) appears unless the app is known to
     do this legitimately.
- The test must be self-contained: if a particular app binary is missing, that
  parameterised case is `GTEST_SKIP()` rather than a failure.
- This gives every new app binary an automatic crash-guard the moment it is added
  to `lxa-apps/`, without waiting for a full driver phase.

**Test gate**: All currently known apps that open windows pass the smoke test. Apps
that are known to exit immediately (e.g., command-line tools) are allowlisted.

---

### Observability and Test Infrastructure

> **Philosophy**: Before sweeping through apps, ensure we can truly *see* and *experience* how they behave — both in automated CI runs and in AI-driven investigation sessions. Every gap in observability is a gap in correctness confidence.

#### Current Capabilities
- `lxa_capture_window()` / `lxa_capture_screen()` — PNG artifact capture on failure
- `lxa_read_pixel()` / pixel-count change detection — basic visual regression
- `lxa_get_gadget_count()` / `lxa_get_gadget_info()` / `LxaTest::ClickGadget()` — Intuition gadget introspection
- `lxa_wait_window_drawn()` / `LxaTest::WaitForWindowDrawn()` — event-driven UI readiness
- `tools/screenshot_review.py` — OpenRouter-backed vision model analysis for failure artifacts
- `lxa_inject_string()`, `lxa_inject_drag()`, `lxa_inject_mouse_click()` — input injection

#### Infrastructure Gaps (with phase assignments)

1. **No screen-content diffing** (Phase 132+): No way to assert "this region looks like a text label" or "this area changed between two states." A reference-image diff tool would let us write layout regression tests that survive minor color changes but catch structural regressions.

2. **No text extraction from screen** → **Phase 130**: Apps render text via ROM `Text()` calls. Adding a host-side `lxa_set_text_hook()` callback in `_graphics_Text()` would let tests assert `"About MaxonBASIC"` appeared without pixel-exact matching. The hook already has a `DPRINTF` log at that call site — the infrastructure change is small.

3. ~~**No window/screen event log** → **Phase 131**~~ **DONE (v0.9.5)**: Circular event buffer with `lxa_drain_intui_events()` implemented.

4. ~~**No menu-state introspection** → **Phase 131**~~ **DONE (v0.9.5)**: `lxa_get_menu_strip()` traverses the emulated `MenuStrip` linked list; menu names, enabled/checked state, and geometry exposed to tests.

5. **No clipboard / string output capture** (Phase 140+): Apps writing to AmigaDOS stdout or the clipboard have no way to expose output to the host test. A `lxa_capture_dos_output()` API would allow asserting compiler output, BASIC program results, etc.

6. **Vision model loop not automated**: `screenshot_review.py` is a manual tool. No test-phase hook automatically invokes it when a pixel assertion fails. Remains a developer investigation tool; automation deferred.

7. **No audio state introspection**: Apps using audio.device have no observable test output. No tests require this yet.

---

## Short-Term: Architecture + Observability (Phases 128–131 ✅ → Phases 132+)

### Phase 128: Display pipeline optimization

- Dirty-region tracking: skip unchanged scanlines in `display_update_planar()`.
- SIMD planar-to-chunky: use SSE2 for the inner loop of the bit-plane interleave.
- Eliminate redundant SDL texture uploads: batch multiple `display_refresh_all()` calls within a single VBlank.

**Test gate**: Same as Phase 127. Visual regression tests (pixel-level) must not change output.

---

### Phase 129 (partial, v0.9.3): Screen-mode emulation — ROM side done, PPaint still blocked

> **Status**: ROM-side screen-mode virtualization is complete and shipped in v0.9.3. All infrastructure changes in the original plan landed. However, **PPaint still exits rv=26** because its internal mode-scan loop depends on a private table whose fourth byte is never populated — that block is orthogonal to the ROM APIs we improved. See the Known Limitations entry above for the deferred next-steps (PPaint binary disassembly, memory-write watchpoint on `A4+$34f0`, PAL/NTSC mismatch investigation). FinalWriter's `DISABLED_AcceptDialogOpensEditorWindow` remains disabled pending this investigation.

**Landed in v0.9.3** (permanent):
1. `g_known_display_ids[]` expanded from 6 to 36 entries (DEFAULT/NTSC/PAL × 12 ECS modes).
2. `GetDisplayInfoData(DTAG_DIMS)` returns realistic `Min`/`MaxRasterWidth` ranges (HIRES 320–1280, LORES 160–640).
3. `BestModeIDA()` honors `BIDTAG_DIPFMustHave`/`MustNotHave` and `BIDTAG_DesiredWidth`/`Height`.
4. `OpenScreenTagList()` virtualization: `SA_DisplayID` mapping, WBENCHSCREEN accepted via CUSTOMSCREEN trick, auto-HIRES for width ≥ 640, screen font opened via `OpenFont()`.
5. `LockIBase()` correctly returns 0 (previously 1 → caused apps to hang on UnlockIBase).
6. `FindDisplayInfo(INVALID_ID)` correctly rejects (not virtualized).
7. `graphics_virtualize_display_id()`: unknown-but-plausible IDs map to closest physical mode (Wine strategy).
8. Diskfont registers fonts by basename so plain OpenFont() can find them after a pathful OpenDiskFont().
9. GfxBase `DisplayFlags = PAL|REALLY_PAL`, `VBlankFrequency = 50`, `PowerSupplyFrequency = 50`.
10. `display_viewport` test expanded with HAM/EHB/LACE property-flag coverage, LORES/HIRES DTAG_DIMS range checks, virtualization, and BestModeIDA MustNotHave tests.

**Test gate** (met): All 66 test drivers pass. No warnings. `graphics_gtest` includes new Phase 129 coverage.

**Deferred** (to Phase 139 — RTG app validation):
- PPaint validation: PPaint is an RTG application; its mode-scan loop targets P96 display IDs. Deferred to Phase 139 where RTG infrastructure is in place.
- FinalWriter `AcceptDialogOpensEditorWindow` re-enablement: depends on RTG screen-mode support landing in Phase 137–138.
- Visual regression tests that require PPaint's or FinalWriter's editor canvas to be reachable.

---

### Phase 129 (original plan — for reference)

> **Highest-value architectural improvement. Directly unblocks PPaint editing UI, FinalWriter editor, and any future app that probes display modes.**

**Root cause identified**: `g_known_display_ids[]` contains only 6 entries (bare LORES/HIRES + NTSC/PAL variants). `GetDisplayInfoData(DTAG_DIMS)` returns `MinRasterWidth == MaxRasterWidth` (no size range), causing screen-mode probes like CloantoScreenManager to conclude the mode is unsuitable. `BestModeIDA()` ignores `BIDTAG_DIPFMustHave`/`MustNotHave` flags entirely.

**What to fix** (most items landed in v0.9.3; PPaint editor-reach deferred):
1. Extend `g_known_display_ids[]` with the full ECS/AGA standard mode set from `<graphics/displayinfo.h>`: LACE variants, SUPER_HIRES, HAM, EHB, EXTRAHALFBRITE — at minimum the modes that real ECS software probes for. **[DONE v0.9.3]**
2. `GetDisplayInfoData(DTAG_DIMS)`: return proper `MinRasterWidth`/`MaxRasterWidth` ranges (e.g., 320–1280 for HIRES), not pinned equal values. **[DONE v0.9.3]**
3. `BestModeIDA()`: respect `BIDTAG_DIPFMustHave`/`MustNotHave` flags before returning a mode ID. **[DONE v0.9.3]**
4. `OpenScreenTagList()` with `SA_DisplayID`: when an app requests a valid mode that maps to our physical LORES/HIRES display, accept and virtualize it. **[DONE v0.9.3]**
5. Add `screenmode_gtest.cpp` driver asserting: PPaint reaches its main editing window; FinalWriter's `DISABLED_AcceptDialogOpensEditorWindow` can be re-enabled and passes. **[DEFERRED — PPaint still blocked by internal mode-scan loop; see Known Limitations]**

**Test gate** (original): PPaint no longer exits rv=26; FinalWriter editor opens after OK; `DISABLED_AcceptDialogOpensEditorWindow` renamed back to active.

**Test gate met** (revised for v0.9.3): ROM-side virtualization complete and fully tested. App-side blocker documented for future investigation.

---

### ~~Phase 130: Text() interception — semantic test assertions~~ ✅ DONE (v0.9.4)

Implemented `lxa_set_text_hook()` / `lxa_clear_text_hook()` via the `emucall` mechanism (`EMU_CALL_GFX_TEXT_HOOK = 2051`). ROM `_graphics_Text()` fires the hook on every text render call. Tests in `api_gtest.cpp` (using `SYS:IntuiText` sample), `dopus_gtest.cpp`, and `kickpascal_gtest.cpp` all pass. Note: DOpus renders text character-by-character — tests concatenate captured chars before asserting. `finalwriter_gtest.cpp` update deferred until Phase 129 editor-open work is revisited.

---

### ~~Phase 131: Window/screen event log + menu introspection~~ ✅ DONE (v0.9.5)

Implemented circular event log (256 entries) in new `lxa_events.c` (compiled into both `lxa` binary and `liblxa`). `lxa_push_intui_event()` is called from `lxa_dispatch.c` at `OpenWindow`, `CloseWindow`, `OpenScreen`, `CloseScreen` sites. `lxa_drain_intui_events()`, `lxa_intui_event_count()`, and `lxa_reset_intui_events()` exposed in `lxa_api.h`. Menu introspection via `lxa_get_menu_strip()` / `lxa_get_menu_info()` traverses emulated Intuition `MenuStrip` linked list using correct NDK struct offsets (`Window.MenuStrip` at offset 28, `Menu.FirstItem` at offset 18, `MenuItem.Flags` ITEMENABLED = 0x0010). Tests in `simplemenu_gtest.cpp` (event log + Project menu + 4 items), `maxonbasic_gtest.cpp` (German menu titles), `devpac_gtest.cpp` (Project/Edit/Program menus), and `api_gtest.cpp` (unit + live tests) all pass. 66/66 tests pass.

---

## Mid-Term: Rendering Fidelity (Phases 132–136)

### Phase 132: SMART_REFRESH full backing store (second attempt) ✅ COMPLETE

The first attempt (Phase 111) was not actually reverted — the backing-store implementation in `lxa_layers.c` (`SaveToBackingStore`, `RestoreBackingStore`, `CreateObscuredClipRect`, `RebuildClipRects` restore→free→recompute→save flow) and the CR-aware rendering primitives in `lxa_graphics.c` were left in place. The "regression" was masked by a defensive workaround in `DamageExposedAreas()` that skipped damage for `IS_SMARTREFRESH(layer)` to suppress spurious REFRESHWINDOW messages.

**Approach taken**:
1. Wrote `samples/intuition/backingstoretest.c` driving the dialog-over-window stamping scenario through three deterministic stages (pattern → dialog open → dialog close).
2. Wrote `tests/drivers/backingstore_gtest.cpp` with 4 tests: `Stage0PatternRenders`, `Stage1DialogObscuresPattern`, `Stage2BackingStoreRestoresPattern`, `Stage2RestoredAreaSweep` — verifying via `lxa_get_window_count()` and `ReadPixel()` that obscured pen-2 pixels are restored to the correct colour after the dialog closes.
3. Confirmed all 4 tests pass with Phase 111's implementation: backing store correctly saves and restores SMART_REFRESH pixels.
4. Removed the `IS_SMARTREFRESH` skip in `DamageExposedAreas()` (around line 990 of `lxa_layers.c`). Full suite re-run: **67/67 tests pass** in ~133 sec wall time, including the previously-fragile SIGMAth2 and requester tests.

**Result**: SMART_REFRESH backing store is fully functional. Apps now receive the correct IDCMP_REFRESHWINDOW messages on uncover (matching real Amiga behaviour) without losing pixels, because backing store has already restored the visible content. The "dialog stamping" known limitation is resolved.

- [x] Regression tests for backing store written *before* touching ROM code
- [x] Layer-level backing store for `WFLG_SMART_REFRESH` validated end-to-end
- [x] Phase 111 `DamageExposedAreas()` skip removed; full suite still green

---

### Phase 133: Menu double-buffering (ASM-One / MaxonBASIC flickering) ✅ COMPLETE

ASM-One and MaxonBASIC exhibited pixel flicker during menu drags (documented in known limitations since Phase 118/116). Root cause confirmed: the menu bitmap was drawn and blit to screen without atomic compositing — each character cell, separator, and highlight update immediately propagated to the screen, producing visible in-progress frames during drag.

**Approach taken**:
1. Added persistent compose state (`g_menu_compose_bm`, `g_menu_compose_w/h/depth`) in `lxa_intuition.c` with `_menu_ensure_compose_bitmap()` sizing + `_menu_init_compose_rp()` helpers.
2. Refactored `_render_menu_bar()` to render the entire menu bar into the compose BitMap at origin (0,0), then `BltBitMap()` the completed frame to the screen RastPort.
3. Refactored `_render_menu_items()` identically: main menu chain + optional sub-menu each rendered at compose origin and BltBitMap'd to their final screen positions.
4. Cleanup hooks in `_intuition_discard_menu_runtime_state()` and `_exit_menu_mode()` free the compose BitMap when menu mode ends.
5. Upgraded `ZMenuFlickerCheck` in `asm_one_gtest.cpp` from the old content-floor guard to a ±5% pixel-stability assertion with a throwaway first drag (ASM-One defers editor repaint until first interaction, so the first drag legitimately grows pixel count).
6. Added `ZMenuDragPixelStability` test in `maxonbasic_gtest.cpp` guarding the editor area against menu residue (observed: 297 → 297 pixels, exact).
7. Cycle budgets bumped in `maxonbasic_gtest.cpp`, `gadtoolsmenu_gtest.cpp`, `blitzbasic2_gtest.cpp` to accommodate the ~2× traversal cost of compose→blit (each byte now passes through m68k memory hooks twice).
8. Rewrote `YEditorSurfaceShowsContentAfterMenuInteraction` in `blitzbasic2_gtest.cpp`: its lower bound (non_grey > 100) relied on flicker residue bleeding into the editor area, which no longer occurs (this is correct Amiga behaviour).

**Result**: ASM-One observed flicker 7978 → 7954 (0.3%) post-fix; MaxonBASIC editor area exact-stable across drag. Full suite 67/67 pass in ~163s wall time.

- [x] Identify the menu render path in `lxa_intuition.c`
- [x] Add off-screen buffer; render menu items to it; atomic blit to display
- [x] Update `ZMenuFlickerCheck` in `asm_one_gtest.cpp` to pixel-stability ±5% assertion with throwaway first drag
- [x] Verify MaxonBASIC menu drag no longer shows residue artifacts (new `ZMenuDragPixelStability` test)

---

### ~~Phase 134: DirectoryOpus button bank rendering~~ ✅ COMPLETE (v0.9.8)

Root cause: the Phase 121 test settle budget (150 VBlank iterations / 7.5M cycles) ended before dopus_task processed its VBlank-driven timer events that trigger the main button bank render. DOpus renders the button bank ~400 VBlanks after startup via its dopus.library init sequence. Diagnozed using the Phase 130 text hook: only the fixed B/R/S/A cluster and ?/E/F/C/I/Q toolbar appeared in the 150-iteration window; bumping to 600 iterations revealed Copy/Move/Rename/Makedir/Hunt/Run/Comment/Read/Assign/Search etc.

**Changes**:
- `tests/drivers/dopus_gtest.cpp`: bumped `DOpusTextHookTest` and `DOpusTest::SetUp()` settle loops from 150 → 600 VBlank iterations; upgraded `TextHookCapturesKnownDOpusLabels` to assert Copy/Move/Rename labels are present.
- `src/rom/exec.c`, `src/rom/lxa_graphics.c`, `src/lxa/lxa_dos_host.c`: cleaned up all diagnostic `LPRINTF(LOG_INFO, ...)` calls from the Phase 121–133 investigation sessions.
- `src/rom/lxa_dos.c`: yield in `_dos_CreateNewProc` retained (gives new processes a timeslice, matching real Amiga scheduler behaviour).

- [x] Diagnose with text hook + Phase 131 event log
- [x] Settle budget increased so dopus_task completes button bank render
- [x] `dopus_gtest.cpp`: `TextHookCapturesKnownDOpusLabels` asserts Copy/Move/Rename labels

---

### Phase 135: BlitzBasic 2 ted editor deep dive ✅ (v0.9.9)

**Original hypothesis (invalidated)**: blitter copy modes, sprite hardware, or copper waits were missing. Instrumentation proved BB2 emits **zero** blitter and zero copper writes during startup — the hypothesis was wrong.

**Actual root cause**: BB2 opens two windows on its custom 640x256x2 screen:
1. The main editor window (BORDERLESS, full screen).
2. An "About BlitzBasic 2" requester with garbage requested coords (LE=32614, TE=32729, 292x75) containing the only "OKEE DOKEE" button the user can click to dismiss it.

BB2 then `WaitPort()`s on the requester's UserPort, blocking all editor activity until the user dismisses the requester. The pre-existing off-screen clamp in `_intuition_OpenWindow` clipped the requester to a single pixel at (639,255), making the dismiss button unreachable. BB2 hung waiting forever, and the editor stayed blank.

**Fixes applied**:
1. `src/rom/lxa_intuition.c` `_intuition_OpenWindow` (~lines 9555-9605): when requested window position would place the window mostly off-screen, **recentre** on the screen instead of clipping to the edge. After fix, the About requester appears centered at (174,90) with full border, version text and visible "OKEE DOKEE" button (verified via `tools/screenshot_review.py`).
2. `src/rom/lxa_intuition.c` `_intuition_ProcessInputEvents` case 3 (~line 8106): RAWKEY events now route to `IntuitionBase->ActiveWindow` (or the global active window) instead of the window under the mouse pointer, matching real Amiga semantics. BB2 expects keyboard input at the active window once the requester is dismissed.

**Test gate**: `tests/drivers/blitzbasic2_gtest.cpp` `StartupOpensVisibleIdeWindow` flipped from upper-bound (`<10` non-grey pixels = "documented as broken") to lower-bound (`>1000`) proving real UI renders. All 10 BB2 driver tests pass.

- [x] Confirmed via instrumentation that BB2 emits zero blitter/copper activity (original hypothesis invalidated)
- [x] Identified the About-requester WaitPort hang as the real cause
- [x] Implemented "recenter when off-screen" in `_intuition_OpenWindow`
- [x] Implemented Amiga-correct RAWKEY routing to ActiveWindow
- [x] Flipped `StartupOpensVisibleIdeWindow` from upper-bound to lower-bound assertion
- [x] All BB2 driver tests pass; About requester + PROJECT menu render correctly

---

### Phase 136: bgui.library → Typeface ✅ COMPLETE (v0.9.10)

The real `bgui.library` v41 binary is available at `others/bgui/bgui/libs/bgui.library` and is already installed to `share/lxa/System/Libs/bgui.library` via the static install rule. Typeface bundles its own `bgui.library` in `Typeface/Libs/`; lxa's auto-prepend of the program-local `Libs/` directory to `LIBS:` means it is found without any extra assign. BGUI gadget binaries (`bgui_bar.gadget`, `bgui_layoutgroup.gadget`, `bgui_palette.gadget`, `bgui_popbutton.gadget`, `bgui_treeview.gadget`) copied to `share/lxa/System/Libs/gadgets/`.

- [x] Verify `bgui.library` loads cleanly under lxa: `NoPanicDuringStartup` asserts no PANIC or rv=26 exit
- [x] Copy gadget binaries from `others/bgui/bgui/libs/gadgets/` to `share/lxa/System/Libs/gadgets/`
- [x] Add `typeface_gtest.cpp` driver: `StartupOpensWindow` asserts Typeface reaches a non-exit state (window visible); `WindowGeometryIsPlausible` guards geometry; `IdleTimeStability` guards survival
- [x] Use text hook (Phase 130) to observe rendering: `TextHookCapturesSomeText` asserts BGUI layout rendered at least some text labels through `_graphics_Text()`

---

## RTG: Retargetable Graphics (Phases 137–139)

> **Strategic pivot**: lxa's display strategy moves from ECS-era planar modes to RTG via Picasso96. Phases 137–139 deliver the full RTG stack — display backend, P96 library, and app validation. All three phases must complete before the next long-term block.

### Phase 137: RTG display foundation

> **Unlocks: RTG screen opening, true-color BitMaps, SDL2 32-bit rendering pipeline.**

**Sub-problem 1 — `AllocBitMap` depth extension** (`lxa_graphics.c`):
Remove the depth 1–8 clamp. For depths > 8, allocate a single contiguous chunky buffer (width × height × bytes-per-pixel) instead of separate bit-planes. Mark such bitmaps with a new `BMF_RTG` internal flag so the rest of the pipeline can distinguish planar from chunky. `GetBitMapAttr(BMA_DEPTH)` must return the real depth.

**Sub-problem 2 — SDL2 backend extension** (`display.c`):
`display_open()` and `display_update_*` currently assume paletted planar. Add `display_update_rtg()` accepting a raw 32-bit RGBA buffer uploaded directly to an `SDL_PIXELFORMAT_RGBA32` texture. Existing planar pipeline is untouched.

**Sub-problem 3 — RTG display IDs in the mode database** (`lxa_graphics.c`):
Add P96 monitor/mode IDs to `g_known_display_ids[]`: at minimum a `P96_MONITOR_ID` base with `RGBFB_R8G8B8A8` (32-bit) and `RGBFB_R5G6B5` (16-bit) variants. `GetDisplayInfoData(DTAG_DIMS)` returns host-resolution ranges for RTG IDs. `OpenScreenTagList()` with an RTG depth allocates a chunky screen bitmap and calls `display_open()` with the RTG depth.

- [ ] Extend `AllocBitMap` to support depth > 8; add `BMF_RTG` internal flag
- [ ] `GetBitMapAttr(BMA_DEPTH/WIDTH/HEIGHT/BYTESPERROW)` correct for RTG bitmaps
- [ ] `display_update_rtg()` in `display.c` / `display.h`; `SDL_PIXELFORMAT_RGBA32` texture path
- [ ] P96 monitor/mode IDs in `g_known_display_ids[]`; `GetDisplayInfoData` returns RTG ranges
- [ ] `OpenScreenTagList()` with RTG depth allocates chunky screen bitmap
- [ ] New `rtg_gtest.cpp`: `AllocBitMap(640,480,32,BMF_CLEAR,NULL)` + `GetBitMapAttr` returns 32; `OpenScreenTagList` with P96 mode ID does not crash

**Test gate**: `rtg_gtest` passes; all 66 existing drivers unchanged.

---

### Phase 138: Picasso96API.library core

> **Unlocks: P96-aware apps can open the library and call core pixel operations.**

New disk library (`src/rom/lxa_p96.c`, installed to `share/lxa/System/Libs/Picasso96API.library`) following the pattern in §6.9 of AGENTS.md.

**Core functions**:

| Function | Purpose |
|---|---|
| `p96AllocBitMap(w,h,depth,flags,friend,rgbformat)` | Allocate RTG bitmap in a specific pixel format |
| `p96FreeBitMap(bm)` | Free it |
| `p96GetBitMapAttr(bm,attr)` | Query RGBFORMAT, WIDTH, HEIGHT, BYTESPERROW, etc. |
| `p96WritePixelArray(ri,sx,sy,bm,dx,dy,w,h)` | Blit pixels from a RenderInfo to a BitMap |
| `p96ReadPixelArray(ri,dx,dy,bm,sx,sy,w,h)` | Read pixels back |
| `p96LockBitMap(bm,tags)` | Lock for direct access; returns RenderInfo |
| `p96UnlockBitMap(bm,ri)` | Unlock |
| `p96GetModeInfo(id,info)` | Query mode properties (resolution, depth, pixel format) |
| `p96BestModeIDA(tags)` | Find best matching RTG mode |
| `p96RequestModeID(tags)` | Mode requester (stub: returns best mode, no UI) |

Also: thin `cybergraphics.library` compatibility shim wrapping the above under CGX names (`GetCyberMapAttr`, `LockBitMapTags`, `WriteLUTPixelArray`, etc.) for apps that probe for CGX first.

**RenderInfo** (`struct RenderInfo { APTR Memory; WORD BytesPerRow; WORD pad; RGBFTYPE RGBFormat; }`) maps directly onto the Phase 137 chunky BitMap representation.

- [ ] `src/rom/lxa_p96.c` with all ten core functions
- [ ] `cybergraphics.library` shim disk library
- [ ] Register both in `sys/CMakeLists.txt` via `add_disk_library()`
- [ ] `p96_gtest.cpp`: unit tests for each function; startup test confirms `OpenLibrary("Picasso96API.library",0)` returns non-NULL

**Test gate**: `p96_gtest` passes; at least one target app opens the library without exiting early.

---

### Phase 139: RTG app validation

> **Unlocks: PPaint editing canvas reachable; FinalWriter editor reachable; first productivity app fully validated.**

Three targets — fixes discovered during validation are committed as part of this phase (matching Phase 108-d convention):

**PPaint** — flagship. PPaint's mode-scan loop should now be populated via P96 mode enumeration. Extend `ppaint_gtest.cpp` with a test asserting the editor canvas is reachable.

**FinalWriter** — re-enable `DISABLED_AcceptDialogOpensEditorWindow`; test gate: clicking OK opens the editor window.

**One new productivity app** — add `wordsworth_gtest.cpp` (or Amiga Writer): app reaches main editor window; Phase 130 text hook confirms editor content renders.

- [ ] Diagnose PPaint's P96 probe path; implement any missing P96 functions or mode IDs
- [ ] Re-enable `DISABLED_AcceptDialogOpensEditorWindow` in `finalwriter_gtest.cpp`
- [ ] Add new productivity app driver (Wordsworth or Amiga Writer)
- [ ] All 66 existing drivers continue to pass

**Test gate**: PPaint editor reachable; FinalWriter editor reachable; new productivity app reaches main UI.

---

## Long-Term: Extended Coverage (Phase 140+)

### Phase 140: External process emulation

DevPac Assemble, ASM-One Assemble, BlitzBasic Run, KickPascal Compile/Run all require this. Needed: AmigaDOS `CreateProc()` / `Execute()` launching a real m68k subprocess within the emulated environment, with stdout piped back to the host.

This is the deepest deferred item. Prerequisites: Phase 131 (event log to detect process lifetime), Phase 130 (capture output), and a stable foundation from all prior phases.

---

## Far Future: GUI Toolkits + CPU Evolution

### Phase 141 (tentative): MUI library stubs

MUI (`MUI:Libs/muimaster.library`) is a disk library — not reimplemented in ROM. The phase work is ensuring lxa's BOOPSI infrastructure (icclass/modelclass/gadgetclass, utility.library tag machinery) is solid enough that a real `muimaster.library` binary can open and function. Test gate: at least one MUI-based app opens its main window.

### Phase 142 (tentative): ReAction / ClassAct stubs

ReAction (AmigaOS 3.5/3.9 GUI toolkit) runs as disk-provided class files (`SYS:Classes/reaction/`). The phase ensures lxa's BOOPSI layer and Intuition class dispatch are complete enough to host ReAction classes. This directly serves the development-platform vision: once Phase 142 lands, lxa becomes a viable host for developing and testing ReAction-based Amiga software on Linux. Test gate: at least one ReAction app opens its main window.

### CPU Emulation Evolution

Musashi (MIT, C89, interpreter) is adequate for correctness but limits throughput. No clean permissively-licensed x86-64 68k JIT exists today. Options assessed: Moira (MIT, no 68030 yet), UAE JIT (GPL, framework-entangled), Emu68 (MPL-2.0, ARM-only), Unicorn/QEMU (GPL).

**Recommendation**: Stay on Musashi. Host-side overhead dominates (~95% of wall time per Phase 126 estimate). Revisit when Moira adds 68030 support or host optimizations (Phases 127/128) are exhausted.

- **Phase 143** (tentative): CPU core evaluation — re-evaluate Moira 68030 support; prototype if available; assess whether 68020 mode suffices for all tested apps.
- **Phase 144** (tentative): Production readiness assessment — define use-case targets; set performance baselines; decide on JIT investment.

---

## Dependency Graph (Critical Path)

```
Phase 125 (decompose lxa.c) ──► Phase 126 (profiling)
                                                                │
                                               ┌────────────────┤
                                               ▼                ▼
                                        Phase 127/128      Phase 129
                                        (perf opts)    (screen-mode emulation)
                                                            │
                                                    ┌───────┴────────┐
                                                    ▼                ▼
                                            Phase 130            Phase 131
                                          (Text hook)        (event log +
                                                             menu introspect)
                                                │                    │
                                         ┌──────┴──────┐            │
                                         ▼             ▼            ▼
                                   Phase 132      Phase 134    Phase 133
                                 (SMART_REFRESH)  (DOpus)     (menu dbl-buf)
                                                      │
                                               Phase 135 (BlitzBasic ted)
                                                      │
                                               Phase 136 (bgui/Typeface)
                                                      │
                                    ┌─────────────────┴──────────────────┐
                                    ▼                                     ▼
                             Phase 137                             Phase 140
                          (RTG display foundation)            (ext processes)
                                    │
                             Phase 138
                          (Picasso96API.library)
                                    │
                             Phase 139
                          (RTG app validation)
                                    │
                          Phase 141/142
                        (MUI / ReAction stubs)
                                    │
                          Phase 143/144
                         (CPU eval / prod ready)
```

---

*Each phase must include updated GTest host-side drivers under `tests/drivers/`, regression assertions, and saved failure artifacts. Use `tools/screenshot_review.py` during investigation, then translate findings into deterministic pixel/geometry assertions for CI.*
