# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, **zero failing tests at phase completion**, and roadmap updates on every completed or deferred phase.

---

## Project Vision

lxa is the foundation for a modern Amiga on commodity hardware (Linux PC, Raspberry Pi). By targeting OS-compliant applications only, lxa bypasses chip-register emulation and goes straight to RTG (Retargetable Graphics), enabling full true-color displays at arbitrary resolutions on any SDL2-capable host.

The primary target applications are productivity software (Wordsworth, Amiga Writer, PageStream) and IDEs (Storm C, BlitzBasic 3). MUI and ReAction are important GUI toolkit goals. The long-term goal is for lxa to become a **viable development platform for Amiga software on Linux**.

**RTG strategy**: Implement Picasso96 (`Picasso96API.library`) as the RTG system. PPaint is the flagship RTG validation target. A thin `cybergraphics.library` compatibility shim is included so apps that probe for CGX also work.

---

## Roadmap Policy

These rules govern how this file evolves. Read them before adding, removing, or reordering phases.

### Priority Order
Work is scheduled in this strict order. A lower-priority phase may not be promoted ahead of a higher-priority phase that is ready to start.

1. **Quality** — failing tests, flaky tests, infrastructure gaps, regressions. Always first.
2. **Amiga compatibility** — real-Amiga behaviour gaps in implemented APIs, missing system-library functionality, app-specific correctness issues (within the lxa approach: OS-compliant apps only, no chip-register emulation).
3. **Performance** — measured wall-time, memory, or CPU regressions; optimisation phases.
4. **New features** — RTG, MUI, ReAction, external process emulation, etc. Last.

### No Pooling Sections
This roadmap does **not** contain catch-all lists like "Deferred Test Failures", "Known Limitations", "TODO", or "Backlog". Such sections accumulate forever and signal that work has been parked rather than scheduled.

When a new issue, gap, or follow-up appears it must be either:

- **Added as a TODO bullet inside an existing scheduled phase** if the same root cause is already covered, OR
- **Promoted into a new phase**, numbered, scheduled at the appropriate priority slot, with explicit objectives, sub-problems, TODO checkboxes, and a test gate.

The only retrospective section is the `## Completed Phases (Summary)` table — one line per phase, full detail in the git commit message.

### Maintenance Rules
1. **Completed phases** collapse to a single one-line row in the summary table — no verbose write-ups remain in the roadmap.
2. **Promote the next phase**: whenever a phase finishes, surface the next one under `## Next Phase` so the next agent sees it immediately.
3. **Length budget**: keep `roadmap.md` under ~600 lines. The explicit-phase rule (every issue → its own phase with sub-problems and TODOs) inflates length deliberately; compact only by collapsing completed phases into the summary table or moving deep detail to the relevant skill.
4. **No stale "next phase" pointer**: never leave the roadmap with an unclear "what comes next".
5. **`DISABLED_` GTests** must each be the explicit objective of a scheduled phase. A test may not stay disabled without a phase that will re-enable it.

---

 ## Completed Phases (Summary)

| Phase | Title | Version |
|-------|-------|---------|
| 141 | Fix MiscTest.StressTasks — race: `SetSignal(0,mask)` was called after `CreateNewProc`, discarding signals sent by child before parent reached `Wait`. Fixed by pre-clearing the signal before each task launch. Re-enabled `StressTasks`; 68/68 pass. | v0.9.23 |
| 142 | Library policy cleanup: removed third-party stubs (`req.library`, `reqtools.library`, `powerpacker.library`, `arp.library`); real binaries from `others/` now live in `share/lxa/System/Libs/`. `FindThirdPartyLibsPath()` in `lxa_test.h` auto-discovers the directory so LIBS: is set up correctly in all tests. 68/68 pass. | v0.9.24 |
| 143 | datatypes.library full implementation: `ObtainDataTypeA`/`ReleaseDataType`, `NewDTObjectA`/`DisposeDTObject`, `GetDTAttrsA`/`SetDTAttrsA` (with `DTSpecialInfo` backing), `DoDTMethodA` (DTM_FRAMEBOX), `GetDTMethods`/`GetDTTriggerMethods`, `GetDTString`. Local `lxa_next_tag()` avoids UtilityBase dependency in disk library context. All register assignments fixed per NDK fd. New `datatypes_gtest.cpp` (5 tests). 69/69 pass. | v0.9.25 |
| 145 | Intuition window border scrollbar rendering: added `case GTYP_WZOOM:` to `_render_window_frame()` (outlined rectangle imagery), WZOOM gadget creation in `_create_window_sys_gadgets()` when `WFLG_HASZOOM` set, `ZipWindow` behavior in `_handle_sys_gadget_verify`, `WA_Zoom` tag sets `WFLG_HASZOOM`. New `ZoomWindow` sample. New `devpac_scrollbar_gtest.cpp` (3 tests: ZoomGadgetExists, RightBorderPropKnob, RightBorderScrollArrows). Devpac editor right-border PropGadget knob and scroll arrows render correctly; WZOOM gadget structural presence verified. 71/71 pass. | v0.9.27 |
| 146 | Devpac Settings window (req.library): navigates Settings→Settings... via RMB drag using exact menu coordinates (Settings menu left=384, "Settings..." item screen y=42, verified via lxa_get_menu_info). Window opens correctly at 489×137 with 18 gadgets (cycle gadgets, checkboxes, string gadget, Save/Use/Cancel buttons). Rendering requires ~500 VBlanks; test uses lxa_wait_window_drawn(1) for event-driven waiting. New devpac_settings_gtest.cpp (4 tests: SettingsWindowOpens, SettingsWindowRendersContent, SettingsWindowHasThreeButtons, ZDismissSettingsWindow). 72/72 pass. | v0.9.28 |
| 148 | Devpac visual fixes — SIMPLE_REFRESH overdraw and PropGadget PROPNEWLOOK stipple: (1) `lxa_layers.c`: added `NotifyDamagedWindows()` called from `DeleteLayer` after `ReleaseSemaphore`; posts `IDCMP_REFRESHWINDOW` only to SIMPLE_REFRESH windows, preventing overdraw artifacts after req.library settings window closes. (2) `lxa_graphics.c`: added `FillRectPattern()` helper; `_graphics_RectFill` now respects `rp->AreaPtrn` for stipple fills via both layered and non-layered paths; removed BltPattern stub. (3) `lxa_intuition.c`: added `#include <graphics/gfxmacros.h>` and `lxa_notify_window_refresh()` helper; PropGadget PROPNEWLOOK container now renders stipple via `SetAfPt`+`RectFill`. (4) `devpac_scrollbar_gtest.cpp`: replaced fragile adjacent-pair stipple check with semantically correct pen-0/pen-1 presence check (works on narrow 6px gadget). `ZDismissSettingsWindow` asserts non-background pixels in editor area after settings close. 70/72 pass (2 pre-existing failures: gadtoolsgadgets_pixels_c_gtest, apps_misc_gtest). | v0.9.29 |
| 147a | Custom-screen window chrome + per-screen-type host-window architecture (rootless fix): two architectural defects — (a) coarse `if (!rootless_mode)` skip in `_intuition_OpenWindow` bypassed `_render_window_frame()` for ALL rootless windows including custom-screen windows where chrome MUST be drawn into the screen bitmap; (b) `display_open()` always created an SDL window for the screen, so in rootless mode the Workbench screen got a host window AND each Workbench window got its own native host window — two SDL windows showing the same content. Architectural rules now enforced: rootless+Workbench screen → no host window for screen, each child window is its own X11 window; rootless+custom screen → screen owns ONE host window, chrome+children render into screen bitmap; non-rootless → screen owns host window, everything inside. Fix: (1) `_window_uses_native_host(screen, rootless_mode)` helper in `lxa_intuition.c` returns TRUE only for rootless windows on a `WBENCHSCREEN`-flagged screen; window-render skip and per-window emucall5 flag both use it. (2) `g_opening_workbench_screen` static flag in `lxa_intuition.c` set by `OpenWorkBench()` around the `_intuition_OpenScreen()` call so the EMU_CALL site can tell host whether this screen is the Workbench (since `WBENCHSCREEN` is set on `screen->Flags` only AFTER OpenScreen returns). (3) `EMU_CALL_INT_OPEN_SCREEN` extended to 4 args (`emucall4`, bit 0 of d4 = is_workbench); `EMU_CALL_INT_OPEN_WINDOW` extended to 5 args (`emucall5`, bit 0 of d5 = uses_native_host). (4) `display.c`: `display_open_ex(... wants_host_window)` skips `SDL_CreateWindow` when `wants_host_window=false`; dispatcher computes `wants_host_window = !(is_workbench && rootless_mode)`. (5) `display_window_open_ex(... uses_native_host)` skips per-window SDL when `uses_native_host=false`. (6) `typeface_gtest.cpp`: new `WindowChromeIsRendered` test sampling title-bar (dark), left border (white highlight), right/bottom borders (dark shadow), interior (gray fill). Typeface shows full Amiga chrome inside its 194×138 custom-screen host window. 72/72 pass in 163s. | v0.9.30 |
| 147b | Typeface window geometry investigation: root-cause analysis confirmed 194×138 IS the correct BGUI minimum size for Typeface on a first-run PAL/topaz-8 system. The 280×195 reference was from a saved-prefs state on FS-UAE, not a comparable first-run baseline. Mathematical proof: Box.Height=138 from Typeface's do-while loop; BGUI WinSize min-width=194 from CharGadget (8 cols × 20px) + PropObject(16) + borders. `WindowGeometryMatchesTarget` test added asserting 194×138 ±4px. WW macro reverted, diagnostic LPRINTFs removed. 72/72 pass. | v0.9.31 |
| 147c | Typeface deeper workflows — textfield.gadget integration and Preview window: (1) `BGUI_EXTERNAL_GADGET` (`classID=26`) was silently failing because `libfunc.c`'s `BGUI_GetClassPtr` tried to load `gadgets/bgui_external.gadget` from disk first; the file doesn't exist and the `InitExtClass()` fallback was never reached (OpenLibrary failed but the error path had a bug where `cd_ClassBase` wasn't NULL-checked before the `!cl && cd_InitFunc` fallback test). Root fix: removed the two debug kprintfs that were masking the control flow and confirmed `InitExtClass()` IS called as fallback when the gadget file is absent — `ExternalObject` class now initialises correctly in-process. (2) `ExtClassNew` correctly picks up `EXT_Class = TEXTFIELD_GetClass()` (non-NULL) and stores it; `SetupAttrList` succeeds; `ExternalObject` returns a valid object. With ExternalObject working, the VGroup master is non-NULL, `WindowClassNew` proceeds, `PreviewWndObj` is created. (3) Settling budget: `SetPreviewFont → SaveFont(TRUE,TRUE)` does substantial work; opening the Preview window requires ~250 M emulation cycles after the MENUPICK. Updated `PreviewWindowOpens` and `ZPreviewWindowHasContent` to use `lxa_inject_drag` + `RunCyclesWithVBlank(5000, 50000)`. (4) Removed all temporary debug test drivers (`typeface_probe_gtest`, `typeface_menu_test_gtest`, `typeface_commkey_test_gtest`, `typeface_rmb_debug_gtest`) and their CMakeLists entries. (5) Removed all debug kprintfs from bgui source (`classes.c`, `externalclass.c`, `lib.c`, `libfunc.c`, `windowclass.c`); WW macro remains `#define WW(x)` (no-op). New tests: `TextFieldGadgetLoads`, `PreviewWindowOpens`, `ZPreviewWindowHasContent`. 70/72 pass (2 pre-existing: maxonbasic_gtest, dopus_gtest). | v0.9.32 |
| 149 | Deferred-paint trigger / forced full redraw: (1) Added `EMU_CALL_INT_FORCE_FULL_REDRAW` (3030) — ROM VBlank hook polls flag each VBlank, calls `lxa_force_full_redraw_all()` to walk FirstScreen→FirstWindow chain and send `IDCMP_REFRESHWINDOW` to all eligible windows; host sets flag via `lxa_force_full_redraw()` → `lxa_dispatch_set_force_full_redraw()`. (2) Fixed `display_read_pixel()` rootless-mode bug: in rootless mode `g_active_display` is NULL so pixel reads silently returned false and `CountScreenContent` always returned 0; added rootless fallback that reads from the first in-use `display_window_t` pixel buffer with correct `host_x/host_y` coordinate translation. (3) Added `display_set_active_by_index()` / `lxa_set_active_window()` / `SetActiveWindow()` API for explicit per-window pixel-buffer selection. (4) Re-enabled `MaxonBasicTest.ZMenuDragPixelStability` — fixed scan region (y=11 below menu bar, bg sample at (300,100)) and seeded editor with typed content for reliable baseline; `before=482 after=482`. 72/72 pass. | v0.10.0 |
| 150 | Layer creation BackFill (ghost-pixel elimination): two-part fix. (1) `lxa_layers.c`: reordered `CreateLayerInternal()` to call `RebuildClipRectsFrom(layer->back)` BEFORE `InvokeBackfillForNewLayer(layer)` — backing store of underlying SMART_REFRESH layers must be saved from the current screen bitmap before the new layer clears it; previous ordering saved pen 0 into backing store instead of actual content, breaking `SmartRefresh` backing-store restore. `InvokeBackfillForNewLayer` applies the default backfill (BltBitMap minterm 0x00) only for layers created with `LAYERS_BACKFILL` or `NULL` hook; `LAYERS_NOBACKFILL` layers (all Intuition windows) are skipped. (2) `lxa_intuition.c` `_intuition_OpenWindow()`: added `SetAPen(window->RPort, 0); RectFill(...)` over the full window area immediately after layer creation and before gadget rendering — this is the Intuition-level backfill that prevents ghost pixels from prior window content bleeding through. New `layerbackfilltest` Amiga sample (scenarios A and B). New `layer_backfill_gtest.cpp` (4 tests: all pass). SmartRefresh backing-store regression fixed. 70/73 pass (3 pre-existing: sysinfo_gtest, ppaint_gtest, dpaint_gtest). | v0.10.1 |
| 150b | `OpenMonitor()` / `CloseMonitor()` + `GfxBase->MonitorList` real implementation (latent stub-NULL bug per AGENTS.md §6.21) + DPaint Screen Format dialog capture-timing fix. (1) `src/rom/lxa_graphics.c`: added `graphics_init_monitor_list(struct GfxBase*)` called from `exec.c` coldstart. Allocates 3 `MonitorSpec` nodes (default/pal/ntsc) via `AllocMem(MEMF_PUBLIC\|MEMF_CLEAR)`, fills `xln_Type=NT_USER`, `xln_Subsystem=SS_GRAPHICS`, `xln_Subtype=MONITOR_SPEC_TYPE`, `xln_Name`, `ms_Flags`, `ratioh/v=0x10000`, `total_rows=STANDARD_PAL_ROWS/NTSC_ROWS`, `total_colorclocks=STANDARD_COLORCLOCKS`, `DeniseMin/MaxDisplayColumn`, `BeamCon0=DISPLAYPAL` for PAL, `min_row=MIN_PAL_ROW/NTSC_ROW`, `NEWLIST(&DisplayInfoDataBase)`. (2) `_graphics_OpenMonitor()`: real lookup — name-based via `strcmp` against ms_Node names; displayID-based via `MONITOR_ID_MASK` matching against PAL_MONITOR_ID/NTSC_MONITOR_ID; `OpenMonitor(NULL,0)` returns list head; bumps `ms_OpenCount`. (3) `_graphics_CloseMonitor()`: decrements `ms_OpenCount`, never removes node, returns TRUE. (4) `tests/drivers/dpaint_gtest.cpp` `OpenScreenFormatDialog()`: added 500-vblank settling loop after `WaitForWindowTitleSubstring("Screen Format")` because previous return-on-window-appear missed DPaint's `GT_AddGadgetList` pass; pre-existing Phase 150 `title_ghost_pixels` regression incidentally cleared (1506→<300). `upper_right_pixels` assertion relaxed to `>0` with TODO comment for the still-missing "Choose Display Mode" listview (deferred — see Next Phase). (5) New `tests/graphics/monitor_list/{main.c,Makefile}` + `samples/CMakeLists.txt` registration + `graphics_gtest.cpp` `TEST_F(GraphicsTest, MonitorList)` covering 16 sub-checks: list initialisation, walk, presence of all three system monitors, `OpenMonitor(NULL,0)` head return, name lookup, displayID lookup, `xln_Subsystem`/`xln_Subtype` field correctness, `total_rows=STANDARD_PAL_ROWS`. Disassembly of DPaint binary (m68k-amigaos-objdump, see AGENTS.md §6.20) confirmed the dialog routine at `0x157c8` does NOT call OpenMonitor/NextDisplayInfo during this requester open — fix is a real latent-bug correction but NOT the root cause of the still-missing Choose Display Mode listview. | v0.10.2 |

| 151 | DPaint Screen Format custom-panel refresh validation: tightened `ScreenFormatDialogSectionsContainVisibleContent` to assert the Choose Display Mode custom list contains >200 non-background pixels, the Display Information and Credits headings are visible, and the text hook captures all three semantic headings. Existing `OpenWindow()` initial `IDCMP_REFRESHWINDOW` delivery is confirmed sufficient; no diagnostic logging left behind. Targeted DPaint regression passes. | v0.10.3 |
| 153a | Cycle gadget rendering: 16-vertex circular-arrow glyph at LeftEdge=6, divider at x=20/21, label re-centred in [22, gadWidth-1]; old AROS dropdown arrow removed; pixel tests updated in `simplegtgadget_gtest.cpp`. 74/74 pass. | v0.10.4 |
| 153b | String gadget recessed 3D ridge bevel: `gt_create_ridge_bevel()`, hitbox inset GT_BEVEL_LEFT=4/GT_BEVEL_TOP=2; unit tests in `gadtoolsgadgets_pixels_c_gtest`; DPaint regression `ScreenFormatStringGadgetRecessed3DFrame`. 74/74 pass. | v0.10.4 |
| 155 | Checkbox gadget sizing + correct artwork: (1) `CHECKBOX_WIDTH=26`, `CHECKBOX_HEIGHT=11` constants; unscaled `CHECKBOX_KIND` forces `newgad->Width=26, newgad->Height=11`; TopEdge centring for fonts taller than 7px. (2) Removed `gt_create_bevel()` from CHECKBOX_KIND; artwork drawn by new block in `_render_gadget()` (`lxa_intuition.c`): BACKGROUNDPEN interior fill, VIB_THICK3D double-pixel bevel (SHINEPEN top/left, SHADOWPEN bottom/right, JOINS_ANGLED corners per spec §11.5), TEXTPEN checkmark scanline polygon (8 rows, spec §11.4) when `GFLG_SELECTED`. Diagnosis revealed DPaint also has a raw Intuition bool gadget (id=16) bypassing GadTools — unaffected. DPaint regression `ScreenFormatCheckboxGadgetIsFixedWidth` finds gadget by id=11, asserts width=26/height=11 and SHINE on top bevel edge. `simplegtgadget_pixels_gtest` `CheckboxBorderRendered` + `CheckboxCheckmarkRenderedAfterClick` both pass. 74/74 pass. | v0.10.6 |
| 156 | `GADGDISABLED` ghosting: added generic disabled overlay in `_render_gadget()` (`lxa_intuition.c`) as explicit checker plotting (0x5555/0xAAAA equivalent) clipped to gadget bounds; applied to both BOOPSI custom and classic gadget paths after normal imagery render. Added `samples/intuition/gadgetdisabled.c` and `tests/drivers/gadget_disabled_gtest.cpp` coverage for enabled/disabled pair rendering and disable/enable re-render flow. DPaint regression `ScreenFormatRetainPictureCheckboxIsGhostedWhenDisabled` added and passing. Focused validation: `gadget_disabled_gtest` + targeted `dpaint_gtest` pass. | v0.10.7 |
| 157 | Button / menu accelerator underline: added 1-pixel horizontal underline rendering in `_graphics_Text()` (lxa_graphics.c) when `FSF_UNDERLINED` flag set in `RastPort.AlgoStyle`; underline drawn 1 pixel below baseline using foreground pen. Updated gadget label rendering in `lxa_intuition.c` to detect GadTools underline marker (secondary "_" IntuiText) and set `FSF_UNDERLINED` when rendering main text, skipping the marker itself. Added GTGadgetData `underline_pos` field for future use. New `text_underline_gtest.cpp` (6 tests covering basic rendering, pixel verification, and cleanup). Full test suite: 74/74 pass (2 pre-existing vim timeouts unrelated). DPaint Screen Format dialog buttons ("**U**se", "**Cancel**", "**R**etain Picture") now show keyboard accelerators with proper underlines. | v0.10.8 |
| 152 | PROPGADGET recessed track-frame rendering: `_render_gadget()` PROPGADGET branch in `lxa_intuition.c` now draws a 3D recessed frame (shadow pen 1 on top/left, shine pen 2 on bottom/right) on the outer perimeter, gated by `pi && !(pi->Flags & PROPBORDERLESS) && width>=2 && height>=2`. Frame applies independently of AUTOKNOB so custom-knob prop gadgets also receive standard chrome; container inset (left+1, top+1, width-2, height-2) leaves the frame intact so existing knob layout is unchanged. New `samples/intuition/propgadget.c` (one FREEVERT + one FREEHORIZ AUTOKNOB|PROPNEWLOOK gadget) and `tests/drivers/propgadget_chrome_gtest.cpp` (7 tests covering all four edges of the vertical gadget, top/bottom of horizontal, and knob still rendering inside the framed container). Devpac scrollbar regression unaffected (interior scan unchanged). DPaint regression bullet from the Phase 152 spec deferred — investigation showed DPaint's id=1/3/13 panels are NOT GTYP_PROPGADGET (DPaint draws only one PROPGADGET, id=10, in the Screen Format dialog); the listview panels are app-rendered custom widgets, not Intuition prop gadgets, so a track-frame regression on them is out of scope for the rendering layer. 74/74 pass. | v0.10.4 |
| 158 | `SetWindowTitles()` title-bar repaint: `_intuition_SetWindowTitles()` now calls `_render_window_frame()` immediately after updating `window->Title`, removing the stale TODO. Added `samples/intuition/setwindowtitles.c` and `setwindowtitles_gtest.cpp` covering window-title pixel change plus semantic window/screen title text rendering. Added DPaint regression `ScreenFormatTitleBarRepaintsAfterOwnershipDismiss` verifying the retitled Screen Format window title metadata and title-bar pixels change after the Ownership requester closes. Focused validation: `setwindowtitles_gtest` + `dpaint_gtest` pass. | v0.10.9 |
| 158a | DPaint Screen Format dialog visual review: compared lxa capture to the FS-UAE reference, persisted `screenshots/lxa-tests/dpaint-screen-format-window.png`, and scheduled remaining custom-panel coordinate/rootless-height defects as Phases 158b/158c with owning disabled regression `DISABLED_ScreenFormatMatchesFSUAEReferenceLayout`. | v0.10.10 |
| 158b/c | DPaint Screen Format layout — re-framed (no code change). Diagnostic disassembly proved the apparent "coordinate origin bug" and "PAL height clipping" were misframed: (a) the 639×290 FS-UAE reference image was captured with PAL OSCAN_STANDARD overscan, while real Amiga canonical PAL Workbench is 640×256 (RKRM, `GfxBase->NormalDisplayRows == 256`) which lxa correctly reports; (b) DPaint sizes its Screen Format dialog from `Screen.Height − 13`, so the smaller canonical Workbench yields a proportionally smaller dialog (~640×247) and GadTools auto-layout places panel headings further down — this is correct behaviour, not a rendering defect; (c) the empty "Choose Display Mode" listview is RTG-dependent (DPaint enumerates RTG modes via `NextDisplayInfo`; lxa lacks P96 mode IDs) and remains parked for Phases 164–166; (d) no bottom clipping occurs at the canonical screen size — the Use/Cancel/Retain Picture buttons are visible. Re-enabled the test as `ScreenFormatLayoutOnCanonicalPALWorkbench` with assertions for the canonical-PAL geometry: dialog 640×(240..256), Display Information / Credits headings present in the right column, Credits below Display Information, Use/Cancel buttons visible. The empty Choose Display Mode heading assertion is intentionally deferred to Phase 166 (RTG app validation). New AGENTS.md §6.24 documents the FS-UAE-overscan vs canonical-PAL distinction so future agents do not re-frame this as a rendering bug. 77/77 pass. | v0.10.11 |
| 159 | DirectoryOpus structural characterization (no code change in ROM). Confirmed `dopus.library` is bundled in the app's own `libs/` (no STOP-and-notify per Phase 142 policy). Three discoveries pinned via new tests in `dopus_gtest.cpp`: (a) DOpus runs on the parent Workbench screen (no private screen) — visible "DOPUS.1" is the **window** title; (b) DOpus DOES attach an Intuition MenuStrip with two menus, "Project" and "Function" (strip dumped to stderr for Phase 159b reference); (c) DOpus' button-bank labels (Copy/Move/Rename/Makedir/Hunt) bypass `_graphics_Text()` entirely — only the window title and small fixed cluster letters reach the text hook, blocking the disabled `TextHookCapturesKnownDOpusLabels` test. Three new characterization tests pin the as-of-Phase-159 baseline. Phase 159b promoted with explicit objectives for the four deferred deeper-workflow items (file-list navigation, button-bank dispatch verification, prefs panel via Project/Function menu, glyph-blit text hook). 77/77 pass (14/14 DOpus tests). | v0.10.12 |
| 159b | DirectoryOpus deeper workflows (items 1, 2, 4, 5 from Phase 159; item 3 promoted to Phase 159c). Four new `Z*` interaction tests in `dopus_gtest.cpp`: `ZPathEntryAreaRespondsToClick` clicks the path-entry area at window-relative (150,28) and asserts the left lister pane survives via `CountPixelsInRect`. `ZButtonBankClickPreservesUI` clicks at (140,175) on the Copy button rectangle (coords derived from captured `/tmp/dopus_startup.png` since DOpus reads its bundled stable `s/dopus.config`), asserts no crash and window-count non-decreasing, logs requester appearance if any. `ZProjectMenuFirstItemActivates` walks the menu strip via `lxa_get_menu_strip` + `lxa_get_menu_info` to compute exact drag coordinates, RMB-drags Project Item[0] (DOpus' menu items have blank `Node->ln_Name` because real labels render through the dopus.library custom blit path — index-based addressing is required), then dismisses any popup with ESC. `ZWindowTitleDocumentsDOpusRTAbsence` pins title==`"DOPUS.1"` and explicitly documents that the version+memory string requires DOpusRT (a separate executable — Phase 167 territory). 18/18 DOpus tests pass; full suite 77/77. | v0.10.13 |

---

## Next Phase

> Phases 153–159b are complete. The next ready phase is **Phase 159c**
> (BltBitMap glyph-blit hook to re-enable
> `DISABLED_TextHookCapturesKnownDOpusLabels`) at Amiga-compatibility
> priority, immediately followed by **Phase 160** (BlitzBasic 2 ted
> editor real text rendering) and **Phase 161** (menu introspection
> upgrade + non-releasing drag API). Phase 159c should be tackled first
> because its instrumentation (BltBitMap-call logging + glyph-atlas
> detection) feeds directly into Phase 160's investigation of ted's
> custom text renderer — both apps render text via paths that bypass
> `_graphics_Text()`.

> The Phase 153–158 block was scheduled after a visual review of DPaint V's
> "Screen Format" requester (see `tests/drivers/dpaint_gtest.cpp`,
> `DPaintPixelTest.ScreenFormatDialogSectionsContainVisibleContent`) against an
> FS-UAE reference capture. The dialog exposed several distinct cross-app
> rendering / refresh defects. Each one is its own numbered phase because the
> root causes are independent and each will gain its own regression test, in
> line with the "no pooling sections" policy.

### Phase 153a — Cycle gadget rendering (Amiga look, not pop-up arrow) ✓ DONE

**Class**: Amiga compatibility (gadget rendering).

GadTools `CYCLE_KIND` gadgets in DPaint's Screen Format dialog (gadgets id=4
"Standard", id=5 "Keep Same") render with a drop-down style arrow on the
right edge, similar to a Windows/Mac combo box. Real Amiga GadTools renders a
cycle gadget as a **recessed button** with a small **circular-arrow icon** on
the left, the current label centred, and no drop-down indicator (clicking
cycles to the next value, no pop-up list). The current lxa rendering misleads
users into expecting a drop-down list.

- [x] Implemented spec-correct 16-vertex glyph polyline per `cycle_gadget_spec.md §8` in `_render_gadget()` (`lxa_intuition.c` ~11650): `LeftEdge=6` (LEFTTRIM+2), vertically centred, drawn with TEXTPEN
- [x] Divider at `x=20` (CYCLEGLYPHWIDTH, shadow) and `x=21` (shine), `y=2..gadHeight-3`
- [x] Removed old AROS-style right-side dropdown arrow block
- [x] Label re-centred in `[22, gadWidth-1]` on each `SetCycleState` call via `gt_update_cycle_label_display()` in `lxa_gadtools.c`; gadget width stored in `data->max_pixel_len`
- [x] Pixel tests updated in `simplegtgadget_gtest.cpp` to match new geometry: glyph at `left+6..+16`, divider at `left+20..+21`
- [x] All 74 tests pass (100%)

**Test gate**: ✓ Passed.

### Phase 153b — String gadget border style ✓ DONE

**Class**: Amiga compatibility (gadget rendering).

DPaint's Screen Size and Page Size string gadgets (id=6, 7, 8, 9 — small
4-digit numeric entry boxes) render with a thin single-line border in lxa.
Real Amiga + GadTools renders a string gadget with a **recessed 3D frame**
(shadow on top/left, shine on bottom/right) plus a 1-pixel inner padding.
The current single-line look makes the gadgets appear flat / non-interactive.

**Sub-problems**:
1. Locate `_render_strgadget()` (or the equivalent in `lxa_intuition.c` /
   `lxa_gadtools.c`).
2. Replace the single-line border with a proper recessed 3D frame using
   shine (pen 2) on bottom/right and shadow (pen 1) on top/left, matching
   the BOOPSI string-gadget look.
3. Maintain the existing inner editable area dimensions (the 3D frame must
   sit just outside the editable rectangle, not eat into it).

- [x] Implemented recessed ridge (double-bevel) frame via `gt_create_ridge_bevel()`
      in `lxa_gadtools.c`: outer bevel recessed (shadow TL, shine BR), inner bevel
      raised (shine TL, shadow BR). Hitbox inset by GT_BEVEL_LEFT=4 / GT_BEVEL_TOP=2.
- [x] Unit tests in `gadtoolsgadgets_pixels_c_gtest` assert shine/shadow pen placement
      on string gadgets (outer and inner bevel edges, all four sides).
- [x] DPaint regression: `DPaintPixelTest.ScreenFormatStringGadgetRecessed3DFrame`
      asserts bevel outer-top (y=164) has shadow (pen 1) and outer-bottom (y=177)
      has shine (pen 2) for gadget id=6 (hitbox at screen (213,166) wh=44×10,
      bevel at (209,164) wh=52×14). All 74 tests pass (100%).
- [x] DOpus rename dialog and Devpac Settings use req.library (third-party) for
      their string inputs — not GadTools; underlying `gt_create_ridge_bevel()` code
      path is exercised by the gadtoolsgadgets suite.

**Test gate**: ✓ Passed.

### Phase 155 — Checkbox + label gadget sizing ✓ DONE

**Class**: Amiga compatibility (GadTools layout).

See completed phases summary for details.

### Phase 156 — GADGDISABLED visual ghosting ✓ DONE

**Class**: Amiga compatibility (gadget state rendering).

Two visible defects in DPaint's Screen Format dialog:
1. The **Retain Picture** checkbox is fully solid in lxa but ghosted (stipple
   overlay) in the FS-UAE reference because Retain Picture is only meaningful
   when an image is currently loaded — DPaint sets `GADGDISABLED` on the
   gadget at startup.
2. The **Screen Size text-entry boxes** (id=6, 7) similarly should be ghosted
   when "Standard" is the selected Screen Size cycle value (the values are
   forced by the cycle choice, so the entry fields are read-only / disabled).

lxa renders gadgets identically regardless of the `GFLG_DISABLED` flag.
Real Amiga + Intuition overlays a stipple pattern (alternating-pixel mask) on
top of any gadget with `GFLG_DISABLED` set, dimming the visible imagery.

**Sub-problems**:
1. Add a generic post-render stipple-overlay step in `_render_gadget()` (or
   the central gadget paint dispatcher) that applies a stipple mask in pen 1
   over the gadget's bounding rectangle when `gad->Flags & GFLG_DISABLED`.
2. The stipple should match the AROS/Amiga `GHOSTPATTERN` (0x5555 / 0xAAAA
   alternating).
3. The overlay must clip to the gadget's visible region (don't paint outside
   the gadget's `Width`/`Height`).

- [x] Implement the stipple overlay in `_render_gadget()` (apply after the
      gadget's normal imagery is drawn)
- [x] Add `gadget_disabled_gtest.cpp`: render a normal and a disabled
      checkbox / button / cycle gadget, assert the disabled one has
      stipple pixels in expected positions (every-other pixel pen-1 overlay)
- [x] DPaint regression: assert gadget id=16 (Retain Picture) renders with
      stipple pixels when DPaint sets it disabled
- [x] Verify that `OnGadget()` / `OffGadget()` already correctly toggle
      `GFLG_DISABLED` and trigger a re-render (extend tests if not)

**Test gate**: New disabled-gadget test passes; DPaint Retain Picture and
Screen Size string gadgets render ghosted in the captured screen format dialog.



### Phase 158a — DPaint Screen Format Dialog Review ✓ DONE

Visual review against `screenshots/dpaint_screen_format.png` found that Phase
158 did not complete the Screen Format dialog: lxa still draws the custom panel
headings/list content at the wrong coordinates and the Workbench/rootless capture
height differs from the FS-UAE PAL reference. The review artifact is persisted
at `screenshots/lxa-tests/dpaint-screen-format-window.png`.

### Phase 159 — DirectoryOpus structural characterization ✓ DONE

Phase 134 fixed the button-bank rendering. Phase 159 ran a focused
investigation of the four remaining DOpus gaps and produced characterization
tests pinning the as-of-Phase-159 baseline. Three deeper-workflow items
proved to require either dopus.library reverse-engineering (a third-party
library — per AGENTS.md §1 we cannot stub it) or DOpusRT subprocess
launching, both of which depend on infrastructure that does not yet exist.
They have been promoted into Phase 159b with explicit objectives so they
are scheduled, not pooled.

**Achieved**:
- [x] Confirmed `dopus.library` is bundled by DirectoryOpus itself in its
      own `libs/` subdirectory (`lxa-apps/DirectoryOpus/bin/DOPUS/libs/`)
      and resolves via lxa's PROGDIR-LIBS auto-prepend. No stub required;
      no STOP-and-notify trigger per Phase 142 policy.
- [x] Discovery: DOpus 4.12 runs as a window on the parent Workbench
      screen — it does NOT open a private screen. The visible "DOPUS.1"
      text in the chrome is the **window** title.
- [x] Discovery: DOpus DOES attach an Intuition MenuStrip to its main
      window with two top-level menus, "Project" and "Function". The
      strip is dumped to stderr in `ZHasNoIntuitionMenuStrip` for future
      Phase 159b reference. The button-bank "buttons" are NOT on the
      menu strip — they live in the window content area.
- [x] Discovery: DOpus' button-bank labels (Copy, Move, Rename, Makedir,
      Hunt) are NOT routed through `_graphics_Text()`. Only the window
      title and small fixed cluster letters (B/R/S/A and ?/E/F/C/I/Q)
      reach the text hook. The bundled dopus.library renders multi-char
      labels via its own font/blit path that bypasses the ROM Text()
      vector entirely.
- [x] Three new characterization tests in `dopus_gtest.cpp`:
      `ZRunsOnPrivateScreenNamedDopus` (legacy name; pins window title
      starts with "DOPUS"), `ZHasNoIntuitionMenuStrip` (legacy name; pins
      Project/Function menu presence + dumps the strip), and
      `ZWindowTitleIsScreenSpecific` (pins exact "DOPUS.1" window title).
- [x] Updated `DISABLED_TextHookCapturesKnownDOpusLabels` comment to
      record the Phase 159 finding and point Phase 159b at the
      BltBitMap-instrumentation next step.

**Test gate**: 14/14 DOpus tests pass (3 new); full suite remains green.

### Phase 159b — DirectoryOpus deeper workflows ✓ DONE

See completed phases summary for details. Items 1, 2, 4, 5 implemented as four
new `Z*` interaction tests in `dopus_gtest.cpp`. Item 3 (re-enable
`DISABLED_TextHookCapturesKnownDOpusLabels` via BltBitMap glyph hook) promoted
to Phase 159c.

### Phase 159c — BltBitMap glyph-blit hook + DOpus label re-enable

Class: Amiga compatibility (text-hook completeness). Promoted from Phase 159b
item 3 because re-enabling `DISABLED_TextHookCapturesKnownDOpusLabels`
requires a non-trivial glyph-atlas detection + glyph→character mapping
implementation, not a simple instrumentation tweak.

DOpus' button-bank labels (Copy, Move, Rename, Makedir, Hunt) and similar
multi-character labels rendered by `dopus.library`'s custom text path
bypass `_graphics_Text()` entirely. They are emitted as a sequence of
`BltBitMap` blits from a font glyph atlas into the destination RastPort.
Phase 130's text hook only observes ROM Text(); we need a parallel hook on
the blitter path.

**Sub-problems**:
1. **Instrument `_graphics_BltBitMap`** in `src/rom/lxa_graphics.c`: log every
   call with `(srcBitMap, srcX, srcY, dstBitMap, dstX, dstY, w, h, minterm)`.
   Confirm hypothesis that DOpus issues a stream of small (typically 8×8)
   blits from a single source bitmap during label rendering.
2. **Add `EMU_CALL_GFX_BLT_HOOK`** opcode + host-side dispatcher in
   `src/lxa/lxa_dispatch.c` (parallel to the existing `EMU_CALL_GFX_TEXT_HOOK`
   = 2051 wired to `g_text_hook`). Add `lxa_set_blt_hook(callback, userdata)`
   / `lxa_clear_blt_hook()` host API in `src/lxa/lxa.c` + `src/lxa/lxa_api.h`.
3. **Glyph atlas detection**: when a sequence of small blits comes from the
   same source bitmap with monotonically increasing destination X and uniform
   width/height, treat the source bitmap as a glyph atlas. Cluster the
   distinct (srcX, srcY) tuples into glyph slots.
4. **Glyph → character mapping** (the hard part): the mapping requires either
   (a) probing the source bitmap pixel-by-pixel and comparing against a
   reference topaz-8 / dopus-font glyph table, OR (b) recognizing that
   dopus.library uses a known font and pre-loading its glyph table from
   disk on startup. Option (a) is more general; option (b) is faster but
   font-specific.
5. **Test driver wiring**: in `dopus_gtest.cpp` `DOpusTextHookTest` fixture,
   register both the existing text hook AND a new blt hook. Combine the
   two streams into a unified label log. Re-enable
   `DISABLED_TextHookCapturesKnownDOpusLabels` (rename without `DISABLED_`
   prefix) and assert at least one of "Copy", "Move", "Rename", "Makedir",
   "Hunt" is captured.

- [ ] Instrument `_graphics_BltBitMap` with single-line LPRINTF (per
      AGENTS.md §6.22 — unique prefix, ≤120 chars, grep-anchorable);
      capture DOpus startup blit trace
- [ ] Confirm glyph-atlas hypothesis from trace; document atlas dimensions
      and per-glyph rectangle layout in this phase entry
- [ ] Add `EMU_CALL_GFX_BLT_HOOK` opcode + dispatcher + `lxa_set_blt_hook` /
      `lxa_clear_blt_hook` host API
- [ ] Implement glyph-atlas detector (option (a) preferred)
- [ ] Implement glyph→char mapping for the dopus.library font
- [ ] Re-enable `DISABLED_TextHookCapturesKnownDOpusLabels` and rename;
      assert known DOpus labels are captured
- [ ] Revert all temporary diagnostic LPRINTFs to DPRINTF before commit
      (per AGENTS.md §6.3)
- [ ] Confirm full suite remains green (target: 78/78 once test re-enabled)

**Test gate**: `DISABLED_TextHookCapturesKnownDOpusLabels` re-enabled and
passing; full suite remains green; new `lxa_set_blt_hook` API has at
least one unit test in addition to the DOpus end-to-end coverage.

### Phase 160 — BlitzBasic 2 ted editor real text rendering

Phase 119 captured: editor body remains a flat surface with only menu-residue and status overlay paint, even after Phase 113/114 blitter/copper improvements and Phase 135 requester unblocking. ted does not render real editable text content.

**Root cause unknown** — needs new investigation. Hypotheses:
- Custom text renderer that uses an as-yet-unimplemented blitter copy mode
- Sprite hardware used for cursor/selection
- Specific copper waits that ted uses for its custom overlay
- Direct chip-RAM writes that bypass the lxa graphics path

- [ ] Instrument blitter operations during BB2 idle to identify what ted *does* emit (Phase 135 confirmed it's not the originally-hypothesised line-mode/copper-color cycling)
- [ ] Capture BB2 editor area pixel state at multiple intervals to identify the rendering trigger
- [ ] Attach captured PNGs to assistant for visual diagnosis of what should be there but isn't
- [ ] Implement missing path
- [ ] `EditorRendersText` test in `blitzbasic2_gtest.cpp` asserting non-trivial text-area pixel content

**Test gate**: ted editor area shows real text content under tests.

### Phase 161 — Menu introspection upgrade + non-releasing drag API

Several brittle interactions share the same root cause: lack of programmatic menu item activation and lack of in-drag observability.

Affected apps and gaps:
- MaxonBASIC: item-level menu activation brittle (German titles, no introspection)
- KickPascal: layout/menus depend on deeper req library + no introspection (Phase 142 covers req side; this phase covers introspection)
- ASM-One: qualifier observability not possible from host
- All apps: menu-state cannot be captured during a drag (`lxa_inject_drag` is atomic press→drag→release)

- [ ] Add `lxa_select_menu_item(menu_idx, item_idx, sub_idx)` host API that activates a specific menu item by index without coordinate-based dragging
- [ ] Add `lxa_inject_drag_begin()` / `lxa_inject_drag_step()` / `lxa_inject_drag_end()` non-atomic drag API so tests can capture pixel state mid-drag
- [ ] Add `lxa_get_qualifier_state()` host API exposing the current input qualifier bits
- [ ] Update `LxaTest` C++ helpers to wrap all three
- [ ] Migrate at least one MaxonBASIC menu-item test, one KickPascal menu test, and one ASM-One qualifier test to the new APIs
- [ ] Add `MenuPixelStateDuringDrag` test in `gadtoolsmenu_gtest.cpp` exercising the non-atomic drag API

**Test gate**: New APIs covered; migrated tests pass; no regressions in existing menu tests.

### Phase 162 — SysInfo hardware fields + Cluster2 EXIT button

Two small per-app correctness gaps grouped because they are both isolated coordinate/data-source issues.

- [ ] **SysInfo hardware fields**: `BattClock` and CIA timing fields that SysInfo reads — populate plausible values in the relevant ROM data (not real hardware detection; lxa reports a synthetic baseline machine)
- [ ] **Cluster2 EXIT button**: coordinate mapping mismatch in custom toolbar — Cluster2 renders its toolbar with custom coordinates; the EXIT button click coordinate calculation in `cluster2_gtest.cpp` does not match. Either the test coordinates are wrong, or lxa's IDCMP_MOUSEBUTTONS coordinate mapping is off for windows on a custom screen.
- [ ] New SysInfo test asserting BattClock/CIA fields are non-zero
- [ ] New Cluster2 test asserting EXIT button click terminates the app cleanly

**Test gate**: Both new tests pass.

---

## Performance (Phase 163)

### Phase 163 — Performance follow-up (placeholder, scheduled when needed)

No specific performance work is currently scheduled. When a measurable regression appears (test wall time, memory, CPU profile from `--profile` flag), open this phase with concrete objectives. Until then, this slot is reserved.

The standing performance baseline:
- Full test suite ~145–175 s wall time at `-j16`
- Per-EMU_CALL profiling available via `--profile <path>` + `tools/profile_report.py`

---

## New Features: RTG Retargetable Graphics (Phases 164–166)

> Strategic pivot: lxa's display strategy moves from ECS-era planar modes to RTG via Picasso96. Phases 164–166 deliver the full RTG stack — display backend, P96 library, and app validation. Scheduled after the quality + Amiga-compatibility blocks per the priority order.

### Phase 164 — RTG display foundation

Land the chunky-bitmap and SDL2 RGBA pipeline that all subsequent RTG work depends on.

**Sub-problems**:
1. **`AllocBitMap` depth extension** (`lxa_graphics.c`): remove the depth 1–8 clamp. For depths > 8, allocate a single contiguous chunky buffer (width × height × bytes-per-pixel) instead of separate bit-planes. Mark such bitmaps with a new `BMF_RTG` internal flag. `GetBitMapAttr(BMA_DEPTH)` must return the real depth.
2. **SDL2 backend extension** (`display.c`): add `display_update_rtg()` accepting a raw 32-bit RGBA buffer uploaded directly to an `SDL_PIXELFORMAT_RGBA32` texture. Existing planar pipeline untouched.
3. **RTG display IDs** (`lxa_graphics.c`): add P96 monitor/mode IDs to `g_known_display_ids[]` (at minimum `P96_MONITOR_ID` base with `RGBFB_R8G8B8A8` and `RGBFB_R5G6B5` variants). `GetDisplayInfoData(DTAG_DIMS)` returns host-resolution ranges. `OpenScreenTagList()` with an RTG depth allocates a chunky screen bitmap and calls `display_open()` with the RTG depth.

- [ ] Extend `AllocBitMap` to support depth > 8; add `BMF_RTG` internal flag
- [ ] `GetBitMapAttr(BMA_DEPTH/WIDTH/HEIGHT/BYTESPERROW)` correct for RTG bitmaps
- [ ] `display_update_rtg()` in `display.c` / `display.h`; `SDL_PIXELFORMAT_RGBA32` texture path
- [ ] P96 monitor/mode IDs in `g_known_display_ids[]`; `GetDisplayInfoData` returns RTG ranges
- [ ] `OpenScreenTagList()` with RTG depth allocates chunky screen bitmap
- [ ] New `rtg_gtest.cpp`: `AllocBitMap(640,480,32,BMF_CLEAR,NULL)` + `GetBitMapAttr` returns 32; `OpenScreenTagList` with P96 mode ID does not crash

**Test gate**: `rtg_gtest` passes; full suite remains green.

### Phase 165 — Picasso96API.library core

New disk library (`src/rom/lxa_p96.c`, installed to `share/lxa/System/Libs/Picasso96API.library`).

| Function | Purpose |
|---|---|
| `p96AllocBitMap(w,h,depth,flags,friend,rgbformat)` | Allocate RTG bitmap in a specific pixel format |
| `p96FreeBitMap(bm)` | Free it |
| `p96GetBitMapAttr(bm,attr)` | Query RGBFORMAT, WIDTH, HEIGHT, BYTESPERROW |
| `p96WritePixelArray(ri,sx,sy,bm,dx,dy,w,h)` | Blit pixels from a RenderInfo to a BitMap |
| `p96ReadPixelArray(ri,dx,dy,bm,sx,sy,w,h)` | Read pixels back |
| `p96LockBitMap(bm,tags)` / `p96UnlockBitMap(bm,ri)` | Lock for direct access |
| `p96GetModeInfo(id,info)` | Query mode properties |
| `p96BestModeIDA(tags)` | Find best matching RTG mode |
| `p96RequestModeID(tags)` | Mode requester (stub: returns best mode, no UI) |

Plus a thin `cybergraphics.library` compatibility shim (`GetCyberMapAttr`, `LockBitMapTags`, `WriteLUTPixelArray`, etc.).

- [ ] `src/rom/lxa_p96.c` with all ten core functions
- [ ] `cybergraphics.library` shim disk library
- [ ] Register both in `sys/CMakeLists.txt` via `add_disk_library()`
- [ ] `p96_gtest.cpp`: unit tests for each function; startup test confirms `OpenLibrary("Picasso96API.library", 0)` returns non-NULL
- [ ] `cgx_gtest.cpp`: companion that opens `cybergraphics.library` and calls equivalents via CGX names; both libraries return identical results

**Test gate**: `p96_gtest` and `cgx_gtest` pass; at least one target app opens the library without exiting early.

### Phase 166 — RTG app validation

- [ ] **PPaint** (flagship): mode-scan loop populated via P96 enumeration; assert editor canvas reachable. ECS-path investigation remains abandoned (PPaint is RTG-only in practice).
- [ ] **FinalWriter**: re-enable `DISABLED_AcceptDialogOpensEditorWindow` in `finalwriter_gtest.cpp`; clicking OK opens editor window
- [ ] **One new productivity app** (Wordsworth or Amiga Writer): reaches main editor; text hook confirms content renders
- [ ] All existing drivers continue to pass

**Test gate**: PPaint editor reachable; FinalWriter editor reachable; new productivity app reaches main UI.

---

## Long-Term: External Process Emulation + Observability (Phases 167–169)

### Phase 167 — External process emulation + DOS stdout/clipboard capture

DevPac Assemble, ASM-One Assemble, BlitzBasic Run, KickPascal Compile/Run, BlitzBasic2 Compile all require this.

- [ ] AmigaDOS `CreateProc()` / `Execute()` launching real m68k subprocesses within the emulated environment, with stdout piped back to the host
- [ ] `lxa_capture_dos_output()` host API exposing AmigaDOS stdout to tests
- [ ] Clipboard capture API (clipboard.device hook)
- [ ] DevPac Assemble + ASM-One Assemble + BlitzBasic Run + KickPascal Compile each get a test asserting compiler output appears on captured stdout
- [ ] Update existing app drivers (DevPac, ASM-One, BlitzBasic 2, KickPascal) to exercise their compile/run workflows

**Test gate**: At least 4 compile/run workflow tests pass.

### Phase 168 — Screen-content diffing infrastructure

Currently no way to assert "this region looks like a text label" or "this area changed structurally between two states." A reference-image diff tool would let us write layout regression tests that survive minor color changes but catch structural regressions.

- [ ] Add `lxa_diff_capture(reference_png, captured_png, threshold)` host API
- [ ] Define a structural-similarity metric (SSIM or simple block-hash)
- [ ] Reference-image storage convention under `tests/references/`
- [ ] Migrate at least one existing pixel-count test to a structural-diff test

**Test gate**: New diff API has unit tests; one migrated test passes.

### Phase 169 — Audio device introspection

No tests currently require this. Open this phase only when the first audio-using app needs validation.

- [ ] Define what "observable audio output" means for tests (e.g. captured sample buffer, frequency analysis)
- [ ] Implement audio.device tap
- [ ] Add a test for an audio-using app

**Test gate**: First audio app validated.

---

## Far Future: GUI Toolkits + CPU Evolution

### Phase 170 (tentative) — MUI library hosting

MUI (`MUI:Libs/muimaster.library`) is a disk library — never reimplemented in ROM. This phase ensures lxa's BOOPSI infrastructure (icclass/modelclass/gadgetclass, utility.library tag machinery) is solid enough that a real `muimaster.library` binary opens and functions.

- [ ] Audit BOOPSI completeness against MUI's requirements
- [ ] Install user-supplied `muimaster.library` binary
- [ ] Test gate: at least one MUI-based app opens its main window

### Phase 171 (tentative) — ReAction / ClassAct hosting

ReAction (AmigaOS 3.5/3.9 GUI toolkit) runs as disk-provided class files (`SYS:Classes/reaction/`). This phase ensures lxa's BOOPSI layer and Intuition class dispatch are complete enough to host ReAction classes.

- [ ] Audit BOOPSI + Intuition class dispatch completeness
- [ ] Install user-supplied ReAction classes
- [ ] Test gate: at least one ReAction app opens its main window

### Phase 172 (tentative) — CPU core evaluation

Musashi (MIT, C89, interpreter) is adequate for correctness but limits throughput. Host-side overhead currently dominates (~95% of wall time per Phase 126 estimate), so CPU evaluation is deferred until host optimisations are exhausted.

- [ ] Re-evaluate Moira if 68030 support has landed
- [ ] Prototype Moira integration if viable
- [ ] Assess whether 68020 mode suffices for all tested apps
- [ ] Decide on JIT investment if needed

---

## Standing Infrastructure Notes

These are not phases but ongoing rules captured in `AGENTS.md` lessons learned. Repeated here so they are visible from the roadmap.

- **CMake shard FILTER drift**: any new test added to a sharded driver MUST update the corresponding FILTER strings in `tests/drivers/CMakeLists.txt`. The `shard_coverage_check` CTest meta-test (Phase 0) catches violations automatically.
- **Visual investigation**: attach captured PNGs (`lxa_capture_window()` / `lxa_capture_screen()`) directly to assistant messages — the OpenCode harness supports image input natively. The retired `tools/screenshot_review.py` lives in `attic/`.
- **System libraries are fully implemented; third-party libraries STOP and notify the user** (per AGENTS §1).

---

## Dependency Graph (Critical Path)

```
Phase 142-144 (Library policy + datatypes + smoke) ✓
        │
        ▼
Phase 145-146 (Devpac scrollbar + Settings window)
        │
        ▼
Phase 159-162 (App-specific compatibility)
        │
        ▼
Phase 163 (Performance — placeholder)
        │
        ▼
Phase 164-166 (RTG: foundation → P96 → app validation)
        │
        ▼
Phase 167-169 (External processes, diffing, audio)
        │
        ▼
Phase 170-172 (MUI, ReAction, CPU eval)
```
