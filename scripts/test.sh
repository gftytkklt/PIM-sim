#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
cd -- "$repo_root"
if (( $# > 1 )) || [[ "${1:-}" =~ [^a-zA-Z0-9_-] ]]; then
    echo "Usage: scripts/test.sh [exact_test_name]" >&2
    exit 2
fi
"$script_dir/build.sh"
args=(--test-dir "${BUILD_DIR:-build/default}" --output-on-failure --no-tests=error)
if [[ -n "${1:-}" ]]; then
    args+=(-R "^${1}$")
fi
"${CTEST_BIN:-ctest}" "${args[@]}"
