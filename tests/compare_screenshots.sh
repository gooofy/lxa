#!/bin/bash
#
# Screenshot Comparison Utility for LXA Tests
# Phase 39b: Enhanced Application Testing Infrastructure
#
# This script compares two PPM files and reports similarity.
# It can also generate diff images for visual inspection.
#
# Usage: compare_screenshots.sh <reference.ppm> <actual.ppm> [threshold] [diff_output.ppm]
#
# Arguments:
#   reference.ppm  - The expected/reference screenshot
#   actual.ppm     - The actual screenshot from test run
#   threshold      - Minimum similarity percentage to pass (default: 95)
#   diff_output    - Optional file to save visual diff
#
# Exit codes:
#   0 - Screenshots match (similarity >= threshold)
#   1 - Screenshots differ (similarity < threshold)
#   2 - Error (file not found, invalid format, etc.)
#

REFERENCE="$1"
ACTUAL="$2"
THRESHOLD="${3:-95}"
DIFF_OUTPUT="${4:-}"

# Check required arguments
if [ -z "$REFERENCE" ] || [ -z "$ACTUAL" ]; then
    echo "Usage: compare_screenshots.sh <reference.ppm> <actual.ppm> [threshold] [diff_output.ppm]"
    exit 2
fi

# Check files exist
if [ ! -f "$REFERENCE" ]; then
    echo "ERROR: Reference file not found: $REFERENCE"
    exit 2
fi

if [ ! -f "$ACTUAL" ]; then
    echo "ERROR: Actual file not found: $ACTUAL"
    exit 2
fi

# Check for ImageMagick compare tool
if command -v compare &> /dev/null; then
    # Use ImageMagick for comparison (most accurate)
    
    # Get image dimensions
    REF_DIMS=$(identify -format "%wx%h" "$REFERENCE" 2>/dev/null)
    ACT_DIMS=$(identify -format "%wx%h" "$ACTUAL" 2>/dev/null)
    
    if [ "$REF_DIMS" != "$ACT_DIMS" ]; then
        echo "ERROR: Image dimensions differ: reference=$REF_DIMS, actual=$ACT_DIMS"
        exit 1
    fi
    
    # Calculate RMSE (Root Mean Square Error)
    # Lower RMSE = more similar
    RMSE=$(compare -metric RMSE "$REFERENCE" "$ACTUAL" /dev/null 2>&1 | grep -oP '^\d+\.?\d*' || echo "999999")
    
    # Calculate PSNR (Peak Signal-to-Noise Ratio) - higher is better
    # PSNR > 30 dB is generally good, > 40 dB is very similar
    PSNR=$(compare -metric PSNR "$REFERENCE" "$ACTUAL" /dev/null 2>&1 | grep -oP '^\d+\.?\d*' || echo "0")
    
    # Convert RMSE to similarity percentage (rough approximation)
    # RMSE of 0 = 100% similar, RMSE of 65535 (max for 8-bit) = 0% similar
    # For practical purposes, RMSE < 1000 is usually very similar
    if [ "$(echo "$RMSE < 0.001" | bc -l 2>/dev/null)" = "1" ]; then
        SIMILARITY=100
    else
        # Convert RMSE to percentage: 100 - (RMSE / 655.35)
        SIMILARITY=$(echo "scale=2; 100 - ($RMSE / 655.35)" | bc -l 2>/dev/null || echo "0")
        # Clamp to 0-100
        SIMILARITY=$(echo "$SIMILARITY" | awk '{if ($1 < 0) print 0; else if ($1 > 100) print 100; else print $1}')
    fi
    
    # Generate diff image if requested
    if [ -n "$DIFF_OUTPUT" ]; then
        compare -highlight-color red "$REFERENCE" "$ACTUAL" "$DIFF_OUTPUT" 2>/dev/null
        echo "Diff image saved to: $DIFF_OUTPUT"
    fi
    
    echo "Comparison results:"
    echo "  Reference: $REFERENCE ($REF_DIMS)"
    echo "  Actual:    $ACTUAL ($ACT_DIMS)"
    echo "  RMSE:      $RMSE"
    echo "  Similarity: ${SIMILARITY}%"
    echo "  Threshold:  ${THRESHOLD}%"
    
    # Check threshold
    PASS=$(echo "$SIMILARITY >= $THRESHOLD" | bc -l 2>/dev/null || echo "0")
    if [ "$PASS" = "1" ]; then
        echo "PASS: Screenshots match (${SIMILARITY}% >= ${THRESHOLD}%)"
        exit 0
    else
        echo "FAIL: Screenshots differ (${SIMILARITY}% < ${THRESHOLD}%)"
        exit 1
    fi

elif command -v python3 &> /dev/null; then
    # Fallback: Use Python for comparison
    python3 << EOF
import sys

def read_ppm(filename):
    """Read a P6 PPM file and return dimensions and pixel data."""
    with open(filename, 'rb') as f:
        # Read magic number
        magic = f.readline().decode('ascii').strip()
        if magic != 'P6':
            raise ValueError(f"Not a P6 PPM file: {magic}")
        
        # Skip comments and read dimensions
        line = f.readline().decode('ascii').strip()
        while line.startswith('#'):
            line = f.readline().decode('ascii').strip()
        
        width, height = map(int, line.split())
        
        # Read max value
        max_val = int(f.readline().decode('ascii').strip())
        
        # Read pixel data
        pixels = f.read()
        
        return width, height, max_val, pixels

try:
    ref_w, ref_h, ref_max, ref_pixels = read_ppm("$REFERENCE")
    act_w, act_h, act_max, act_pixels = read_ppm("$ACTUAL")
except Exception as e:
    print(f"ERROR: Failed to read PPM files: {e}")
    sys.exit(2)

if (ref_w, ref_h) != (act_w, act_h):
    print(f"ERROR: Image dimensions differ: reference={ref_w}x{ref_h}, actual={act_w}x{act_h}")
    sys.exit(1)

if len(ref_pixels) != len(act_pixels):
    print(f"ERROR: Pixel data size differs")
    sys.exit(1)

# Count matching pixels
total_pixels = ref_w * ref_h
matching = 0
tolerance = 8  # Allow small color differences

for i in range(0, len(ref_pixels), 3):
    r1, g1, b1 = ref_pixels[i], ref_pixels[i+1], ref_pixels[i+2]
    r2, g2, b2 = act_pixels[i], act_pixels[i+1], act_pixels[i+2]
    
    if (abs(r1-r2) <= tolerance and 
        abs(g1-g2) <= tolerance and 
        abs(b1-b2) <= tolerance):
        matching += 1

similarity = (matching * 100.0) / total_pixels

print(f"Comparison results:")
print(f"  Reference: $REFERENCE ({ref_w}x{ref_h})")
print(f"  Actual:    $ACTUAL ({act_w}x{act_h})")
print(f"  Matching pixels: {matching}/{total_pixels}")
print(f"  Similarity: {similarity:.2f}%")
print(f"  Threshold:  ${THRESHOLD}%")

if similarity >= ${THRESHOLD}:
    print(f"PASS: Screenshots match ({similarity:.2f}% >= ${THRESHOLD}%)")
    sys.exit(0)
else:
    print(f"FAIL: Screenshots differ ({similarity:.2f}% < ${THRESHOLD}%)")
    sys.exit(1)
EOF

else
    # No suitable tool found - do basic byte comparison
    echo "WARNING: Neither ImageMagick nor Python3 found, using basic comparison"
    
    if cmp -s "$REFERENCE" "$ACTUAL"; then
        echo "PASS: Screenshots are identical"
        exit 0
    else
        echo "FAIL: Screenshots differ (exact comparison)"
        exit 1
    fi
fi
