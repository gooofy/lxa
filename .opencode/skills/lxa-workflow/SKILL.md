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
3. **Finish**:
   - Update `roadmap.md` (`[x]`).
   - Update `README.md`.
   - Update Version Number.
   - Validate (Tests + Coverage).

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
- **Run Tests**: `make -C tests test`

### Artifacts
- Host: `build/host/bin/lxa`
- ROM: `build/target/rom/lxa.rom`
- Commands: `build/target/sys/C/`

### Toolchains
- Host: `gcc`
- Amiga: `m68k-amigaos-gcc` (libnix)
