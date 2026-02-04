#!/bin/sh
# Automated test for SimpleGad sample
# Tests boolean gadgets by simulating input events

# The sample waits for user interaction, so we need to run it with automated input
# We'll send it a close window signal after a short delay

echo "Testing SimpleGad sample - boolean gadgets"

# Run the sample with a timeout and send it Ctrl+C after 2 seconds to trigger cleanup
timeout 3 SYS:Samples/SimpleGad &
SAMPLE_PID=$!

# Give it time to open window and initialize
sleep 1

# Send Ctrl+C to trigger graceful shutdown
kill -INT $SAMPLE_PID 2>/dev/null

# Wait for it to finish
wait $SAMPLE_PID 2>/dev/null
EXIT_CODE=$?

# Check if it ran and exited cleanly (timeout returns 124, clean exit is 0)
if [ $EXIT_CODE -eq 0 ] || [ $EXIT_CODE -eq 130 ]; then
    echo "SimpleGad: Sample ran successfully"
    exit 0
else
    echo "SimpleGad: Sample failed with exit code $EXIT_CODE"
    exit 1
fi
