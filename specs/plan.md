# Plan: Clean-Architecture Refactor of OpenAstroTracker-Firmware

## TL;DR
Refactor a ~5k-line `Mount` god-object firmware toward clean architecture for embedded:
a pure **Domain Core** (no Arduino, fully unit-testable) sitting behind **Port interfaces**,
with **Adapters** wrapping hardware (AccelStepper, TMC2209, EEPROM, displays, Wi-Fi, clock).
Approach: **hybrid** — extract pure logic in place first to build a regression-prevention test net (Unity + FFF on the existing `native` PIO env), then **strangler-fig** the hardware-coupled pieces (drivers, slewing loop, command executor) behind ports. Compile-time `#ifdef` axes/drivers migrate to **runtime polymorphism** so unsupported combinations no longer change the call graph. Goal endpoint: `src/core/` is buildable & 100% unit-tested on host; `src/adapters/` contains all Arduino/library coupling; `src/app/` wires them up per board.

---

## Goal Architecture (target)

Five layers, dependencies point inward only. The **HAL** sits between domain ports and the actual hardware libraries, so swapping AccelStepper/TMCStepper/EEPROM/SSD1306/Wi-Fi never touches `core/` or `ports/`.

```
+----------------------------------------------------------------+
|  app/        per-board composition root (main.cpp + setup     |
|   |          wiring, replaces the legacy .ino entry point)    |
|   v                                                            |
|  adapters/   bind domain ports to HAL (and to non-HAL libs    |
|   |          such as TinyGPS data structs); thin glue.        |
|   v                                                            |
|  hal/        Hardware Abstraction Layer — pure C++ interfaces |
|   |          for physical components + per-platform backends: |
|   |            IGpioPin, ISerialPort, ISpiBus, II2cBus,       |
|   |            IEeprom, IStepperMotor, ITmcDriver, IOledPanel,|
|   |            ICharLcd, IButtonMatrix, ITimerService,        |
|   |            ISystemClock, IWifiStack.                      |
|   |          Backends: hal/arduino/, hal/esp32/, hal/avr/.    |
|   |          Host-side fakes for unit tests live under        |
|   |          unit_tests/test_common/ (test code, not src/).   |
|   v                                                            |
|  ports/      domain-level interfaces consumed by core:        |
|   |          IClock, ILogger, IPersistentStore, IStepperAxis, |
|   |          IMotorDriver, IDisplay, IInfoDisplay, ITransport,|
|   |          IHomingSensor, IEndSwitch, IGps.                 |
|   v                                                            |
|  core/       pure domain logic, no Arduino, host-testable:    |
|              CoordinateMath, MountState, SlewController,      |
|              GuidingController, TrackingController,           |
|              ParkingController, HomingController,             |
|              FocusController, SiderealClock, MeadeParser,     |
|              MountConfig, EventBus.                            |
+----------------------------------------------------------------+
```

**HAL vs Ports — why both:**
- `hal/` describes *what the hardware can do* (pin toggles, UART bytes, timer ticks). One backend per platform; one in-memory backend for tests.
- `ports/` describes *what the domain needs* (axis position, persistent value, "now"). Adapters compose one or more HAL services to satisfy a port — e.g. `AccelStepperAxis` adapter implements `IStepperAxis` using `IStepperMotor` + `ITimerService` from HAL.

Cross-cutting:
- **Configuration** becomes a runtime `MountConfig` struct populated at composition time from `Configuration.hpp` constants (single translation unit reads the macros). `#ifdef` no longer leaks into `core/` or `ports/`; HAL backend selection is the only place feature flags survive.
- **Time** is a `IClock` port backed by `hal::ISystemClock`; `core/` never calls `millis()` directly.
- **Logging** is an `ILogger` port backed by `hal::ISerialPort`; `core/` never includes `Serial`.
- Optional axes (`AZ`, `ALT`, `Focus`) become `std::optional<AxisController>` or null-object pattern — no `#ifdef` branches in controllers.

`Mount.cpp` ends up as a thin **facade** (≤ 500 LOC) over `core/` controllers, retained for Meade protocol back-compat; gradually deprecated.

---

## Current State (after Meade parser migration)

### ✅ Completed: Meade parser migrated to `core/`

The Meade LX200 command parser has been fully extracted into `src/core/meade/` as a pure, host-testable, allocation-light parser with no side effects. The split is:

| Layer | Location | Status |
|-------|----------|--------|
| **Parser** (pure) | `src/core/meade/MeadeParser.*` + 11 family-specific `.cpp` files | ✅ Done — 100% covered by 13 test files in `unit_tests/test_meade/` |
| **Protocol spec** | `src/core/meade/MeadeProtocol.hpp` | ✅ Done — comprehensive protocol documentation |
| **Handler interfaces** | `src/core/meade/MeadeParser.hpp` (12 `IMeade*Handlers` interfaces + aggregate `IMeadeHandlers`) | ✅ Done — clean typed contracts |
| **Executor/Adapter** | `src/MeadeCommandProcessor.hpp/cpp` | ✅ Done — implements `IMeadeHandlers`, delegates to `Mount`/`LcdMenu` |

The parser directory (`src/core/meade/`) contains 16 files covering all 12 command families (Get, Set, Quit, Distance, Init, SyncControl, Home, SlewRate, GPS, Focus, Movement, Extra). Each family has:
- A typed handler interface (e.g. `IMeadeGetHandlers`, `IMeadeMovementHandlers`)
- A `handleMeade*` entry point that parses suffixes, invokes handler callbacks, and serializes `MeadeResponse`
- Value types for coordinates (e.g. `RaCoordinate`, `DecCoordinate`)

The `MeadeCommandProcessor` adapter bridges the parser to the legacy `Mount` singleton, implementing all ~90 handler overrides. It is the **only** file in `src/` root that references the core parser.

### Test coverage

13 test files in `unit_tests/test_meade/` provide comprehensive wire-byte coverage for every parser family using fake handler stubs. Tests verify:
- Exact wire-byte formatting (zero-padding, sign rules, terminators)
- Suffix classification and handler dispatch routing
- Edge cases (malformed input, overflow, unknown sub-commands)
- Parser-level validation (the `parseMeadeCommand` classifier)

### What remains

The parser is pure and tested. The executor (`MeadeCommandProcessor`) is an adapter that still couples to `Mount*` and `LcdMenu*` directly. The `Mount` god-object still contains all domain logic (slewing, tracking, guiding, parking, homing, focus, coordinate math). No HAL, ports, or controllers exist yet beyond the meade parser.

---

## Phased Plan (each phase shippable & green in CI)

### Phase 0 — Safety net & tooling (no behavior change)
*Foundation for everything else; must land first.*

1. Add **Fake Function Framework (FFF)** as a header-only dep in `unit_tests/test_common/fakes/`.
2. Add a new PIO env `native_core` (extends `native`) with stricter warnings (`-Wall -Wextra -Werror`) for host-only `core/` builds; keep existing `native` for compatibility.
3. Add gcovr/lcov-based **coverage reporting** to the `native` env; publish summary in CI.
4. Extend `.github/workflows/platformio_unit_tests.yml`:
   - Run `pio test -e native -v`.
   - Run coverage and fail if `core/` coverage drops below configured threshold (start at 0, ratchet upward).
5. Add **ArduinoFake** as a `lib_deps` dependency (PlatformIO package `ArduinoFake@^0.4.0`) for Arduino API mocking (`millis`, `String`, `pinMode`, `digitalWrite`, fake `EEPROM`, fake `Serial`, fake `Wire`, fake `SPI`). Provides stubbing/verification via FakeIt-based API. Complements FFF (function-level fakes in `unit_tests/test_common/fakes/`) — ArduinoFake handles the Arduino API layer, FFF handles custom port/adapter function mocking. Replaces the Phase 0 original plan of a custom shim; ArduinoFake was chosen for its richer mocking capabilities (stub, verify, reset).
6. Establish folders: `src/ports/`, `src/hal/`, `src/adapters/`, `src/app/` (READMEs already exist); leave existing files in place. Host-side HAL fakes will land under `unit_tests/test_common/hal_fakes/` when Phase 3 begins.

**Verify:** `pio test -e native -v` green; coverage report artifact produced in CI; build for all existing boards still green via `matrix_build.py`.

### Phase 1 — Characterize existing pure logic (regression net)
*All pure-logic files identified by the audit get exhaustive tests before being moved.*

Steps (parallel after Phase 0):
1. Add Unity tests for `DayTime`, `Declination`, `Latitude`, `Longitude` (arithmetic, parse/format round-trips, sign edges, overflow).
2. Expand `test_sidereal.h` into full `Sidereal` coverage (LST/HA from date/time across edge dates, leap years, DST-irrelevant UTC).
3. Add tests for `MappedDict` boundary behaviors not yet covered.
4. Add **golden-master tests** for the largest pure-ish methods currently in `Mount.cpp` by exercising them as-is through a thin test driver:
   - `Mount::calculateRAandDECSteppers` (parametrize over hemisphere, meridian-flip, latitude, target).
   - `Mount::syncPosition` math.
   - `Mount::getLocalDate` calendar increment (leap years, year/month wrap).
   - `Mount::DECString` / `Mount::RAString` formatting.
   These tests link against a stripped-down `Mount` compiled with ArduinoFake — they fail the moment behavior shifts during extraction.

**Verify:** All new tests green; coverage report shows non-trivial line coverage on the listed methods; CI threshold ratcheted up.

### Phase 2 — Extract pure domain (in place → `src/core/`)
*Move characterized logic into `core/` with no behavior change. Tests from Phase 1 prevent regressions.*

1. Create `core/CoordinateMath` from `Mount::calculateRAandDECSteppers` + helpers; replace original with a thin delegate. Inputs/outputs as plain structs (`MountGeometry`, `EquatorialTarget`, `StepperTarget`).
2. Create `core/SiderealClock` wrapping `Sidereal::` statics behind an instance API that takes an `IClock`.
3. Create `core/CalendarMath` from `Mount::getLocalDate`.
4. Create `core/CoordinateFormatter` from RA/DEC string formatters.
5. Create `core/MountGeometry` value type holding steps-per-degree, calibration angles, hemisphere, backlash — replaces scattered Mount fields used by math.
6. ~~Create `core/MeadeParser` by splitting `MeadeCommandProcessor`: pure tokenize/dispatch lookup tables in `core/`; execution stays in adapter for now.~~ **✅ DONE** — Meade parser is already in `core/meade/` with 12 family-specific handler interfaces, 13 comprehensive test files, and `MeadeCommandProcessor` as the adapter implementing `IMeadeHandlers`.

**Verify:** Phase 1 tests still green unchanged; firmware binary for each board builds identically (size diff ≈ 0); new `core/` files all covered by host tests.

### Phase 3 — Introduce HAL, Ports & Adapters
*Define the HAL surface, define the domain ports, and route current call sites through them. Keep current behavior bit-for-bit.*

1. Define **HAL interfaces** in `src/hal/`:
   - `IGpioPin`, `ISerialPort`, `ISpiBus`, `II2cBus`, `IEeprom`,
   - `IStepperMotor` (step/dir pulses, microstep config), `ITmcDriver` (UART register IO),
   - `IOledPanel`, `ICharLcd`, `IButtonMatrix`,
   - `ITimerService` (periodic/one-shot callbacks, used by interrupt stepper), `ISystemClock` (`millis`/`micros`),
   - `IWifiStack`.
2. Implement HAL backends:
   - `hal/arduino/` — generic Arduino implementation (`ArduinoGpioPin`, `ArduinoSerialPort`, `ArduinoEeprom`, `ArduinoSystemClock`, …).
   - `hal/avr/` — AVR-specific bits (Timer1/Timer3 interrupt service, fast pin IO).
   - `hal/esp32/` — ESP32-specific (hardware timers, Wi-Fi stack glue).
   - `unit_tests/test_common/hal_fakes/` — pure C++ test fakes (in-memory EEPROM, virtual GPIO, controllable clock, fake serial). Lives in test code, **not** in `src/`. Complements ArduinoFake (ArduinoFake handles the Arduino API layer; hal_fakes handles custom HAL interfaces).
3. Define domain **ports** in `src/ports/`:
   - `IClock`, `ILogger`, `IPersistentStore`,
   - `IStepperAxis` (position, target, speed, accel, run, stop, isRunning, `Snapshot()` for ISR safety),
   - `IMotorDriver` (enable/disable, microsteps, current; null implementation for non-TMC),
   - `IHomingSensor`, `IEndSwitch`, `IDisplay`, `IInfoDisplay`, `ITransport`, `IGps`.
4. Provide **adapters** in `src/adapters/` that bind ports to HAL:
   - `ArduinoClock` (port `IClock` ← hal `ISystemClock`), `SerialLogger`, `EepromPersistentStore`,
   - `AccelStepperAxis`, `InterruptAccelStepperAxis` (use `hal::ITimerService` + `hal::IStepperMotor`),
   - `Tmc2209Driver` (UART + standalone variants over `hal::ISerialPort`), `NullMotorDriver`,
   - `HallHomingSensor`, `GpioEndSwitch` (over `hal::IGpioPin`),
   - `LcdMenuDisplay`, `Ssd1306InfoDisplay` (over `hal::IOledPanel` / `hal::ICharLcd`),
   - `SerialTransport`, `WifiTransport` (over `hal::ISerialPort` / `hal::IWifiStack`),
   - `TinyGpsAdapter`.
5. Refactor `Mount` to **hold port pointers** (`IStepperAxis* _ra; IClock* _clock; ...`) injected at construction instead of owning concrete types. Composition happens in `app/` (currently `b_setup.hpp`).
6. Replace direct `millis()`, `digitalWrite()`, `EEPROMStore::` calls inside `Mount` with port calls; replace `LOG()` macro with `_logger->log(...)`.

**Verify:** All Phase 1/2 tests still green; new contract tests for each port using HAL fakes from `unit_tests/test_common/hal_fakes/` (e.g., `EepromPersistentStore` round-trips via an in-memory `FakeEeprom`); golden-master tests on `Mount` still pass; firmware builds identical-sized binaries on at least one board (other variants ±1%).

### Phase 4 — Decompose `Mount` into controllers
*Strangler-fig: move responsibilities out of `Mount` into `core/` controllers, one at a time. Mount becomes a facade.*

Recommended slice order (each is an independent step, parallelizable after Phase 3):
1. `core/TrackingController` — tracking speed, sidereal rate, tracker-stop compensation. Owns `IStepperAxis* trk`.
2. `core/SlewController` — slew state machine extracted from the 280-line `Mount::loop`. Inputs are target + geometry; outputs are stepper commands and state events.
3. `core/GuidingController` — guide-pulse direction/speed math + timed completion (uses `IClock`).
4. `core/HomingController` — hall-sensor/end-switch state machine; owns `IHomingSensor* ra`, `IHomingSensor* dec`.
5. `core/ParkingController` — park position, parking transitions.
6. `core/FocusController` — focus motor (only constructed when focus axis is present).
7. `core/AzAltController` — AZ/ALT motors (only constructed when present).
8. `core/MountState` — single source of truth for the `_mountStatus` bitfield, with typed enum API (`Status::isSlewing()` etc.). Controllers mutate `MountState`; Mount facade reads it.
9. `core/EventBus` — controllers publish `PositionChanged`, `SlewStarted`, `Parked`, etc.; display adapter subscribes (removes Mount → display direct coupling).

Each step: extract → add focused unit tests with FFF-faked ports → remove the original code from `Mount.cpp` → ship.

**Verify per step:** unit tests for the new controller; golden-master tests on `Mount` still green; firmware behavior on hardware unchanged (manual smoke checklist).

### Phase 5 — Compile-time flags → runtime polymorphism
*Eliminate `#ifdef` axes in `core/`, `ports/`, and most of `adapters/`. Feature flags survive only in the composition root and in HAL backend selection.*

1. Replace `AZ_STEPPER_TYPE`, `ALT_STEPPER_TYPE`, `FOCUS_STEPPER_TYPE` checks: composition root either constructs the controller and injects it, or injects a **null-object** controller. `core/` code calls unconditionally.
2. Replace `*_DRIVER_TYPE` checks with selection of the right `IMotorDriver` implementation at composition time (`Tmc2209Driver` vs `NullMotorDriver`).
3. `USE_HALL_SENSOR_*_AUTOHOME`, `USE_*_END_SWITCH` → presence of a non-null port implementation.
4. Truly board-specific code (interrupt registers, board pins) lives **only inside the relevant `hal/<platform>/` backend**; `core/`, `ports/`, and `adapters/` become `#ifdef`-free.
5. Add a `MountConfig` builder in `app/` that reads the `Configuration*.hpp` macros and produces a runtime config object.

**Verify:** `core/` and `ports/` contain zero `#ifdef` for features (CI grep check); all 5 existing board matrix builds still pass; binary size delta within budget (set explicit per-board limit, e.g., +3% allowed).

### Phase 6 — Meade execution layer cleanup
*The parser is already in `core/`. This phase finishes the Meade slice by refining the executor and transport layers.*

1. `MeadeCommandProcessor` (current adapter) is already in `src/` root implementing `IMeadeHandlers` → move it to `src/adapters/MeadeCommandAdapter` to match layer conventions.
2. Introduce `adapters/SerialTransport` + `adapters/WifiTransport` to feed bytes to `core/meade/MeadeParser`; parsed commands dispatch to the adapter.
3. Remove `Mount* _mount` raw pointer from `MeadeCommandProcessor` — replace with port-based interfaces from Phase 3/4 (e.g. `IMeadeHandlers` implemented over controller interfaces, not the god-object).
4. Remove the legacy `Mount::delay()` blocking call from GPS acquisition handler — replace with `IClock`-based non-blocking state machine.
5. The existing 13 test files in `unit_tests/test_meade/` already cover all parser families. Add integration tests wiring `SerialTransport` → `MeadeParser` → `MeadeCommandAdapter` with faked ports.

**Verify:** Meade test suite green (existing 13 files + new integration tests); Stellarium/ASCOM round-trip smoke test (manual) recorded as a regression checklist; coverage of `core/meade/` ≥ 80% (already achieved).

### Phase 7 — Cleanup & documentation
1. Mount facade slimmed to a thin compat shim (or removed if no external dependents).
2. Move display-related code paths off the Mount → display direct call into the `EventBus`.
3. Architecture doc (`docs/architecture.md`) with the layer diagram, port catalog, and "where to add a new feature" guide.
4. Ratchet CI coverage gate to its final value (`core/` ≥ 85%, `adapters/` ≥ 40%).

**Verify:** Architecture doc reviewed; CI gates final; full board matrix green; smoke-tested on at least one real mount.

---

## Relevant files

### Already migrated to `core/`
- [`src/core/meade/`](src/core/meade/) — 16 files: parser, 12 family dispatchers, helpers, protocol spec, typed handler interfaces
- [`unit_tests/test_meade/`](unit_tests/test_meade/) — 13 test files covering all parser families with fake handler stubs

### Phase 0–1 targets
- [`src/Mount.cpp`](src/Mount.cpp), [`src/Mount.hpp`](src/Mount.hpp) — the god-object being decomposed; `loop()`, `calculateRAandDECSteppers()`, `guidePulse()`, `startSlewing()`, `readPersistentData()` are the biggest extraction targets.
- [`src/Sidereal.cpp`](src/Sidereal.cpp), [`src/DayTime.cpp`](src/DayTime.cpp), [`src/Declination.cpp`](src/Declination.cpp), [`src/Latitude.cpp`](src/Latitude.cpp), [`src/Longitude.cpp`](src/Longitude.cpp) — already pure; move into `core/` in Phase 2.
- [`src/EPROMStore.cpp`](src/EPROMStore.cpp), [`src/EPROMStore.hpp`](src/EPROMStore.hpp) — already a good seam; becomes `IPersistentStore` + adapter.

### Phase 2–3 targets
- [`src/HallSensorHoming.cpp`](src/HallSensorHoming.cpp), [`src/EndSwitches.cpp`](src/EndSwitches.cpp), [`src/Gyro.cpp`](src/Gyro.cpp), [`src/LcdMenu.cpp`](src/LcdMenu.cpp), [`src/SSD1306_128x64_Display.cpp`](src/SSD1306_128x64_Display.cpp), [`src/WifiControl.cpp`](src/WifiControl.cpp), [`src/LcdButtons.cpp`](src/LcdButtons.cpp) — become adapters behind ports.
- [`src/Core.cpp`](src/Core.cpp), [`src/a_inits.hpp`](src/a_inits.hpp), [`src/b_setup.hpp`](src/b_setup.hpp), [`src/f_serial.hpp`](src/f_serial.hpp) — wiring code gradually migrates into `src/app/`.

### Phase 6 targets
- [`src/MeadeCommandProcessor.cpp`](src/MeadeCommandProcessor.cpp), [`src/MeadeCommandProcessor.hpp`](src/MeadeCommandProcessor.hpp) — adapter already bridges parser to `Mount`; move to `src/adapters/` and wire through ports.
- [`src/f_serial.hpp`](src/f_serial.hpp) — serial framing code that calls `MeadeCommandProcessor::instance()->processCommand()`; becomes `SerialTransport` adapter.

### Infrastructure
- [`platformio.ini`](platformio.ini) — add `native_core` env, coverage flags, FFF include path.
- [`.github/workflows/platformio_unit_tests.yml`](.github/workflows/platformio_unit_tests.yml) — coverage gating, ratchet.
- [`unit_tests/test_common/`](unit_tests/test_common/) — expand with FFF-based ports tests.
- [`Configuration.hpp`](Configuration.hpp), [`Configuration_adv.hpp`](Configuration_adv.hpp) — read once by `MountConfig` builder in Phase 5.

---

## Verification strategy

Automated:
1. `pio test -e native -v` — full host test suite, runs every PR.
2. `pio run -e <board>` for the existing 5-board matrix — must remain green every phase.
3. Coverage gate via gcovr in CI — ratchets up phase by phase; `core/` final target ≥ 85%.
4. CI grep check: `core/` contains zero `#include <Arduino.h>` or `#ifdef <FEATURE_FLAG>` (after Phase 5).
5. Binary-size budget check per board (warn at +1%, fail at +3%).

Manual smoke checklist (per shippable phase end):
1. Mount slews to known coordinates within tolerance on real hardware (one volunteer-owned board).
2. Park / unpark cycle.
3. Guide pulses in all four directions produce expected micro-moves.
4. Stellarium/ASCOM LX200 round-trip (date, time, RA/DEC, sync) succeeds.
5. Hall-sensor auto-home succeeds (on a board so equipped).

---

## Decisions

- **Test stack:** Unity (already in PIO) + **FFF (Fake Function Framework)** for mocking Arduino/library calls. No GoogleTest.
- **Migration style:** **Hybrid** — extract pure logic in place first (Phase 1–2), then strangler-fig hardware-coupled layers (Phase 3–6).
- **Config flags:** Migrate to **runtime polymorphism behind interfaces**; composition root reads the `Configuration*.hpp` macros once. `core/` becomes `#ifdef`-free for features.
- **Back-compat:** Meade serial protocol behavior is invariant (external interface); internal C++ APIs may change freely.
- **Out of scope:** UI menu screens (`c*_menu*.hpp`) refactor; new features; supporting new boards; replacing AccelStepper/TMCStepper libraries; switching build system.

## Further Considerations

1. **C++ standard.** `core/` benefits from at least C++17 (`std::optional`, `std::variant`, `if constexpr`). PlatformIO defaults vary by board (some AVR ports stuck on C++11/14). *Recommendation:* set `build_flags = -std=gnu++17` for `native_core`; verify each board env supports it (likely yes on current toolchains) — fall back to `-std=gnu++14` + tagged unions if AVR pinches.
2. **Binary size on AVR_MEGA2560.** Polymorphism + extra indirection costs flash on AVR. *Recommendation:* keep vtables small (≤ ~12 ports), mark adapters `final`, allow link-time devirtualization. If we still bust the budget, accept template-based static dispatch for the hot path (`SlewController<RaAxis, DecAxis>`) — adds complexity but keeps AVR shipping.
3. **Interrupt-driven stepping.** `InterruptAccelStepper` mutates state from ISR context. Ports for axes need explicit thread/ISR-safety contract documented; `core/` controllers must never assume single-threaded access to axis state. *Recommendation:* document this in `ports/IStepperAxis.h`; add a `Snapshot()` method returning a consistent state read.
