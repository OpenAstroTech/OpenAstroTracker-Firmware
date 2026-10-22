#!/usr/bin/env bash
# Run pio test and auto-generate coverage report.
# Usage: ./scripts/test-coverage.sh [-e <env>] [extra pio test args...]
#
# The report is informational; the gate is not. `src/core/` carries a
# --fail-under-line threshold (CORE_COVERAGE_MIN below) because it is the layer
# the refactor exists to make testable: a change that adds untested domain
# decisions should fail CI, not quietly lower a number nobody reads. The
# threshold is the ratchet PLAN.md Phase 6 records — raise it, never lower it.
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

# The gate for `src/core/`, in percent of lines. The Phase 6 ratchet: 85 % was the plan's target
# and core/ sits well above it, so this keeps the target while leaving room for a thin new file.
CORE_COVERAGE_MIN=85

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
echo "Checking the core/ coverage gate (CORE_COVERAGE_MIN=$CORE_COVERAGE_MIN)..."
if gcovr \
    --root . \
    --gcov-executable "$GCOV_TOOL" \
    --filter 'src/core/' \
    --exclude '.pio/*' \
    --exclude 'unit_tests/*' \
    --gcov-ignore-errors=no_working_dir_found \
    --gcov-ignore-errors=source_not_found \
    --fail-under-line "$CORE_COVERAGE_MIN"; then
    echo "✅ core/ coverage is at or above the gate"
else
    echo ""
    echo "❌ core/ coverage is below the gate ($CORE_COVERAGE_MIN%)."
    echo "   The refactor exists to make core/ testable — add a behavioural test for the"
    echo "   decision you added, rather than lowering CORE_COVERAGE_MIN in this script."
    exit 1
fi

echo ""
echo "✅ Coverage report: $OUTPUT_DIR/index.html"
