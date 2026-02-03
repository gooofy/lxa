#!/bin/bash
#
# App Test Runner for LXA
#
# This script runs tests against real Amiga applications.
# It sets up the necessary assigns and environment.
#
# Usage: app_test_runner.sh <lxa_bin> <lxa_rom> <test_binary> <expected> <actual> <sys_dir> <apps_dir>
#

LXA_BIN="$1"
LXA_ROM="$2"
TEST_BINARY="$3"
EXPECTED_OUTPUT="$4"
ACTUAL_OUTPUT="$5"
LXA_SYS="$6"
APPS_DIR="${7:-$HOME/.lxa/System/Apps}"

# Check if lxa binary exists
if [ ! -f "$LXA_BIN" ]; then
    echo "ERROR: LXA binary not found at $LXA_BIN"
    exit 1
fi

# Check if ROM exists
if [ ! -f "$LXA_ROM" ]; then
    echo "ERROR: LXA ROM not found at $LXA_ROM"
    exit 1
fi

# Check if test binary exists
if [ ! -f "$TEST_BINARY" ]; then
    echo "ERROR: Test binary not found at $TEST_BINARY"
    exit 1
fi

# Get directory and basename of test binary
TEST_DIR=$(dirname "$TEST_BINARY")
TEST_NAME=$(basename "$TEST_BINARY")

# Create a temporary config file with APPS assign
CONFIG_FILE=$(mktemp)
trap "rm -f $CONFIG_FILE" EXIT

cat > "$CONFIG_FILE" << EOF
[system]
rom_path = $LXA_ROM

[drives]
SYS = $TEST_DIR

[assigns]
APPS = $APPS_DIR
EOF

# Link C: commands if test doesn't have its own C directory
if [ -d "$LXA_SYS/C" ] && [ ! -d "$TEST_DIR/C" ]; then
    ln -sf "$LXA_SYS/C" "$TEST_DIR/C" 2>/dev/null || true
fi

# Run test binary with config file (with timeout)
timeout 55 sh -c 'LXA_PREFIX="$1" "$2" -c "$3" "$4" > "$5" 2>&1' -- "$TEST_DIR" "$LXA_BIN" "$CONFIG_FILE" "$TEST_NAME" "$ACTUAL_OUTPUT"
EXIT_CODE=$?

# Filter debug output - aggressive filtering matching test_runner.sh
if [ -f "$ACTUAL_OUTPUT" ]; then
    TEMP_OUTPUT=$(mktemp)
    
    # First, remove everything after "*** LXA DEBUGGER ***" line (including that line)
    sed '/\*\*\* LXA DEBUGGER \*\*\*/,$d' "$ACTUAL_OUTPUT" > "$TEMP_OUTPUT"
    
    # Filter out debug output matching test_runner.sh patterns
    grep -v \
        -e '^0x[0-9a-f]' \
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
        -e '^_gadtools:' \
        -e '^_workbench:' \
        -e '^_asl:' \
        -e '^_icon:' \
        -e '^_locale:' \
        -e '^_expansion:' \
        -e '^_keymap:' \
        -e '^_boopsi:' \
        -e '^display:' \
        -e '^ERROR:' \
        -e "^           " \
        -e "^          " \
        -e "^  LeftEdge=" \
        -e "^  DetailPen=" \
        -e "^  IDCMPFlags=" \
        -e "^  FirstGadget=" \
        -e "^  Title=" \
        -e "^  Title string:" \
        -e "^  MinWidth=" \
        -e "' len=" \
        -e '^LXA DEBUGGER' \
        -e '^---> ' \
        -e '^\[?25h' \
        -e '^Traceback:' \
        -e '^>> $' \
        -e '^\[CLI ' \
        -e '^\*\*\* ' \
        "$TEMP_OUTPUT" > "${TEMP_OUTPUT}.2" 2>/dev/null || true
    
    # Move filtered output back
    mv "${TEMP_OUTPUT}.2" "$ACTUAL_OUTPUT"
    rm -f "$TEMP_OUTPUT"
    
    # Remove trailing newlines and leading blank lines
    sed -i -e :a -e '/^\n*$/{$d;N;};/\n$/ba' "$ACTUAL_OUTPUT" 2>/dev/null || true
    sed -i '/./,$!d' "$ACTUAL_OUTPUT" 2>/dev/null || true
fi

# Compare output with expected
if ! diff -u "$EXPECTED_OUTPUT" "$ACTUAL_OUTPUT"; then
    echo "FAIL: Output mismatch"
    exit 1
fi

echo "PASS"
exit 0
