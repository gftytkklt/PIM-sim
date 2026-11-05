#!/usr/bin/env python3
"""Collect reference data before refactoring.

Usage:
    scripts/python.sh scripts/regression/collect_reference.py --output-dir /path/to/new/snapshot [--full]
"""

import os
import sys
import pickle
import time
import subprocess
import argparse
import json
import platform
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.append(str(REPO_ROOT / "src" / "python"))
sys.path.append(str(REPO_ROOT / "third_party"))

from pim_sim.onnx_analysis import make_hw_info
from pim_sim.analysis import perf_analysis, _implementation_fingerprint, _file_hash
from pim_sim.paths import DEMO_MODELS_DIR, MODELS_DIR, DEFAULT_SIM_CONFIG


def run_ctest(build_dir: Path) -> dict:
    """Run ctest and capture results."""
    print("=" * 60)
    print("Running ctest (reference) ...")
    if not build_dir.exists():
        print(f"  WARNING: build directory {build_dir} does not exist")
        return {"passed": False, "stdout": "", "stderr": "build dir missing", "returncode": -1}
    result = subprocess.run(
        ["ctest", "--test-dir", str(build_dir), "--no-tests=error", "-V"],
        capture_output=True, text=True, timeout=120
    )
    passed = result.returncode == 0 and "100% tests passed" in result.stdout and "0 tests failed" in result.stdout
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
    parser = argparse.ArgumentParser(description="Create a new reviewable reference snapshot; never overwrite one.")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, default=REPO_ROOT / "build" / "default")
    parser.add_argument("--full", action="store_true")
    args = parser.parse_args()
    if args.output_dir.exists():
        parser.error("output-dir already exists; choose a new snapshot directory")
    hw_info = make_hw_info((256, 256), 8, (0, 0), 1)
    ctest_result = run_ctest(args.build_dir)
    if not ctest_result["passed"]:
        raise RuntimeError("Cannot generate a passing reference from failed/empty tests")
    payloads = {"ctest_reference.pkl": ctest_result}
    mappings = {}
    inputs = [("demo", DEMO_MODELS_DIR)]
    if args.full:
        inputs.append(("models", MODELS_DIR))
    for label, directory in inputs:
        mapped, results = run_perf_analysis(str(directory), hw_info)
        payloads[f"perf_{label}_results.pkl"] = results
        mappings[label] = mapped.provenance
    # Produce files only after every requested run succeeds. Existing snapshots
    # are immutable, even when the caller accidentally repeats this command.
    args.output_dir.mkdir(parents=True, exist_ok=False)
    for name, payload in payloads.items():
        with open(args.output_dir / name, "xb") as handle:
            pickle.dump(payload, handle)
    manifest = {"python": platform.python_version(), "inputs": mappings,
                "python_executable": sys.executable,
                "implementation": _implementation_fingerprint(),
                "simconfig_sha256": _file_hash(DEFAULT_SIM_CONFIG),
                "command": sys.argv, "build_dir": str(args.build_dir.resolve()),
                "role": "candidate reference; independent review is required"}
    (args.output_dir / "manifest.json").write_text(json.dumps(manifest, indent=2))
    print(f"Candidate reference written to {args.output_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
