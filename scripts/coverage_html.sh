#!/usr/bin/env sh
set -eu

if command -v python3 >/dev/null 2>&1; then
    exec python3 -m gcovr --config gcovr.cfg --html-details .pio/coverage.html "$@"
elif command -v python >/dev/null 2>&1; then
    exec python -m gcovr --config gcovr.cfg --html-details .pio/coverage.html "$@"
else
    exec gcovr --config gcovr.cfg --html-details .pio/coverage.html "$@"
fi
