# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, and roadmap updates on every completed phase.

## Future Phases (placeholders)

- Phase 96: Manual Testing Results

The following issues were discovered when comparing apps running via lxa to running them on a real amiga. All of them need to be investigated thoroughly and fixed.
The goal of this phase is to make all apps run exactly like they would on a real amiga.

* [x] Add a screenshot review helper at `tools/screenshot_review.py` that sends one or more captures to an OpenRouter vision model (using `OPENROUTER_API_KEY`) so manual app-review work can inspect Amiga UI output directly.

* [x] ASM-One: bundle the disk-provided `reqtools.library` in `lxa-apps/Asm-One/bin/ASM-One/Libs`, wire the ASM-One test/launch paths to add that directory to `LIBS:`, and confirm startup no longer logs the missing-library failure.
* [x] ASM-One: startup now proceeds directly into the editor without leaving the documented ALLOCATE/WORKSPACE prompt sequence on screen, and driver coverage asserts no extra startup requester window remains.
* [x] ASM-One: menu bar now renders, and host-side coverage opens the menu bar without logging `_render_menu_bar()` / `_render_menu_items()` invalid `RastPort` bitmap failures.
* [x] ASM-One: serialized host-side log writes so startup diagnostics no longer interleave the `OpenWorkBench()` return line with timer-device debug output, and driver coverage now asserts the startup log stays clean while the editor reaches its main window.
* [x] DPaint V: Ownership information window now receives the initial `IDCMP_REFRESHWINDOW` cycle expected by refresh-managed apps, and driver coverage asserts the ownership requester body renders visible content instead of staying blank.
* [x] DPaint V: Screen Format dialog now renders visible `Choose Display Mode`, `Display Information`, and `Credits` sections after the ownership requester closes, and driver coverage opens the dialog and asserts those regions contain rendered content.
* [x] Rootless host windows now preserve the screen rows above a logical window, fixing the off-the-top clipping root cause that hid top strips such as Devpac's menu bar.
* [x] Devpac: Rootless menu bar repaint now keeps visible content stable across repeated menu open/close cycles, and driver coverage captures the host window strip before/after interaction to guard against regressions.
* [x] Devpac: Rootless host window refresh now clips out any higher z-order windows on the same screen, and driver coverage opens Project -> About... and asserts the dialog only appears in its own rootless window instead of being duplicated inside the main editor window.
* [x] Devpac: "About" dialog "Continue" button now keeps its bevel lines on the frame instead of drawing a diagonal strike-through across the label, and requester pixel coverage guards the border geometry.
* [x] KickPascal: "presents" string no longer collides with the splash logo/company text because layered `BltBitMapRastPort()` calls now sign-extend negative `WORD` coordinates correctly, and coverage guards the clipped-negative blit path.
* [x] KickPascal: masked splash/logo blits now sign-extend `WORD` source and destination coordinates before clipping, so the `KickPascal` logo stays centered and its right edge remains visible; graphics coverage now guards the negative-coordinate masked blit path and host-side app coverage checks the splash logo bounds.
* [x] KickPascal: console.device now honors the Amiga private `CSI x/y/u/t` geometry controls for explicit left/top offsets and page/line dimensions, so the `Workspace/KBytes (max = ...` prompt starts fully inside the visible text area; console coverage now exercises those private geometry sequences and KickPascal startup coverage keeps the prompt rendering stable.
* [x] KickPascal: console.device now forwards backspace to one-byte cooked reads when there is no buffered user text to erase, so KickPascal can delete the default `200` workspace entry before submitting a replacement value; host-side KickPascal coverage now exercises deleting the default entry and completing startup.
* [x] Import and wire additional real-world app bundles for integration coverage: copied `BlitzBasic2`, `FinalWriter_D`, `ppaint`, `ProWrite`, `SIGMAth2`, `Sonix 2`, `Typeface`, and `vim-5.3` into `../lxa-apps`, added launch configs for each, and added startup smoke coverage for the apps that currently boot under lxa (`FinalWriter_D`, `ppaint`, `ProWrite`, `SIGMAth2`).
* [x] Followed up the imported app startup blockers from the wiring sweep: `ProWrite` now reaches its main window without logging a `hotlinks.library` startup failure, while the still-blocked imports are explicitly parked for future work (`BlitzBasic2` invalid screen dimensions crash, `Sonix 2` missing `serial.device`, `Typeface` missing external `commodities.library`/`datatypes.library`, and `vim-5.3` early invalid-PC crash).

---

## Completed Milestones

- Phase 79 (`0.8.1`): Complete `keyboard.device` surface and add targeted keyboard regressions — done.
- Phase 80 (post-0.8.0): Architecture and performance follow-up, refactors and shared helpers — done.
- Phase 80-2: UI/sample fixes and redraw/menu/gadget regressions — done.
- Phase 81 (`0.8.10`): Directory Opus launch support and host-side UI driver infra — done.
- Phase 82: Headless/rootless sizing and app-specific regressions — done.
- Phases 83–84: `dos.library` and `utility.library` stub closures and verification — done.
- Phases 85–95 (`0.8.37`–`0.8.46`): Library/device stub eradication sweep and final compatibility polish — done.

## Completed Milestones (compact)

- Phase 78 (`0.8.0`): Major compatibility release — AROS/FS-UAE verification, regression sweep, and release sync.
- Phases 1–77: Project foundation, host/ROM infrastructure, testing migration, and incremental compatibility groundwork.
