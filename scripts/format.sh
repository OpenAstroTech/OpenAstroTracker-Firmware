#!/usr/bin/env sh

find . \( -path './.git' -o -path './.pio' -o -path './build_cache' -o -path './src/libs/TimerInterrupt' \) -prune -o -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -exec clang-format -i {} +