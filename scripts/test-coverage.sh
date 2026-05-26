#!/usr/bin/env bash
# Run pio test and auto-generate coverage report.
# Usage: ./scripts/test-coverage.sh [-e <env>] [extra pio test args...]
#
# This wrapper is needed because pio test spawns the test binary as a
# subprocess — coverage data (.gcda files) is only flushed when that
# subprocess exits, so we can't generate the report from inside the
# test binary or from PlatformIO's pre-scripts.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

# Extract environment name from args (default: native)
ENV="native"
NEXT_IS_ENV=0
for arg in "$@"; do
    if [ "$NEXT_IS_ENV" -eq 1 ]; then
        ENV="$arg"
        break
    fi
    if [ "$arg" = "-e" ]; then
        NEXT_IS_ENV=1
    fi
done

# Run tests (default to -e native if no args)
if [ $# -gt 0 ]; then
    pio test "$@" 2>&1
else
    pio test -e native 2>&1
fi
TEST_EXIT=$?

if [ $TEST_EXIT -ne 0 ]; then
    echo ""
    echo "⚠️  Tests failed — skipping coverage report."
    exit $TEST_EXIT
fi

# Determine gcov executable
if [ "$(uname -s)" = "Darwin" ]; then
    GCOV_TOOL="llvm-cov gcov"
else
    GCOV_TOOL="gcov"
fi

# Check if gcovr is available
if ! command -v gcovr &>/dev/null; then
    echo ""
    echo "⚠️  gcovr not found — skipping coverage report (pip install gcovr)"
    exit 0
fi

# Generate coverage report
OUTPUT_DIR=".pio/build/$ENV/coverage_report"
mkdir -p "$OUTPUT_DIR"

echo ""
echo "Generating coverage report..."
gcovr \
    --root . \
    --gcov-executable "$GCOV_TOOL" \
    --html-details "$OUTPUT_DIR/index.html" \
    --markdown "$OUTPUT_DIR/coverage.md" \
    --exclude '.pio/*' \
    --exclude 'unit_tests/*' \
    --gcov-ignore-errors=no_working_dir_found \
    --gcov-ignore-errors=source_not_found \
    --print-summary

echo ""
echo "✅ Coverage report: $OUTPUT_DIR/index.html"
