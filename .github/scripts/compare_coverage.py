#!/usr/bin/env python3
"""
Coverage comparison script for entservices-appgateway coverage gate.

Parses L0 and L1 lcov filtered_coverage.info files, extracts overall line
coverage percentages, and compares them against the stored baseline from the
build-metadata branch.

Output is a human-readable summary printed to stdout.

NOTE: This script is INFORMATIONAL ONLY — it always exits 0 and never blocks
      a PR. Threshold and regression warnings are advisory until the gate is
      enforced.
"""

import argparse
import json
import os
import sys
from typing import Optional


# Minimum acceptable line coverage percentage (advisory, not enforced yet).
THRESHOLD = 75.0


# ---------------------------------------------------------------------------
# lcov .info parsing
# ---------------------------------------------------------------------------

def parse_lcov_coverage(path: str) -> Optional[float]:
    """Return overall line coverage % from an lcov .info file, or None.

    An lcov .info file contains per-source-file records separated by
    ``end_of_record``.  Each record may include:
      LF:<lines found>   — total instrumented lines in that file
      LH:<lines hit>     — lines executed at least once

    We aggregate across all records to produce a single project-wide %.
    Returns None when the file is absent, empty, or contains no line data.
    """
    if not path or not os.path.isfile(path):
        return None

    total_found = 0
    total_hit = 0

    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            for raw in fh:
                line = raw.strip()
                if line.startswith("LF:"):
                    try:
                        total_found += int(line[3:])
                    except ValueError:
                        pass
                elif line.startswith("LH:"):
                    try:
                        total_hit += int(line[3:])
                    except ValueError:
                        pass
    except OSError as exc:
        print(f"  WARNING: Could not read {path}: {exc}", file=sys.stderr)
        return None

    if total_found == 0:
        return None

    return round((total_hit / total_found) * 100.0, 2)


# ---------------------------------------------------------------------------
# Baseline loading
# ---------------------------------------------------------------------------

def load_baseline(path: str) -> dict:
    """Load baseline JSON; return an empty dict on any error."""
    if not path or not os.path.isfile(path):
        return {}
    try:
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)
        if isinstance(data, dict):
            return data
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"  WARNING: Could not parse baseline {path}: {exc}", file=sys.stderr)
    return {}


# ---------------------------------------------------------------------------
# Reporting helpers
# ---------------------------------------------------------------------------

def _delta_str(current: float, baseline: float) -> str:
    delta = current - baseline
    sign = "+" if delta >= 0 else ""
    return f"{sign}{delta:.2f}%"


def _threshold_label(value: float) -> str:
    return "PASS" if value >= THRESHOLD else f"WARN  (below {THRESHOLD}% threshold)"


def _regression_label(current: float, baseline: float) -> str:
    if current >= baseline:
        return "PASS"
    return f"WARN  (regression: {_delta_str(current, baseline)} vs baseline)"


def report_suite(
    name: str,
    current: Optional[float],
    baseline: Optional[float],
) -> bool:
    """Print a per-suite summary block.  Returns True when all checks pass."""

    sep = "-" * 56
    print(sep)
    print(f"  Suite : {name}")

    if current is None:
        print("  Status: SKIP  (coverage data not available)")
        print(f"          Check that the {name} test job ran and uploaded")
        print(f"          filtered_coverage.info as part of its artifacts.")
        print(sep)
        return True  # Treat unavailable data as a pass (non-blocking).

    print(f"  Current  : {current:.2f}%")

    if baseline is not None:
        print(f"  Baseline : {baseline:.2f}%  (delta: {_delta_str(current, baseline)})")
        regression_ok = current >= baseline
    else:
        print("  Baseline : N/A  (first-time setup — regression check skipped)")
        regression_ok = True

    threshold_ok = current >= THRESHOLD
    print(f"  Threshold: {_threshold_label(current)}")
    if baseline is not None:
        print(f"  Regression: {_regression_label(current, baseline)}")

    all_ok = threshold_ok and regression_ok
    status = "PASS" if all_ok else "WARN  (advisory — PR is not blocked)"
    print(f"  Result   : {status}")
    print(sep)
    return all_ok


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Compare L0/L1 coverage against the develop baseline.\n"
            "Always exits 0 — coverage gate is informational only."
        )
    )
    parser.add_argument(
        "--baseline",
        required=True,
        metavar="PATH",
        help="Path to coverage-baseline.json (from the build-metadata branch).",
    )
    parser.add_argument(
        "--l0",
        required=False,
        metavar="PATH",
        help="Path to the L0 lcov filtered_coverage.info file.",
    )
    parser.add_argument(
        "--l1",
        required=False,
        metavar="PATH",
        help="Path to the L1 lcov filtered_coverage.info file.",
    )
    args = parser.parse_args()

    baseline = load_baseline(args.baseline)
    baseline_l0: Optional[float] = baseline.get("L0")
    baseline_l1: Optional[float] = baseline.get("L1")

    l0_coverage = parse_lcov_coverage(args.l0) if args.l0 else None
    l1_coverage = parse_lcov_coverage(args.l1) if args.l1 else None

    # ------------------------------------------------------------------
    # Header
    # ------------------------------------------------------------------
    eq = "=" * 56
    print()
    print(eq)
    print("  Coverage Gate Report  (informational — not blocking)")
    print(eq)
    if baseline:
        commit = baseline.get("commit", "unknown")
        ts = baseline.get("timestamp", "unknown")
        print(f"  Baseline commit   : {commit}")
        print(f"  Baseline timestamp: {ts}")
        print(f"  Baseline L0       : {baseline_l0 if baseline_l0 is not None else 'N/A'}%")
        print(f"  Baseline L1       : {baseline_l1 if baseline_l1 is not None else 'N/A'}%")
    else:
        print("  Baseline: not found — first-time setup, regression check skipped.")
    print()

    # ------------------------------------------------------------------
    # Per-suite checks
    # ------------------------------------------------------------------
    l0_ok = report_suite("L0 (unit tests)",        l0_coverage, baseline_l0)
    print()
    l1_ok = report_suite("L1 (integration tests)", l1_coverage, baseline_l1)
    print()

    # ------------------------------------------------------------------
    # Summary
    # ------------------------------------------------------------------
    print(eq)
    if l0_ok and l1_ok:
        print("  Overall: PASS")
    else:
        print("  Overall: WARN  (advisory — PR is NOT blocked)")
        print()
        print("  Action items:")
        if l0_coverage is not None and l0_coverage < THRESHOLD:
            print(f"    - L0 coverage {l0_coverage:.2f}% is below the {THRESHOLD}% target.")
        if (
            baseline_l0 is not None
            and l0_coverage is not None
            and l0_coverage < baseline_l0
        ):
            print(
                f"    - L0 coverage has regressed by "
                f"{_delta_str(l0_coverage, baseline_l0)} vs develop baseline "
                f"({baseline_l0:.2f}%)."
            )
        if l1_coverage is not None and l1_coverage < THRESHOLD:
            print(f"    - L1 coverage {l1_coverage:.2f}% is below the {THRESHOLD}% target.")
        if (
            baseline_l1 is not None
            and l1_coverage is not None
            and l1_coverage < baseline_l1
        ):
            print(
                f"    - L1 coverage has regressed by "
                f"{_delta_str(l1_coverage, baseline_l1)} vs develop baseline "
                f"({baseline_l1:.2f}%)."
            )
    print(eq)
    print()

    # Always exit 0 — this gate is informational only.
    sys.exit(0)


if __name__ == "__main__":
    main()
