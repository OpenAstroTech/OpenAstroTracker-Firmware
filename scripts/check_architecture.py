#!/usr/bin/env python3
"""Enforce the layering rules in ARCHITECTURE.md section 5.

`core/` is the domain and `ports/` is what it needs from the outside world. Neither may
reach for a hardware API, a platform type, or a compile-time feature switch: that is what
keeps them host-testable and what makes the `#ifdef`-free promise of Phase 4 achievable.

`ports/` may also not name `core/`: a port describes a need, so pointing it at the layer
that consumes it inverts the dependency arrow. `ports/` speaks `types/` and nothing else.

`src/app/ui/` obeys the same rules (PLAN.md 8.8): it is compiled by `[env:native]` and
host-tested, so a menu screen may reach hardware only through a port.

`hal/` is the bottom layer, so it may not name `ports/`: an adapter composes a HAL backend to
satisfy a port, which makes the port the *consumer* of the hardware capability, not something
the capability depends on. HAL is scanned with that rule only — it is where the Arduino headers
and the feature `#if`s are supposed to live.

Usage:
    python scripts/check_architecture.py          # exit 1 and list violations
    python scripts/check_architecture.py -v       # also print the files scanned
"""

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SUFFIXES = (".h", ".hpp", ".cpp")

# Name -> pattern. Matched against the code with comments removed, so prose that mentions
# "String" or "millis()" does not trip the check.
RULES = {
    "Arduino header": re.compile(r'#\s*include\s*[<"]Arduino\.h[>"]'),
    "String type": re.compile(r'(?<![\w."/>])String\s+[A-Za-z_]'),
    "Serial": re.compile(r"(?<![\w.])Serial\s*[.(]"),
    "millis()": re.compile(r"(?<![\w.])millis\s*\(\s*\)"),
    "micros()": re.compile(r"(?<![\w.])micros\s*\(\s*\)"),
    # The legacy macro's definition lives in `ports/ILogger.hpp` next to the sink it feeds; what is
    # forbidden is a *call* to it from the domain, which is what the lookbehind excludes.
    "LOG() macro": re.compile(r"(?<!define )(?<![\w.])LOG\s*\("),
    "hal/ include": re.compile(r'#\s*include\s*[<"](?:\.\./)?hal/'),
    "adapters/ include": re.compile(r'#\s*include\s*[<"](?:\.\./)?adapters/'),
    "feature #if": re.compile(
        r"#\s*(?:if|ifdef|ifndef|elif)\b[^\n]*(?:AZ_STEPPER_TYPE|ALT_STEPPER_TYPE|FOCUS_STEPPER_TYPE"
        r"|RA_DRIVER_TYPE|DEC_DRIVER_TYPE|DISPLAY_TYPE|INFO_DISPLAY_TYPE|USE_HALL_SENSOR|USE_RA_END_SWITCH"
        r"|USE_DEC_END_SWITCH|USE_GPS|USE_GYRO_LEVEL|WIFI_ENABLED)"
    ),
}

# Directory -> the rules applied to the files under it.
#
# `core/`, `ports/` and `app/ui/` are the host-testable layers: the hardware-free set above plus
# the one layering rule each has of its own. `hal/` gets its layering rule *only*, because the
# hardware APIs and the platform feature `#if`s are exactly what that layer exists to use.
SCANNED_RULES = {
    "src/core": RULES,
    "src/app/ui": RULES,
    "src/ports": {
        **RULES,
        # A port names `types/` and nothing that consumes it.
        "core/ include": re.compile(r'#\s*include\s*[<"](?:\.\./)*core/'),
    },
    "src/hal": {
        # The bottom layer: backends expose capabilities, they do not ask for domain interfaces.
        "ports/ include": re.compile(r'#\s*include\s*[<"](?:\.\./)*ports/'),
    },
}


def _blank_block_comment(match: "re.Match[str]") -> str:
    """Replace a block comment with its own newlines, so line numbers stay meaningful."""
    return "\n" * match.group(0).count("\n")


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", _blank_block_comment, text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def scan(verbose: bool) -> int:
    violations = 0
    scanned = 0
    for directory, rules in SCANNED_RULES.items():
        base = ROOT / directory
        if not base.is_dir():
            print(f"check-architecture: missing {directory}", file=sys.stderr)
            return 1
        for path in sorted(base.rglob("*")):
            if path.suffix not in SUFFIXES:
                continue
            scanned += 1
            code = strip_comments(path.read_text(encoding="utf-8", errors="ignore"))
            for rule, pattern in rules.items():
                for match in pattern.finditer(code):
                    line = code.count("\n", 0, match.start()) + 1
                    rel = path.relative_to(ROOT)
                    print(f"{rel}:{line}: {rule}: {match.group(0).strip()[:70]}")
                    violations += 1
    if verbose:
        print(f"check-architecture: scanned {scanned} files in {', '.join(SCANNED_RULES)}")
    return violations


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-v", "--verbose", action="store_true", help="report what was scanned")
    args = parser.parse_args()

    violations = scan(args.verbose)
    if violations:
        print(f"\ncheck-architecture: {violations} violation(s)", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
