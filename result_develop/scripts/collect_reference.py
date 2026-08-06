#!/usr/bin/env python3
"""Collect reference data before refactoring.

Usage:
    source /home/gftyt/PIMapping/pimapping/bin/activate
    python3 result_develop/scripts/collect_reference.py [--full]
"""

import os
import sys
import pickle
import time
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT))

from onnx_analysis import make_hw_info
from perf import perf_analysis
from logger import Logger


def run_ctest(build_dir: Path) -> dict:
    """Run ctest and capture results."""
    print("=" * 60)
    print("Running ctest (reference) ...")
    if not build_dir.exists():
        print(f"  WARNING: build directory {build_dir} does not exist")
        return {"passed": False, "stdout": "", "stderr": "build dir missing", "returncode": -1}
    result = subprocess.run(
        ["/usr/bin/ctest", "--test-dir", str(build_dir), "-V"],
        capture_output=True, text=True, timeout=120
    )
    passed = "100% tests passed" in result.stdout and "0 tests failed" in result.stdout
    if not passed:
        print(f"  ctest returncode={result.returncode}")
        print(f"  stdout tail: {result.stdout[-500:]}")
    print(f"  ctest passed: {passed}")
    return {
        "passed": passed,
        "stdout": result.stdout,
        "stderr": result.stderr,
        "returncode": result.returncode,
    }


def run_perf_analysis(models_dir: str, hw_info) -> tuple:
    """Run perf_analysis and return results."""
    print("=" * 60)
    print(f"Running perf_analysis(models_dir='{models_dir}') ...")
    start = time.time()
    comm_segs, comm_results = perf_analysis(models_dir=models_dir, hwinfo=hw_info)
    elapsed = time.time() - start
    print(f"  Completed in {elapsed:.1f}s")
    print(f"  Models: {list(comm_results.keys())}")
    for model, opts in comm_results.items():
        for opt, data in opts.items():
            print(f"    {model} opt={opt}: {data}")
    return comm_segs, comm_results


def main():
    # Logger setup
    home_path = str(REPO_ROOT)
    logger = Logger(f"{home_path}/runs", "perf.log")

    hw_info = make_hw_info((256, 256), 8, (0, 0), 1)

    ref_dir = REPO_ROOT / "result_develop" / "reference"
    ref_dir.mkdir(parents=True, exist_ok=True)

    # 1. Run ctest
    build_dir = REPO_ROOT / "build"
    ctest_result = run_ctest(build_dir)
    with open(ref_dir / "ctest_reference.pkl", "wb") as f:
        pickle.dump(ctest_result, f)
    print(f"  ctest reference saved to {ref_dir / 'ctest_reference.pkl'}")

    # 2. Run perf_analysis with demo (quick)
    print("\n" + "=" * 60)
    print("Quick regression (demo):")
    _, comm_results_demo = run_perf_analysis("demo", hw_info)
    with open(ref_dir / "perf_demo_results.pkl", "wb") as f:
        pickle.dump(comm_results_demo, f)
    print(f"  demo reference saved.")

    # 3. Check if --full flag
    if "--full" in sys.argv:
        print("\n" + "=" * 60)
        print("Full regression (models):")
        _, comm_results_full = run_perf_analysis("models", hw_info)
        with open(ref_dir / "perf_models_results.pkl", "wb") as f:
            pickle.dump(comm_results_full, f)
        print(f"  models reference saved.")

    print("\n" + "=" * 60)
    print("Reference data collection complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())