#!/usr/bin/env sh
set -eu

PATH="$HOME/Library/Python/3.14/bin:$PATH"
exec gcovr --config gcovr.cfg --html-details .pio/coverage.html "$@"
