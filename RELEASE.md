# Release Process

This document describes how releases are managed for OpenAstroTracker-Firmware. It is intended for project developers with write access to the repository.

## Overview

Releases are fully automated via [release-please](https://github.com/googleapis/release-please). The process requires no manual version bumping or changelog editing — both are driven by PR titles following the [Conventional Commits](https://www.conventionalcommits.org/) specification.

```
PR merged → release-please updates its Release PR → maintainer merges Release PR → tag + GitHub Release created automatically
```

## Conventional Commits (required for all PRs)

Every PR title **must** follow this format:

```
<type>: <short description>
```

CI will reject PR titles that do not conform.

| Type | Triggers | Example |
|------|----------|---------|
| `feat!` or `BREAKING CHANGE:` footer | major version bump | `feat!: remove legacy serial protocol` |
| `feat` | minor version bump | `feat: add AZ/ALT steps-per-degree Meade command` |
| `fix` | patch version bump | `fix: correct sidereal rate after meridian flip` |
| `perf` | patch version bump | `perf: reduce interrupt latency in stepper ISR` |
| `chore` | no version bump | `chore: update platformio dependencies` |
| `docs` | no version bump | `docs: clarify wiring diagram for RAMPS` |
| `refactor` | no version bump | `refactor: extract stepper configuration logic` |
| `test` | no version bump | `test: add unit tests for DayTime rollover` |
| `ci` | no version bump | `ci: update clang-format action version` |

The description becomes the changelog entry, so write it as a user-facing statement of what changed — not what you did internally.

## How release-please Works

After each PR is merged into `develop`, the `release-please` GitHub Action updates an open **Release PR**. This PR:

- Accumulates all `feat`, `fix`, and other notable commits since the last release
- Proposes the next semantic version (patch / minor / major based on commit types)
- Updates `CHANGELOG.md` with categorised entries linked to PRs
- Updates `Version.h` with the new version string

The Release PR stays open and self-updates as more PRs are merged. When you are ready to cut a release, simply **merge the Release PR**.

## Cutting a Release

1. Review the open Release PR (titled `chore(main): release V<version>`) — check the proposed version and changelog entries.
2. Merge it into `develop`.
3. `release-please` automatically:
   - Creates a git tag `V<version>` (e.g. `V1.14.0`)
   - Publishes a GitHub Release with the generated changelog

No manual tag pushing, no manual `Version.h` edits.

## Version Semantics

Versions follow [Semantic Versioning](https://semver.org/) (`MAJOR.MINOR.PATCH`):

| Change | Bump |
|--------|------|
| Breaking change in Meade protocol or configuration | **Major** |
| New feature, new board support, new Meade command | **Minor** |
| Bug fix, performance improvement | **Patch** |
| Chore, CI, refactor, test, docs | **None** |

## CI Checks on Every PR

All of the following must pass before a PR can be merged:

| Check | What it validates |
|-------|------------------|
| **PR Title (Conventional Commits)** | PR title follows `<type>: <description>` format |
| **Unit Tests** | `pio test -e native` passes |
| **Build (mksgenlv21)** | Firmware compiles for MKS Gen L V2.1 |
| **Build (mksgenlv2)** | Firmware compiles for MKS Gen L V2 |
| **Build (mksgenlv1)** | Firmware compiles for MKS Gen L V1 |
| **Build (esp32)** | Firmware compiles for ESP32 |
| **Build (ramps)** | Firmware compiles for RAMPS |
| **clang-format** | Code formatting matches `.clang-format`; auto-fixed on same-repo branches |

## Firmware Binaries

Pre-built binaries are **not** published in GitHub Releases. Because the firmware is configured via generated header files (stepper drivers, sensors, display type, etc.), a generic binary would not be usable without recompilation. Users must build the firmware for their specific hardware configuration using PlatformIO.


