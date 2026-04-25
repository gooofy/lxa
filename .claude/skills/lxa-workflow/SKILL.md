---
name: lxa-workflow
description: Roadmap-driven development process, version management, and build system instructions.
---

# lxa Workflow Skill

This skill guides you through the project management, versioning, and build processes.

## 1. Roadmap Driven Development
**Source of Truth**: `roadmap.md`

### Workflow
1. **Start**: Consult `roadmap.md`. Identify the active phase under `## Next Phase` (or open a new one if the previous next-phase just landed).
2. **Develop**:
   - Write tests first (TDD).
   - Implement minimum code.
   - Ensure no warnings.
   - **Always** strive towards completing the phase you're working on. Create elaborate TODO lists to achieve that, do **not** stop early.
3. **Finish**:
   - Validate (Tests + Coverage). The full suite must be green — see "Done Definition" below.
   - **If you added tests to a sharded driver**: verify you updated the corresponding FILTER string in `tests/drivers/CMakeLists.txt`. Run `tools/check_shard_coverage.py` if available.
   - Update `roadmap.md` per the maintenance rules below.
   - If you intentionally defer unfinished work, add it to the "Deferred Test Failures" or "Known Open Limitations" section with an explicit justification — the deferral must be unambiguous.
   - Remove or rewrite roadmap items that would incorrectly re-implement third-party libraries; document that those libraries must be loaded from disk instead.
   - **RTG phases (137–139)**: `Picasso96API.library` and `cybergraphics.library` are disk libraries — never add P96/CGX ROM stubs. They are compiled via `add_disk_library()` in `sys/CMakeLists.txt`. New RTG bitmap code goes in `lxa_graphics.c` (the `BMF_RTG` chunky path); new P96 API code goes in `lxa_p96.c`.
   - Update `README.md`.
   - Update Version Number.

### Roadmap Maintenance Rules
The roadmap is **future-focused**. Every commit that completes or defers work must keep it that way:

1. **Completed phases** collapse to a single one-line row in the `## Completed Phases (Summary)` table — no verbose write-ups remain in the roadmap (they live in the git commit message instead).
2. **Promote the next phase**: whenever you finish a phase, identify the next phase from the planned-work sections and surface it in the `## Next Phase` heading at the top so the next agent sees it immediately.
3. **Deferred test failures**: any `DISABLED_` GTest must have a corresponding bullet under `## Deferred Test Failures` with the test name, file, root-cause hypothesis, and a short rationale for the deferral.
4. **Length budget**: keep `roadmap.md` under ~250 lines. If it grows past that, compact older completed phases further or move detail to the relevant skill.
5. **No stale "next phase" pointers**: never leave the roadmap with an unclear "what comes next" — that is what triggered Phase 136-c's overhaul.

### "Done" Definition
1. Functionality works.
2. 100% Test Coverage.
3. No warnings.
4. **Full test suite is green**: `ctest --test-dir build -j16 --timeout 180` reports zero failures and zero timeouts. Inherited "pre-existing" failures must either be fixed or formally quarantined as `DISABLED_<TestName>` with a corresponding entry under `## Deferred Test Failures` in `roadmap.md`. Hand-waving "N pre-existing failures, unrelated to my phase" is forbidden.
5. Documentation updated.

## 2. Version Management
**File**: `src/include/lxa_version.h`

**Rules**:
- **MAJOR**: Breaking changes (Reset MINOR/PATCH).
- **MINOR**: New features (Reset PATCH).
- **PATCH**: Bug fixes, refactoring.
- **MUST** increment for every functional commit.
- Keep `LXA_VERSION_STRING` in sync.

## 3. Build System
**Tool**: CMake

### Commands
- **Full Build**: `./build.sh`
- **Install**: `make -C build install`
- **Run Tests**: `ctest --test-dir build --output-on-failure --timeout 180 -j16`

### Artifacts
- Host: `build/host/bin/lxa`
- ROM: `build/target/rom/lxa.rom`
- Commands: `build/target/sys/C/`

### Toolchains
- Host: `gcc`
- Amiga: `m68k-amigaos-gcc` (libnix)
