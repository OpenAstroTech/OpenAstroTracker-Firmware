# Plan: Clean-Architecture Refactor of OpenAstroTracker-Firmware

## TL;DR
Refactor a ~5k-line `Mount` god-object firmware toward clean architecture for embedded:
a pure **Domain Core** (no Arduino, fully unit-testable) sitting behind **Port interfaces**,
with **Adapters** wrapping hardware (AccelStepper, TMC2209, EEPROM, displays, Wi-Fi, clock).
Approach: **hybrid** — extract pure logic into `core/` first, then test (invert the typical "test-first" order because `src/` root files transitively include `<Arduino.h>` via `inc/Globals.hpp` and are untestable in the `native` PIO env), then **strangler-fig** the hardware-coupled pieces (drivers, slewing loop, command executor) behind ports. Compile-time `#ifdef` axes/drivers migrate to **runtime polymorphism** so unsupported combinations no longer change the call graph. Goal endpoint: `src/core/` is buildable & 100% unit-tested on host; `src/adapters/` contains all Arduino/library coupling; `src/app/` wires them up per board.

---

## Architecture

### Goals

| # | Goal | Rationale |
|---|------|-----------|
| 1 | **Make domain logic host-testable** | The `native` PIO env can't compile `src/` root files (they transitively include `<Arduino.h>` via `inc/Globals.hpp`). Pure logic must live in `src/core/` with zero Arduino deps so tests run in milliseconds, not on hardware. |
| 2 | **Shrink the `Mount` god-object** | `Mount.cpp` is ~5000 LOC holding all domain behavior. End state: `Mount` is a thin facade (≤ 500 LOC) over focused controllers in `core/`. |
| 3 | **Eliminate `#ifdef` from domain code** | Feature flags for axes, driver types, and sensors currently produce 2^N untested build combinations. These become runtime polymorphism behind interfaces — the composition root makes one choice; `core/` never branches. |
| 4 | **Swap hardware without touching logic** | Changing stepper library, display type, or EEPROM backend must not require edits to `core/` or `ports/`. |
| 5 | **Coverage that means something** | Target: `core/` ≥ 85% line coverage with tests that verify behavior (not tautologies). |

### Requirements & Constraints

| Category | Constraint | Why |
|----------|-----------|-----|
| **Hardware** | Must build & run on all supported boards: AVR_MEGA2560, MKS_GEN_L_V1/V2/V21, ESP32_ESP32DEV, RP2040_JACKW01, RP2040_SKR_PICO (`oaeboardv1` is also a PIO env). The CI matrix builds **7** boards | Existing user base; CI matrix enforces this. |
| **Memory** | AVR_MEGA2560 has 256 KB flash, 8 KB SRAM | Flash overflow is caught at link time. Mitigation: keep vtable count ≤ ~12, mark adapters `final`, consider template-based static dispatch as escape hatch. |
| **C++ standard** | C++17 for `core/` (needs `etl::optional`, `if constexpr`); boards may require C++14 fallback with ETL polyfills | `native` env already uses `-std=gnu++17`. Board envs will be audited per phase. |
| **Testability** | `core/` must never include `<Arduino.h>` or use `String`, `Serial`, `millis()`, `LOG()` | Enforced by CI grep check. `native` env only compiles `core/`, `ports/`, `adapters/`. |
| **Back-compat** | Meade LX200 serial protocol behavior is invariant | External interfaces (Stellarium, ASCOM) must not break. Internal C++ APIs may change freely. |
| **Shippable increments** | Every phase/step must be independently shippable — all boards green, all tests green | No flag days; thin overlays preserve old call-sites during transition. |
| **Tooling** | PlatformIO + Googletest + FakeIt (bundled with ArduinoFake) + gcovr | Googletest + gcovr are configured in `platformio.ini` and CI (`ci.yml`); ArduinoFake/FakeIt is still **not** wired up (Phase 0, item 5). |

### Layers & Dependencies

Dependencies point inward only. No layer may reference a layer above it.

```mermaid
graph TD
    APP["app/<br/>Composition Root"]
    ADAPTERS["adapters/<br/>Port → HAL glue"]
    PORTS["ports/<br/>Domain interfaces"]
    CORE["core/<br/>Pure domain logic"]
    HAL["hal/<br/>Hardware abstraction<br/>interfaces + backends"]
    HW["Hardware / Libraries<br/>Arduino, AccelStepper, TMCStepper,<br/>EEPROM, SSD1306, TinyGPS++"]

    APP --> ADAPTERS
    APP --> HAL
    APP --> PORTS
    APP --> CORE
    ADAPTERS --> PORTS
    ADAPTERS --> HAL
    CORE --> PORTS
    HAL --> HW

    style CORE fill:#e1f5fe
    style PORTS fill:#fff9c4
    style ADAPTERS fill:#dcedc8
    style HAL fill:#f3e5f5
    style APP fill:#ffe0b2
    style HW fill:#e0e0e0
```

| Layer | Directory | Depends on | Contains | Test strategy |
|-------|-----------|------------|----------|---------------|
| **core/** | `src/core/` | `ports/` only | Domain controllers (`SlewController`, `TrackingController`, …), algorithms (`SiderealClock`, `CoordinateMath`, …), `MeadeParser`, `MountState`, `EventBus`, `MountConfig` | Pure unit tests with FakeIt-faked ports. No hardware, no Arduino. `native` env. |
| **ports/** | `src/ports/` | nothing | Domain interfaces: `IClock`, `ILogger`, `IPersistentStore`, `IStepperAxis`, `IMotorDriver`, `IDisplay`, `IHomingSensor`, `IEndSwitch`, `IGps`, `ITransport` | Pure abstract interfaces — no implementation to test. |
| **adapters/** | `src/adapters/` | `ports/` + `hal/` | Thin glue: `AccelStepperAxis`, `EepromPersistentStore`, `SerialLogger`, `Tmc2209Driver`, `HallHomingSensor`, `SerialTransport`, `WifiTransport`, `MeadeCommandAdapter` | Integration tests: adapter → faked HAL port. |
| **hal/** | `src/hal/` | hardware libraries only | Interfaces (`IGpioPin`, `ISerialPort`, `IStepperMotor`, `ITmcDriver`, `ISystemClock`, `ITimerService`, …) + per-platform backends (`hal/arduino/`, `hal/avr/`, `hal/esp32/`) | Not host-tested (wraps hardware). Verified by on-device smoke tests. |
| **app/** | `src/app/` | all layers | Composition root. Board-specific configuration comes from a header (e.g. `Configuration_local_oaeboardv1.hpp`, selected by the PIO environment) which populates a runtime `MountConfig` struct. Wires up the object graph. Replaces the legacy `.ino` entry point. | Verified by on-device smoke tests across the CI-matrix boards. |

**Why both `hal/` and `ports/`:**
- `hal/` describes *what the hardware can do* (pin toggles, UART bytes, timer ticks). One backend per platform.
- `ports/` describes *what the domain needs* (axis position, persistent value, "now"). Adapters compose one or more HAL services to satisfy a port — e.g. `AccelStepperAxis` implements `IStepperAxis` using `IStepperMotor` + `ITimerService` from HAL.
- This split means a new stepper library requires a new HAL backend, not a port change. A new domain requirement (e.g. "axis acceleration profile") requires a port change, not a HAL change.

### Cross-cutting rules

| Rule | Enforcement |
|------|-------------|
| `core/` never includes `<Arduino.h>`, `String`, `Serial`, `millis()`, `LOG()`, or any board pin header | CI grep check (Phase 4+) |
| `core/` and `ports/` contain zero `#ifdef` for feature flags | CI grep check (Phase 4+) |
| Time = `IClock` port (backed by `hal::ISystemClock`); no direct `millis()` | Interface contract |
| Logging = `ILogger` port (backed by `hal::ISerialPort`); no direct `Serial.print()` | Interface contract |
| Configuration = runtime `MountConfig` struct populated once in `app/` from `Configuration*.hpp` macros | Single translation unit reads macros |
| Optional axes (AZ, ALT, Focus) = `etl::optional` or null-object pattern; no `#ifdef` branches | Composition root constructs or injects null |
| ISR-safety: `IStepperAxis::Snapshot()` returns an ISR-safe state snapshot; mutation methods are main-loop-only | Settled in Phase 2 before Phase 3 controller extraction |
| `Mount` becomes a thin facade (≤ 500 LOC) over `core/` controllers, retained for Meade back-compat, gradually deprecated | Shrinks each phase as controllers are extracted |

---

## Current State (after Meade parser migration + Phase 1 extraction)

### ✅ Completed: Meade parser migrated to `core/`

The Meade LX200 command parser has been fully extracted into `src/core/meade/` as a pure, host-testable, allocation-light parser with no side effects. The split is:

| Layer | Location | Status |
|-------|----------|--------|
| **Parser** (pure) | `src/core/meade/MeadeParser.*` + 12 family-specific `.cpp` files | ✅ Done — covered by 14 test files in `unit_tests/test_core/meade/`; `core/meade/` line coverage 87.0% |
| **Protocol spec** | `src/core/meade/MeadeProtocol.hpp` | ✅ Done — comprehensive protocol documentation |
| **Handler interfaces** | `src/core/meade/MeadeParser.hpp` (12 `IMeade*Handlers` interfaces + aggregate `IMeadeHandlers`) | ✅ Done — clean typed contracts |
| **Executor/Adapter** | `src/MeadeCommandProcessor.hpp/cpp` | ✅ Done — implements `IMeadeHandlers`, delegates to `Mount`/`LcdMenu` |

The parser directory (`src/core/meade/`) contains 17 files (12 family dispatchers + `MeadeParser.cpp/.hpp` + `MeadeParserHelpers.cpp/.hpp` + `MeadeProtocol.hpp`) covering all 12 command families (Get, Set, Quit, Distance, Init, SyncControl, Home, SlewRate, GPS, Focus, Movement, Extra). Each family has:
- A typed handler interface (e.g. `IMeadeGetHandlers`, `IMeadeMovementHandlers`)
- A `handleMeade*` entry point that parses suffixes, invokes handler callbacks, and serializes `MeadeResponse`
- Value types for coordinates (e.g. `RaCoordinate`, `DecCoordinate`)

The `MeadeCommandProcessor` adapter bridges the parser to the legacy `Mount` singleton, implementing all 115 handler overrides. It is the **only** file in `src/` root that references the core parser.

### Test coverage

14 test files in `unit_tests/test_core/meade/` provide comprehensive wire-byte coverage for every parser family using fake handler stubs. Line coverage of `core/meade/` is 87.0% (`MeadeParser.cpp` 43.1%, `MeadeParserExtra.cpp` 69.6%, most family dispatchers ≥ 95%). Tests verify:
- Exact wire-byte formatting (zero-padding, sign rules, terminators)
- Suffix classification and handler dispatch routing
- Edge cases (malformed input, overflow, unknown sub-commands)
- Parser-level validation (the `parseMeadeCommand` classifier)

### ✅ Completed: Phase 1 pure-logic extraction (modules + tests)

All seven Phase 1 modules exist under `src/core/`, plus `src/hal/EepromLayout.hpp`, and are covered by host tests — see the status markers on each Phase 1 step below. Pure arithmetic for `DayTime`, `Declination`, `Latitude`, `Longitude`, `RightAscension` and `Sidereal` is fully extracted, and the `src/` originals are now thin overlays that delegate to `core/`.

**Wiring is complete (Steps 4–7):**

1. ✅ `Mount::calculateRAandDECSteppers()` now builds a `core::SlewGeometry`, calls `core::calculateStepperPositions()`, and copies the result back. Rounding was reconciled to the legacy expressions (`long(-moveRA) * stepsPerSiderealHour`, product truncated on assignment) so the refactor is bit-for-bit identical; `SlewResult` now reports which candidate was selected (`SlewSolution`) plus the post-flip RA/DEC, so `Mount` keeps its existing `DEBUG_COORD_CALC` logs. Pinned by unit tests.
2. ✅ `Mount::getLocalDate()` now converts the elapsed day-ends into a whole-day count and calls `core::addDays()`; the inline month-length/leap-year loop is gone.
3. ✅ `Mount::RAString()`/`DECString()` call `core::formatRA()`/`core::formatDEC()`. `formatRA` gained a `bufSize` argument (it previously hard-coded `snprintf(..., 32, ...)` against `Mount`'s 24-byte `scratchBuffer`) and no longer forces `abs()`, matching the legacy `sprintf` output exactly.
4. ✅ `src/hal/EepromLayout.hpp` is now the single source of truth for magic markers, item flags, item addresses and `SteppingStorageNormalized`. It exposes them as `hal::eeprom::*` (a namespace rather than a struct, so unscoped enumerators can be brought into `EPROMStore.cpp` with one `using namespace` on pre-C++20 toolchains), `EEPROMStore`'s private aliases point at it, and `test_hal/test_eeprom_layout.cpp` pins the on-device layout on the host.

### What remains (Phase 2 onward)

The parser is pure and tested. The executor (`MeadeCommandProcessor`) is an adapter that still couples to `Mount*` and `LcdMenu*` directly. The `Mount` god-object still contains all domain logic (slewing, tracking, guiding, parking, homing, focus, coordinate math). No ports, adapters, HAL interfaces, or controllers exist yet — `src/ports/`, `src/adapters/`, and `src/app/` contain only READMEs, and `src/hal/` contains only `EepromLayout.hpp`.

---

## Phased Plan (each phase shippable & green in CI)

### Phase 0 — Safety net & tooling (no behavior change) ✅ COMPLETE (one open item)
*Foundation for everything else; must land first. Mostly implemented — per-item status below.*

1. ~~Add FFF as header-only dep~~ — **Superseded.** FakeIt (bundled with ArduinoFake) replaces FFF. No separate FFF needed.
2. ~~Add `native_core` PIO env~~ — **Superseded.** Only the `native` env is used. The existing `native` env already has strict warnings (`-Wall -Wextra -Werror -Wpedantic -Wshadow`). Note: `src/core/README.md` still mentions `native_core` and needs updating.
3. ✅ gcovr-based **coverage reporting** in `native` env — **Done.** `scripts/test-coverage.py` appends `--coverage` to `LINKFLAGS`; `build_src_flags` carries the compile-side flag. Reports (HTML + markdown) are produced by `./scripts/test-coverage.sh`. **There is no `coverage` PlatformIO target** — `pio run -e native -t coverage` fails with "unknown target". Current: 90.2% lines, 98.2% functions, 79.6% branches (366 test cases).
4. ✅ CI workflow — **Done.** `.github/workflows/ci.yml` runs `./scripts/test-coverage.sh` (which runs `pio test -e native`, then gcovr) and publishes `coverage.md` to the step summary. Threshold gating deferred to a later phase.
5. ⬜ **ArduinoFake** as `test_lib_deps` — **NOT done.** Absent from `platformio.ini` (no `test_lib_deps` key, no ArduinoFake/FakeIt dependency). `unit_tests/test_common/ArduinoFake/` holds only six `CMakeLists.txt` files — no headers or sources — and no test uses FakeIt; all host tests use plain googletest. Required before Phase 2 can add adapter tests that fake the Arduino API.
6. ✅ Folder structure — **Done.** `src/ports/`, `src/hal/`, `src/adapters/`, `src/app/` all exist with descriptive README files (they link to `specs/plan.md`, which does not exist in the repo).

**Verify:** `./scripts/test-coverage.sh` → 366 test cases, 0 failures; coverage report generates; the 7-board CI matrix builds green via `matrix_build.py`.

**Changes from original plan (per feedback):** FFF replaced by FakeIt (in ArduinoFake); `native_core` env eliminated (use `native` only); coverage threshold gating deferred to a later phase. **Open item:** ArduinoFake/FakeIt is still not wired up (item 5).

### Phase 1 — Extract pure domain logic into `core/`, then test — ✅ COMPLETE
*The original plan called for testing before extraction. That doesn't work here: every `src/` root file includes `inc/Globals.hpp` → `<Arduino.h>`, and uses `String`/`LOG()`/Arduino APIs. The `native` PIO env only builds `core/`, `ports/`, `adapters/` — not `src/` root files. **New approach: extract pure subsets into `core/` first, then write exhaustive tests.** Each step is independently shippable. No behavior change.*

**Structure:** Data containers (`DayTime`, `Declination`, `Latitude`, `Longitude`, `RightAscension`) go under `src/core/types/`. Algorithm modules (`SiderealClock`, `CoordinateMath`, `CalendarMath`, `CoordinateFormatter`) live at `src/core/` root. EEPROM layout constants (`EepromLayout`) live in `src/hal/` — they describe a storage format, not domain behavior.

```
src/core/
├── meade/                        # ✅ extracted — parser + protocol
├── types/                        # ✅ extracted — pure data containers
│   ├── DayTime.hpp/.cpp
│   ├── Declination.hpp/.cpp
│   ├── Latitude.hpp/.cpp
│   ├── Longitude.hpp/.cpp
│   └── RightAscension.hpp/.cpp
├── SiderealClock.hpp/.cpp        # ✅ algorithm producing DayTime outputs
├── CoordinateMath.hpp/.cpp       # ✅ wired into Mount::calculateRAandDECSteppers
├── CalendarMath.hpp/.cpp         # ✅ wired into Mount::calculateLst + Mount::getLocalDate
└── CoordinateFormatter.hpp/.cpp  # ✅ wired into Mount::RAString/DECString
```

The `src/` originals become **thin overlays**: they `#include` the `core/` version and add only Arduino-dependent methods (`ParseFromMeade`, `ToString`; `Declination` and `Longitude` also override `formatString` for their wire formats). This preserves back-compat — no flag day. **Status:** all seven steps are extracted, wired and tested — the `src/` originals no longer hold their own copies of the extracted logic.

#### Step 1: DayTime — extract pure arithmetic subset ✅ DONE
*Parallel with Steps 5, 7.*

**What's pure** (uses only `long` math, no Arduino types): constructors, `getHours/getMinutes/getSeconds/getTotalHours/getTotalMinutes/getTotalSeconds`, `set`, `addHours/addMinutes/addSeconds`, `addTime/subtractTime`, `sign`, `checkHours`.

**What stays** (uses Arduino `String` or `LOG()`): `ParseFromMeade`, `ToString`, `formatString`.

**Plan:**
1. ✅ Create `src/core/types/DayTime.hpp/.cpp` — pure header with arithmetic only.
2. ✅ Keep original `src/DayTime.hpp/cpp` as overlay: `#include "core/types/DayTime.hpp"` + Arduino-dependent methods (`ParseFromMeade`, `ToString`).
3. ✅ Write `unit_tests/test_core/types/test_daytime.cpp` — construction, 24h wrap, negative hours, add/subtract across midnight.

#### Step 2: Declination, Latitude, Longitude, RightAscension — extract pure subsets ✅ DONE
*Depends on Step 1.*

All four inherit `DayTime`. Pure parts: `checkHours()` clamping, degree conversion, constructors. What stays: `ParseFromMeade(String const&)`, `ToString()`, `formatString()`.

**Plan:**
1. ✅ Create `src/core/types/Declination.hpp/.cpp`, `Latitude.hpp/.cpp`, `Longitude.hpp/.cpp`, `RightAscension.hpp/.cpp` — each inherits `core::DayTime`.
2. ✅ Old `src/Declination.hpp` etc. become: `#include "core/types/<Type>.hpp"` + `ParseFromMeade`/`ToString`/`formatString` overlay.
3. ✅ Write `test_declination.cpp`, `test_latitude.cpp`, `test_longitude.cpp`, `test_right_ascension.cpp` — clamping edges, hemisphere handling, 24h wrapping.

#### Step 3: Sidereal — extract pure math, leave GPS behind ✅ DONE
*Depends on Step 1.*

`Sidereal.cpp` is 130 lines. The GPS path (`calculateByGPS`) uses TinyGPS++. The pure math (`calculateByDateAndTime`, `calculateHa`, `calculateTheta`, `calculateDeltaJd`) uses only `double` + `DayTime`.

**Plan:**
1. ✅ Create `src/core/SiderealClock.hpp/.cpp` — pure primitives `calculateTheta`/`calculateDeltaJd`/`calculateHaDegrees`; the `calculateByDateAndTime`/`calculateHa` entry points remain in `src/Sidereal.cpp` and compose them.
2. ✅ Keep old `src/Sidereal.cpp` with `calculateByGPS` + thin wrappers delegating to `core::SiderealClock`.
3. ✅ Create `test_core/test_sidereal.cpp` — LST/HA from known dates (equinoxes, solstices), leap years.

#### Step 4: Mount — extract `calculateRAandDECSteppers` math ✅ DONE
*Depends on Steps 1-2.*

The ~200-line function computes stepper positions from RA/DEC targets. Core math is pure arithmetic on floats/longs (hemisphere, meridian flip, 3-solution selection).

**Plan:**
1. ✅ Create `src/core/CoordinateMath.hpp/.cpp` with plain structs (`SlewGeometry`, `SlewResult`) and free function `calculateStepperPositions()`. `SlewResult` returns the three candidate solutions inline rather than via the out-parameters the original plan described.
2. ✅ `Mount::calculateRAandDECSteppers()` fills a `SlewGeometry` and calls the free function. Rounding was reconciled to the legacy expressions — the candidate solutions truncate the hour delta to whole hours before scaling and the selected target scales the unrounded delta in float — so results are identical to the pre-refactor code. `SlewResult` also reports the selected candidate (`SlewSolution`) and the post-flip RA/DEC, letting `Mount` keep its `DEBUG_COORD_CALC` logs.
3. ✅ Write `test_core/test_coordinate_math.cpp` — covers both flip directions, meridian-flip boundaries, pole targets (including a flipped pole target), the reported solution, and the legacy rounding of the candidate solutions.

#### Step 5: Mount — extract calendar math ✅ DONE
*Parallel with Steps 1, 7.*

`Mount::getLocalDate()` handles year/month/day increment with month-length tables and leap-year logic. Pure integer math.

**Plan:**
1. ✅ Create `src/core/CalendarMath.hpp/.cpp` with `struct CalendarDate`, `addDays()` and `localToUtcDate()`.
2. ✅ `localToUtcDate()` is used by `Mount::calculateLst`, and `Mount::getLocalDate()` now folds the elapsed day-ends into a whole-day count and delegates to `core::addDays()` — the inline month-length/leap-year loop is gone.
3. ✅ Write `test_core/test_calendar.cpp` — month boundaries, leap years incl. century rules.

#### Step 6: Mount — extract coordinate formatting ✅ DONE
*Depends on Steps 1-2.*

`Mount::RAString()` and `Mount::DECString()` format RA as HH:MM:SS# and DEC as sDD*MM:SS#. Pure char-buffer manipulation.

**Plan:**
1. ✅ Create `src/core/CoordinateFormatter.hpp/.cpp` with `formatRA(char*, size_t, const core::DayTime&, const char*)` and `formatDEC(char*, const Declination&, const char*)`. `formatRA` takes the buffer capacity because it previously hard-coded `snprintf(..., 32, ...)` while `Mount::scratchBuffer` is only 24 bytes.
2. ✅ `Mount::RAString()`/`DECString()` call the formatter. `formatRA` no longer forces `abs()` on the hours, so output matches the legacy `sprintf` byte-for-byte (including a negative-hours edge case) and DEC still dispatches through the virtual `Declination::formatString`.
3. ✅ Write `test_core/test_coordinate_formatter.cpp` — exact wire-byte output, sign rules, zero-padding.

#### Step 7: EPROMStore — extract validation logic ✅ DONE
*Parallel with Steps 1, 5.*

`EPROMStore` uses Arduino EEPROM API directly. Data-validation logic (magic markers `0xCE`/`0xCF`, struct layouts) is separable.

**Plan:**
1. ✅ `src/hal/EepromLayout.hpp` holds the layout — magic markers, item flags, extended flags, item addresses and `SteppingStorageNormalized` — with no Arduino deps. It exposes them as `hal::eeprom` (a namespace rather than a struct, so the unscoped enumerators can be pulled into `EPROMStore.cpp` with one `using namespace` on pre-C++20 toolchains instead of being re-listed). `EEPROMStore`'s private `ItemFlag`/`ExtendedItemFlag`/`ItemAddress` are aliases of it, and `unit_tests/test_hal/test_eeprom_layout.cpp` pins every address, flag mask and the store size against the shipped V1.x layout.
2. Full `IPersistentStore` port + adapter comes in Phase 2.

**Verify:** `./scripts/test-coverage.sh` — all new tests pass (366 test cases); every board env builds green via the CI matrix (`python matrix_build.py -b <board>`; a bare `pio run -e <board>` needs an untracked `Configuration_local_*.hpp`); coverage shows ≥ 85% line coverage on new `core/` files (**met** — every Phase 1 module is ~100% except `core/meade/` at 87.0%); manual: mount slews to known coordinates on real hardware.

### Phase 2 — Introduce HAL, Ports & Adapters
*Define the HAL surface, define the domain ports, and start routing current call sites through them. Keep current behavior bit-for-bit. Split into three independently shippable sub-phases: **2a** contracts (no production change), **2b** adapters + `Mount` injection, **2c** call-site cleanup.*

**Blocking prerequisite:** Phase 0 item 5 — ArduinoFake/FakeIt must be wired up (`test_lib_deps` plus real sources under `unit_tests/test_common/ArduinoFake/`). Without it neither Arduino-API fakes nor the adapter integration tests required by ARCHITECTURE.md can be written.

**Calibration facts (measured on the current tree):** `Mount.cpp` is 4,361 LOC with 116 public methods and contains **274 `LOG()` sites (273 literal formats totalling 13,857 bytes, currently kept in PROGMEM via `F()`), 47 `EEPROMStore::` sites, 21 `millis()`, 19 `digitalWrite()`, 184 TMC-driver calls and ~150 stepper-axis calls**. The global is defined in `src/b_setup.hpp` (`Mount mount(&lcdMenu);`); the only `extern Mount mount;` is in `src/testmenu.cpp`. Stepping has **three** concurrency models, not two (step 9).

#### 2a — Contracts (no production changes)

1. Define **HAL interfaces** in `src/hal/`:
   - `IGpioPin`, `ISerialPort`, `ISpiBus`, `II2cBus`, `IEeprom`,
   - `IStepperMotor`, `ITmcDriver` (UART register IO),
   - `IOledPanel`, `ICharLcd`, `IButtonMatrix`,
   - `ITimerService` (periodic/one-shot callbacks), `ISystemClock` (`millis`/`micros`),
   - `IWifiStack`.
   - **Decide `IStepperMotor`'s level first.** Either it exposes step/dir pulses + microstep config (then `AccelStepperAxis` owns the acceleration engine), or it is the motion engine itself. [`src/adapters/README.md`](src/adapters/README.md) assumes the latter (`setTargetSteps`/`startMotion`/`currentSteps`/`isRunning`); the description above assumes the former. The choice determines whether `AccelStepperAxis` can be host-compiled at all.
2. Implement HAL backends (headers stay Arduino-free; each backend directory compiles only for its platform):
   - `hal/arduino/` — generic Arduino implementation (`ArduinoGpioPin`, `ArduinoSerialPort`, `ArduinoEeprom`, `ArduinoSystemClock`, …).
   - `hal/avr/` — AVR-specific bits: the vendored `InterruptStepper` bindings now in [`src/StepperConfiguration.hpp`](src/StepperConfiguration.hpp) and [`src/InterruptAccelStepper.h`](src/InterruptAccelStepper.h), the Timer1/3/4/5 ISR vectors in [`src/Core.cpp`](src/Core.cpp), and the Timer2 path in [`src/InterruptCallback.cpp`](src/InterruptCallback.cpp).
   - `hal/rp2040/` — RP2040-specific: the `add_repeating_timer_us` alarm ISR in [`src/InterruptCallback.cpp`](src/InterruptCallback.cpp) (`TimerInterrupt` is already `lib_ignore`d for those envs).
   - `hal/esp32/` — ESP32-specific (hardware timers, Wi-Fi stack glue).
   - `unit_tests/test_common/hal_fakes/` — pure C++ fakes (in-memory EEPROM, virtual GPIO, controllable clock, fake serial). Test code only, **not** in `src/`.
3. Define domain **ports** in `src/ports/`:
   - `IClock`, `ILogger`, `IPersistentStore`,
   - `IStepperAxis` — sized to the real call surface: `currentPosition`, `setCurrentPosition`, `targetPosition`, `distanceToGo`, `move`, `moveTo`, `runToNewPosition`, `runToPosition`, `run`, `runSpeed`, `stop`, `isRunning`, `setMaxSpeed`, `maxSpeed`, `setAcceleration`, `setSpeed`, `speed`, `setPinsInverted`, plus the ISR-safe `Snapshot()`. Keep the existing two-instances-per-axis split (RA slew + tracking, DEC slew + guide).
   - `IMotorDriver` — sized to the real call surface: `beginSerial`/`begin`, `microsteps`, `rms_current`, `toff`, `blank_time`, `vsense`, `semin`, `iholddelay`, `TPOWERDOWN`, `TCOOLTHRESH`, `SGTHRS`, `en_spreadCycle`, `pdn_disable`, `mstep_reg_select`, `I_scale_analog`, `fclktrim`, `cs_actual`; null implementation for non-TMC.
   - `IHomingSensor`, `IEndSwitch`, `IDisplay`, `IInfoDisplay`, `ITransport`, `IGps`, `IGyro`.
4. Update **`platformio.ini`**: the `native` env compiles *all* of `src/adapters/`, so either every adapter must be host-compilable or `build_src_filter` needs per-file exclusions for Arduino-dependent adapters. Also add a test filter/directory so adapter tests actually run (today only `test_core` + `test_hal` are filtered).

**Verify (2a):** `pio test -e native` green; board builds unchanged (nothing references the new interfaces yet).

#### 2b — Adapters + `Mount` injection

5. Provide **adapters** in `src/adapters/` that bind ports to HAL. Only adapters with no upward dependency on the legacy `Mount` (and no Phase 5 dependency) belong here:
   - `ArduinoClock` (port `IClock` ← hal `ISystemClock`), `SerialLogger`, `EepromPersistentStore` (the [`src/EPROMStore.cpp`](src/EPROMStore.cpp) layout logic moves in; `EEPROMStore` survives as a thin static overlay over the injected `IPersistentStore` while menus and `b_setup` still call it),
   - `AccelStepperAxis`, `InterruptAccelStepperAxis`,
   - `Tmc2209Driver` (UART + standalone variants over `hal::ISerialPort`), `NullMotorDriver`,
   - `TinyGpsAdapter` — define it, but note its only consumer today is the out-of-scope [`src/c72_menuHA_GPS.hpp`](src/c72_menuHA_GPS.hpp) plus a forward-declared `gpsAqcuisitionComplete()` in [`src/MeadeCommandProcessor.cpp`](src/MeadeCommandProcessor.cpp); the real consumer arrives in Phase 5.
   - **Deferred to Phase 3** (they call `Mount` today and would violate "adapters depend only on ports + HAL"): `HallHomingSensor` (~11 Mount calls), `GpioEndSwitch` (~9 Mount calls), `Ssd1306InfoDisplay` (~16 Mount getters), `LcdMenuDisplay` (the menu screens call `mount.*` 170+ times and instantiate LiquidCrystal/U8g2 inside [`src/LcdMenu.hpp`](src/LcdMenu.hpp)). Phase 3's `HomingController`/`EventBus` supplies the read model they need.
   - **Deferred to Phase 5:** `SerialTransport` and `WifiTransport` (Phase 5 step 2 introduces both; they feed `core/meade/MeadeParser` and need the Meade execution layer). Their ports (`ITransport`, `IDisplay`, `IInfoDisplay`) are still defined in 2a and stay unimplemented until then.
6. Refactor `Mount` to **hold port pointers** (`IStepperAxis* _ra; IClock* _clock; ...`) injected at construction instead of owning concrete types. Composition happens in `app/` (currently `b_setup.hpp`). **Migration note:** the global is defined in [`src/b_setup.hpp`](src/b_setup.hpp) (`Mount mount(&lcdMenu);`); the only `extern Mount mount;` is in [`src/testmenu.cpp`](src/testmenu.cpp). Existing `mount.xxx()` call sites keep working through that global — no call-site flag day. The step is atomic *inside* `Mount` (member types change together); keep the `new` sites in `setupRA()`/`setupRADriver()`/`setupHoming()`/`setupEndSwitches()` as the seam until they move into `app/`.
7. **`Mount::delay()`** ([`src/Mount.cpp`](src/Mount.cpp)) re-enters `loop()` and calls `yield()`. Give it its own decision (IClock + a yield/scheduler port, or move it into the app layer) — do not fold it into the generic `millis()` replacement.

**Verify (2b):** adapter tests run against `hal_fakes`; all prior tests green; every board env builds; manual smoke: slew, park/unpark, guide pulses (same objects, same behaviour).

#### 2c — Call-site cleanup

8. Replace `millis()`, `digitalWrite()` and `EEPROMStore::` calls inside `Mount` with port calls. For `LOG()`: **the AVR RAM budget forbids a naive `_logger->log(level, "literal", ...)`** — the 273 format literals (13.9 KB) would move from PROGMEM into `.data` on an 8 KB-SRAM ATmega2560. Settle the `ILogger` contract first: it must preserve PROGMEM on AVR (opaque format type, adapter-side `F()` expansion, or a string table). Only then migrate the 274 call sites.
9. **ISR contract for `IStepperAxis`:** this phase MUST settle the threading model. There are three:
   - **AVR + `NEW_STEPPER_LIB`** (ramps, oae): `Mount::interruptLoop()` is **compiled out** ([`src/Mount.cpp`](src/Mount.cpp)); the vendored `InterruptStepper` ISR steps autonomously and `run()`/`runSpeed()` are stubs. `IStepperAxis` mutation is main-loop-only; reads must go through `Snapshot()`.
   - **ESP32**: `stepperControlTask` on core 0 calls `interruptLoop()` in a tight loop ([`src/b_setup.hpp`](src/b_setup.hpp)).
   - **RP2040 / AVR without `NEW_STEPPER_LIB`**: `InterruptCallback` invokes `interruptLoop()` from a hardware timer ISR (2 kHz AVR, 5 kHz RP2040).
   The contract must also cover `_mountStatus` (a `volatile int`, non-atomic on AVR) and must define timer arbitration: timers are shared, not per-axis (Timer3 = RA slew + track, Timer4 = DEC slew + guide, Timer1 = AZ, Timer5 = ALT *and* Focus). This contract is a prerequisite for Phase 3's `SlewController`.
10. The feature `#if`s inside `interruptLoop()` stay as-is until Phase 4.

**Verify (2c):** all Phase 1 tests green; new adapter tests green; every board env builds; **record flash and SRAM before/after on `ramps`** (the AVR budget is the binding constraint for the `ILogger` change); the manual smoke checklist from "Verification strategy" passes (slew, park, guide, home).

**Doc sync:** ARCHITECTURE.md §3/§4 must gain `hal/rp2040/` and `IGyro`, and must show the hall/end-switch/display adapters as Phase 3 rather than Phase 2.

### Phase 3 — Decompose `Mount` into controllers
*Strangler-fig: move responsibilities out of `Mount` into `core/` controllers, one at a time. Mount becomes a facade.*

**Prerequisite:** Phase 2's ISR contract for `IStepperAxis` must be settled. The threading model for `SlewController::update()` (main loop vs ISR context) follows from that contract.

Recommended slice order (each is an independent step, parallelizable after Phase 2):
1. `core/TrackingController` — tracking speed, sidereal rate, tracker-stop compensation. Owns `IStepperAxis* trk`.
2. `core/SlewController` — slew state machine extracted from the 280-line `Mount::loop`. Inputs are target + geometry; outputs are stepper commands and state events. **Threading model deferred to Phase 2's ISR contract.**
3. `core/GuidingController` — guide-pulse direction/speed math + timed completion (uses `IClock`).
4. `core/HomingController` — hall-sensor/end-switch state machine; owns `IHomingSensor* ra`, `IHomingSensor* dec`.
5. `core/ParkingController` — park position, parking transitions.
6. `core/FocusController` — focus motor (only constructed when focus axis is present; uses `etl::optional` in composition root).
7. `core/AzAltController` — AZ/ALT motors (only constructed when present; uses `etl::optional` in composition root).
8. `core/MountState` — single source of truth for the `_mountStatus` bitfield, with typed enum API (`Status::isSlewing()` etc.). Controllers mutate `MountState`; Mount facade reads it.
9. `core/EventBus` — controllers publish `PositionChanged`, `SlewStarted`, `Parked`, etc.; display adapter subscribes (removes Mount → display direct coupling).

Each step: extract → add focused unit tests with FakeIt-faked ports → remove the original code from `Mount.cpp` → ship.

**Verify per step:** unit tests for the new controller; all prior tests still green; firmware behavior on hardware unchanged (manual smoke checklist).

### Phase 4 — Compile-time flags → runtime polymorphism
*Eliminate `#ifdef` axes in `core/`, `ports/`, and most of `adapters/`. Feature flags survive only in the composition root and in HAL backend selection.*

1. Replace `AZ_STEPPER_TYPE`, `ALT_STEPPER_TYPE`, `FOCUS_STEPPER_TYPE` checks: composition root either constructs the controller and injects it, or injects a **null-object** controller. `core/` code calls unconditionally.
2. Replace `*_DRIVER_TYPE` checks with selection of the right `IMotorDriver` implementation at composition time (`Tmc2209Driver` vs `NullMotorDriver`).
3. `USE_HALL_SENSOR_*_AUTOHOME`, `USE_*_END_SWITCH` → presence of a non-null port implementation.
4. Truly board-specific code (interrupt registers, board pins) lives **only inside the relevant `hal/<platform>/` backend**; `core/`, `ports/`, and `adapters/` become `#ifdef`-free.
5. Add a `MountConfig` builder in `app/` that reads the `Configuration*.hpp` macros and produces a runtime config object.

**Verify:** `core/` and `ports/` contain zero `#ifdef` for features (CI grep check); all board-env builds in the CI matrix still pass (toolchain enforces flash limits).

### Phase 5 — Meade execution layer cleanup
*The parser is already in `core/`. This phase finishes the Meade slice by refining the executor and transport layers.*

**Prerequisite:** Phase 3 must be substantially complete. The `MeadeCommandProcessor` currently holds a `Mount*` raw pointer and calls Mount methods directly. It can only be re-wired to port-based interfaces once Phase 3's controllers expose those ports.

1. `MeadeCommandProcessor` (current adapter) is already in `src/` root implementing `IMeadeHandlers` → move it to `src/adapters/MeadeCommandAdapter` to match layer conventions.
2. Introduce `adapters/SerialTransport` + `adapters/WifiTransport` to feed bytes to `core/meade/MeadeParser`; parsed commands dispatch to the adapter.
3. Remove `Mount* _mount` raw pointer from `MeadeCommandProcessor` — replace with port-based interfaces from Phase 2/3 (e.g. `IMeadeHandlers` implemented over controller interfaces, not the god-object).
4. Remove the legacy `Mount::delay()` blocking call from GPS acquisition handler — replace with `IClock`-based non-blocking state machine.
5. The existing 14 test files in `unit_tests/test_core/meade/` already cover all parser families. Add integration tests wiring `SerialTransport` → `MeadeParser` → `MeadeCommandAdapter` with faked ports.

**Verify:** Meade test suite green (existing 14 files + new integration tests); Stellarium/ASCOM round-trip smoke test (manual) recorded as a regression checklist; coverage of `core/meade/` ≥ 80% (already achieved — 87.0%).

### Phase 6 — Cleanup & documentation
1. Mount facade slimmed to a thin compat shim (or removed if no external dependents).
2. Move display-related code paths off the Mount → display direct call into the `EventBus`.
3. Architecture doc (`docs/architecture.md`) with the layer diagram, port catalog, and "where to add a new feature" guide.
4. Ratchet CI coverage gate to its final value (`core/` ≥ 85%).

**Verify:** Architecture doc reviewed; CI gates final; full board matrix green; smoke-tested on at least one real mount.

---

## Relevant files

### Already migrated to `core/`
- [`src/core/meade/`](src/core/meade/) — 17 files: parser, 12 family dispatchers, helpers, protocol spec, typed handler interfaces
- [`unit_tests/test_core/meade/`](unit_tests/test_core/meade/) — 14 test files covering all parser families with fake handler stubs

### Phase 1 targets — ✅ modules, tests and wiring done
- [`src/DayTime.cpp`](src/DayTime.cpp), [`src/DayTime.hpp`](src/DayTime.hpp) — pure arithmetic in `core/types/DayTime` (Step 1, wired)
- [`src/Declination.cpp`](src/Declination.cpp), [`src/Latitude.cpp`](src/Latitude.cpp), [`src/Longitude.cpp`](src/Longitude.cpp), [`src/RightAscension.cpp`](src/RightAscension.cpp) — pure subsets in `core/types/` (Step 2, wired)
- [`src/Sidereal.cpp`](src/Sidereal.cpp) — pure math in `core/SiderealClock` (Step 3, wired)
- [`src/Mount.cpp`](src/Mount.cpp) — `calculateRAandDECSteppers` → `core/CoordinateMath` (Step 4, wired), `getLocalDate` → `core/CalendarMath::addDays` (Step 5, wired), `RAString`/`DECString` → `core/CoordinateFormatter` (Step 6, wired)
- [`src/EPROMStore.cpp`](src/EPROMStore.cpp) / [`src/EPROMStore.hpp`](src/EPROMStore.hpp) — layout constants sourced from `hal/EepromLayout.hpp` (Step 7, wired)

### New files created in Phase 1
- `src/core/types/DayTime.hpp`, `src/core/types/DayTime.cpp`
- `src/core/types/Declination.hpp`, `src/core/types/Declination.cpp`
- `src/core/types/Latitude.hpp`, `src/core/types/Latitude.cpp`
- `src/core/types/Longitude.hpp`, `src/core/types/Longitude.cpp`
- `src/core/types/RightAscension.hpp`, `src/core/types/RightAscension.cpp`
- `src/core/SiderealClock.hpp`, `src/core/SiderealClock.cpp`
- `src/core/CoordinateMath.hpp`, `src/core/CoordinateMath.cpp`
- `src/core/CalendarMath.hpp`, `src/core/CalendarMath.cpp`
- `src/core/CoordinateFormatter.hpp`, `src/core/CoordinateFormatter.cpp`
- `src/hal/EepromLayout.hpp` (definition of the on-device EEPROM layout, used by `EPROMStore`)
- `unit_tests/test_core/types/test_daytime.cpp`, `test_declination.cpp`, `test_latitude.cpp`, `test_longitude.cpp`, `test_right_ascension.cpp`
- `unit_tests/test_core/test_sidereal.cpp`, `test_coordinate_math.cpp`, `test_calendar.cpp`, `test_coordinate_formatter.cpp`
- `unit_tests/test_core/test_main.cpp` (googletest main)
- `unit_tests/test_hal/test_main.cpp` (googletest main) and `unit_tests/test_hal/test_eeprom_layout.cpp` (9 EEPROM layout tests)

### Phase 2 targets
- [`src/Gyro.cpp`](src/Gyro.cpp) — behind a new `IGyro` port (MPU6050 access goes through `hal::II2cBus`).
- [`src/Core.cpp`](src/Core.cpp), [`src/InterruptCallback.cpp`](src/InterruptCallback.cpp), [`src/StepperConfiguration.hpp`](src/StepperConfiguration.hpp), [`src/InterruptAccelStepper.h`](src/InterruptAccelStepper.h) — board-specific timer/ISR code moves into `src/hal/avr/` and `src/hal/rp2040/`.
- [`src/a_inits.hpp`](src/a_inits.hpp), [`src/b_setup.hpp`](src/b_setup.hpp), [`src/f_serial.hpp`](src/f_serial.hpp) — wiring code gradually migrates into `src/app/`.
- Deferred to Phase 3 (they still call `Mount` directly): [`src/HallSensorHoming.cpp`](src/HallSensorHoming.cpp), [`src/EndSwitches.cpp`](src/EndSwitches.cpp), [`src/SSD1306_128x64_Display.cpp`](src/SSD1306_128x64_Display.cpp), [`src/LcdMenu.cpp`](src/LcdMenu.cpp), [`src/LcdButtons.cpp`](src/LcdButtons.cpp). Deferred to Phase 5: [`src/WifiControl.cpp`](src/WifiControl.cpp).

### Phase 5 targets
- [`src/MeadeCommandProcessor.cpp`](src/MeadeCommandProcessor.cpp), [`src/MeadeCommandProcessor.hpp`](src/MeadeCommandProcessor.hpp) — adapter already bridges parser to `Mount`; move to `src/adapters/` and wire through ports.
- [`src/f_serial.hpp`](src/f_serial.hpp) — serial framing code that calls `MeadeCommandProcessor::instance()->processCommand()`; becomes `SerialTransport` adapter.

### Infrastructure
- [`platformio.ini`](platformio.ini) — `native` env with coverage flags (`build_src_flags` + `LINKFLAGS`), coverage extra script, `test_filter = test_core` + `test_hal` (catches `test_core/meade/` too; `test_hal` holds the EEPROM layout tests). **ArduinoFake/FakeIt `test_lib_deps` is not configured yet.**
- [`.github/workflows/ci.yml`](.github/workflows/ci.yml) — runs `./scripts/test-coverage.sh` + publishes `coverage.md` to the step summary; builds 7 boards (ramps, mksgenlv1/v2/v21, esp32, rp2040_jackw01, rp2040_skr_pico).
- [`unit_tests/test_common/`](unit_tests/test_common/) — expand with FakeIt-based port fakes for Phase 2+.
- [`Configuration.hpp`](Configuration.hpp), [`Configuration_adv.hpp`](Configuration_adv.hpp) — read once by `MountConfig` builder in Phase 4.

---

## Verification strategy

Automated:
1. `./scripts/test-coverage.sh` — full host test suite, runs every PR (366 test cases). `test_filter` = `test_core` + `test_hal` (covers `test_core/meade/` and `test_core/types/`).
2. Every board env in `platformio.ini` (CI matrix: 7 boards) builds green every phase via `matrix_build.py`, which supplies the generated configuration. A bare `pio run -e <board>` additionally needs the developer's untracked `Configuration_local_*.hpp`.
3. Coverage gate via gcovr in CI — ratchets up phase by phase; `core/` final target ≥ 85%.
4. CI grep check: `core/` contains zero `#include <Arduino.h>` or `#ifdef <FEATURE_FLAG>` (after Phase 4).

Manual smoke checklist (per shippable phase end):
1. Mount slews to known coordinates within tolerance on real hardware (one volunteer-owned board).
2. Park / unpark cycle.
3. Guide pulses in all four directions produce expected micro-moves.
4. Stellarium/ASCOM LX200 round-trip (date, time, RA/DEC, sync) succeeds.
5. Hall-sensor auto-home succeeds (on a board so equipped).

---

## Decisions

- **Test stack:** **Googletest** + **FakeIt** (via ArduinoFake) for host-side unit tests. The `native` PIO env uses `test_framework = googletest`. FakeIt is bundled with ArduinoFake (`ArduinoFake@^0.4.0`) and provides the mocking/stubbing API (`When(Method(...)).Return(...)`, `Verify(Method(...))`). No Unity. No GoogleMock. **Current state:** only Googletest is in place — ArduinoFake/FakeIt has not been added to `platformio.ini` yet (see Phase 0, item 5).
- **Migration style:** **Hybrid** — extract pure logic first (Phase 1), then strangler-fig hardware-coupled layers (Phase 2–5).
- **Config flags:** Migrate to **runtime polymorphism behind interfaces**; composition root reads the `Configuration*.hpp` macros once. `core/` becomes `#ifdef`-free for features.
- **Optional axes:** Use `etl::optional` (from Embedded Template Library) — not `std::optional`. The firmware builds with `-D ETL_NO_STL` on AVR targets. `core/` uses `etl::optional` for consistency across all build targets. **Open:** ETL is not currently a `lib_deps` entry anywhere (`src/libs/` holds only `MappedDict` and `TimerInterrupt`) and nothing uses `etl::` yet — ETL must be added to both the board and `native` envs before Phase 4 relies on it.
- **Mount global migration:** `Mount` is currently a global variable (defined as `Mount mount(&lcdMenu);` in `src/b_setup.hpp`; the only `extern Mount mount;` is in `src/testmenu.cpp`). Call sites that use `mount.xxx()` will be migrated using the thin-overlay pattern: existing code delegates through the global during transition; when a module becomes a proper adapter (Phase 2+), it receives injected references instead. No flag day needed.
- **ISR threading model:** Deferred to Phase 2. The `IStepperAxis` interface contract (which methods are ISR-safe, `Snapshot()` semantics) will be settled before Phase 3 begins extracting controllers. Without this contract, `SlewController`'s threading model is undefined.
- **Back-compat:** Meade serial protocol behavior is invariant (external interface); internal C++ APIs may change freely.
- **Out of scope:** UI menu screens (`c*_menu*.hpp`) refactor; new features; supporting new boards; replacing AccelStepper/TMCStepper libraries; switching build system.
- **Extract-before-test:** All `src/` root files include `inc/Globals.hpp` → `<Arduino.h>` and are untestable in the `native` PIO env (which only builds `core/`, `ports/`, `adapters/`). Pure logic is extracted into `core/` first, then tested — inverted from the typical test-first order. Only already-pure code (`core/meade/`, `MappedDict`) gets tests before extraction.


## Further Considerations

1. **C++ standard.** `core/` benefits from at least C++17 (`etl::optional`, `if constexpr`). PlatformIO defaults vary by board (some AVR ports stuck on C++11/14). *Decision:* native env already uses `-std=gnu++17`. Each board env will be checked for C++17 support and upgraded if needed. Fallback: `-std=gnu++14` + ETL polyfills on boards that can't support C++17.
2. **Binary size on AVR_MEGA2560.** Polymorphism + extra indirection costs flash on AVR. The toolchain errors if flash is exceeded, so no explicit budget is needed. *Mitigation:* keep vtables small (≤ ~12 ports), mark adapters `final`, allow link-time devirtualization. If flash still overflows, accept template-based static dispatch for the hot path (`SlewController<RaAxis, DecAxis>`) — adds complexity but keeps AVR shipping.
3. **Characterization / golden-master tests.** A future task: capture known-good (RA,DEC → stepper position) pairs from real hardware for the `calculateRAandDECSteppers` math. This would serve as regression tests for the meridian-flip solution selector — the highest-risk coordinate math in the system. Not part of Phase 1; left as a follow-up task after extraction is complete and the pure math is independently testable on native.
4. **MeadeCommandProcessor singleton.** `MeadeCommandProcessor::instance()` is a true singleton (unlike `Mount`, which is a global). It's accessed from `f_serial.hpp`, `WifiControl.cpp`, `c_buttons.hpp`, and `testmenu.cpp`. Phase 5 moves it to `src/adapters/MeadeCommandAdapter`; the singleton pattern should be eliminated there in favor of a non-owning reference from the composition root.
