#!/bin/bash
# Test runner for filesystem stress test

cd "$(dirname "$0")"

LXA_BIN="../../../build/host/bin/lxa"
LXA_ROM="../../../build/target/rom/lxa.rom"
TEST_BINARY="fs_stress"
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

# Create temporary RAM: directory
RAMDIR=$(mktemp -d)
trap "rm -rf $RAMDIR $CONFIG_FILE" EXIT

cat > "$CONFIG_FILE" << EOF
[system]
rom_path = $LXA_ROM

[drives]
SYS = .
RAM = $RAMDIR
EOF

# Run test with a timeout (filesystem tests may take longer)
LXA_PREFIX="." "$LXA_BIN" -c "$CONFIG_FILE" "$TEST_BINARY" > "$ACTUAL_OUTPUT" 2>&1 || true

# Check for PASS in output
if grep -q "PASS: All filesystem stress tests passed" "$ACTUAL_OUTPUT"; then
    echo "PASS"
    exit 0
else
    echo "FAIL: Filesystem stress test did not pass"
    echo "Output:"
    cat "$ACTUAL_OUTPUT"
    exit 1
fi
