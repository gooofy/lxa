# lxa Development Guide for Agents

This document is the entry point for AI agents working on the `lxa` codebase. The detailed instructions have been split into specialized skills.

## 1. Core Mandates
- **Stability**: Zero tolerance for crashes.
- **Coverage**: 100% test coverage required.
- **Reference**: ALWAYS consult RKRM and NDK.

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
  - Test requirements checklist.

- **`graphics-testing`**:
  - Specific strategies for headless Graphics/Intuition testing.
  - BitMap verification techniques.

## 3. Quick Start
1. Check `roadmap.md`.
2. Load `lxa-workflow` to understand the process.
3. Load `develop-amiga` for coding standards.
4. Load `lxa-testing` to plan your tests.
