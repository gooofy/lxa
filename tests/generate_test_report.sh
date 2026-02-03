#!/bin/bash
#
# Test Report Generator for LXA Application Tests
# Phase 39b: Enhanced Application Testing Infrastructure
#
# This script runs all app tests and generates a comprehensive report
# including screen dimensions, window counts, and validation results.
#
# Usage: generate_test_report.sh [output_file]
#

OUTPUT_FILE="${1:-test_report.txt}"
REPORT_DIR="${2:-/tmp/lxa_test_report}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Create report directory
mkdir -p "$REPORT_DIR"

# Header
{
    echo "============================================================"
    echo "           LXA Application Test Report"
    echo "           Phase 39b: Enhanced Testing Infrastructure"
    echo "============================================================"
    echo ""
    echo "Generated: $(date)"
    echo "LXA Version: $(cat "$BASE_DIR/src/include/lxa_version.h" 2>/dev/null | grep VERSION_STRING | sed 's/.*"\([^"]*\)".*/\1/' || echo "unknown")"
    echo ""
    echo "============================================================"
    echo "                    TEST SUMMARY"
    echo "============================================================"
    echo ""
} > "$OUTPUT_FILE"

# Function to extract info from raw output
extract_validation_info() {
    local raw_output="$1"
    local app_name="$2"
    
    echo "--- $app_name ---"
    
    # Screen info
    local screens=$(grep -c "_intuition: OpenScreen() SUCCESS:" "$raw_output" 2>/dev/null || echo "0")
    echo "  Screens opened: $screens"
    
    if [ "$screens" -gt 0 ]; then
        grep "_intuition: OpenScreen() SUCCESS:" "$raw_output" | while read line; do
            local dims=$(echo "$line" | sed -n "s/.*size=\([0-9]*x[0-9]*x[0-9]*\).*/\1/p")
            local title=$(echo "$line" | sed -n "s/.*title='\([^']*\)'.*/\1/p")
            echo "    Screen: $dims '$title'"
        done
    fi
    
    # Window info
    local windows=$(grep -c "_intuition: OpenWindow() SUCCESS:" "$raw_output" 2>/dev/null || echo "0")
    echo "  Windows opened: $windows"
    
    if [ "$windows" -gt 0 ]; then
        grep "_intuition: OpenWindow() SUCCESS:" "$raw_output" | head -5 | while read line; do
            local dims=$(echo "$line" | sed -n "s/.*size=\([0-9]*x[0-9]*\).*/\1/p")
            local title=$(echo "$line" | sed -n "s/.*title='\([^']*\)'.*/\1/p")
            echo "    Window: $dims '$title'"
        done
        if [ "$windows" -gt 5 ]; then
            echo "    ... and $((windows - 5)) more"
        fi
    fi
    
    # Check for errors
    local errors=$(grep -c "FAIL:\|SEGFAULT\|Traceback:\|CRASH\|NULL pointer" "$raw_output" 2>/dev/null || echo "0")
    if [ "$errors" -gt 0 ]; then
        echo "  ERRORS DETECTED: $errors"
        grep "FAIL:\|SEGFAULT\|Traceback:\|CRASH\|NULL pointer" "$raw_output" | head -3 | while read line; do
            echo "    $line"
        done
    fi
    
    # Screen dimension validation
    local screen_info=$(grep "_intuition: OpenScreen() SUCCESS:" "$raw_output" | tail -1)
    if [ -n "$screen_info" ]; then
        local height=$(echo "$screen_info" | sed -n 's/.*size=[0-9]*x\([0-9]*\)x[0-9]*.*/\1/p')
        if [ -n "$height" ] && [ "$height" -lt 100 ]; then
            echo "  ⚠️  WARNING: Screen height $height is suspiciously small!"
        fi
    fi
    
    echo ""
}

# Track results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
VALIDATION_ISSUES=0

# Run app tests
echo "Running application tests..."
echo ""

# Find all app tests
for test_dir in "$SCRIPT_DIR/apps/"*/; do
    if [ -d "$test_dir" ] && [ -f "$test_dir/Makefile" ]; then
        test_name=$(basename "$test_dir")
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        
        echo "Testing: $test_name..."
        
        # Build if needed
        make -C "$test_dir" 2>/dev/null
        
        # Run with raw output capture
        raw_file="$REPORT_DIR/${test_name}_raw.out"
        
        # Run the test (using app_test_runner.sh directly)
        if [ -f "$test_dir/$test_name" ] || [ -f "$test_dir/main" ]; then
            local_binary="$test_dir/$test_name"
            [ -f "$local_binary" ] || local_binary="$test_dir/main"
            
            timeout 60 "$BASE_DIR/build/host/bin/lxa" \
                -r "$BASE_DIR/build/target/rom/lxa.rom" \
                "$local_binary" > "$raw_file" 2>&1
            exit_code=$?
        else
            echo "  No binary found, skipping"
            continue
        fi
        
        # Extract validation info
        extract_validation_info "$raw_file" "$test_name" >> "$OUTPUT_FILE"
        
        # Check for issues
        if grep -q "FAIL:\|SEGFAULT\|CRASH" "$raw_file"; then
            FAILED_TESTS=$((FAILED_TESTS + 1))
            echo "  Result: FAILED"
        else
            PASSED_TESTS=$((PASSED_TESTS + 1))
            echo "  Result: PASSED"
        fi
        
        # Check for validation issues (screen too small, empty windows, etc.)
        if grep -q "suspiciously small\|WARNING:" "$raw_file"; then
            VALIDATION_ISSUES=$((VALIDATION_ISSUES + 1))
        fi
    fi
done

# Summary
{
    echo "============================================================"
    echo "                    RESULTS SUMMARY"
    echo "============================================================"
    echo ""
    echo "Total tests run:      $TOTAL_TESTS"
    echo "Passed:               $PASSED_TESTS"
    echo "Failed:               $FAILED_TESTS"
    echo "Validation warnings:  $VALIDATION_ISSUES"
    echo ""
    if [ "$FAILED_TESTS" -gt 0 ]; then
        echo "STATUS: ❌ SOME TESTS FAILED"
    elif [ "$VALIDATION_ISSUES" -gt 0 ]; then
        echo "STATUS: ⚠️  PASSED WITH WARNINGS"
    else
        echo "STATUS: ✅ ALL TESTS PASSED"
    fi
    echo ""
    echo "============================================================"
    echo "Raw output files saved to: $REPORT_DIR"
    echo "============================================================"
} >> "$OUTPUT_FILE"

echo ""
echo "Report generated: $OUTPUT_FILE"
echo "Raw outputs in: $REPORT_DIR"
echo ""
echo "Summary: $PASSED_TESTS/$TOTAL_TESTS passed, $VALIDATION_ISSUES validation warnings"

# Exit with appropriate code
if [ "$FAILED_TESTS" -gt 0 ]; then
    exit 1
else
    exit 0
fi
