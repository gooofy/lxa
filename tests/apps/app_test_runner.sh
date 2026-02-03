#!/bin/bash
#
# App Test Runner for LXA
# Phase 39b: Enhanced Application Testing Infrastructure
#
# This script runs tests against real Amiga applications.
# It sets up the necessary assigns and environment, and provides
# enhanced validation beyond just "doesn't crash".
#
# Usage: app_test_runner.sh <lxa_bin> <lxa_rom> <test_binary> <expected> <actual> <sys_dir> <apps_dir> [validation_script]
#
# The optional validation_script will be sourced to define custom validation functions.
# It should define:
#   - validate_screen_dims()  - Check screen dimensions
#   - validate_window_dims()  - Check window dimensions
#   - validate_content()      - Check screen has content
#   - validate_logs()         - Check logs for expected entries
#

LXA_BIN="$1"
LXA_ROM="$2"
TEST_BINARY="$3"
EXPECTED_OUTPUT="$4"
ACTUAL_OUTPUT="$5"
LXA_SYS="$6"
APPS_DIR="${7:-$HOME/.lxa/System/Apps}"
VALIDATION_SCRIPT="${8:-}"

# Output files for validation
RAW_OUTPUT="${ACTUAL_OUTPUT}.raw"
SCREENSHOT_FILE="${ACTUAL_OUTPUT%.out}.ppm"

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
trap "rm -f $CONFIG_FILE $RAW_OUTPUT 2>/dev/null" EXIT

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
# Save raw output for validation analysis
timeout 55 sh -c 'LXA_PREFIX="$1" "$2" -c "$3" "$4" > "$5" 2>&1' -- "$TEST_DIR" "$LXA_BIN" "$CONFIG_FILE" "$TEST_NAME" "$RAW_OUTPUT"
EXIT_CODE=$?

# Copy raw output for filtering
cp "$RAW_OUTPUT" "$ACTUAL_OUTPUT"

#
# Phase 39b: Extract validation data from raw output
#

# Extract screen dimensions from logs
# Format: "_intuition: OpenScreen() SUCCESS: screen=0x... size=WxHxD title='...'"
extract_screen_info() {
    grep "_intuition: OpenScreen() SUCCESS:" "$RAW_OUTPUT" | tail -1 | \
        sed -n 's/.*size=\([0-9]*\)x\([0-9]*\)x\([0-9]*\).*/\1 \2 \3/p'
}

# Extract window dimensions from logs  
# Format: "_intuition: OpenWindow() SUCCESS: window=0x... size=WxH pos=(X,Y) title='...'"
extract_window_info() {
    grep "_intuition: OpenWindow() SUCCESS:" "$RAW_OUTPUT" | while read line; do
        echo "$line" | sed -n "s/.*size=\([0-9]*\)x\([0-9]*\) pos=(\([0-9-]*\),\([0-9-]*\)) title='\([^']*\)'.*/\1 \2 \3 \4 \5/p"
    done
}

# Count OpenScreen successes
count_screens() {
    grep -c "_intuition: OpenScreen() SUCCESS:" "$RAW_OUTPUT" 2>/dev/null || echo 0
}

# Count OpenWindow successes  
count_windows() {
    grep -c "_intuition: OpenWindow() SUCCESS:" "$RAW_OUTPUT" 2>/dev/null || echo 0
}

# Default validation functions (can be overridden by validation script)
validate_screen_dims() {
    # Default: screen must be at least 320x100
    local dims=$(extract_screen_info)
    if [ -n "$dims" ]; then
        local width=$(echo "$dims" | cut -d' ' -f1)
        local height=$(echo "$dims" | cut -d' ' -f2)
        if [ "$width" -lt 320 ] || [ "$height" -lt 100 ]; then
            echo "VALIDATION_FAIL: Screen dimensions ${width}x${height} too small (expected at least 320x100)"
            return 1
        fi
    fi
    return 0
}

validate_window_dims() {
    # Default: if windows opened, they should be at least 50x30
    local count=$(count_windows)
    if [ "$count" -gt 0 ]; then
        extract_window_info | while read w h x y title; do
            if [ "$w" -lt 50 ] || [ "$h" -lt 30 ]; then
                echo "VALIDATION_FAIL: Window '$title' dimensions ${w}x${h} too small"
                return 1
            fi
        done
    fi
    return 0
}

validate_content() {
    # Default: no content validation (would need test program cooperation)
    return 0
}

validate_logs() {
    # Default: check for critical errors
    if grep -q "SEGFAULT\|Traceback:\|CRASH\|FATAL" "$RAW_OUTPUT"; then
        echo "VALIDATION_FAIL: Critical error found in logs"
        return 1
    fi
    return 0
}

# Source custom validation script if provided
if [ -n "$VALIDATION_SCRIPT" ] && [ -f "$VALIDATION_SCRIPT" ]; then
    source "$VALIDATION_SCRIPT"
fi

#
# Run validations (Phase 39b)
#
VALIDATION_ERRORS=0

# Run validation functions
if ! validate_screen_dims; then
    VALIDATION_ERRORS=$((VALIDATION_ERRORS + 1))
fi

if ! validate_window_dims; then
    VALIDATION_ERRORS=$((VALIDATION_ERRORS + 1))
fi

if ! validate_content; then
    VALIDATION_ERRORS=$((VALIDATION_ERRORS + 1))
fi

if ! validate_logs; then
    VALIDATION_ERRORS=$((VALIDATION_ERRORS + 1))
fi

# Print validation summary
echo "=== Validation Summary ===" >> "$ACTUAL_OUTPUT"
echo "Screens opened: $(count_screens)" >> "$ACTUAL_OUTPUT"
echo "Windows opened: $(count_windows)" >> "$ACTUAL_OUTPUT"

SCREEN_INFO=$(extract_screen_info)
if [ -n "$SCREEN_INFO" ]; then
    echo "Last screen: ${SCREEN_INFO}x" >> "$ACTUAL_OUTPUT"
fi

if [ "$VALIDATION_ERRORS" -gt 0 ]; then
    echo "Validation errors: $VALIDATION_ERRORS" >> "$ACTUAL_OUTPUT"
fi

#
# Filter debug output for comparison
#
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
        -e '^=== Validation' \
        -e '^Screens opened:' \
        -e '^Windows opened:' \
        -e '^Last screen:' \
        -e '^Validation errors:' \
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

# Check validation errors
if [ "$VALIDATION_ERRORS" -gt 0 ]; then
    echo "FAIL: $VALIDATION_ERRORS validation error(s)"
    exit 1
fi

echo "PASS"
exit 0
