#!/usr/bin/env python3
"""
check_shard_coverage.py - Verify that all TEST_F entries in sharded GTest driver
source files are covered by a FILTER string in tests/drivers/CMakeLists.txt.

Exit code 0: all tests are covered.
Exit code 1: one or more tests are orphaned (not in any FILTER string).

Usage:
    python tools/check_shard_coverage.py [--drivers-dir PATH] [--cmake PATH]
"""

import argparse
import os
import re
import sys


def find_project_root():
    """Walk up from the script location to find the project root (contains build.sh)."""
    here = os.path.dirname(os.path.abspath(__file__))
    candidate = here
    for _ in range(5):
        if os.path.isfile(os.path.join(candidate, "build.sh")):
            return candidate
        candidate = os.path.dirname(candidate)
    # Fall back to parent of tools/
    return os.path.dirname(here)


def collect_sharded_targets(cmake_path):
    """
    Parse CMakeLists.txt for add_gtest_driver_target() calls (which mark a binary
    as sharded) and add_gtest_driver_shard() FILTER strings.

    Returns:
        sharded_targets: set of target names (e.g. "gadtoolsgadgets_gtest")
        filter_tests: dict {target_name: set of "ClassName.TestName"}
    """
    with open(cmake_path, "r") as f:
        content = f.read()

    # Collect targets declared with add_gtest_driver_target()
    sharded_targets = set(re.findall(r"add_gtest_driver_target\((\w+)\)", content))

    # Collect all FILTER strings and map them back to their target
    filter_tests = {}
    # Match: add_gtest_driver_shard(SHARD_NAME TARGET_NAME ... FILTER "..." ...)
    shard_pattern = re.compile(
        r"add_gtest_driver_shard\(\s*(\w+)\s+(\w+)"  # shard name, target name
        r"(?:[^)]*?)FILTER\s+\"([^\"]+)\"",          # FILTER "..."
        re.DOTALL,
    )
    for m in shard_pattern.finditer(content):
        target = m.group(2)
        filter_str = m.group(3)

        # A filter string can be "Class.*" (wildcard) or "A:B:C" (explicit list)
        entries = [e.strip() for e in filter_str.split(":")]
        if target not in filter_tests:
            filter_tests[target] = set()
        for entry in entries:
            filter_tests[target].add(entry)

    return sharded_targets, filter_tests


def collect_test_cases(cpp_path):
    """
    Extract TEST_F(ClassName, TestName) entries from a .cpp file.
    Skips DISABLED_ tests (GTest skips them by default; they don't need filter coverage).
    Returns list of "ClassName.TestName" strings.
    """
    tests = []
    pattern = re.compile(r"^TEST_F\(\s*(\w+)\s*,\s*(\w+)\s*\)", re.MULTILINE)
    with open(cpp_path, "r") as f:
        content = f.read()
    for m in pattern.finditer(content):
        class_name = m.group(1)
        test_name = m.group(2)
        if test_name.startswith("DISABLED_"):
            continue  # Skipped by GTest; no filter entry needed
        tests.append(f"{class_name}.{test_name}")
    return tests


def is_covered(test_name, filter_entries):
    """
    Return True if test_name is covered by any entry in filter_entries.
    An entry "ClassName.*" covers all tests in that class.
    An entry "ClassName.TestName" covers exactly that test.
    """
    class_name = test_name.split(".")[0]
    for entry in filter_entries:
        if entry == test_name:
            return True
        if entry == f"{class_name}.*":
            return True
    return False


def main():
    parser = argparse.ArgumentParser(description="Check GTest shard filter coverage.")
    parser.add_argument(
        "--drivers-dir",
        default=None,
        help="Path to tests/drivers/ directory (auto-detected if omitted).",
    )
    parser.add_argument(
        "--cmake",
        default=None,
        help="Path to tests/drivers/CMakeLists.txt (auto-detected if omitted).",
    )
    args = parser.parse_args()

    root = find_project_root()
    drivers_dir = args.drivers_dir or os.path.join(root, "tests", "drivers")
    cmake_path = args.cmake or os.path.join(drivers_dir, "CMakeLists.txt")

    if not os.path.isfile(cmake_path):
        print(f"ERROR: CMakeLists.txt not found at {cmake_path}", file=sys.stderr)
        sys.exit(2)

    sharded_targets, filter_tests = collect_sharded_targets(cmake_path)

    if not sharded_targets:
        print("No sharded targets found. Nothing to check.")
        sys.exit(0)

    orphaned = {}  # target -> list of orphaned test names
    all_ok = True

    for target in sorted(sharded_targets):
        # Derive cpp filename: e.g. "gadtoolsgadgets_gtest" -> "gadtoolsgadgets_gtest.cpp"
        cpp_path = os.path.join(drivers_dir, f"{target}.cpp")
        if not os.path.isfile(cpp_path):
            print(f"WARNING: Source file not found for sharded target '{target}': {cpp_path}")
            continue

        tests = collect_test_cases(cpp_path)
        covered = filter_tests.get(target, set())
        missing = [t for t in tests if not is_covered(t, covered)]

        if missing:
            orphaned[target] = missing
            all_ok = False

    if all_ok:
        total_shards = len(sharded_targets)
        print(f"OK: All sharded driver tests are covered ({total_shards} sharded targets checked).")
        sys.exit(0)
    else:
        print("FAIL: The following tests are NOT covered by any shard FILTER string:")
        for target, tests in sorted(orphaned.items()):
            print(f"\n  Target: {target}")
            for t in tests:
                print(f"    ORPHANED: {t}")
        print(
            "\nFix: update the corresponding add_gtest_driver_shard FILTER string in "
            "tests/drivers/CMakeLists.txt to include the listed tests."
        )
        sys.exit(1)


if __name__ == "__main__":
    main()
