#!/usr/bin/env bash
# Run the full ctest suite N times, optionally under synthetic CPU load, and
# report every failure.  Phase 201 test gate: 20 runs under load, 0 flakes.
#
#   tools/stress_suite.sh [-n RUNS] [-l LOAD_THREADS] [-o OUTDIR] [-b BUILD_DIR]
#
# A watchdog attaches gdb to any test driver process older than 60 s and
# saves its backtrace to OUTDIR/hang-<pid>.txt (hangs are bugs, not noise).
set -u
RUNS=20
LOAD=0
OUT=${TMPDIR:-/tmp}/lxa-stress
BUILD=
while getopts "n:l:o:b:" o; do
    case $o in
        n) RUNS=$OPTARG ;;
        l) LOAD=$OPTARG ;;
        o) OUT=$OPTARG ;;
        b) BUILD=$OPTARG ;;
        *) echo "usage: $0 [-n RUNS] [-l LOAD_THREADS] [-o OUTDIR]"; exit 2 ;;
    esac
done
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-$ROOT/build}
mkdir -p "$OUT"

burners=()
cleanup() {
    for p in "${burners[@]}"; do kill "$p" 2>/dev/null; done
    [ -n "${WATCH:-}" ] && kill "$WATCH" 2>/dev/null
}
trap cleanup EXIT

for ((i = 0; i < LOAD; i++)); do
    ( while :; do :; done ) &
    burners+=($!)
done

(
    while :; do
        for p in $(pgrep -f "tests/drivers/[a-z0-9_]*_gtest"); do
            age=$(ps -o etimes= -p "$p" 2>/dev/null | tr -d ' ')
            if [ -n "$age" ] && [ "$age" -gt 60 ] && [ ! -e "$OUT/hang-$p.txt" ]; then
                { tr '\0' ' ' < "/proc/$p/cmdline"; echo; gdb -p "$p" -batch -ex "thread apply all bt 30" 2>&1; } \
                    > "$OUT/hang-$p.txt"
            fi
        done
        sleep 5
    done
) &
WATCH=$!

fails=0
for ((r = 1; r <= RUNS; r++)); do
    log="$OUT/run-$r.log"
    ctest --test-dir "$BUILD" -j16 --timeout 180 > "$log" 2>&1
    summary=$(grep -E "tests passed|Total Test time" "$log" | tr '\n' ' ')
    bad=$(grep -E "\*\*\*(Failed|Timeout)" "$log" | sed 's/.*Test *#[0-9]*: //' | awk '{print $1}' | tr '\n' ' ')
    echo "run $r/$RUNS: $summary ${bad:+FAILED: $bad}"
    [ -n "$bad" ] && fails=$((fails + 1))
done
echo "stress: $fails of $RUNS runs had failures (load threads: $LOAD)"
ls "$OUT"/hang-*.txt 2>/dev/null && echo "hang backtraces saved above"
exit $((fails > 0))
