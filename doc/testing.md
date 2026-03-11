# Testing Guide

This document describes the current test workflow for `lxa`. The authoritative path is the unified CMake/CTest build, with host-side Google Test drivers for interactive and application coverage and Unity-based unit tests for focused host-side helpers.

## Overview

The repository currently exposes **49 CTest entries**:
- **Google Test drivers** in `tests/drivers/` for DOS, Exec, Shell, graphics, Intuition, device, sample, and application coverage
- **Sharded UI/application suites** for the longest interactive tests so `ctest -j16` stays efficient
- **Unity unit tests** in `tests/unit/` for focused host-side logic such as VFS and configuration parsing

Interactive UI tests must use the host-side `liblxa` driver infrastructure. The legacy in-ROM `test_inject.h` approach is obsolete and must not be used for new work.

## Quick Start

### Build Everything

```bash
./build.sh
```

### Run the Full Test Suite

```bash
ctest --test-dir build --output-on-failure --timeout 60 -j16
```

`-j16` is the project default. The suite is fully isolated, and higher parallelism is safe, but it does not materially improve wall time on the current sharded test layout.

### Run One CTest Entry

```bash
ctest --test-dir build --output-on-failure --timeout 60 -R shell_gtest
```

### Run Specific GTest Cases

```bash
./build/tests/drivers/shell_gtest --gtest_filter="ShellTest.Variables"
./build/tests/drivers/dos_gtest --gtest_filter="DosTest.Lock*"
./build/tests/drivers/exec_gtest --gtest_list_tests
```

### Reliability Loops

```bash
for i in $(seq 1 50); do
    timeout 30 ./build/tests/drivers/shell_gtest \
        --gtest_filter="ShellTest.Variables" 2>&1 | tail -1
done
```

## Test Types

### Host-Side Google Test Drivers

Location: `tests/drivers/`

Use these for:
- interactive UI verification
- app compatibility coverage
- direct DOS/Exec/graphics/Intuition regression coverage
- pixel or output assertions through `liblxa`

Typical driver capabilities:
- launch Amiga programs with `liblxa`
- wait for windows and query geometry
- inject mouse and keyboard input
- capture textual output
- run deterministic VBlank-driven execution

Example binaries include `shell_gtest`, `dos_gtest`, `graphics_gtest`, `devpac_gtest`, `kickpascal_gtest`, and the sharded gadget/menu suites.

### Unity Unit Tests

Location: `tests/unit/`

These tests remain useful for small host-side helpers and focused logic that does not need a full emulator instance. They are built by the main CMake flow and registered with CTest as `unit_*` entries.

Run only the unit tests with:

```bash
ctest --test-dir build --output-on-failure --timeout 60 -R '^unit_'
```

## Writing New Tests

### For Interactive UI Work

Add or extend a Google Test driver in `tests/drivers/`. Use the `LxaUITest` base class and the `liblxa` helpers from `lxa_test.h`.

Preferred pattern:
1. load the program
2. wait for the expected windows
3. run a few VBlank iterations so the task reaches its event loop
4. inject input
5. run more VBlank iterations
6. assert on output, window state, or pixels

### For Non-Interactive Functional Coverage

Prefer extending an existing driver such as `dos_gtest.cpp`, `exec_gtest.cpp`, `commands_gtest.cpp`, or `graphics_gtest.cpp`. These suites already wrap the m68k-side regression programs and match the current project workflow.

### For Focused Host-Side Helpers

If the code is pure host logic and does not need a running emulator, extend `tests/unit/` and register the new executable in `tests/unit/CMakeLists.txt`.

## Coverage

Coverage support is driven from the CMake build:

```bash
cmake -S . -B build -DCOVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
cmake --build build --target coverage
```

The HTML report is generated at:

```text
build/coverage_report/index.html
```

Project expectations remain strict:
- new behavior needs automated coverage
- critical paths should reach 100% coverage
- documentation must be updated alongside new tests or workflow changes

## Debugging Failures

### Re-run a Single CTest Entry Verbosely

```bash
ctest --test-dir build --output-on-failure --timeout 60 -V -R console_gtest
```

### Run a Driver Directly

```bash
./build/tests/drivers/devpac_gtest --gtest_filter="DevpacTest.*"
```

### Debug the Host Emulator

```bash
gdb --args ./build/host/bin/lxa SYS:System/Shell
```

### Useful Failure Tools

- use `-v` or direct driver execution when you need more logs
- use screenshots or pixel assertions from UI drivers when visual output matters
- use reliability loops for intermittent failures
- use `valgrind` or `perf` for host-side memory and performance investigations

## Pre-Commit Validation

Before finishing substantial work:

```bash
./build.sh
ctest --test-dir build --output-on-failure --timeout 60 -j16
```

If you changed only one subsystem, run the targeted suite first, then run the full suite when the work is ready.
