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

### Core Modules

- **`Mount` (src/Mount.hpp/cpp)** - Central controller. Manages stepper motors (RA, DEC, AZ, ALT, Focus), coordinate calculations, slewing, tracking, parking, and guiding. This is by far the largest module (~150KB source).
- **`MeadeCommandProcessor` (src/MeadeCommandProcessor.hpp/cpp)** - Serial command interface implementing the Meade LX200 protocol. Handles all external communication.
- **`EPROMStore` (src/EPROMStore.hpp/cpp)** - Platform-agnostic non-volatile storage abstraction (coordinates, calibration, motor config). Uses magic markers `0xCE`/`0xCF` for validation.
- **`b_setup.hpp`** - Arduino `setup()` implementation and hardware initialization.
- **`a_inits.hpp`** - Global variable initialization.

### Entry Point

`OpenAstroTracker-Firmware.ino` is the Arduino sketch entry point. It delegates to `src/a_inits.hpp` (globals) and `src/b_setup.hpp` (setup/loop).

### Stepper Control

Two modes depending on platform:

- **AVR (NEW_STEPPER_LIB)**: Interrupt-driven via Timer 3 (RA) and Timer 4 (DEC) at 2 kHz. Uses `InterruptAccelStepper` library. Configured via `StepperConfiguration.hpp`.
- **ESP32**: FreeRTOS task on Core 0 at 1 kHz (`stepperControlTask`). Core 1 runs `loop()` for serial/UI.

Both call `Mount::interruptLoop()` for step generation.

### Board Pin Definitions

Board-specific pin mappings live in `boards/<board_name>/pins_<board>.h`. Each board has its own directory.

### Display & UI

Optional LCD menu system (`LcdMenu`, `LcdButtons`) supporting multiple display types: direct LCD keypad, I2C LCD (MCP23008/MCP23017), and I2C SSD1306 OLED with joystick. An optional separate 128x64 info display shows real-time status.

## Key Conventions

- Primary branch: `develop`
- PRs must pass CI (build all boards + clang-format + unit tests)
- PR comments are resolved by OAT developers, not PR authors
- clang-format v12 is the enforced formatter (column limit: 140, Allman-style braces, 4-space indent, no tabs)
- Pointer alignment: right (`int *ptr`)
- Supported driver types: `DRIVER_TYPE_A4988_GENERIC`, `DRIVER_TYPE_TMC2209_STANDALONE`, `DRIVER_TYPE_TMC2209_UART`
- Supported stepper types: `STEPPER_TYPE_NONE`, `STEPPER_TYPE_ENABLED`
- Debug logging uses bit-flag levels (INFO, SERIAL, WIFI, MOUNT, MEADE, STEPPERS, EEPROM, GYRO, GPS, FOCUS, etc.)
