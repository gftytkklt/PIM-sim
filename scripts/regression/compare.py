#!/usr/bin/env python3
"""Compare current test results against reference data.

Usage:
    scripts/python.sh scripts/regression/compare.py [--full] [--ctest-only] [--perf-only]
"""

import os
import sys
import pickle
import subprocess
import time
import math
import argparse
import re
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
# The launcher supplies the selected extension; never add the repository root.
sys.path.append(str(REPO_ROOT / "src" / "python"))
sys.path.append(str(REPO_ROOT / "third_party"))

from pim_sim.onnx_analysis import make_hw_info
from pim_sim.analysis import perf_analysis
from pim_sim.paths import DEMO_MODELS_DIR, MODELS_DIR

REFERENCE_DIR = REPO_ROOT / "experiments" / "references" / "legacy"


def deep_compare(a: Any, b: Any, path: str = "") -> list[str]:
    """Deep compare two data structures, return list of differences."""
    diffs = []
    if type(a) != type(b):
        diffs.append(f"{path}: type mismatch {type(a).__name__} vs {type(b).__name__}")
        return diffs
    if isinstance(a, dict):
        all_keys = set(a.keys()) | set(b.keys())
        for k in sorted(all_keys, key=str):
            if k not in a:
                diffs.append(f"{path}.{k}: missing in current")
            elif k not in b:
                diffs.append(f"{path}.{k}: extra in current")
            else:
                diffs.extend(deep_compare(a[k], b[k], f"{path}.{k}"))
    elif isinstance(a, (list, tuple)):
        if len(a) != len(b):
            diffs.append(f"{path}: length mismatch {len(a)} vs {len(b)}")
        else:
            for i, (va, vb) in enumerate(zip(a, b)):
                diffs.extend(deep_compare(va, vb, f"{path}[{i}]"))
    elif isinstance(a, float):
        if not math.isfinite(a) or not math.isfinite(b) or not math.isclose(a, b, rel_tol=0, abs_tol=1e-9):
            diffs.append(f"{path}: {a} != {b}")
    else:
        if a != b:
            diffs.append(f"{path}: {a!r} != {b!r}")
    return diffs


def _performance_payload_error(results):
    if not isinstance(results, dict) or not results:
        return "Expected a nonempty model-result dictionary"
    metrics = ("path_num", "datavolume", "total_hops", "total_congestion")
    for model, strategies in results.items():
        if not isinstance(model, str) or not model:
            return "Model identities must be nonempty strings"
        if not isinstance(strategies, dict) or not strategies:
            return f"{model}: expected at least one strategy result"
        for strategy, values in strategies.items():
            if (not isinstance(strategy, tuple) or len(strategy) != 2
                    or any(not isinstance(value, (int, bool)) or value not in (0, 1)
                           for value in strategy)):
                return f"{model}: invalid strategy {strategy!r}"
            if not isinstance(values, dict):
                return f"{model}/{strategy}: expected a metric dictionary"
            for metric in metrics:
                value = values.get(metric)
                if (isinstance(value, bool) or not isinstance(value, (int, float))
                        or not math.isfinite(value) or value < 0):
                    return f"{model}/{strategy}: {metric} must be finite and nonnegative"
    return None


def _ctest_summary(stdout):
    if not isinstance(stdout, str):
        return None
    match = re.search(r"(\d+)% tests passed, (\d+) tests failed out of (\d+)", stdout)
    if not match or int(match[3]) <= 0:
        return None
    return tuple(int(value) for value in match.groups())


def _read_reference(path):
    try:
        with open(path, "rb") as handle:
            return pickle.load(handle), None
    except (OSError, pickle.PickleError, EOFError, ValueError, TypeError, AttributeError, ImportError) as error:
        return None, f"Unreadable reference {path}: {type(error).__name__}: {error}"


def compare_ctest(build_dir: Path, ref_path=None) -> dict:
    """Run ctest and compare with reference."""
    print("=" * 60)
    print("Running ctest (current) ...")
    ref_path = Path(ref_path) if ref_path else REFERENCE_DIR / "ctest_reference.pkl"
    if not ref_path.is_file():
        return {"status": "MISSING_REFERENCE", "reason": str(ref_path)}
    ref, error = _read_reference(ref_path)
    if error:
        return {"status": "INVALID_REFERENCE", "reason": error}
    if (not isinstance(ref, dict) or not isinstance(ref.get("passed"), bool)
            or type(ref.get("returncode")) is not int or _ctest_summary(ref.get("stdout")) is None):
        return {"status": "INVALID_REFERENCE", "reason": "Expected ctest metadata with a nonempty test summary"}
    reference_summary = _ctest_summary(ref["stdout"])
    result = subprocess.run(
        ["ctest", "--test-dir", str(build_dir), "--no-tests=error", "--output-on-failure"],
        capture_output=True, text=True, timeout=120
    )
    current_summary = _ctest_summary(result.stdout)
    passed = result.returncode == 0 and current_summary is not None and current_summary[:2] == (100, 0)

    if not passed or not ref["passed"] or ref["returncode"] != 0 or reference_summary[:2] != (100, 0):
        return {"status": "FAIL", "reason": f"ctest passed: {passed} (ref: {ref.get('passed')})"}
    return {"status": "PASS", "passed": passed,
            "tests": current_summary[2], "reference_tests": reference_summary[2]}


def compare_perf(models_dir: str, hw_info, ref_results_path: Path) -> dict:
    """Run perf_analysis and compare with reference."""
    label = models_dir
    print("=" * 60)
    print(f"Running perf_analysis(models_dir='{label}') ...")

    if not ref_results_path.exists():
        return {"status": "MISSING_REFERENCE", "reason": str(ref_results_path)}

    ref_results, error = _read_reference(ref_results_path)
    error = error or _performance_payload_error(ref_results)
    if error:
        return {"status": "INVALID_REFERENCE", "reason": error}

    start = time.time()
    _, comm_results = perf_analysis(models_dir=models_dir, hwinfo=hw_info)
    elapsed = time.time() - start
    print(f"  Completed in {elapsed:.1f}s")
    error = _performance_payload_error(comm_results)
    if error:
        return {"status": "INVALID_CURRENT", "reason": error}

    diffs = deep_compare(comm_results, ref_results, "comm_results")
    if diffs:
        print(f"  FAIL: {len(diffs)} difference(s) found:")
        for d in diffs[:20]:
            print(f"    {d}")
        if len(diffs) > 20:
            print(f"    ... and {len(diffs) - 20} more")
        return {"status": "FAIL", "differences": diffs, "models": list(comm_results.keys())}
    else:
        print("  PASS: Results identical to reference.")
        return {"status": "PASS", "models": list(comm_results.keys())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--full", action="store_true")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--ctest-only", action="store_true")
    group.add_argument("--perf-only", action="store_true")
    parser.add_argument("--build-dir", type=Path, default=REPO_ROOT / "build" / "default")
    parser.add_argument("--reference-dir", type=Path, default=REFERENCE_DIR)
    args = parser.parse_args()
    hw_info = make_hw_info((256, 256), 8, (0, 0), 1)
    build_dir = args.build_dir
    ref_dir = args.reference_dir

    run_ctest = not args.perf_only
    run_perf = not args.ctest_only
    full = args.full

    results = {}

    def record(name, operation):
        try:
            results[name] = operation()
        except Exception as error:
            results[name] = {"status": "ERROR", "reason": f"{type(error).__name__}: {error}"}
            print(f"  ERROR: {results[name]['reason']}")

    if run_ctest:
        record("ctest", lambda: compare_ctest(build_dir, ref_dir / "ctest_reference.pkl"))

    if run_perf:
        record("perf_demo", lambda: compare_perf(
            str(DEMO_MODELS_DIR), hw_info,
            ref_dir / "perf_demo_results.pkl",
        ))
        if full:
            record("perf_models", lambda: compare_perf(
                str(MODELS_DIR), hw_info,
                ref_dir / "perf_models_results.pkl",
            ))

    print("\n" + "=" * 60)
    print("SUMMARY")
    all_pass = True
    for name, result in results.items():
        status = result.get("status", "UNKNOWN")
        print(f"  {name}: {status}")
        if status != "PASS":
            all_pass = False

    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
