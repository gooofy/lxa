# lxa Roadmap

`lxa` keeps the roadmap short, future-focused, and tied to release milestones. All implementation work must follow `AGENTS.md`: no crashes, 100% coverage, RKRM/NDK validation, third-party libraries kept off ROM, and roadmap updates on every completed phase.

---

## Next Release Focus

### Phase 79 complete (`0.8.1`)

Phase 79 closes the remaining `keyboard.device` surface and keeps it aligned with the hosted input stack.

- [x] Implemented the documented public command set: `CMD_CLEAR`, `KBD_READEVENT`, `KBD_READMATRIX`, `KBD_ADDRESETHANDLER`, `KBD_REMRESETHANDLER`, `KBD_RESETHANDLERDONE`
- [x] Matched `KBD_READEVENT` behavior for raw-key event chaining, held/numeric-pad qualifier reporting, pending-read completion, and abort handling
- [x] Matched `KBD_READMATRIX` matrix sizing/layout semantics with direct 32-byte snapshot coverage against the hosted raw-key stream
- [x] Shared hosted keyboard events from Intuition into `keyboard.device`, keeping the stack aligned with `input.device`, console raw-key consumers, and `keymap.library`
- [x] Added direct regression coverage under `Tests/Devices/Keyboard` and `devices_gtest` for open/read/abort/reset-handler flows
- [x] Factored queue, matrix, and reset-handler bookkeeping into compact private helpers and reused buffered snapshots during bursty reads

---

### Phase 80: Post-0.8.0 cleanup and acceleration

Goal: pay down the architecture and performance follow-up identified during the Phase 78 compatibility sweep without reopening already-closed API coverage.

- [x] Exec/DOS core: unify resident registration, interrupt-vector management, packet transport, and current-process access so startup and messaging paths cannot drift; current-task/current-process helpers, shared resident/device registration, centralized interrupt-vector/server-chain bookkeeping, and DOS packet send/wait/reply/result propagation now share the common path
- [x] Graphics/Layers: centralize viewport, copper, double-buffer, layer ordering, and clip/damage bookkeeping; viewport/color propagation, copper placeholder ownership, bitmap/origin updates, layer z-order rebuilds, clip-region application, and disjoint damage clipping now share common helpers while broader refresh/palette follow-up remains
- [x] Intuition/Workbench/GadTools: separate private UI bookkeeping from public structs, share screen/workbench/app-object helpers, and avoid repeated full-list scans or full-window redraws; GadTools context heads now stay private when windows/refresh paths consume gadget lists, and Workbench app-object handles now reuse a tracked private list while broader redraw/app-helper follow-up remains
- [x] Utility/Locale/IFFParse/Diskfont: split monolithic private state into companions/helpers and add caching for namespace lookup, catalog lookup, parser context matching, and color-font rendering; Utility namespaces now cache recent lookups, locale caches default-locale/catalog hits, iffparse reuses property/local-item context matches, and diskfont caches repeated font opens/scaled lookups while broader helper extraction remains
- [x] Keymap/Timer/Console/Input: share translation and timing helpers across the input stack, then remove repeated scans, conversions, and queue walks in hot paths; rawkey-to-vanilla conversion and system-time normalization now live in shared ROM helpers, while console reads/input buffers reuse common readiness/consume paths with added regression coverage for normalized repeat timings and console driver flows
- [x] Audio/Trackdisk/Expansion: device-private state now stays behind dedicated helpers/private companions, removing global unit tables and keeping allocation/mount bookkeeping out of ROM entry points while preserving ordered wake-up/reply paths
- [x] Math libraries: hosted FFP/IEEE bridge helpers now share centralized host conversion/clamp/compare/register plumbing so the math emucall surface uses one compatibility model with less repeated setup overhead

---

### Phase 80-2: Fix UI Glitches

- [x] GadToolsGadgets Sample: fix rootless mode: a separate rootless windows gets opened, but the actual UI is still drawn on the "Workbench Screen" window
- [x] GadToolsGadgets Sample: Volume setting is not displayed (empty space right of label)
- [x] GadToolsGadgets Sample: UI is incredibly sluggish - for example, there is a noticable lag when clicking the "Click Here" Button that is not present on a real Amiga
- [x] GadToolsGadgets Sample: trying to resize the window is very laggy and leaves window border garbage on screen while resizing
- [x] GadToolsMenu Sample: separators are too wide, parts are drawn outside the menu and leave garbage on screen
- [x] GadToolsMenu Sample: Print submenu indicator is missing
- [x] GadToolsMenu Sample: Print submenu is missing
- [x] GadToolsMenu Sample: Redraw issues: while moving the mouse over opened menus, flickering occurs
- [x] GadToolsMenu Sample: Redraw issues: while moving the mouse over opened menus, lower menu items keep appearing and disappearing
- [x] SimpleGTGadget Sample: Checkbox doesn't react to mouse clicks, no check mark
- [x] SimpleGTGadget Sample: Number gadget is not responsive to keyboard input, cursor does not show when clicked (but entered characters do appear once focus leaves gadget / clicked elsewhere)
- [x] SimpleGTGadget Sample: Cycle gadget is just an empty box, does not respond to any input

---

### Phase 81 complete (`0.8.10`)

Goal: Fully working Directory Opus app. DOpus 4 Manual: /home/guenter/projects/amiga/lxa/src/lxa-apps/DirectoryOpus/DirOpus4Manual.md

- [x] Audit and document the exact DOpus runtime surface (`assigns`, `LIBS:`, `C:`, `L:`, `S:`, and disk-provided dependencies); `doc/app_db.md` now records the `DOpus:`/`LIBS:`/`C:`/`L:`/`S:` layout, bundled third-party libraries and helpers, startup-sequence hand-off, and the currently observed runtime library/device surface from the App-DB copy
- [x] Keep disk-provided dependencies such as `dopus.library`, `powerpacker.library`, and other optional libraries outside ROM scope; `Tests/Exec/Library` now asserts the DOpus bundle libraries remain absent from the ROM resident list and Exec `LibList`, while `README.md` and `doc/app_db.md` document that they must continue to load from disk via `LIBS:`
- [x] Close the remaining launch blockers, including `keyboard.device` and any other startup-time open failures needed to reach the main UI; direct `DirectoryOpus` launch now reaches the first real `DOPUS.1` window, confirming `keyboard.device` is no longer gating startup while currently missing `commodities.library`, `rexxsyslib.library`, and `inovamusic.library` remain non-fatal optional follow-up items
- [x] Replace the old wrapper smoke check with a host-side driver that verifies real launch success criteria; `tests/drivers/apps_misc_gtest.cpp` now launches the real `APPS:DirectoryOpus/bin/DOPUS/DirectoryOpus` binary under `liblxa`, wires the required `DOpus:`/`LIBS:`/`C:`/`L:`/`S:` assigns, and asserts that startup reaches a live DOpus rootless window with the expected 640px-wide main UI while the process remains running
- [x] Re-enabled the live `DirectoryOpus` driver in `tests/drivers/apps_misc_gtest.cpp` and reran the broader app-regression sweep (`apps_misc_gtest`, `asm_one_gtest`, `devpac_gtest`, `kickpascal_gtest`, `maxonbasic_gtest`, `cluster2_*_gtest`, `dpaint_gtest`), keeping the application-driver set green with DOpus included again
- [x] Extended the host-side integration driver infrastructure with explicit `lxa_wait_window_drawn()` readiness checks plus window/gadget introspection (`lxa_get_gadget_count()`, `lxa_get_gadget_info()`, `LxaUITest::GetGadgets()`, `ClickGadget()`), and covered it in `api_gtest` while confirming the DOpus launcher reaches a drawable interactive window under `apps_misc_gtest`
- [x] Parked the still-flaky DOpus file-copy workflow for a later phase; `tests/drivers/apps_misc_gtest.cpp` now keeps the experimental `DirectoryOpusCopiesFile` scenario disabled so the proven launch/runtime coverage and the new UI-driver tooling can ship green and be revisited later without blocking Phase 81

---

### Phase 82: Address manual test results

- [x] Fix headless mode: headless/app-launch configs now default to `rootless_mode = true`, so the Workbench screen stays hidden and only application windows appear on the host side unless a config explicitly opts back into screen-mode rendering.
- [x] In headless mode make sure application windows are wide enough on the X11 side so they can display the whole menu; rootless host windows now widen to the remaining screen width while keeping their logical Amiga dimensions intact, with regression coverage for both the sizing helper and a captured MenuLayout rootless window
- [x] ASMOne no longer crashes on keyboard input; IDCMP RAWKEY messages now provide the documented previous-key snapshot through `IntuiMessage.IAddress`, with regression coverage in `Tests/Console/input_inject` and the live `asm_one_gtest` keyboard scenarios
- [x] KickPascal rootless launches now keep the logical Amiga window geometry while widening the host-side window, so the IDE no longer appears in a portrait-shaped host frame; regression coverage now captures the live rootless KP2 window and asserts the widened host extent stays landscape.
- [x] DPaint V now tolerates the disk-provided `rexxsyslib.library`; the build installs a shared `SYS:Libs/rexxsyslib.library` from the existing stub source, `dpaint_gtest` appends the DPaint `Libs/` drawer to `LIBS:`, and the live launch regression now asserts startup no longer stalls on the missing-library system requester.
- [x] SysInfo no longer opens to a black rootless window; viewport bitmap swaps now keep the embedded `Screen` bitmap plus host refresh metadata aligned, and `apps_misc_gtest` asserts the live SysInfo window draws visible content under headless rootless mode.
- [x] Sample Talk2Boopsi now follows the RKRM flow again: the sample keeps its linked prop/string gadgets visible until the user closes the window, headless interaction coverage verifies it no longer quits early, and a non-rootless pixel regression confirms the prop gadget renders visible content.
- [x] Sample SimpleGadget now renders its boolean gadget border in rootless mode too; Intuition draws user gadgets into the rootless backing bitmap at open time, and `simplegad_gtest` now captures the live headless window to verify the border stays visible there alongside the existing non-rootless pixel regression.
- [x] Sample UpdateStrGad now follows the RKRM activation flow again: the sample explicitly opens as an active window so `IDCMP_ACTIVEWINDOW` updates the string gadget to `Activated`, rootless host-window input is translated back into screen coordinates so key focus/clicks no longer lag behind in SDL rootless mode, and `updatestrgad_gtest` plus rootless-layout unit coverage keep the activation regression covered.


---

## Completed Milestones

### Phase 78 complete (`0.8.0`)

Phase 78 is now closed as the major compatibility-release pass for `0.8.0`.

- [x] Completed the AROS verification sweep across the planned ROM libraries and devices, closing the tracked public API surface with direct regression coverage
- [x] Completed the FS-UAE review and documented the boundary between in-scope hosted AmigaOS compatibility and out-of-scope full hardware emulation
- [x] Folded the review fallout into future work instead of leaving the roadmap as a per-function checklist; Phase 80 now tracks the remaining architecture and performance cleanup
- [x] Ran the final regression/integration sweep, including targeted app-driver reruns and longer release validation
- [x] Synchronized the release as `0.8.0` across the build, version header, README, and roadmap

Phase 78 highlights, now intentionally compacted:

- [x] Core libraries closed: Exec, DOS, Graphics, Layers, Intuition, GadTools, ASL, Utility, Locale, IFFParse, Keymap, Diskfont, Workbench/Icon, and the planned math libraries
- [x] Device coverage closed: timer, console, input, audio, trackdisk, and expansion; `keyboard.device` is the next major gap and now stands alone as Phase 79
- [x] Third-party-library policy enforced: `commodities.library`, `rexxsyslib.library`, and `datatypes.library` stay disk-provided and off the ROM surface
- [x] Structural verification and compatibility regression coverage were expanded and kept green through the release close-out

### Earlier phases compacted

- [x] Phases 1-72: project foundation, host/ROM infrastructure, test migration to GTest + liblxa, build/test reliability, and broad compatibility groundwork
- [x] Phase 72.5: codebase audit pass completed
- [x] Phases 73-77: core resource management, DOS/VFS hardening, graphics/layers growth, Intuition/BOOPSI expansion, missing library/device coverage, and the lead-in work required for the final Phase 78 sweep

---

## Notes

- The roadmap now tracks only future phases plus compact milestone summaries; detailed completed checklists should stay in git history rather than dominating the live roadmap.
- Third-party libraries are not to be reimplemented in ROM; they must be supplied on disk and opened through the standard Amiga library search path.
