# LXA Test Reliability and Performance Report

**Version**: 0.6.76 | **Date**: 2026-03-08 | **Status**: CURRENT

## Executive Summary

The current `lxa` test suite exposes **49 CTest entries** and is designed for parallel execution through:

```bash
ctest --test-dir build --output-on-failure -j8
```

The suite is fully isolated: each test uses its own emulator instance and does not depend on shared mutable runtime state. The longest historical UI/application suites have been split into smaller shards, which keeps the default `-j8` run efficient and predictable.

## Current Recommendations

- Use `-j8` as the default parallelism level
- Treat higher parallelism as safe but usually not meaningfully faster
- Run individual drivers directly when you need GTest filters or tighter debugging loops

## Why `-j8` Remains the Default

The suite previously had a few oversized UI drivers that dominated wall time. Those drivers are now sharded into smaller CTest entries such as:
- `simplegad_behavior_gtest` and `simplegad_pixels_gtest`
- `simplemenu_behavior_gtest` and `simplemenu_pixels_gtest`
- `menulayout_behavior_gtest` and `menulayout_pixels_gtest`
- `gadtoolsgadgets_*_gtest` shards
- `cluster2_*_gtest` shards

This rebalancing reduced full-suite wall time from roughly **124 seconds** to roughly **95 seconds** while keeping total CPU cost close to flat. `-j8` remains the best default because it captures the practical speedup without adding noise to local debugging or CI output.

## Timeouts With Explicit Coverage

Several interactive tests have explicit CTest timeouts in `tests/drivers/CMakeLists.txt`. Representative examples include:

| Test | Timeout |
|------|---------|
| `easyrequest_gtest` | 120s |
| `rgbboxes_gtest` | 25s |
| `simplegad_behavior_gtest` | 60s |
| `simplegad_pixels_gtest` | 90s |
| `gadtoolsgadgets_pixels_a_gtest` | 90s |
| `cluster2_navigation_gtest` | 60s |

All other suites use CTest's default timeout unless explicitly overridden.

## Reliability Workflow

When debugging intermittent failures, use focused loops instead of repeatedly running the whole suite:

```bash
for i in $(seq 1 50); do
    timeout 30 ./build/tests/drivers/shell_gtest \
        --gtest_filter="ShellTest.Variables" 2>&1 | tail -1
done
```

Or for repeated suite validation:

```bash
for run in $(seq 1 5); do
    echo "=== Run $run ==="
    ctest --test-dir build --output-on-failure -j8 2>&1 | grep "tests passed"
done
```

## Notes

This document is intentionally a current operational reference, not a full postmortem archive. Historical debugging details belong in Git history and commit messages once the suite behavior has stabilized.
