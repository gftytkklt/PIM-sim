#!/usr/bin/env bash
# Incremental build, then exactly one ctest run plus immutable reference comparison.
# BUILD_DIR=/tmp/pim-build PYTHON=/path/to/python scripts/regression/regression_test.sh [compare.py options]
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
cd -- "$repo_root"
build_dir="$(realpath -m -- "${BUILD_DIR:-build/default}")"
BUILD_DIR="$build_dir" "$repo_root/scripts/build.sh"
# compare.py runs ctest once unless --perf-only was explicitly requested.
# No grep/tail pipe can hide a failed comparison or empty test collection.
BUILD_DIR="$build_dir" "$repo_root/scripts/python.sh" \
    "$script_dir/compare.py" --build-dir "$build_dir" "$@"
