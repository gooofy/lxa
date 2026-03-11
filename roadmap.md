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

- [ ] Exec/DOS core: unify resident registration, interrupt-vector management, packet transport, and current-process access so startup and messaging paths cannot drift; current-task/current-process helpers, shared resident/device registration, centralized interrupt-vector/server-chain bookkeeping, and DOS packet send/wait/reply/result propagation now share the common path
- [ ] Graphics/Layers: centralize viewport, copper, double-buffer, layer ordering, and clip/damage bookkeeping; viewport/color propagation, copper placeholder ownership, bitmap/origin updates, layer z-order rebuilds, clip-region application, and disjoint damage clipping now share common helpers while broader refresh/palette follow-up remains
- [ ] Intuition/Workbench/GadTools: separate private UI bookkeeping from public structs, share screen/workbench/app-object helpers, and avoid repeated full-list scans or full-window redraws; GadTools context heads now stay private when windows/refresh paths consume gadget lists, and Workbench app-object handles now reuse a tracked private list while broader redraw/app-helper follow-up remains
- [ ] Utility/Locale/IFFParse/Diskfont: split monolithic private state into companions/helpers and add caching for namespace lookup, catalog lookup, parser context matching, and color-font rendering
- [ ] Keymap/Timer/Console/Input: share translation and timing helpers across the input stack, then remove repeated scans, conversions, and queue walks in hot paths
- [ ] Audio/Trackdisk/Expansion: isolate device-private state from ROM entry points and replace repeated host rescans/requeues with cached handles and ordered wake-up bookkeeping
- [ ] Math libraries: consolidate hosted FFP/IEEE bridge helpers so the math libraries share one compatibility model and avoid repeated conversion/setup overhead

---

### Phase 81: Directory Opus follow-up

Goal: restore meaningful Directory Opus app coverage once the remaining external runtime dependencies are satisfied in a policy-correct way.

- [ ] Audit and document the exact DOpus runtime surface (`assigns`, `LIBS:`, `C:`, `L:`, `S:`, and disk-provided dependencies)
- [ ] Keep disk-provided dependencies such as `dopus.library`, `powerpacker.library`, and other optional libraries outside ROM scope
- [ ] Close the remaining launch blockers, including `keyboard.device` and any other startup-time open failures needed to reach the main UI
- [ ] Replace the old wrapper smoke check with a host-side driver that verifies real launch success criteria
- [ ] Re-enable `DISABLED_DirectoryOpus` and rerun the broader app-regression sweep once the driver is stable

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
