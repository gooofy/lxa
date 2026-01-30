#!/bin/bash

# Test runner script for LXA integration tests

set -e

LXA_BIN="$1"
LXA_ROM="$2"
TEST_BINARY="$3"
EXPECTED_OUTPUT="$4"
ACTUAL_OUTPUT="$5"
LXA_SYS="$6"

# Check if lxa binary exists
if [ ! -f "$LXA_BIN" ]; then
    echo "ERROR: LXA binary not found at $LXA_BIN"
    echo "Please build project first: make -C build"
    exit 1
fi

# Check if ROM exists
if [ ! -f "$LXA_ROM" ]; then
    echo "ERROR: LXA ROM not found at $LXA_ROM"
    echo "Please build project first: make -C build"
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

# Create a temporary config file for the test
CONFIG_FILE=$(mktemp)
trap "rm -f $CONFIG_FILE" EXIT

cat > "$CONFIG_FILE" << EOF
[system]
rom_path = $LXA_ROM

[drives]
SYS = $TEST_DIR
EOF

# If LXA_SYS is provided and has System directory, use it for shell tests
if [ -n "$LXA_SYS" ] && [ -d "$LXA_SYS/System" ]; then
    # Check if test directory has its own System directory (shell tests)
    if [ -d "$TEST_DIR/System" ]; then
        # Copy shell from build to test's System directory
        cp "$LXA_SYS/System/Shell" "$TEST_DIR/System/Shell" 2>/dev/null || true
    fi
fi

# Check if this is a shell script test (has script.txt and System/Shell)
if [ -f "$TEST_DIR/script.txt" ] && [ -f "$TEST_DIR/System/Shell" ]; then
    # Run the shell directly with the script
    LXA_PREFIX="$TEST_DIR" "$LXA_BIN" -c "$CONFIG_FILE" "SYS:System/Shell" "script.txt" > "$ACTUAL_OUTPUT" 2>&1
    EXIT_CODE=$?
else
    # Run test binary with config file (use LXA_PREFIX to prevent ~/.lxa setup)
    LXA_PREFIX="$TEST_DIR" "$LXA_BIN" -c "$CONFIG_FILE" "$TEST_NAME" > "$ACTUAL_OUTPUT" 2>&1
    EXIT_CODE=$?
fi

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
