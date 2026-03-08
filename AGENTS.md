# lxa Development Guide for Agents

This document is the entry point for AI agents working on the `lxa` codebase. The detailed instructions have been split into specialized skills.

## 1. Core Mandates
- **Stability**: Zero tolerance for crashes.
- **Coverage**: 100% test coverage required.
- **Reference**: ALWAYS consult RKRM and NDK.
- **Complete Phases if possible**: When working on a phase of the roadmap, plan ahead and make sure you create all the TODO items needed to **successfully complete** the phase. Do not stop early, but strive towards reaching the goal of finishing a phase.
- **Keep the roadmap.md file updated**: Whenever you complete a task or finish work, make sure you update the `roadmap.md` file accordingly. Keep it nice, clean and tidy so we always know where we stand and what the next steps are. Summarize, re-organize, create new phases as needed.
- **Host-Side Test Drivers for UI Tests**: All interactive UI tests (gadget clicks, menu selection, keyboard input) MUST use the host-side driver infrastructure (`tests/drivers/` with liblxa) and Google Test. The legacy `test_inject.h` approach has been removed. All tests are integrated into the unified GTest suite.

## 2. Available Skills
Load the specific skill relevant to your task:

- **`develop-amiga`**:
  - AmigaOS implementation rules.
  - Code style (ROM vs System vs Host).
  - Memory management & Stack safety.
  - Debugging tips.

- **`lxa-workflow`**:
  - Roadmap-driven development process.
  - Version management rules (MAJOR.MINOR.PATCH).
  - Build system (`cmake`, `build.sh`) instructions.

- **`lxa-testing`**:
  - General TDD workflow.
  - Integration and Unit testing strategies.
  - **Host-side driver infrastructure** for UI testing.
  - Test requirements checklist.

- **`graphics-testing`**:
  - Specific strategies for headless Graphics/Intuition testing.
  - **Host-side driver patterns** for interactive testing.
  - BitMap verification techniques.

## 3. Build & Test Quick Reference

```bash
# Build (from project root):
./build.sh

# Run full test suite (ALWAYS use -j8 for parallel execution):
ctest --test-dir build --output-on-failure -j8

# Run a specific test:
ctest --test-dir build --output-on-failure -R shell_gtest

# Run specific test cases via GTest filter:
./build/tests/drivers/shell_gtest --gtest_filter="ShellTest.Variables"

# Reliability loop (for debugging intermittent failures):
for i in $(seq 1 50); do
    timeout 30 ./build/tests/drivers/shell_gtest \
        --gtest_filter="ShellTest.Variables" 2>&1 | tail -1
done
```

**Parallelism**: `-j8` is the project default. The suite's longest interactive
drivers are now sharded, which keeps full-suite wall time around the current
~95 second range on this machine class. Higher parallelism is safe, but usually
does not improve wall time meaningfully.

See `lxa-testing` skill for detailed test execution docs and
`doc/test-reliability-report.md` for parallelism benchmarks.

## 4. Quick Start
1. Check `roadmap.md`.
2. Load `lxa-workflow` to understand the process.
3. Load `develop-amiga` for coding standards.
4. Load `lxa-testing` to plan your tests.
5. For UI tests, load `graphics-testing` for host-side driver patterns.
