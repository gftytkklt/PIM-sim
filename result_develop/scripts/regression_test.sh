#!/bin/bash
# Quick regression test: build + ctest + perf_analysis(demo)
# Usage: source /home/gftyt/PIMapping/pimapping/bin/activate && bash result_develop/scripts/regression_test.sh
#
# Full regression test: add --full flag
# Usage: source /home/gftyt/PIMapping/pimapping/bin/activate && bash result_develop/scripts/regression_test.sh --full
#
# Ctest only:
# Usage: source /home/gftyt/PIMapping/pimapping/bin/activate && bash result_develop/scripts/regression_test.sh --ctest-only

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PYTHON="/home/gftyt/PIMapping/pimapping/bin/python3"

cd "$REPO_ROOT"

echo "========================================="
echo "PIM-sim Regression Test"
echo "========================================="

# 1. Build
echo ""
echo "[1/3] Building..."
rm -rf build
mkdir build
cd build
/usr/bin/cmake .. > /dev/null 2>&1
make -j$(nproc) > /dev/null 2>&1
cd "$REPO_ROOT"
echo "  Build complete."

# 2. ctest
echo ""
echo "[2/3] Running ctest..."
/usr/bin/ctest --test-dir "$REPO_ROOT/build" -V 2>&1 | tail -5
if /usr/bin/ctest --test-dir "$REPO_ROOT/build" --output-on-failure > /dev/null 2>&1; then
    echo "  ctest: PASS"
else
    echo "  ctest: FAIL"
    exit 1
fi

# 3. perf_analysis
FULL_FLAG=""
if [ "$1" = "--full" ]; then
    FULL_FLAG="--full"
fi
CTEST_ONLY=""
if [ "$1" = "--ctest-only" ]; then
    CTEST_ONLY="--ctest-only"
fi

echo ""
echo "[3/3] Running perf_analysis comparison..."
$PYTHON "$SCRIPT_DIR/compare.py" $FULL_FLAG $CTEST_ONLY 2>&1 | grep -E "^(===|  |SUMMARY|  ctest|  perf|  FAIL|  PASS)"
echo ""
echo "========================================="
echo "Regression test complete."