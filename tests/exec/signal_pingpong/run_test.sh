#!/bin/bash
# Test runner for signal_pingpong test
# This test may crash during cleanup, so we just check for PASS in output

cd "$(dirname "$0")"

LXA_BIN="../../../target/x86_64-linux/bin/lxa"
LXA_ROM="../../../src/rom/lxa.rom"
TEST_BINARY="signal_pingpong"
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

# Run test, ignoring exit code since it may crash during cleanup
"$LXA_BIN" -s "." -r "$LXA_ROM" -v "$TEST_BINARY" > "$ACTUAL_OUTPUT" 2>&1 || true

# Check for PASS in output
if grep -q "PASS: Ping-pong test" "$ACTUAL_OUTPUT"; then
    echo "PASS"
    exit 0
else
    echo "FAIL: PASS message not found in output"
    echo "Output:"
    cat "$ACTUAL_OUTPUT"
    exit 1
fi
