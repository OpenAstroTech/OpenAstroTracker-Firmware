# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OpenAstroTracker-Firmware is embedded C++ firmware for the OpenAstroTracker automated astronomy mount. It supports multiple hardware platforms (AVR ATmega2560 and ESP32) with various stepper motor drivers, display types, and accessories.

## Build System

PlatformIO is the build system. The project uses the Arduino framework.

### Common Commands

```shell
# Build for a specific board environment
pio run -e ramps
pio run -e esp32
pio run -e mksgenlv21
pio run -e mksgenlv2
pio run -e mksgenlv1
pio run -e oaeboardv1

# Upload firmware
pio run -e ramps -t upload

# Run unit tests (native platform)
pio test -e native

# Run unit tests with coverage report (requires gcovr: pip install gcovr)
./scripts/test-coverage.sh          # defaults to -e native
./scripts/test-coverage.sh -e native
# Report: .pio/build/native/coverage_report/index.html

# Run matrix build (tests many configuration combinations across boards)
python matrix_build.py -b ramps     # single board
python matrix_build.py              # all boards

# Format code (clang-format v12 required, enforced by CI)
bash -c 'shopt -s nullglob globstar;GLOBIGNORE=./src/libs/TimerInterrupt/*; for i in ./{.,src/**,unit_tests,boards/**}/*.{c,cpp,h,hpp}; do clang-format -i $i; done'

# Debug (ATmega2560 only, uses avr-stub)
pio run -e ramps -t clean && piodebuggdb -e ramps

# Generate Meade command Wiki docs
python scripts/MeadeCommandParser.py
```

### Build Notes

- `-Werror` is enabled: all warnings are errors
- Uses ETL (Embedded Template Library) in non-STL mode (`-D ETL_NO_STL`), not the C++ standard library
- Optimization: `-O2` (production), `-Og` (debug)
- Monitor baud rate: 19200
- `src/libs/TimerInterrupt/` is excluded from formatting checks
- CI runs on every PR: build all boards, clang-format check, unit tests

## Architecture

### Configuration Hierarchy (evaluated in this order)

1. **`Configuration_local.hpp`** - User hardware config (not tracked by git). Generate at https://config.openastrotech.com/
2. **`Configuration_local_<board>.hpp`** - Board-specific local config variant
3. **`Configuration.hpp`** - Default values for anything not set in local config
4. **`Configuration_adv.hpp`** - Advanced settings (motor steps, speed, microstepping, TMC driver params)
5. **`Constants.hpp`** - System constants (board IDs, feature flag values). **Do not modify for user configuration.**

### Layers (`src/` holds nothing but directories; no source file lives at its top)

| Layer | Directory | Contains |
|---|---|---|
| **types/** | `src/types/` | Dependency-free value vocabulary (`DayTime`, `RightAscension`, `Declination`, `Latitude`, `Longitude`, `MathUtil`, `Button`, `FocuserDirection`, `MountEvent`). Standard headers only — every other layer may name these. |
| **core/** | `src/core/` | Pure domain: controllers (`MountCoordinator` + tracking/slew/guiding/homing/focus/parking/az-alt/drift-alignment), `MountState`, `EventBus`, `SiteTime`/`SiteTimeService`, the menu model. No Arduino, no `String`, no `millis()`, no board config — host-tested. |
| **ports/** | `src/ports/` | Pure abstract interfaces the domain needs (`IMountMotion`, `IMountReadModel`, `IMountConfiguration`, `IMountSiteTime`, `IDisplay`, `IButtons`, `IClock`, `ILogger`, `IStepperAxis`, …). |
| **hal/** | `src/hal/` | Hardware abstraction and one backend per platform (`arduino/`, `avr/`, `esp32/`, `rp2040/`): pin toggles, UART, EEPROM, timer services, stepper engines, panels. |
| **adapters/** | `src/adapters/` | Port implementations that stitch HAL together: `MeadeCommandAdapter`, the Meade protocol driver (`adapters/meade/`), `StepperAxis`, `EepromSettingsFormat`/`MountSettingsStore`, `SerialLogger`, displays, GPS, Wi-Fi, the `String`-based wire/format overlays, and the presentation/input helpers (`InfoDisplayLayout`, `ConsoleLog`, `ButtonDebounce`). |
| **app/** | `src/app/` | Composition root and device policy: `main.cpp` (`setup`/`loop`), `BoardWiring`, `MotorWiring`/`DriverWiring`/`SensorWiring`, `MountExecutor` + `AxisBank`/`MountReadout`/`MountSettings`/`StepPump`, and the Arduino-free, host-tested UI (`app/ui/` screens + `MenuHost`). |

Dependencies point inward: `app → adapters → ports → types`, `app → hal`, `core → ports → types`. `core/` and `ports/` may not include `hal/`, `adapters/` or `<Arduino.h>`; `adapters/` never includes `core/`; `types/` includes nothing but the standard headers. `scripts/check_architecture.py` enforces the `core/`/`ports/`/`app/ui` rules.

- **`app::MountExecutor`** (`src/app/MountExecutor*.cpp`) — the actuator side: axes, drivers, microstep switching and the hardware side effects. The decision layer is `core::MountCoordinator`.
- **`adapters::MeadeCommandAdapter`** (`src/adapters/`) — the Meade LX200 command layer; the parse itself is `adapters/meade/`.
- **`adapters::EepromSettingsFormat`** (`src/adapters/`) — the persisted format over `ports::IPersistentStore`; magic markers `0xCE`/`0xCF` live in `hal/EepromLayout.hpp`.

### Entry Point

`src/app/main.cpp` defines `setup()` and `loop()`; `OpenAstroTracker-Firmware.ino` is an empty Arduino-IDE stub kept only as a sketch anchor. `app/BoardWiring.cpp` owns the object graph.

### Stepper Control

Two modes depending on platform:

- **AVR (NEW_STEPPER_LIB)**: Interrupt-driven via Timer 3 (RA) and Timer 4 (DEC) at 2 kHz. Uses `InterruptAccelStepper` library. Configured via `StepperConfiguration.hpp`.
- **ESP32**: FreeRTOS task on Core 0 at 1 kHz (`stepperControlTask`). Core 1 runs `loop()` for serial/UI.

Both call the step pump (`app::StepPump`) for step generation; the engines themselves are `hal/` backends behind `hal::IStepperMotor`.

### Board Pin Definitions

Board-specific pin mappings live in `boards/<board_name>/pins_<board>.h`. Each board has its own directory.

### Display & UI

The menu screens are classes in `src/app/ui/screens/`, driven by `app::MenuHost.hpp`, written against `ports::IDisplay`/`ports::IButtons` and host-tested byte-by-byte (`unit_tests/test_ui/`). Panel and keypad backends live in `src/hal/` (`hal::ICharLcd` with `CharLcd`/`I2cCharLcd`/`U8x8CharLcd`, `hal::IButtonMatrix` with analog/joystick/I2C-expander decoders) and are selected by exactly one `#if DISPLAY_TYPE` in `app/BoardWiring.cpp`. An optional separate 128×64 info display (`hal::Ssd1306Panel` behind `ports::IInfoDisplay`) shows real-time status.

## Key Conventions

- Primary branch: `develop`
- PRs must pass CI (build all boards + clang-format + unit tests)
- PR comments are resolved by OAT developers, not PR authors
- clang-format v12 is the enforced formatter (column limit: 140, Allman-style braces, 4-space indent, no tabs)
- Pointer alignment: right (`int *ptr`)
- Supported driver types: `DRIVER_TYPE_A4988_GENERIC`, `DRIVER_TYPE_TMC2209_STANDALONE`, `DRIVER_TYPE_TMC2209_UART`
- Supported stepper types: `STEPPER_TYPE_NONE`, `STEPPER_TYPE_ENABLED`
- Debug logging uses bit-flag levels (INFO, SERIAL, WIFI, MOUNT, MEADE, STEPPERS, EEPROM, GYRO, GPS, FOCUS, etc.)
