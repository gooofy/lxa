# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, and roadmap updates on every completed phase.

## Future Phases (placeholders)

- Phase 96: Manual Testing Results

The following issues were discovered when comparing apps running via lxa to running them on a real amiga. All of them need to be investigated thoroughly and fixed.
The goal of this phase is to make all apps run exactly like they would on a real amiga.

* [x] Add a screenshot review helper at `tools/screenshot_review.py` that sends one or more captures to an OpenRouter vision model (using `OPENROUTER_API_KEY`) so manual app-review work can inspect Amiga UI output directly.

* [ ] ASM-One: no screen mode requester on startup (blocked on providing the disk-only `reqtools.library`; tests now cover both the optional-missing path and the disk-provided path via `LXA_TEST_THIRD_PARTY_LIBS`)
* [ ] ASM-One: does not ask for ALLOCATE/WORKSPACE sizes on startup, does not show prompt
* [ ] ASM-One: no menu bar
    _intuition: _render_menu_bar() invalid RastPort BitMap
    _intuition: _render_menu_bar() invalid RastPort BitMap
    _intuition: _render_menu_bar() invalid RastPort BitMap
    _intuition: _render_menu_items() invalid RastPort BitMap
    _intuition: _render_menu_bar() invalid RastPort BitMap
    _intuition: _render_menu_items() invalid RastPort BitMap
    _intuition: _render_menu_bar() invalid RastPort BitMap
    _intuition: _render_menu_items() invalid RastPort BitMap
    _intuition: _render_menu_items() invalid RastPort BitMap
    _intuition: _render_menu_items() invalid RastPort BitMap
    _intuition: _render_menu_bar() invalid RastPort BitMap
    _intuition: _render_menu_items() invalid RastPort BitMap
* [ ] ASM-One: errors on startup
    _intuition: OpenWorkBench() returning, IntuitionBase=0x000113ec
    _dos: LoadSeg() called, name=LIBS:reqtools.library
    _dos: LoadSeg() Open() for name=LIBS:reqtools.library failed
    _exec: OpenLibrary: *** ERROR: requested library reqtools.library was not found.
    _intuition: OpenScreen() newScreen=0x0008041c
    display: opened 800x600x2 virtual display (rootless backing store)
    [ROM] OpenScreen: offsetof(FirstScreen)=60, sizeof(IntuitionBase)=80, sizeof(Library)=34, sizeof(View)=18
    [ROM] OpenScreen: IntuitionBase=0x000113ec, FirstScreen set to 0x0001cb48
    _intuition: OpenWindow() newWindow=0x0008040c
    Title string: 'ASM-One V1.48'
    display: opened rootless window 800x600 'ASM-One V1.48' (SDL ID 4)
    _console: Open() called, unitn=-1, flags=0x00000000, io_Data(Window)=0x00000000
    _console: Opened as CONU_LIBRARY (library mode only, no unit)
* [ ] DPaint V: Ownership information window is empty
* [ ] DPaint V: Screen Format dialog completely garbled, especially the `Choose Display Mode`, `Display Information` and `Credits` section
* [x] Rootless host windows now preserve the screen rows above a logical window, fixing the off-the-top clipping root cause that hid top strips such as Devpac's menu bar.
* [ ] Devpac: Menu bar very flickery / repaint issues
* [ ] Devpac: "About" dialog drawn twice: in separate rootless window *and* inside the main rootless window
* [ ] Devpac: "About" dialog: "Continue" button border is drawn wrong, probably draw order wrong, appears striken through
* [ ] KickPascal: "presents" string drawn at wrong coordinates: collides with logo/"Company" string
* [ ] KickPascal: `KickPascal` logo image off to the left of the screen, also: right part of logo cut off
* [ ] KickPascal: "Workspace/KBytes (max = ..." string draw partially off to the left of the screen
* [ ] KickPascal: cannot delete the default "200" entry for the "Workspace/KBytes" question

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
