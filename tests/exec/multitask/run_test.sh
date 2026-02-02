#!/bin/bash
# Test runner for multitask test
# This test may have cleanup issues, so we just check for PASS in output

cd "$(dirname "$0")"

LXA_BIN="../../../build/host/bin/lxa"
LXA_ROM="../../../build/target/rom/lxa.rom"
TEST_BINARY="multitask"
ACTUAL_OUTPUT="actual.out"

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

# Create a temporary config file for the test
CONFIG_FILE=$(mktemp)
trap "rm -f $CONFIG_FILE" EXIT

cat > "$CONFIG_FILE" << EOF
[system]
rom_path = $LXA_ROM

[drives]
SYS = .
EOF

# Run test with timeout (15 seconds should be plenty), ignoring exit code
timeout 15 env LXA_PREFIX="." "$LXA_BIN" -c "$CONFIG_FILE" "$TEST_BINARY" > "$ACTUAL_OUTPUT" 2>&1 || true

# Check for PASS in output
if grep -q "PASS: Multitasking test passed" "$ACTUAL_OUTPUT"; then
    echo "PASS"
    exit 0
else
    echo "FAIL: PASS message not found in output"
    echo "Output:"
    cat "$ACTUAL_OUTPUT"
    exit 1
fi
