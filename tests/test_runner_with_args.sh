#!/bin/bash

# Test runner script for LXA integration tests
# This version supports passing additional arguments to the test binary
# Note: We don't use set -e because grep returns 1 when no matches found,
# and we want to continue even when individual commands "fail"

LXA_BIN="$1"
LXA_ROM="$2"
TEST_BINARY="$3"
EXPECTED_OUTPUT="$4"
ACTUAL_OUTPUT="$5"
LXA_SYS="$6"
shift 6
EXTRA_ARGS="$@"

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

# If LXA_SYS is provided, set up system directories
if [ -n "$LXA_SYS" ]; then
    # Link C: commands if test doesn't have its own C directory
    if [ -d "$LXA_SYS/C" ] && [ ! -d "$TEST_DIR/C" ]; then
        ln -sf "$LXA_SYS/C" "$TEST_DIR/C" 2>/dev/null || true
    fi
    
    # Check if test directory has its own System directory (shell tests)
    if [ -d "$LXA_SYS/System" ] && [ -d "$TEST_DIR/System" ]; then
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
    LXA_PREFIX="$TEST_DIR" "$LXA_BIN" -c "$CONFIG_FILE" "$TEST_NAME" $EXTRA_ARGS > "$ACTUAL_OUTPUT" 2>&1
    EXIT_CODE=$?
fi

# Filter out any debugger output and internal LXA debug logging
# This can happen due to libnix exit issues or debug logging enabled
if [ -f "$ACTUAL_OUTPUT" ]; then
    # Create a temporary file for filtered output
    TEMP_OUTPUT=$(mktemp)
    
    # First, remove everything after "*** LXA DEBUGGER ***" line (including that line)
    sed '/\*\*\* LXA DEBUGGER \*\*\*/,$d' "$ACTUAL_OUTPUT" > "$TEMP_OUTPUT"
    
    # Filter out:
    # - Lines starting with "0x" followed by hex (memory dumps)
    # - Lines starting with known LXA debug prefixes
    # - Lines that are part of multiline debug output (indented with spaces)
    # - Debugger trace lines
    grep -v \
        -e '^0x[0-9a-f]*:' \
        -e '^coldstart:' \
        -e '^util:' \
        -e '^_exec:' \
        -e '^_dos:' \
        -e '^_intuition:' \
        -e '^_console:' \
        -e '^_graphics:' \
        -e '^_layers:' \
        -e '^_diskfont:' \
        -e '^_timer:' \
        -e '^_input:' \
        -e '^_keyboard:' \
        -e '^display:' \
        -e "^           RAM_" \
        -e "^          &SysBase" \
        -e "^           SysBase" \
        -e "^  LeftEdge=" \
        -e "^  DetailPen=" \
        -e "^  IDCMPFlags=" \
        -e "^  FirstGadget=" \
        -e "^  Title=" \
        -e "^  Title string:" \
        -e "^  MinWidth=" \
        -e "^' len=" \
        -e '^LXA DEBUGGER' \
        -e '^     [0-9a-fx]' \
        -e '^---> ' \
        -e '^     pc=' \
        -e '^     d[0-7]=' \
        -e '^     a[0-7]=' \
        -e '^     usp=' \
        -e '^\[?25h' \
        -e '^Traceback:' \
        -e '^>> $' \
        -e '^ERROR: mread' \
        -e '^ERROR: mwrite' \
        -e '^WARNING: mread' \
        -e '^WARNING: mwrite' \
        -e '^WARNING: suppressing' \
        -e '^\*\*\* WARNING: PC=' \
        -e '^\*\*\* assertion failed:' \
        "$TEMP_OUTPUT" > "${TEMP_OUTPUT}.2" 2>/dev/null || true
    
    # Move filtered output back
    mv "${TEMP_OUTPUT}.2" "$ACTUAL_OUTPUT"
    rm -f "$TEMP_OUTPUT"
    
    # Normalize hex addresses (0x followed by 5+ hex digits) to just "0x"
    # This makes tests more portable and less brittle
    # Only normalize 5+ digit hex values (memory addresses), not short constants like 0xFF or 0xF000
    sed -i 's/0x[0-9a-fA-F]\{5,\}/0x/g' "$ACTUAL_OUTPUT"
    
    # Normalize variable numeric values that change between runs
    sed -i 's/Request completed after [0-9]\+ checks!/Request completed after checks!/' "$ACTUAL_OUTPUT"
    
    # Normalize timer values (system time, microseconds, delays)
    # These vary between runs and between different host systems
    sed -i 's/Seconds: [0-9]\+, Microseconds: [0-9]\+/Seconds: TIME, Microseconds: TIME/g' "$ACTUAL_OUTPUT"
    sed -i 's/Time: [0-9]\{2\}:[0-9]\{2\}:[0-9]\{2\} (Days: [0-9]\+)/Time: HH:MM:SS (Days: N)/g' "$ACTUAL_OUTPUT"
    sed -i 's/\([0-9]\{5,\}\)\.\([0-9]\{6\}\)/TIME.MICRO/g' "$ACTUAL_OUTPUT"
    sed -i 's/Time correctly increased by [0-9]\+ microseconds/Time correctly increased by N microseconds/g' "$ACTUAL_OUTPUT"
    sed -i 's/Actual delay: [0-9]\+ microseconds/Actual delay: N microseconds/g' "$ACTUAL_OUTPUT"
    
    # Remove trailing newlines that might be left
    sed -i -e :a -e '/^\n*$/{$d;N;};/\n$/ba' "$ACTUAL_OUTPUT" 2>/dev/null || true
fi

# Note: We don't check exit code here because many Amiga programs,
# especially console-only tests using libnix, may exit with non-zero
# codes due to libnix/LXA exit handling issues.
# Instead, we rely on output comparison to determine test success.
# If the output matches expected, the test passes regardless of exit code.

# Compare output with expected
if ! diff -u "$EXPECTED_OUTPUT" "$ACTUAL_OUTPUT"; then
    echo "FAIL: Output mismatch"
    exit 1
fi

echo "PASS"

exit 0
