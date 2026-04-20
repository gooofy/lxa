#!/usr/bin/env python3
"""
profile_report.py - lxa profiling JSON report tool (Phase 126)

Reads the JSON output of lxa_profile_write_json() and prints:
  - Top-10 EMU_CALLs by cumulative wall-clock nanoseconds
  - Top-10 EMU_CALLs by call count

Usage:
    python tools/profile_report.py <profile.json> [--top N] [--csv]

The JSON file is produced by lxa_profile_write_json() when running lxa with
profiling enabled (PROFILE_BUILD=ON).

To enable profiling:
    cmake -DPROFILE_BUILD=ON ...
    ./build.sh            # or cmake --build build
    ./build/host/bin/lxa --profile /tmp/lxa_profile.json SYS:SomeApp

Then analyse:
    python tools/profile_report.py /tmp/lxa_profile.json
"""

import argparse
import json
import sys


def load(path: str) -> list[dict]:
    with open(path) as f:
        data = json.load(f)
    if not isinstance(data, list):
        raise ValueError("Expected a JSON array at top level")
    return data


def print_table(title: str, rows: list[dict], key: str, label: str) -> None:
    print(f"\n=== {title} ===")
    header = f"{'Rank':>4}  {'EMU_CALL':<40}  {'Calls':>12}  {label:>16}"
    print(header)
    print("-" * len(header))
    for rank, row in enumerate(rows, 1):
        print(
            f"{rank:>4}  {row['name']:<40}  {row['calls']:>12,}  {row[key]:>16,}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description="lxa profiling report")
    parser.add_argument("profile_json", help="Path to profile JSON file")
    parser.add_argument("--top", type=int, default=10, help="Number of entries to show (default: 10)")
    parser.add_argument("--csv", action="store_true", help="Output CSV instead of formatted table")
    args = parser.parse_args()

    try:
        data = load(args.profile_json)
    except (OSError, json.JSONDecodeError, ValueError) as e:
        print(f"Error reading {args.profile_json}: {e}", file=sys.stderr)
        return 1

    if not data:
        print("Profile file is empty (no EMU_CALLs were recorded).")
        return 0

    # Sort by total_ns descending for the first table
    by_ns    = sorted(data, key=lambda x: x.get("total_ns", 0), reverse=True)[: args.top]
    # Sort by calls descending for the second table
    by_calls = sorted(data, key=lambda x: x.get("calls", 0), reverse=True)[: args.top]

    total_ns    = sum(r.get("total_ns", 0) for r in data)
    total_calls = sum(r.get("calls", 0)    for r in data)

    if args.csv:
        print("rank,name,calls,total_ns,pct_ns")
        for rank, row in enumerate(by_ns, 1):
            pct = 100.0 * row.get("total_ns", 0) / max(total_ns, 1)
            print(f"{rank},{row['name']},{row['calls']},{row['total_ns']},{pct:.2f}")
        return 0

    print(f"\nlxa Profile Report")
    print(f"  Entries recorded : {len(data)}")
    print(f"  Total calls      : {total_calls:,}")
    print(f"  Total ns         : {total_ns:,}  ({total_ns/1e9:.3f} s)")

    # Annotate with percentage
    for row in by_ns + by_calls:
        row["pct_ns"] = 100.0 * row.get("total_ns", 0) / max(total_ns, 1)

    # Table 1: top by cumulative time
    print_table(
        f"Top {args.top} by cumulative time",
        by_ns,
        "total_ns",
        "Total ns",
    )
    print()
    pct_top = sum(r["pct_ns"] for r in by_ns)
    print(f"  Top-{args.top} account for {pct_top:.1f}% of total recorded time.")

    # Table 2: top by call count
    print_table(
        f"Top {args.top} by call count",
        by_calls,
        "calls",
        "Call count",
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
