# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, and roadmap updates on every completed phase.

## Current Phases

- Phase 97: Complete set of devices

Goal: Complete the implementation of standard devices as far as possible within LXA

- [ ] scsi.device
- [x] parallel.device
- [x] serial.device
- [x] narrator.device
- [x] ramdrive.device
- [x] printer.device

---

## Completed Milestones

- Phase 79 (`0.8.1`): Complete `keyboard.device` surface and add targeted keyboard regressions — done.
- Phase 80 (post-0.8.0): Architecture and performance follow-up, refactors and shared helpers — done.
- Phase 80-2: UI/sample fixes and redraw/menu/gadget regressions — done.
- Phase 81 (`0.8.10`): Directory Opus launch support and host-side UI driver infra — done.
- Phase 82: Headless/rootless sizing and app-specific regressions — done.
- Phases 83–84: `dos.library` and `utility.library` stub closures and verification — done.
- Phases 85–95 (`0.8.37`–`0.8.46`): Library/device stub eradication sweep and final compatibility polish — done.
- Phase 96: App testing

## Completed Milestones (compact)

- Phase 78 (`0.8.0`): Major compatibility release — AROS/FS-UAE verification, regression sweep, and release sync.
- Phases 1–77: Project foundation, host/ROM infrastructure, testing migration, and incremental compatibility groundwork.

## Future Phases (per-app)

For each app below the agent should follow a test-driven workflow: (1) run or create a reliable startup host-side driver test to exercise launch and reachability; (2) if the driver reveals failures, fix them and keep the test green; (3) capture screenshots on failure and use `tools/screenshot_review.py` to identify UI/rendering issues and hypotheses; (4) author additional automated pixel or interaction tests that assert the UI issues are fixed; (5) implement fixes and iterate until tests are green.

- Phase 96: Asm-One
	- Run or create a host-side startup test (GTest driver) that launches Asm-One and asserts the main window opens.
	- If launch or runtime failures occur, fix them and keep the startup test green.
	- On visual regressions, capture window artifacts and run `tools/screenshot_review.py` to triage rendering or layout issues; add pixel/introspection tests accordingly and fix the root cause.
	- Add interaction tests: open About dialog via menu, verify About renders and closes; test keyboard input in editor buffer; test open-file requester flow.

- Phase 99: Cluster2
	- Create or extend a host-side driver to launch Cluster2 and assert its main UI appears.
	- Fix any startup or resource-load issues discovered by the driver.
	- Use `lxa_capture_window()` and `tools/screenshot_review.py` for visual triage; convert findings into automated pixel or geometry assertions.
	- Add tests for About dialog, basic file open, and a simple action (e.g., open a project file and verify expected UI change).

- Phase 100: DirectoryOpus (follow-up)
	- Ensure existing DirectoryOpus startup test reaches the main rootless window; fix regressions if any.
	- Use screenshot review for menu/layout quirks and add pixel assertions for key UI regions (toolbar, main list area).
	- Add interaction tests: open About, trigger Preferences/requesters, and verify closing behavior.

- Phase 101: DPaintV
	- Create a startup test that launches DPaintV and verifies the canvas window appears.
	- Fix crashes, missing palettes, or widget rendering failures revealed by the driver.
	- Use screenshot analysis for palette/menu/brush-rendering issues and add pixel checks for a few representative operations (draw a short line, open palette requester).
	- Add tests for About dialog, Open/Save requester, and simple keyboard shortcuts.

- Phase 102: kickpascal2
	- Add a host-side driver that launches KickPascal2 and asserts the IDE window and menus open.
	- Resolve any rootless-geometry, menu, or keyboard focus problems the driver reveals.
	- Run `tools/screenshot_review.py` on captures to localize rendering faults and add automated UI tests: open Help/About, type into the editor, open file requester.

- Phase 103: MaxonBASIC
	- Create a startup test that ensures MaxonBASIC launches and the editor/console are reachable.
	- Fix any immediate startup, input, or display problems found by the driver.
	- Use screenshot-review to identify rendering issues (font/line wrap/menu clipping) and add pixel/geometry assertions.
	- Add interaction tests: About dialog, run a short BASIC program and assert expected output in console.

- Phase 104: ProWrite
	- Add or run a startup driver that verifies ProWrite opens its main document window.
	- Fix launch-time or editor rendering issues revealed by the test.
	- Use screenshot analysis for text rendering, caret placement, and menu layout; add relevant automated checks.
	- Add tests: open About, open file requester, basic typing and save/close cycle.

- Phase 105: Sonix 2
	- Create a host-side test to launch Sonix 2 and confirm the main UI appears.
	- Address any audio/device startup interactions or UI rendering problems uncovered.
	- Use screenshot-review for UI layout/toolbar/icon issues and add pixel/assertion tests.
	- Add interaction tests for About dialog, basic settings dialog, and simple playback UI controls if applicable.

- Phase 106: vim-5.3
	- Add a startup test that runs `vim-5.3` under lxa, opens a file, and verifies the editor draws (cursor, mode indicator).
	- Fix keyboard/mode-switch or rendering bugs revealed by the test.
	- Use screenshot analysis for caret rendering and status-line display; add automated checks for typing and saving a small file.

- Phase 107: BlitzBasic2
	- Launch BlitzBasic2 via a host-side driver and assert the IDE/window opens.
	- Fix any initialization, input, or UI drawing issues found by the test.
	- Use screenshot-review to inspect IDE layout and add pixel/interaction tests: About dialog, project open, and a small script run.

- Phase 108: DevPac
	- Create/extend a host-side driver to launch DevPac and verify the editor/assembler window appears.
	- Repair crashes or display failures surfaced by the driver.
	- Use `tools/screenshot_review.py` for visual triage and add tests for About dialog, open-file requester, and assemble/run basic example.

- Phase 109: FinalWriter_D
	- Add a startup test for FinalWriter_D, confirming its main document/editor window renders.
	- Fix font/rendering and dialog issues exposed by the driver.
	- Use screenshot analysis for text/layout defects and add automated tests for About, open/save requester, and simple typing.

- Phase 110: ppaint
	- Create a host-side test to start ppaint and assert canvas availability.
	- Fix palette, brush, or menu rendering problems found.
	- Use screenshot-review to locate drawing artefacts and add pixel tests for a small draw sequence and requester flows.

- Phase 111: SIGMAth2
	- Launch SIGMAth2 with a driver that asserts the main UI loads.
	- Fix any initial rendering, math-widget, or menu issues found.
	- Use screenshot analysis to capture UI anomalies and add tests for About dialog, input to math widgets, and file requester.

- Phase 112: SysInfo
	- Ensure the existing SysInfo launch test verifies its window draws; fix any rendering regressions.
	- Use `tools/screenshot_review.py` on captures for bitmap/viewport swap issues and add pixel-based assertions for key widgets.
	- Add interaction tests: About dialog, refresh/stat update, and close behavior.

- Phase P: Typeface
	- Add a startup test that opens Typeface and verifies the font preview/render area is visible.
	- Fix missing-font, scaling, or render bugs uncovered by the driver.
	- Use screenshot-review for glyph rendering issues and add tests for opening font requesters, About, and preview zoom/close.

Each phase must include updated GTest host-side drivers under `tests/drivers/`, regression assertions, and saved failure artifacts used to justify added tests.  Use `tools/screenshot_review.py` interactively during development, then translate findings into deterministic pixel/geometry assertions so CI can detect regressions without the vision model.
