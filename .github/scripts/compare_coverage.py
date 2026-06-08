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
import datetime
import json
import os
import sys
from typing import Optional


# Minimum acceptable line coverage percentage (advisory, not enforced yet).
THRESHOLD = 75.0

_SEP    = "\u2500" * 63
_HEADER = "\u2500\u2500 Coverage Gate Report " + "\u2500" * 39


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

def _fmt_timestamp(ts: str) -> str:
    """Convert '2026-05-28T12:00:00Z' -> '2026-05-28 12:00 UTC'."""
    try:
        dt = datetime.datetime.strptime(ts, "%Y-%m-%dT%H:%M:%SZ")
        return dt.strftime("%Y-%m-%d %H:%M UTC")
    except ValueError:
        return ts


def _delta_str(current: float, baseline: float) -> str:
    delta = current - baseline
    sign = "+" if delta >= 0 else ""
    return f"{sign}{delta:.2f}%"


def _suite_result(current: Optional[float], baseline: Optional[float]) -> tuple:
    """Return (result_label, threshold_ok, regression_ok, skipped)."""
    if current is None:
        return "SKIP", True, True, True
    threshold_ok = current >= THRESHOLD
    regression_ok = baseline is None or current >= baseline
    if not threshold_ok:
        label = "WARN (below threshold)"
    elif not regression_ok:
        label = "WARN (regression)"
    else:
        label = "PASS"
    return label, threshold_ok, regression_ok, False


def _join_names(names: list) -> str:
    return names[0] if len(names) == 1 else " and ".join(names)


def _build_summary(suite_data: list) -> str:
    """Return a concise single-sentence summary.

    suite_data: list of (name, threshold_ok, regression_ok, skipped)
    """
    active = [(n, t, r) for n, t, r, sk in suite_data if not sk]
    if not active:
        return "Coverage data unavailable — checks skipped."

    below_and_regressed = [n for n, t, r in active if not t and not r]
    below_only          = [n for n, t, r in active if not t and r]
    regressed_only      = [n for n, t, r in active if t and not r]

    if not below_and_regressed and not below_only and not regressed_only:
        return "All checks passed."

    parts = []
    # Same suites are both below threshold AND regressed — combine into one clause.
    if below_and_regressed and not below_only:
        names = _join_names(below_and_regressed)
        verb = "is" if len(below_and_regressed) == 1 else "are"
        hv   = "has" if len(below_and_regressed) == 1 else "have"
        parts.append(
            f"{names} {verb} below the {THRESHOLD}% target and {hv} regressed vs baseline"
        )
    else:
        all_below = below_and_regressed + below_only
        if all_below:
            names = _join_names(all_below)
            verb  = "is" if len(all_below) == 1 else "are"
            parts.append(f"{names} {verb} below the {THRESHOLD}% target")
        if regressed_only:
            names = _join_names(regressed_only)
            hv    = "has" if len(regressed_only) == 1 else "have"
            parts.append(f"{names} {hv} regressed vs baseline")

    return ". ".join(parts) + "."


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
    parser.add_argument("--baseline", required=True, metavar="PATH",
                        help="Path to coverage-baseline.json.")
    parser.add_argument("--l0", required=False, metavar="PATH",
                        help="Path to the L0 lcov filtered_coverage.info file.")
    parser.add_argument("--l1", required=False, metavar="PATH",
                        help="Path to the L1 lcov filtered_coverage.info file.")
    args = parser.parse_args()

    baseline    = load_baseline(args.baseline)
    baseline_l0: Optional[float] = baseline.get("L0")
    baseline_l1: Optional[float] = baseline.get("L1")

    l0_coverage = parse_lcov_coverage(args.l0) if args.l0 else None
    l1_coverage = parse_lcov_coverage(args.l1) if args.l1 else None

    l0_result, l0_t_ok, l0_r_ok, l0_skip = _suite_result(l0_coverage, baseline_l0)
    l1_result, l1_t_ok, l1_r_ok, l1_skip = _suite_result(l1_coverage, baseline_l1)

    suite_data = [
        ("L0", l0_t_ok, l0_r_ok, l0_skip),
        ("L1", l1_t_ok, l1_r_ok, l1_skip),
    ]
    all_ok         = all(t and r for _, t, r, sk in suite_data if not sk)
    overall_status = "PASS" if all_ok else "WARN (advisory — PR not blocked)"
    summary        = _build_summary(suite_data)

    # ------------------------------------------------------------------
    # Output
    # ------------------------------------------------------------------
    print()
    print(_HEADER)
    if baseline:
        commit = baseline.get("commit", "unknown")
        ts     = _fmt_timestamp(baseline.get("timestamp", ""))
        print(f"  Baseline  {commit}  ({ts})")
    else:
        print("  Baseline  N/A  (first-time setup — regression check skipped)")
    print(f"  Threshold {THRESHOLD}%  |  Status {overall_status}")
    print(_SEP)

    # Table
    print(f"  {'Suite':<10}{'Current':<10}{'Baseline':<11}{'Delta':<10}Result")
    for name, current, base, result in [
        ("L0", l0_coverage, baseline_l0, l0_result),
        ("L1", l1_coverage, baseline_l1, l1_result),
    ]:
        cur_str   = f"{current:.2f}%" if current is not None else "N/A"
        base_str  = f"{base:.2f}%"    if base    is not None else "N/A"
        delta_str = _delta_str(current, base) if (current is not None and base is not None) else "N/A"
        print(f"  {name:<10}{cur_str:<10}{base_str:<11}{delta_str:<10}{result}")

    print(_SEP)
    print(f"  {summary}")
    print(_SEP)
    print()

    # Always exit 0 — this gate is informational only.
    sys.exit(0)


if __name__ == "__main__":
    main()
