# PLAN — Minimal entry point & `#if`-guard reduction

> **Scope.** This is the forward-looking plan for the two remaining refactoring goals:
>
> 1. `src/app/main.cpp` becomes a minimal entry point — start the app, run the app, nothing else.
> 2. Preprocessor conditionals (`#if` / `#ifdef` / `#ifndef` / `#elif`) are reduced to the minimum:
>    the guards that select **which hardware a build uses and how it is configured**.
>
> Everything before this (Phases 1–8, the `src/` → layered migration) is complete and documented in
> [ARCHITECTURE.md](ARCHITECTURE.md). This plan adds **Phase 9** (minimal entry point) and
> **Phase 10** (`#if`-guard reduction). [src/app/README.md](src/app/README.md) already points at this
> file for "the target architecture", and [ARCHITECTURE.md](ARCHITECTURE.md) links it as the
> migration plan.

---

## 0. Progress log

### Phase 9 — minimal `main.cpp`: **complete**

| Item | Before | After |
|------|--------|-------|
| `src/app/main.cpp` lines | 389 | 18 |
| `src/app/main.cpp` conditionals | 42 | **0** |
| `src/app/` conditionals | 324 | **316** |
| `src/app/AppBootstrap.cpp` conditionals | — | 34 (the boot steps' own backend/pin guards) |

What landed:

- New `src/app/AppBootstrap.{hpp,cpp}`: `app::bootstrap()` runs the whole boot sequence as named
  steps (`startSerial`, `logBootBanner`, `startInfoDisplay`, `startGps`, `startDewHeater`,
  `startFan`, `startSensors`, `remapUartPins`, `startDriverPins`, `enableSharedDrivers`,
  `startHomingSensors`, `startPersistentStore`, `startMenu`, `logHardwareInfo`,
  `startCommandProcessor`, `startWifi`, `startSteppers`, `loadConfiguration`,
  `startStepperService`, `runUartSelfTest`, `finishBoot`, `showTestMenu`) in the original order,
  with the original log lines and console text.
- `src/app/main.cpp` is now `<Arduino.h>` + the two app headers + `setup()`/`loop()`.
- `src/app/BoardPresence.hpp` gained `kHasEndSwitch`, `kHasHomingSensor`, `kHasMenu`,
  `kTrackOnBoot`.
- Boot steps that were `#if`s now read those facts: end-switch console line, homing-sensor pins,
  AZ/ALT/Focus driver pins, the menu bring-up, `TRACK_ON_BOOT`. The GPS console lines dropped their
  panel guard because `addConsoleText`/`updateConsoleText` are already null helpers.

Validation: `ramps`, `mksgenlv21`, `esp32`, `oaeboardv1`, `rp2040_skr_pico` all build;
`python3 matrix_build.py -b ramps` passes all 192 configuration combinations (it varies
`DISPLAY_TYPE`, `INFO_DISPLAY_TYPE`, driver types, AZ/ALT/Focus steppers, GPS and gyro, so both
`kHasMenu` paths and every `kHas*Stepper` path are compiled); `pio test -e native` 950/950 pass;
`scripts/check_architecture.py` clean; clang-format clean. The three headless builds (`esp32`,
`oaeboardv1`, `rp2040_skr_pico` are all `DISPLAY_TYPE_NONE` + `INFO_DISPLAY_TYPE_NONE`) exercise the
`kHasMenu == false` path; `ramps` and `mksgenlv21` exercise `kHasMenu == true`.

**Finding — `main.cpp` must include `<Arduino.h>`.** The AVR core declares `setup`/`loop` *inside*
`extern "C"` (`cores/MegaCore/Arduino.h:195`). A translation unit that defines them without seeing
that declaration mangles them as C++ and the framework's `main()` fails to link with
`undefined reference to 'setup'`. The include is wrapped in `PUSH_NO_WARNINGS`/`POP_NO_WARNINGS`
because the RP2040 core's `ArduinoCore-API/api/Interrupts.h` trips `-Werror=shadow`.

**Finding — three environments cannot build in this checkout.** `mksgenlv1`, `mksgenlv2` and
`rp2040_jackw01` fail at `ConfigurationValidation.hpp` because their `Configuration_local_*.hpp`
are not tracked. Pre-existing and unrelated to this work; CI provides them.

**Deliberately deferred — product-name consolidation** (`#ifdef OAM` / `#elif defined(OAE)`). A
`constexpr const char *` would move the 16–17 byte splash/banner literal from flash into `.data` on
AVR, violating the documented "literals that outlive one call go in flash" rule, and normalising the
leading space in `" OpenAstroMount"` would visibly change the splash. It needs a flash-safe accessor
(`PSTR`-backed, read through `pgm_read_byte`) before it can be collapsed.

### Phase 10 — `#if`-guard reduction: **in progress**

| Area | Baseline | Now |
|------|---------:|----:|
| `src/app/` | 324 | **289** |
| `src/app/main.cpp` | 42 | **0** |
| `src/app/MenuHost.cpp` | 28 | **9** |
| `src/app/UiScreens.cpp` | 15 | **8** |
| `src/app/AppBootstrap.cpp` | — | 34 (moved out of `main.cpp`) |
| `src/core/`, `src/types/` | 0 | 0 |
| `src/ports/` | 3 | 3 |
| `src/adapters/` | 53 | 53 |
| `src/hal/` | 55 | 55 |

Mechanisms used so far, all of them free at run time (the compiler folds the constant and drops the
branch):

- **Board/feature facts** — `BoardPresence.hpp` now also carries `kHasGuidedStartup`,
  `kHasSerialControl`, `kHasGyro`, `kHasGps`. The menu loop asks `if (kHasGuidedStartup && …)` and
  `if (!kHasGps)` instead of wrapping each question in its own `#if`.
- **Null-object header fallbacks** — `SerialPump.hpp` declares no-op `serialLoop()` /
  `processSerialData()` for builds without serial control, so `menuLoop()` no longer needs a
  `#if SUPPORT_SERIAL_CONTROL` around its serial branch.
- **Deleting tautologies** — inside `#if DISPLAY_TYPE > 0`, `Configuration_adv.hpp` hard-codes
  `SUPPORT_MANUAL_CONTROL`, `SUPPORT_CALIBRATION` and `SUPPORT_INFO_DISPLAY` to `1`, so their guards
  in `MenuHost.cpp` and `UiScreens.{hpp,cpp}` were dead weight and are gone.

Flash/RAM cost of the whole exercise, measured on `ramps`: **173 554 → 173 628 bytes of program
(+74 B, +0.04 %), 3629 bytes of RAM (unchanged)** — the delta is section alignment, not new code.

The 9 guards left in `MenuHost.cpp` are the ones that *must* stay preprocessor: the
`DISPLAY_TYPE > 0` body itself, the two `LCD_BUTTON_TEST` bring-up blocks, the `focusMenu` object
declaration, the RP2040 poll, and the two `SUPPORT_POINTS_OF_INTEREST` / two `FOCUS_STEPPER_TYPE`
branches that choose *which screen object exists* — converting those would either move a string into
RAM or link a screen a build does not have.

---

## 1. Baseline measurements

> The numbers below are the **starting point** this plan was written against; §0 tracks the current
> figures.

Conditional-directive counts at the time of writing (one line = one `#if`/`#ifdef`/`#ifndef`/`#elif`):

| Area | Count | Notes |
|------|-------|-------|
| `src/app/` | **324** | the composition root; most of the reducible mass |
| `src/hal/` | 55 | platform backends — largely legitimate |
| `src/adapters/` | 53 | `MeadeCommandAdapter.cpp` 27, `Ssd1306InfoDisplay.cpp` 10, `EepromSettingsFormat.cpp` 7 |
| `src/libs/` | 115 | 73 in the vendored `TimerInterrupt/` library (excluded from formatting; out of scope) |
| `src/ports/` | 3 | `LogFormat.hpp` only |
| `src/macros/` | 1 | toolchain shim |
| `src/core/` | **0** | already clean |
| `src/types/` | **0** | already clean |

Hot spots inside `src/app/`:

| File | Count |
|------|-------|
| `DriverWiring.cpp` | 47 |
| `main.cpp` | 42 |
| `BoardWiring.cpp` | 35 |
| `MountExecutor.hpp` | 31 |
| `MenuHost.cpp` | 28 |
| `MountReadout.cpp` | 27 |
| `UiScreens.cpp` | 15 |
| `MotorWiring.cpp` | 12 |
| `BoardWiring.hpp` | 11 |
| `test/TestMenu.cpp` + `.hpp` | 21 |
| `SensorWiring.cpp` | 9 |

`main.cpp` today is **389 lines**, of which `loop()` is two lines and `setup()` is the entire boot
sequence, guarded by ~42 conditionals.

---

## 2. What stays vs. what goes

### 2.1 Guards that stay (by design)

These are "configuring which hardware is used and how it is configured during build", and they stay:

- **Backend selection** — the single `#if` per family in the wiring modules that picks a concrete
  backend: display panel (`DISPLAY_TYPE_*`), keypad (`DISPLAY_TYPE_*`), stepper engine
  (`NEW_STEPPER_LIB` / `ARDUINO_ARCH_RP2040` / `ESP32`), TMC driver type
  (`*_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART`), info panel (`INFO_DISPLAY_TYPE_*`).
- **Pin presence** — `#ifdef RA_MS0_PIN`, `#ifdef LCD_BRIGHTNESS_PIN`, `#ifdef RP2040_*_PIN`, and the
  `defined(DEW_HEATER_1_PIN)` guards: a pin that a board does not declare must simply not compile.
- **Platform guards** — `defined(ESP32)`, `defined(ARDUINO_ARCH_RP2040)`,
  `defined(__AVR_ATmega2560__)`, `BOARD < 1000` (AVR vs non-AVR), inside `hal/<platform>/` and the
  wiring modules.
- **Configuration defaults** — the `#ifndef X / #define X` defaults in `Configuration*.hpp` and
  `Constants.hpp`.

### 2.2 Guards that go (reducible today)

- **Boot-sequence guards in `main.cpp`** — every `#if` currently in `setup()` moves behind an
  `app::` function; the guard moves with it or becomes a compile-time fact/null object (Phase 9).
- **Behavioural feature flags scattered across call sites** — `SUPPORT_GUIDED_STARTUP`,
  `SUPPORT_SERIAL_CONTROL`, `SUPPORT_POINTS_OF_INTEREST`, `SUPPORT_MANUAL_CONTROL`,
  `SUPPORT_CALIBRATION`, `SUPPORT_INFO_DISPLAY`, `SUPPORT_DRIFT_ALIGNMENT`, `TRACK_ON_BOOT`,
  `LCD_BUTTON_TEST`, `USE_GYRO_LEVEL`. These are 0/1 `#define`s consumed in `MenuHost.cpp`,
  `UiScreens.cpp`, `MountReadout.cpp` and `MeadeCommandAdapter.cpp`. They are decisions, not
  hardware facts, so per ARCHITECTURE.md principle 5 they become runtime/compile-time *facts*
  resolved once in `app/`, never scattered `#if`s.
- **Product-variant guards** — `#ifdef OAM`, `#elif defined(OAE)` (the boot banner and startup
  wizard). Collapse to a single `PRODUCT_NAME` constant chosen once.
- **Axis-presence guards re-expressed as raw `#if`** — `AZ_STEPPER_TYPE != STEPPER_TYPE_NONE`,
  `ALT_STEPPER_TYPE != STEPPER_TYPE_NONE`, `FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE`,
  `USE_RA_END_SWITCH == 1`, `USE_HALL_SENSOR_*_AUTOHOME == 1` are already mirrored in
  [BoardPresence.hpp](src/app/BoardPresence.hpp) as `constexpr bool` facts; call sites should use
  the facts (`if constexpr`) instead of repeating the macro test.

---

## 3. Phase 9 — Minimal `main.cpp`

### 3.1 Target shape

`src/app/main.cpp` shrinks to a stub (≈15 lines, **zero** conditionals):

```cpp
// Entry point: start the app, then run it. The object graph lives in app/BoardWiring.cpp;
// the boot sequence lives in app/AppBootstrap.cpp. This file deliberately knows nothing else.
#include "app/AppBootstrap.hpp"
#include "app/MenuHost.hpp"

void setup()
{
    app::bootstrap();
}

void loop()
{
    app::menuLoop();
}
```

`app::bootstrap()` is declared in a new `src/app/AppBootstrap.hpp` and defined in
`src/app/AppBootstrap.cpp`. It performs the entire boot sequence `setup()` performs today, in the
same order, calling small `app::startX()` helpers that carry the hardware guards. `app::menuLoop()`
already exists in [MenuHost.hpp](src/app/MenuHost.hpp).

### 3.2 Boot steps and where each goes

Each block of today's `setup()` moves to a named function. "Guard home" says where the remaining
`#if` (if any) lives — a wiring module, a constexpr fact, or nowhere (null object).

| # | `setup()` block today | New function | Guard home |
|---|-----------------------|--------------|------------|
| 1 | `debug_init()` / `breakpoint()` (AVR) | `app::startDebug()` | `hal/avr/` + `#if BOARD < 1000` in AppBootstrap |
| 2 | `Serial.begin`, RP2040 DTR, debug serial, console-wait | `app::startSerial()` | platform guards, in AppBootstrap |
| 3 | Boot banner (OAM/OAE/OAT × TEST_VERIFY_MODE) | `app::logBootBanner()` | single `PRODUCT_NAME` constant |
| 4 | Info-display begin + console text | `app::startInfoDisplay()` | already `#if`-free via `addConsoleText` helpers |
| 5 | GPS init + pin remap | `app::startGps()` | `USE_GPS`, pin `#ifdef`s |
| 6 | Dew heater | `app::startDewHeater()` | `DEW_HEATER`, pin `#ifdef`s |
| 7 | Fan (`RP2040_FAN1_PIN`) | `app::startFan()` | pin `#ifdef` |
| 8 | End-switch console line + `attachSensors` | fold into `app::startSensors()` | `if constexpr (kHasAnyEndSwitch)` |
| 9 | RP2040 UART1 pin remap | `app::remapUartPins()` | `ARDUINO_ARCH_RP2040` + pin `#ifdef`s |
| 10 | RA/DEC driver pins | `app::startDrivers()` | `initDriverPins()` already in DriverWiring |
| 11 | AZ/ALT/Focus driver pins | same, via `if constexpr (kHasAzStepper)` etc. | `BoardPresence.hpp` facts |
| 12 | `RP2040_SHARED_EN_PIN` force-LOW | `app::applySharedEnablePin()` | board fact |
| 13 | Homing-sensor pins | `app::initHomingSensorPins()` (exists) | `if constexpr (kHasRaHoming \|\| kHasDecHoming)` |
| 14 | EEPROM store | `app::initPersistentStore()` (exists) | none |
| 15 | LCD startup, factory-reset, menu items | `app::startMenu()` | `DISPLAY_TYPE > 0` once, in AppBootstrap |
| 16 | Meade command processor wiring | `app::startCommandProcessor()` | none (gyro/gps/logRetention already null-safe) |
| 17 | Wi-Fi | `app::startWifi()` | `WIFI_ENABLED == 1` |
| 18 | Stepper attach + 1 s delay | `app::startSteppers()` | none (attach* already exist) |
| 19 | Stepper service (ESP32 task / AVR timer) | `app::startStepperService()` | `ESP32` vs AVR, in AppBootstrap |
| 20 | UART connection self-test | `app::runUartSelfTest()` | `UART_CONNECTION_TEST_TX` |
| 21 | Hemisphere, `TRACK_ON_BOOT`, boot-complete | `app::finishBoot()` | `TRACK_ON_BOOT` → compile-time fact |
| 22 | Test-menu display | `app::showTestMenu()` | `TEST_VERIFY_MODE` |

Ordering is preserved exactly; the only change is location. Log lines (`LOG(DEBUG_ANY, ...)`) keep
their current text and sequence so the boot console and `:D#` behaviour are byte-identical.

### 3.3 New files

- `src/app/AppBootstrap.hpp` — `namespace app { void bootstrap(); }` plus the `app::startX()`
  declarations the wiring modules don't already own.
- `src/app/AppBootstrap.cpp` — the boot sequence; the only place outside the wiring modules where a
  boot-order `#if` may live.
- [BoardWiring.hpp](src/app/BoardWiring.hpp) / [BoardWiring.cpp](src/app/BoardWiring.cpp) stay the
  object-graph owner; `bootstrap()` reads its `extern`s.

### 3.4 Exit criterion

`grep -cE '^\s*#\s*(if|ifdef|ifndef|elif)' src/app/main.cpp` returns **0**, and `main.cpp` is ≤ ~30
lines. Every `#if` that leaves `main.cpp` either disappears (null object/fact) or lands in a
wiring/bootstrap module.

---

## 4. Phase 10 — `#if`-guard reduction across the codebase

### 4.1 The three mechanisms

1. **Null object / port** — for *optional capabilities* (gyro, GPS, info panel, Wi-Fi, optional
   axes, non-TMC drivers). The composition root installs either the real object or its null object;
   call sites stay unconditional. Already the pattern for `gyro`, `gpsPort`, `logRetention`,
   `menuDisplayPort` and the info-display helpers — extend it, don't invent a new one.
2. **Compile-time facts + `if constexpr`** — for *static board facts* that must still be resolved at
   compile time (a pin constant that only exists on one board, an axis a board physically lacks).
   One definition per fact in [BoardPresence.hpp](src/app/BoardPresence.hpp) (extended to a fuller
   "board traits" header); call sites write `if constexpr (kHasAzStepper)` instead of repeating
   `#if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE`. This turns preprocessor branches into ordinary,
   formattable, lint-able C++ and gives the fact a single definition point.
3. **Runtime facts read once in `app/`** — for *behavioural feature flags* (`SUPPORT_*`,
   `TRACK_ON_BOOT`). Read them once into `MountConfig` (or a small `app::BootOptions` struct) and
   pass the value down; the menu registers only the screens it has, the boot sequence starts
   tracking only when asked. No virtual dispatch is introduced (see §6, AVR budget).

Decision rule, in order: **null object if the thing is optional → constexpr fact if it must be
compile-time → runtime option if it's a behaviour switch.** `#if` remains only when none of the
three applies (a pin/backend that only exists under a platform/board).

### 4.2 Per-area work

- **`src/app/main.cpp`** — 42 → 0 (Phase 9).
- **`src/app/MenuHost.cpp`** — 28 → 0. The `SUPPORT_*` branches decide *which menu items exist*.
  Move item registration to `app/` (the composition root builds the menu table with only the
  screens a feature enables); `MenuHost` iterates whatever it is given. `TRACK_ON_BOOT` and
  `LCD_BUTTON_TEST` become facts read once. `FOCUS_STEPPER_TYPE` becomes `kHasFocusStepper` (the
  focus controller is already null when absent).
- **`src/app/UiScreens.cpp`** — 15 → 0. Same treatment: screen availability is a menu-table
  decision made in `app/`, not per-screen `#if`s.
- **`src/app/MountReadout.cpp`** — 27 → 0. Optional-axis reads are null-object no-ops on the
  readout already; remove the repeated `*_STEPPER_TYPE` tests in favour of the controller/axis
  null-objects.
- **`src/app/MountExecutor.hpp`** — 31 → ≤ ~5. Optional-axis members and enable pins are the bulk;
  the enable pins stay a `MountDeps` concern (see ARCHITECTURE.md §6: a board without an axis pays
  no pointer). Keep only the `#ifdef ESP32` `DRAM_ATTR` guards and the TMC-only enable pins.
- **`src/app/BoardWiring.{hpp,cpp}`** — 46 → the *canonical* backend-selection home. One `#if` per
  backend family (display, keypad, stepper engine, driver, info panel, gyro, GPS, Wi-Fi), and the
  `#if`-free helpers stay. Behavioural flags leave this file entirely.
- **`src/app/DriverWiring.cpp`** — 47 → keep as the hardware-wiring home; use the board facts for
  axis presence so each macro test appears once instead of per-axis/per-register.
- **`src/app/MotorWiring.cpp` / `SensorWiring.cpp`** — 12 / 9 → keep; platform guards only.
- **`src/app/test/`** — 21 → `TEST_VERIFY_MODE` / `LCD_BUTTON_TEST` wiring only, consolidated.
- **`src/adapters/MeadeCommandAdapter.cpp`** — 27 → ≤ ~5. The command-handler `#if`s
  (`SUPPORT_*`, `USE_GYRO_LEVEL`, optional axes) become unconditional handlers over null objects /
  runtime facts; what remains is transport/pin wiring.
- **`src/adapters/Ssd1306InfoDisplay.cpp` / `EepromSettingsFormat.cpp`** — 10 / 7 → reduce using
  constexpr facts + null panel/settings paths.
- **`src/ports/LogFormat.hpp`** — 3 → keep (debug-mask formatting, config-adjacent).
- **`src/hal/`** — 55 → keep platform backends; remove only redundant duplicates.

### 4.3 Exit criterion

An audit greps every remaining conditional directive and classifies it into exactly one of:

1. backend/pin selection in `app/` wiring + `AppBootstrap`,
2. platform guard in `hal/<platform>/`,
3. configuration default in `Configuration*.hpp` / `Constants.hpp`,
4. vendored third-party code (`src/libs/TimerInterrupt`, `src/libs/MappedDict`).

`core/`, `ports/`, `types/` and `app/ui/` remain at **0** feature `#if`s — [scripts/check_architecture.py](scripts/check_architecture.py)
already enforces `core/`, `ports/` and `app/ui`.

---

## 5. Order of work

1. **Phase 9 first.** Extract `setup()` into `app::bootstrap()` mechanically, block by block, with a
   `pio run -e ramps -e esp32` + native tests after each block. `main.cpp` minimalism is the
   observable, low-risk half and it makes Phase 10's guard audit tractable (every remaining guard
   is now in a module whose job is "hardware wiring").
2. **Extend the facts header** — widen [BoardPresence.hpp](src/app/BoardPresence.hpp) into the full
   board-trait set (`kHasX` for every optional part, `kUsesTmcUart`, `kHasAnyEndSwitch`,
   `kHasSharedEnablePin`, `kTrackOnBoot`, `kProductName`, …), one `#if` per fact at definition only.
3. **Convert `app/` call sites** — replace raw macro tests with `if constexpr` on the facts, in
   `BoardWiring`, `DriverWiring`, `MountExecutor.hpp`, `MountReadout`, `MenuHost`, `UiScreens`.
4. **Menu-table registration** — move `SUPPORT_*` out of `MenuHost`/`UiScreens` into a single
   `app/` registration function; the menu host and screens become `#if`-free.
5. **Adapters sweep** — `MeadeCommandAdapter`, `Ssd1306InfoDisplay`, `EepromSettingsFormat`.
6. **Audit + CI** — run the classifier of §4.3, fix strays, then the full validation matrix.

Each step lands as its own commit on `refactoring` (primary branch `develop` per
[CLAUDE.md](CLAUDE.md); this work is already on the `refactoring` branch).

---

## 6. Risks & mitigations

| Risk | Mitigation |
|------|------------|
| **AVR flash/SRAM budget** (principle 9: 256 KB / 8 KB). Runtime options must not add vtables or per-board pointer members a board does not use | Prefer `constexpr` facts + `if constexpr` (zero runtime cost) over runtime bools; keep null objects concrete and `final`; keep the "no `MountDeps` entry for an absent axis" rule |
| **Boot-sequence regression** (ordering of logs, EEPROM, `bootComplete()`, `:D#`) | Mechanical extraction, one block per commit, byte-compare boot console; native tests + `ramps`/`esp32` builds after each block |
| **`DEBUG_LEVEL` bit-mask semantics** — `LOG()` compiles out per-mask | Keep the existing `NullLogger` switch in `BoardWiring`; do not reintroduce raw `#if DEBUG_LEVEL` at call sites |
| **OAM/OAE product variants** | Collapse `#ifdef OAM`/`#elif OAE` to a single `kProductName` fact chosen once; update the startup wizard and banner together |
| **Flash cost of "linking all backends to make it a runtime choice"** | Explicitly out of scope (see §2.1): the display/keypad/driver family *is* a build-time board fact and keeps its single backend-selection `#if` |
| **Vendored libs** | `src/libs/TimerInterrupt` (73 conditionals) is third-party and format-excluded — untouched |

---

## 7. Acceptance criteria

1. `src/app/main.cpp` ≤ ~30 lines, **0** conditional directives.
2. `core/`, `ports/`, `types/`, `app/ui/` contain **0** feature `#if`s (enforced).
3. Every remaining `#if` passes the §4.3 classifier (backend/pin, platform, config default, or
   vendored).
4. Behavioural flags (`SUPPORT_*`, `TRACK_ON_BOOT`, `OAM`/`OAE`) no longer appear as `#if`s outside
   `Configuration*.hpp`/`Constants.hpp`/the facts header; their single definition is consumed in
   `app/`.
5. Boot output and Meade LX200 behaviour are byte-for-byte unchanged (frozen contract).
6. RAMPS flash/RAM report unchanged or smaller (`pio run -e ramps` memory report, before/after).

## 8. Validation

```shell
# Guard audit (expect 0 for the clean layers, classified remainder elsewhere)
grep -rE '^\s*#\s*(if|ifdef|ifndef|elif)' src/app/main.cpp src/core src/ports src/types src/app/ui

# Architecture rules
python scripts/check_architecture.py

# All board builds (CI parity)
pio run -e ramps -e esp32 -e mksgenlv21 -e mksgenlv2 -e mksgenlv1 -e oaeboardv1

# Host unit tests
pio test -e native

# Matrix build (configuration combinations)
python matrix_build.py

# Formatting (clang-format v12, CI-enforced)
bash -c 'shopt -s nullglob globstar;GLOBIGNORE=./src/libs/TimerInterrupt/*; for i in ./{.,src/**,unit_tests,boards/**}/*.{c,cpp,h,hpp}; do clang-format --dry-run $i; done'

# Size check
pio run -e ramps -t size   # compare Program/Data against baseline
```
