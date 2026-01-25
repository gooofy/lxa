#!/bin/bash

# Test runner script for LXA integration tests

set -e

LXA_BIN="$1"
LXA_ROM="$2"
TEST_BINARY="$3"
EXPECTED_OUTPUT="$4"
ACTUAL_OUTPUT="$5"

# Check if lxa binary exists
if [ ! -f "$LXA_BIN" ]; then
    echo "ERROR: LXA binary not found at $LXA_BIN"
    echo "Please build project first: make all"
    exit 1
fi

# Check if ROM exists
if [ ! -f "$LXA_ROM" ]; then
    echo "ERROR: LXA ROM not found at $LXA_ROM"
    echo "Please build project first: make all"
    exit 1
fi

# Check if test binary exists
if [ ! -f "$TEST_BINARY" ]; then
    echo "ERROR: Test binary not found at $TEST_BINARY"
    echo "Please build test first"
    exit 1
fi

# Get directory and basename of test binary
TEST_DIR=$(dirname "$TEST_BINARY")
TEST_NAME=$(basename "$TEST_BINARY")

# Run test with test directory as sysroot
"$LXA_BIN" -s "$TEST_DIR" -r "$LXA_ROM" -v "$TEST_NAME" > "$ACTUAL_OUTPUT" 2>&1
EXIT_CODE=$?

# Compare exit code (0 = success)
if [ $EXIT_CODE -ne 0 ]; then
    echo "FAIL: Test exited with code $EXIT_CODE (expected 0)"
    echo "Output:"
    cat "$ACTUAL_OUTPUT"
    exit 1
fi

# Compare output with expected
if ! diff -u "$EXPECTED_OUTPUT" "$ACTUAL_OUTPUT"; then
    echo "FAIL: Output mismatch"
    exit 1
fi

echo "PASS"

exit 0
