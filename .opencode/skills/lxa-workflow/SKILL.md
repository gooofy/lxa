---
name: lxa-workflow
description: Roadmap-driven development process, version management, and build system instructions.
---

# lxa Workflow Skill

This skill guides you through the project management, versioning, and build processes.

## 1. Roadmap Driven Development
**Source of Truth**: `roadmap.md`

### Workflow
1. **Start**: Consult `roadmap.md`. Identify Phase/Task.
2. **Develop**:
   - Write tests first (TDD).
   - Implement minimum code.
   - Ensure no warnings.
   - **Always** strive towards completing the phase you're working on. Create elaborate TODO lists to achieve that, do **not** stop early.
3. **Finish**:
   - Validate (Tests + Coverage).
   - **If you added tests to a sharded driver**: verify you updated the corresponding FILTER string in `tests/drivers/CMakeLists.txt`. Run `tools/check_shard_coverage.py` if available.
   - Update `roadmap.md` (`[x]`), compact/summarize done tasks, keep it clean and focused on the future.
   - If you intentionally defer unfinished work, rewrite the roadmap entry so the deferral is explicit and the phase status remains unambiguous.
   - Remove or rewrite roadmap items that would incorrectly re-implement third-party libraries; document that those libraries must be loaded from disk instead.
   - Update `README.md`.
   - Update Version Number.

### "Done" Definition
1. Functionality works.
2. 100% Test Coverage.
3. No warnings.
4. All tests pass.
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
