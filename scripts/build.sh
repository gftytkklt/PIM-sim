#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
cd -- "$repo_root"

# Incremental by default: never remove an existing research build implicitly.
build_dir="${BUILD_DIR:-build/default}"
build_dir="$(realpath -m -- "$build_dir")"
cmake_bin="${CMAKE_BIN:-cmake}"
if [[ -n "${PYTHON:-}" ]]; then
    python_bin="$PYTHON"
elif [[ -x "$repo_root/.venv/bin/python3" ]]; then
    python_bin="$repo_root/.venv/bin/python3"
else
    python_bin=python3
fi
python_bin="$(command -v -- "$python_bin")"
"$cmake_bin" -S "$repo_root" -B "$build_dir" -DPython3_EXECUTABLE="$python_bin" "$@"
"$cmake_bin" --build "$build_dir" --parallel "${BUILD_JOBS:-2}"
