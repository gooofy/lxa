# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, and roadmap updates on every completed phase.

## Current Phases

- No active device-completion phase; next queued work is Phase 104 SIGMAth2 app-focused follow-up coverage.

## Completed Milestones

- Phase 79 (`0.8.1`): Complete `keyboard.device` surface and add targeted keyboard regressions — done.
- Phase 80 (post-0.8.0): Architecture and performance follow-up, refactors and shared helpers — done.
- Phase 80-2: UI/sample fixes and redraw/menu/gadget regressions — done.
- Phase 81 (`0.8.10`): Directory Opus launch support and host-side UI driver infra — done.
- Phase 82: Headless/rootless sizing and app-specific regressions — done.
- Phases 83–84: `dos.library` and `utility.library` stub closures and verification — done.
- Phases 85–95 (`0.8.37`–`0.8.46`): Library/device stub eradication sweep and final compatibility polish — done.
- Phase 96: App testing
- Phase 97 (`0.8.56`): Complete device set with hosted `parallel.device` and `scsi.device` coverage plus device/exec regressions — done.
- Phase 98 (`0.8.57`): ProWrite app testing — done.
  - 6 passing tests: window startup, document region rendering, caret column, menu open/close, About dialog, file requester.
  - 1 skipped test: typing injection triggers AddTail crash with corrupt list pointer (ROM compatibility issue — to be fixed in a future phase).
  - Fixed PPM capture limitation by switching ProWrite tests to pen-based pixel verification.
  - Documented ASL requester dismiss limitation in headless mode.
- Phase 99 (`0.8.58`): PNG screenshot artifact migration for hosted integration tests and review tooling — done.
  - Replaced host screenshot capture output with PNG across `display.c`, `liblxa`, and GTest drivers.
  - Updated screenshot-review and UI-testing documentation to use PNG examples and expectations.
  - Kept visual-debugging and reference-comparison paths working while avoiding accidental agent text reads of raw PPM data.
  - Hardened IDCMP posting so closed windows no longer recreate transient window state during late close/input delivery, fixing the intermittent `GadToolsMenuTest.CloseGadget` `_exec_AddTail` corruption.
- Phase 99 (`0.8.60`): Sonix 2 startup compatibility and CIA resource callable surface — done.
  - Added callable `ciaa.resource`/`ciab.resource` vectors (`AddICRVector`, `RemICRVector`, `AbleICR`, `SetICR`) instead of bare resource nodes so hardware-aware apps can complete startup probing.
  - Extended exec resource coverage to validate the CIA resource callable surface and mask bookkeeping.
  - Enabled and strengthened the Sonix 2 host-side startup driver to verify the main window opens, draws visible content, and stays running.
- Phase 100 (`0.8.61`): vim-5.3 startup/editing coverage and hosted console input bridge — done.
- Phase 101 (`0.8.63`): BlitzBasic2 UI fixes and extended interaction coverage — done.
- Phase 102 (`0.8.64`): SysInfo rootless gadget coverage and viewport-backed interaction fixes — done.
- Phase 103-A (`0.8.65`): FinalWriter_D app testing and UI fixes — done.
  - Startup dialog requester opens, accepts "Same as Workbench" selection, and transitions to full editor window.
  - Fixed WA_PubScreen/WA_PubScreenName fall-through, SA_Pens/SA_PubName/SA_DisplayID/SA_Colors32, per-screen DrawInfo/pens, gadgetclass OM_SET, NULL tf_CharLoc glyph rendering, and NEWLIST for pr_MsgPort.
  - 3 passing tests: startup window, editor region capture, accept-dialog-opens-editor.
  - Extended screen_basic test with SA_Pens, DrawInfo, and WA_PubScreen/WA_PubScreenName coverage.
- Phase 103-B (`0.8.66`): ppaint startup coverage and menu smoke interaction — done.
  - Added a dedicated `ppaint_gtest` host-side driver with startup, no-missing-library, and right-menu interaction coverage.
  - Verified ppaint reaches its main paint window, stays running after startup, and survives RMB menu interaction without losing the main window.
  - Deeper screenshot/pixel assertions remain deferred until the screenshot API can capture ppaint's app-owned screens.

## Completed Milestones (compact)

- Phase 78 (`0.8.0`): Major compatibility release — AROS/FS-UAE verification, regression sweep, and release sync.
- Phases 1–77: Project foundation, host/ROM infrastructure, testing migration, and incremental compatibility groundwork.

## Future Phases (per-app)

For each app below the agent should follow a test-driven workflow: (1) run or create a reliable startup host-side driver test to exercise launch and reachability; (2) if the driver reveals failures, fix them and keep the test green; (3) capture screenshots on failure and use `tools/screenshot_review.py` to identify UI/rendering issues and hypotheses; (4) author additional automated pixel or interaction tests that assert the UI issues are fixed; (5) implement fixes and iterate until tests are green.


- Phase 104: SIGMAth2
	- Launch SIGMAth2 with a driver that asserts the main UI loads.
	- Fix any initial rendering, math-widget, or menu issues found.
	- Use screenshot analysis to capture UI anomalies and add tests for About dialog, input to math widgets, and file requester.
	- Update launch.json for SIGMAth2 and the other apps, make sure assigns and other env settings match those of the test drivers so manual testing runs them using the same conditions as the automated test.

- Phase 105: Asm-One
	- Run or create a host-side startup test (GTest driver) that launches Asm-One and asserts the main window opens.
	- If launch or runtime failures occur, fix them and keep the startup test green.
	- On visual regressions, capture window artifacts and run `tools/screenshot_review.py` to triage rendering or layout issues; add pixel/introspection tests accordingly and fix the root cause.
	- Add interaction tests: open About dialog via menu, verify About renders and closes; test keyboard input in editor buffer; test open-file requester flow.
	- Update launch.json for SIGMAth2 and the other apps, make sure assigns and other env settings match those of the test drivers so manual testing runs them using the same conditions as the automated test.

- Phase 106: Cluster2
	- Create or extend a host-side driver to launch Cluster2 and assert its main UI appears.
	- Fix any startup or resource-load issues discovered by the driver.
	- Use `lxa_capture_window()` and `tools/screenshot_review.py` for visual triage; convert findings into automated pixel or geometry assertions.
	- Add tests for About dialog, basic file open, and a simple action (e.g., open a project file and verify expected UI change).
	- Update launch.json for SIGMAth2 and the other apps, make sure assigns and other env settings match those of the test drivers so manual testing runs them using the same conditions as the automated test.

- Phase 107: DirectoryOpus (follow-up)
	- Ensure existing DirectoryOpus startup test reaches the main rootless window; fix regressions if any.
	- Use screenshot review for menu/layout quirks and add pixel assertions for key UI regions (toolbar, main list area).
	- Add interaction tests: open About, trigger Preferences/requesters, and verify closing behavior.
	- Update launch.json for SIGMAth2 and the other apps, make sure assigns and other env settings match those of the test drivers so manual testing runs them using the same conditions as the automated test.

- Phase 108: DPaintV
	- Create a startup test that launches DPaintV and verifies the canvas window appears.
	- Fix crashes, missing palettes, or widget rendering failures revealed by the driver.
	- Use screenshot analysis for palette/menu/brush-rendering issues and add pixel checks for a few representative operations (draw a short line, open palette requester).
	- Add tests for About dialog, Open/Save requester, and simple keyboard shortcuts.
	- Update launch.json for SIGMAth2 and the other apps, make sure assigns and other env settings match those of the test drivers so manual testing runs them using the same conditions as the automated test.

- Phase 109: kickpascal2
	- Add a host-side driver that launches KickPascal2 and asserts the IDE window and menus open.
	- Resolve any rootless-geometry, menu, or keyboard focus problems the driver reveals.
	- Run `tools/screenshot_review.py` on captures to localize rendering faults and add automated UI tests: open Help/About, type into the editor, open file requester.
	- Update launch.json for SIGMAth2 and the other apps, make sure assigns and other env settings match those of the test drivers so manual testing runs them using the same conditions as the automated test.

- Phase 110: MaxonBASIC
	- Create a startup test that ensures MaxonBASIC launches and the editor/console are reachable.
	- Fix any immediate startup, input, or display problems found by the driver.
	- Use screenshot-review to identify rendering issues (font/line wrap/menu clipping) and add pixel/geometry assertions.
	- Add interaction tests: About dialog, run a short BASIC program and assert expected output in console.
	- Update launch.json for SIGMAth2 and the other apps, make sure assigns and other env settings match those of the test drivers so manual testing runs them using the same conditions as the automated test.

- Phase 111: DevPac
	- Create/extend a host-side driver to launch DevPac and verify the editor/assembler window appears.
	- Repair crashes or display failures surfaced by the driver.
	- Use `tools/screenshot_review.py` for visual triage and add tests for About dialog, open-file requester, and assemble/run basic example.
	- Update launch.json for SIGMAth2 and the other apps, make sure assigns and other env settings match those of the test drivers so manual testing runs them using the same conditions as the automated test.

- Phase 113: Typeface
	- Add a startup test that opens Typeface and verifies the font preview/render area is visible.
	- Fix missing-font, scaling, or render bugs uncovered by the driver.
	- Use screenshot-review for glyph rendering issues and add tests for opening font requesters, About, and preview zoom/close.
	- Update launch.json for SIGMAth2 and the other apps, make sure assigns and other env settings match those of the test drivers so manual testing runs them using the same conditions as the automated test.

Each phase must include updated GTest host-side drivers under `tests/drivers/`, regression assertions, and saved failure artifacts used to justify added tests.  Use `tools/screenshot_review.py` interactively during development, then translate findings into deterministic pixel/geometry assertions so CI can detect regressions without the vision model.
