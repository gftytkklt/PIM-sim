#!/usr/bin/env python3
"""Compare current test results against reference data.

Usage:
    source /home/gftyt/PIMapping/pimapping/bin/activate
    python3 result_develop/scripts/compare.py [--full] [--ctest-only] [--perf-only]
"""

import os
import sys
import pickle
import subprocess
import time
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT))

from onnx_analysis import make_hw_info
from perf import perf_analysis
from logger import Logger


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
        if abs(a - b) > 1e-9 and (abs(a) > 1e-9 or abs(b) > 1e-9):
            diffs.append(f"{path}: {a} != {b}")
    else:
        if a != b:
            diffs.append(f"{path}: {a!r} != {b!r}")
    return diffs


def compare_ctest(build_dir: Path) -> dict:
    """Run ctest and compare with reference."""
    print("=" * 60)
    print("Running ctest (current) ...")
    result = subprocess.run(
        ["/usr/bin/ctest", "--test-dir", str(build_dir), "-V"],
        capture_output=True, text=True, timeout=120
    )
    passed = "100% tests passed" in result.stdout

    ref_path = REPO_ROOT / "result_develop" / "reference" / "ctest_reference.pkl"
    if not ref_path.exists():
        print("  WARNING: No ctest reference found. Saving current as reference.")
        ref_path.parent.mkdir(parents=True, exist_ok=True)
        with open(ref_path, "wb") as f:
            pickle.dump({"passed": passed, "stdout": result.stdout, "stderr": result.stderr, "returncode": result.returncode}, f)
        return {"status": "NEW_REFERENCE", "passed": passed}

    with open(ref_path, "rb") as f:
        ref = pickle.load(f)

    if passed != ref.get("passed", False):
        return {"status": "FAIL", "reason": f"ctest passed: {passed} (ref: {ref.get('passed')})"}
    return {"status": "PASS", "passed": passed}


def compare_perf(models_dir: str, hw_info, ref_results_path: Path) -> dict:
    """Run perf_analysis and compare with reference."""
    label = models_dir
    print("=" * 60)
    print(f"Running perf_analysis(models_dir='{label}') ...")

    if not ref_results_path.exists():
        print(f"  WARNING: No reference data for '{label}'. Saving current as reference.")
        start = time.time()
        _, comm_results = perf_analysis(models_dir=models_dir, hwinfo=hw_info)
        elapsed = time.time() - start
        print(f"  Completed in {elapsed:.1f}s")
        ref_results_path.parent.mkdir(parents=True, exist_ok=True)
        with open(ref_results_path, "wb") as f:
            pickle.dump(comm_results, f)
        return {"status": "NEW_REFERENCE", "models": list(comm_results.keys())}

    with open(ref_results_path, "rb") as f:
        ref_results = pickle.load(f)

    start = time.time()
    _, comm_results = perf_analysis(models_dir=models_dir, hwinfo=hw_info)
    elapsed = time.time() - start
    print(f"  Completed in {elapsed:.1f}s")

    diffs = deep_compare(ref_results, comm_results, "comm_results")
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
    hw_info = make_hw_info((256, 256), 8, (0, 0), 1)
    build_dir = REPO_ROOT / "build"
    ref_dir = REPO_ROOT / "result_develop" / "reference"

    run_ctest = "--perf-only" not in sys.argv
    run_perf = "--ctest-only" not in sys.argv
    full = "--full" in sys.argv

    results = {}

    if run_ctest:
        results["ctest"] = compare_ctest(build_dir)

    if run_perf:
        results["perf_demo"] = compare_perf(
            "demo", hw_info,
            ref_dir / "perf_demo_results.pkl",
        )
        if full:
            results["perf_models"] = compare_perf(
                "models", hw_info,
                ref_dir / "perf_models_results.pkl",
            )

    print("\n" + "=" * 60)
    print("SUMMARY")
    all_pass = True
    for name, result in results.items():
        status = result.get("status", "UNKNOWN")
        print(f"  {name}: {status}")
        if status not in ("PASS", "NEW_REFERENCE"):
            all_pass = False

    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())