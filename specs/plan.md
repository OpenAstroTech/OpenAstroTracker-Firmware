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
|   |          Backends: hal/arduino/, hal/esp32/, hal/avr/,    |
|   |                    hal/host/ (for native tests).          |
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
|              MeadeExecutor, MountConfig, EventBus.            |
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

## Phased Plan (each phase shippable & green in CI)

### Phase 0 — Safety net & tooling (no behavior change)
*Foundation for everything else; must land first.*

1. Add **Fake Function Framework (FFF)** as a header-only dep in `unit_tests/test_common/fakes/`.
2. Add a new PIO env `native_core` (extends `native`) with stricter warnings (`-Wall -Wextra -Werror`) for host-only `core/` builds; keep existing `native` for compatibility.
3. Add gcovr/lcov-based **coverage reporting** to the `native` env; publish summary in CI.
4. Extend `.github/workflows/platformio_unit_tests.yml`:
   - Run `pio test -e native -v`.
   - Run coverage and fail if `core/` coverage drops below configured threshold (start at 0, ratchet upward).
5. Add a tiny **Arduino host shim** under `unit_tests/test_common/arduino_shim/` providing minimal stubs (`millis`, `String`, `pinMode`, `digitalWrite`, fake `EEPROM`, fake `Serial`) for files that include `<Arduino.h>` but whose logic we want to test on host. This shim will later be replaced by the proper `hal/host/` backend (Phase 3).
6. Establish folders: `src/core/`, `src/ports/`, `src/hal/`, `src/hal/host/`, `src/adapters/`, `src/app/` (empty + READMEs); leave existing files in place.

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
   These tests link against a stripped-down `Mount` compiled with the host shim — they fail the moment behavior shifts during extraction.

**Verify:** All new tests green; coverage report shows non-trivial line coverage on the listed methods; CI threshold ratcheted up.

### Phase 2 — Extract pure domain (in place → `src/core/`)
*Move characterized logic into `core/` with no behavior change. Tests from Phase 1 prevent regressions.*

1. Create `core/CoordinateMath` from `Mount::calculateRAandDECSteppers` + helpers; replace original with a thin delegate. Inputs/outputs as plain structs (`MountGeometry`, `EquatorialTarget`, `StepperTarget`).
2. Create `core/SiderealClock` wrapping `Sidereal::` statics behind an instance API that takes an `IClock`.
3. Create `core/CalendarMath` from `Mount::getLocalDate`.
4. Create `core/CoordinateFormatter` from RA/DEC string formatters.
5. Create `core/MountGeometry` value type holding steps-per-degree, calibration angles, hemisphere, backlash — replaces scattered Mount fields used by math.
6. Create `core/MeadeParser` by splitting `MeadeCommandProcessor`: pure tokenize/dispatch lookup tables in `core/`; execution stays in adapter for now.

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
   - `hal/host/` — pure C++ test backend (in-memory EEPROM, virtual GPIO, controllable clock); replaces and absorbs the Phase 0 ad-hoc shim.
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

**Verify:** All Phase 1/2 tests still green; new contract tests for each port using `hal::host` backends (e.g., `EepromPersistentStore` round-trips via in-memory `hal::host::HostEeprom`); golden-master tests on `Mount` still pass; firmware builds identical-sized binaries on at least one board (other variants ±1%).

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

### Phase 6 — Meade execution layer
*Finish what Phase 2 started: separate parser from executor.*

1. `core/MeadeExecutor` operating on controller interfaces (no `Mount*`).
2. `adapters/SerialTransport` + `adapters/WifiTransport` feed bytes to `MeadeParser`; parsed commands dispatch to `MeadeExecutor`.
3. Remove `Mount* _mount` from `MeadeCommandProcessor` and the legacy `Mount::delay()` blocking call.
4. Add a comprehensive Meade-protocol test suite using `scripts/MeadeCommandParser.py` traces as fixtures.

**Verify:** Meade test suite green; Stellarium/ASCOM round-trip smoke test (manual) recorded as a regression checklist; coverage of `core/` ≥ 80%.

### Phase 7 — Cleanup & documentation
1. Mount facade slimmed to a thin compat shim (or removed if no external dependents).
2. Move display-related code paths off the Mount → display direct call into the `EventBus`.
3. Architecture doc (`docs/architecture.md`) with the layer diagram, port catalog, and "where to add a new feature" guide.
4. Ratchet CI coverage gate to its final value (`core/` ≥ 85%, `adapters/` ≥ 40%).

**Verify:** Architecture doc reviewed; CI gates final; full board matrix green; smoke-tested on at least one real mount.

---

## Relevant files (initial focus)

- [src/Mount.cpp](src/Mount.cpp), [src/Mount.hpp](src/Mount.hpp) — the god-object being decomposed; `loop()`, `calculateRAandDECSteppers()`, `guidePulse()`, `startSlewing()`, `readPersistentData()` are the biggest extraction targets.
- [src/MeadeCommandProcessor.cpp](src/MeadeCommandProcessor.cpp), [src/MeadeCommandProcessor.hpp](src/MeadeCommandProcessor.hpp) — split into parser (`core/`) + executor (Phase 6).
- [src/EPROMStore.cpp](src/EPROMStore.cpp), [src/EPROMStore.hpp](src/EPROMStore.hpp) — already a good seam; becomes `IPersistentStore` + adapter.
- [src/Sidereal.cpp](src/Sidereal.cpp), [src/DayTime.cpp](src/DayTime.cpp), [src/Declination.cpp](src/Declination.cpp), [src/Latitude.cpp](src/Latitude.cpp), [src/Longitude.cpp](src/Longitude.cpp) — already pure; move into `core/` in Phase 2.
- [src/HallSensorHoming.cpp](src/HallSensorHoming.cpp), [src/EndSwitches.cpp](src/EndSwitches.cpp), [src/Gyro.cpp](src/Gyro.cpp), [src/LcdMenu.cpp](src/LcdMenu.cpp), [src/SSD1306_128x64_Display.cpp](src/SSD1306_128x64_Display.cpp), [src/WifiControl.cpp](src/WifiControl.cpp), [src/LcdButtons.cpp](src/LcdButtons.cpp) — become adapters behind ports.
- [src/Core.cpp](src/Core.cpp), [src/a_inits.hpp](src/a_inits.hpp), [src/b_setup.hpp](src/b_setup.hpp), [src/f_serial.hpp](src/f_serial.hpp) — wiring code gradually migrates into `src/app/`.
- [platformio.ini](platformio.ini) — add `native_core` env, coverage flags, FFF include path.
- [.github/workflows/platformio_unit_tests.yml](.github/workflows/platformio_unit_tests.yml) — coverage gating, ratchet.
- [unit_tests/test_common/](unit_tests/test_common/test_MappedDict.cpp), [unit_tests/test_embedded/](unit_tests/test_embedded/main.cpp) — expand with FFF-based ports tests.
- [Configuration.hpp](Configuration.hpp), [Configuration_adv.hpp](Configuration_adv.hpp) — read once by `MountConfig` builder in Phase 5.

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
3. **Interrupt-driven stepping.** `InterruptAccelStepper` mutates state from ISR context. Ports for axes need explicit thread/ISR-safety contract documented; `core/` controllers must never assume single-threaded access to axis state. *Recommendation:* document this in `ports/IStepperAxis.h`; add a `Snapshot()` method returning a consistent state read.\n