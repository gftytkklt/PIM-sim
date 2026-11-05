#!/usr/bin/env bash
# Resolve repository code and the selected extension without changing the caller's cwd.
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
build_dir="${BUILD_DIR:-build/default}"
if [[ "$build_dir" != /* ]]; then build_dir="$repo_root/$build_dir"; fi
build_dir="$(realpath -m -- "$build_dir")"
if [[ -n "${PYTHON:-}" ]]; then
    python_bin="$PYTHON"
elif [[ -x "$repo_root/.venv/bin/python3" ]]; then
    python_bin="$repo_root/.venv/bin/python3"
else
    python_bin=python3
fi
export PYTHONPATH="$build_dir/python:$repo_root/src/python:$repo_root/third_party"
export PYTHONPYCACHEPREFIX="${PYTHONPYCACHEPREFIX:-$build_dir/pycache}"
export PIM_ARTIFACTS_DIR="${PIM_ARTIFACTS_DIR:-$repo_root/artifacts}"
exec "$python_bin" -P "$@"
