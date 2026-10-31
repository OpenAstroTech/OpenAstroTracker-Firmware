# PLAN — Configuration structs: read the `Configuration*.hpp` defines once, consume structs in `app/`

> **Scope.** The board's compile-time configuration lives in three headers —
> [`Configuration.hpp`](Configuration.hpp), [`Configuration_adv.hpp`](Configuration_adv.hpp),
> [`Constants.hpp`](Constants.hpp) — and is currently re-expanded as `#define` macros at dozens of
> call sites, most of them in [`src/app/`](src/app/). This plan moves every *runtime value* those
> macros express into plain `core::` structs that the composition root fills **once** (exactly the
> pattern `core::MountConfig` + `app::buildMountConfig()` already establishes), and turns every
> *compile-time presence/selector* macro into a `app::BoardPresence`-style `constexpr` fact.
>
> The exit criterion: **`src/app/` reads each `Configuration*.hpp` macro in at most one place — the
> builder that fills the struct — and the rest of the app consumes struct fields and `constexpr`
> facts.** Feature gates that must stay preprocessor (they select which object exists or which
> backend compiles) are catalogued in §5, not swept away.

This continues the work of Phases 1–10 recorded in [ARCHITECTURE.md](ARCHITECTURE.md) and the
previous `PLAN.md`; this is **Phase 11**.

---

## 0. Progress log

| Step | Status |
|------|--------|
| 1. `core/` value structs | **planned** |
| 2. `app/build*Config()` builders | **planned** |
| 3. Thread structs through `MountExecutor` / `MountReadout` / `MountSettings` | **planned** |
| 4. Rewire `MotorWiring` / `DriverWiring` / `SensorWiring` | **planned** |
| 5. Rewire `AppBootstrap` / `BoardWiring` / `SerialPump` | **planned** |
| 6. Extend `BoardPresence` `constexpr` facts | **planned** |
| 7. Verification (matrix build, `pio test -e native`, `check_architecture.py`, clang-format) | **planned** |

---

## 1. Current state

The three config headers expose **155 macros** (`grep '^#define'` across
[`Configuration.hpp`](Configuration.hpp) + [`Configuration_adv.hpp`](Configuration_adv.hpp)).
Usage is heavily concentrated in `app/` — 24 files read them, ~330 macro expansions — plus three
adapter-local clusters that are already de-facto localised (§5.3).

Largest `app/` consumers (raw macro expansions):

| File | Reads | Dominant macros |
|------|------:|-----------------|
| [`src/app/DriverWiring.cpp`](src/app/DriverWiring.cpp) | 44 | `*_DRIVER_TYPE`, `*_RMSCURRENT`, `*_STALL_VALUE`, `*_MOTOR_HOLD_SETTING`, `*_UART_STEALTH_MODE`, `*_MICROSTEPPING`, `UART_CONNECTION_TEST_*`, `USE_VREF` |
| [`src/app/MotorWiring.cpp`](src/app/MotorWiring.cpp) | 39 | `*_STEPPER_SPR/SPEED/ACCELERATION`, `*_MICROSTEPPING`, `*_TRANSMISSION`, `*_CIRCUMFERENCE`, `*_INVERT_DIR`, `*_STEPS_PER_REV`, `*_STEPS_PER_ARC_MINUTE` |
| [`src/app/BoardWiring.cpp`](src/app/BoardWiring.cpp) | 39 | `DISPLAY_TYPE`, `INFO_DISPLAY_TYPE*`, `*_DRIVER_TYPE`, `*_STEPPER_TYPE`, `SIDEREAL_SECONDS_PER_DAY`, `RA_ZERO_POINT_SHIFT_HOURS`, `POLARIS_RA_*`, `BACKLASH_STEPS`, `DEBUG_LEVEL`, `USE_*` |
| [`src/app/MountReadout.cpp`](src/app/MountReadout.cpp) | 35 | `*_DRIVER_TYPE`, `DISPLAY_TYPE`, `INFO_DISPLAY_TYPE`, `*_STEPPER_TYPE`, `RA_LIMIT_*`, `POLARIS_RA_*`, `RA/DEC_PULLEY_TEETH`, `RA/DEC_STEPPER_SPR`, `USE_*` |
| [`src/app/AppBootstrap.cpp`](src/app/AppBootstrap.cpp) | 21 | `SERIAL_BAUDRATE`, `DEBUG_*`, `INFO_DISPLAY_TYPE`, `GPS_BAUD_RATE`, `USE_GPS`, `WIFI_ENABLED`, `DEW_HEATER`, `STEPPER_CONTROL_INTERVAL_MS` |
| [`src/app/MountExecutor.hpp`](src/app/MountExecutor.hpp) | 18 | `*_DRIVER_TYPE`, `UART_CONNECTION_TEST_*`, `USE_HALL_SENSOR_*`, `USE_GYRO_LEVEL`, `SUPPORT_DRIFT_ALIGNMENT` |
| [`src/app/SensorWiring.cpp`](src/app/SensorWiring.cpp) | 15 | `USE_HALL_SENSOR_*`, `USE_*_END_SWITCH`, `*_HOMING_SENSOR_ACTIVE_STATE`, `*_END_SWITCH_ACTIVE_STATE` |
| [`src/app/BoardPresence.hpp`](src/app/BoardPresence.hpp) | 12 | presence + software-feature switches (`kHas*` facts already) |
| [`src/app/MountExecutorSlew.cpp`](src/app/MountExecutorSlew.cpp) | 8 | `RA_PHYSICAL_LIMIT`, `RA_TRACKING_LIMIT`, `DEC_LIMIT_UP/DOWN`, `POLARIS_RA_*` |
| [`src/app/MountSettings.cpp`](src/app/MountSettings.cpp) | 8 | `DEC_LIMIT_UP/DOWN`, `USE_GYRO_LEVEL` |
| remainder (`MenuHost`, `UiScreens`, `MountExecutorConfig/Guide/Homing/Tick/OptionalAxes`, `MountExecutor.cpp`, `SerialPump`, `EndSwitchMonitor`, `AxisBank`, `test/TestMenu*`) | 78 | mix |

The established pattern to replicate:

- [`src/core/MountConfig.hpp`](src/core/MountConfig.hpp) — plain aggregate; the motion half of
  the config (microsteps + default steps-per-degree) with one derived helper
  (`trackingMicrostepRatio()`).
- [`src/app/MountConfigBuilder.hpp`](src/app/MountConfigBuilder.hpp) — `app::buildMountConfig()`,
  the **only** place the motion macros are read.
- [`src/app/BoardPresence.hpp`](src/app/BoardPresence.hpp) — `constexpr` compile-time facts for
  presence (`kHasAzStepper`, `kHasRaEndSwitch`, `kHasMenu`, `kTrackOnBoot`, …).

---

## 2. Hard rules the design must obey

From [`scripts/check_architecture.py`](scripts/check_architecture.py) and
[`ARCHITECTURE.md` §5](ARCHITECTURE.md):

1. `src/core`, `src/app/ui` and `src/ports` contain **no** `Configuration*.hpp` feature macro
   (`AZ_STEPPER_TYPE`, `RA_DRIVER_TYPE`, `DISPLAY_TYPE`, `INFO_DISPLAY_TYPE`, `USE_HALL_SENSOR*`,
   `USE_*_END_SWITCH`, `USE_GPS`, `USE_GYRO_LEVEL`, `WIFI_ENABLED`, …), no `<Arduino.h>`, no
   `String`, no `Serial`, no `millis()`, no `LOG()`.
   ⇒ **The `core/` structs are pure values; the macro reads live in `app/` builders, never in
   `core/` or `app/ui/`.**
2. `adapters/` never includes `core/` (documented convention). Wi-Fi config is consumed only by
   [`src/adapters/WifiControl.cpp`](src/adapters/WifiControl.cpp), so it **cannot** become a
   `core/` struct handed to that adapter. It is out of scope (§5.3).
3. `[env:native]` compiles `core/`, `types/`, `ports/`, `adapters/` (minus the hardware ones) and
   `app/ui/` — **not** `app/` root. ⇒ The builders are board-build-only; host tests cover the
   `core/` structs, not the builders.
4. `-Werror` on every embedded env: structs must not leave unused-field or shadow warnings; every
   field the builder fills must have a consumer on every board (see §7).

---

## 3. Target design

### 3.1 The value structs (new, all under `src/core/`, all plain aggregates)

Field types are chosen at implementation; every struct follows `core::MountConfig`'s discipline:
filled once by an `app/` builder, aggregate-initialised, no dependence on Arduino or the config
macros.

```cpp
// src/core/SiteConfig.hpp
namespace core {
struct SiteConfig {
    float siderealSecondsPerDay;   // SIDEREAL_SECONDS_PER_DAY
    float raZeroPointShiftHours;   // RA_ZERO_POINT_SHIFT_HOURS
    bool northernHemisphere;       // NORTHERN_HEMISPHERE
    types::DayTime polarisRa;      // POLARIS_RA_HOUR / _MINUTE / _SECOND
};
}
```

```cpp
// src/core/LimitConfig.hpp
namespace core {
struct LimitConfig {
    float raLimitLeftHours;        // RA_LIMIT_LEFT
    float raLimitRightHours;       // RA_LIMIT_RIGHT
    float raTrackingLimitHours;    // RA_TRACKING_LIMIT
    float raPhysicalLimitDegrees;  // RA_PHYSICAL_LIMIT
    float decLimitUpDegrees;       // DEC_LIMIT_UP
    float decLimitDownDegrees;     // DEC_LIMIT_DOWN
};
}
```

```cpp
// src/core/GuideConfig.hpp
namespace core {
struct GuideConfig {
    float raPulseMultiplier;   // RA_PULSE_MULTIPLIER
    float decPulseMultiplier;  // DEC_PULSE_MULTIPLIER
};
}
```

```cpp
// src/core/SensorConfig.hpp
namespace core {
struct SensorConfig {
    int raHomingSensorActiveState;      // RA_HOMING_SENSOR_ACTIVE_STATE
    int decHomingSensorActiveState;     // DEC_HOMING_SENSOR_ACTIVE_STATE
    float raHomingSensorSearchDegrees;  // RA_HOMING_SENSOR_SEARCH_DEGREES
    float decHomingSensorSearchDegrees; // DEC_HOMING_SENSOR_SEARCH_DEGREES
    int raEndSwitchActiveState;         // RA_END_SWITCH_ACTIVE_STATE
    int decEndSwitchActiveState;        // DEC_END_SWITCH_ACTIVE_STATE
    float raEndSwitchBackslewDegrees;   // RA_ENDSWITCH_BACKSLEW_DEG
    float decEndSwitchBackslewDegrees;  // DEC_ENDSWITCH_BACKSLEW_DEG
};
}
```

```cpp
// src/core/AxisMechanicsConfig.hpp
namespace core {
struct AxisSpec {
    unsigned long speed;        // *_STEPPER_SPEED
    unsigned long acceleration; // *_STEPPER_ACCELERATION
    bool invertDirection;       // *_INVERT_DIR
};
struct AxisMechanicsConfig {
    AxisSpec ra, dec, az, alt, focus;
    uint8_t raPulleyTeeth;     // RA_PULLEY_TEETH
    uint8_t decPulleyTeeth;    // DEC_PULLEY_TEETH
    uint16_t raStepperSpr;     // RA_STEPPER_SPR
    uint16_t decStepperSpr;    // DEC_STEPPER_SPR
    float raTransmission;      // RA_TRANSMISSION
    float decTransmission;     // DEC_TRANSMISSION
    uint16_t azMicrosteps;     // AZ_MICROSTEPPING
    uint16_t altMicrosteps;    // ALT_MICROSTEPPING
    uint16_t focusMicrosteps;  // FOCUS_MICROSTEPPING
    float azimuthStepsPerRev;       // AZIMUTH_STEPS_PER_REV
    float azimuthStepsPerArcMinute; // AZIMUTH_STEPS_PER_ARC_MINUTE
    float altitudeStepsPerRev;      // ALTITUDE_STEPS_PER_REV
    float altitudeStepsPerArcMinute;// ALTITUDE_STEPS_PER_ARC_MINUTE
};
}
```

> `AZ_CIRCUMFERENCE` / `ALT_CIRCUMFERENCE` / `ALT_WORMGEAR_RATIO` are intermediate terms that only
> exist to derive `*_STEPS_PER_REV`; they do not need struct fields. If the `:GX#` hardware-info
> string is found to report them, a readout-only field can be added in step 4.

```cpp
// src/core/DriverConfig.hpp
namespace core {
struct AxisDriverSpec {
    int rmsCurrent;        // *_RMSCURRENT (mA)
    int stallValue;        // *_STALL_VALUE
    float holdMultiplier;  // *_MOTOR_HOLD_SETTING / 100
    bool stealthMode;      // *_UART_STEALTH_MODE
    bool audioFeedback;    // *_AUDIO_FEEDBACK
};
struct DriverConfig {
    AxisDriverSpec ra, dec, az, alt, focus;  // meaningful only for TMC2209-UART axes
    bool useVref;                   // USE_VREF
    bool uartConnectionTestTxRx;    // UART_CONNECTION_TEST_TXRX
    bool uartConnectionTestTx;      // UART_CONNECTION_TEST_TX
    float uartConnectionTestTxDeg;  // UART_CONNECTION_TEST_TX_DEG
};
}
```

```cpp
// src/core/CommsConfig.hpp
namespace core {
struct CommsConfig {
    uint32_t serialBaudrate;       // SERIAL_BAUDRATE
    uint16_t debugLevel;           // DEBUG_LEVEL
    bool debugSeparateSerial;      // DEBUG_SEPARATE_SERIAL
    uint32_t debugSerialBaudrate;  // DEBUG_SERIAL_BAUDRATE
    uint32_t gpsBaudrate;          // GPS_BAUD_RATE
};
}
```

### 3.2 The builders (new, under `src/app/`, mirroring `buildMountConfig()`)

Each builder is `inline` in a header next to [`MountConfigBuilder.hpp`](src/app/MountConfigBuilder.hpp)
so it constant-folds where the macros allow, costs no start-up work, and is the single reader of its
macro group:

- `app::buildSiteConfig()` → `core::SiteConfig`
- `app::buildLimitConfig()` → `core::LimitConfig`
- `app::buildGuideConfig()` → `core::GuideConfig`
- `app::buildSensorConfig()` → `core::SensorConfig`
- `app::buildAxisMechanicsConfig()` → `core::AxisMechanicsConfig`
- `app::buildDriverConfig()` → `core::DriverConfig`
- `app::buildCommsConfig()` → `core::CommsConfig`

One builder header is acceptable (e.g. the existing placeholder
[`src/app/ConfigBuilder.hpp`](src/app/ConfigBuilder.hpp), filled in step 2), or one header per
struct — choose the former unless it grows past ~80 lines; the builders are mechanical and belong
together.

### 3.3 Compile-time facts (extend `app::BoardPresence`)

Add `constexpr` facts that the remaining `#if` chains can read instead of the raw macros:

```cpp
constexpr bool kHasRaTmcUart    = (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART);
constexpr bool kHasDecTmcUart   = (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART);
constexpr bool kHasAzTmcUart    = (kHasAzStepper && AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART);
constexpr bool kHasAltTmcUart   = (kHasAltStepper && ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART);
constexpr bool kHasFocusTmcUart = (kHasFocusStepper && FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART);
constexpr bool kHasAnyTmcUart   = kHasRaTmcUart || kHasDecTmcUart || kHasAzTmcUart || kHasAltTmcUart || kHasFocusTmcUart;
constexpr bool kHasUartTest     = (UART_CONNECTION_TEST_TXRX == 1);
```

These do **not** remove the `#if`s that change a class's member layout
([`MountExecutor.hpp`](src/app/MountExecutor.hpp) lines 84–150) — an `#if` that adds a member must
stay an `#if` — but they let the *predicate* be written once and read as a name.

---

## 4. Migration order

Interfaces first, then the one macro reader, then the consumers, then tests — per call site so each
step leaves the tree building.

### Step 1 — `core/` structs (no behaviour change)

Add the seven headers from §3.1. Add host tests under
[`unit_tests/test_core/`](unit_tests/test_core/), reusing the empty placeholder
[`unit_tests/test_core/test_app_config.cpp`](unit_tests/test_core/test_app_config.cpp): default/
aggregate construction, the `holdMultiplier` derivation in `DriverConfig` if it becomes a helper,
and any derived accessors the structs grow. Follow
[`test_mount_config.cpp`](unit_tests/test_core/test_mount_config.cpp) as the template.

**Gate:** `pio test -e native` and `python scripts/check_architecture.py` stay green (the structs
touch nothing yet).

### Step 2 — `app/` builders

Fill [`src/app/ConfigBuilder.hpp`](src/app/ConfigBuilder.hpp) with the seven `inline` functions.
Each reads its macros verbatim; no consumer switches over yet, so this compiles on its own but is
not linked by anything — to satisfy `-Werror` (unused static functions) the header is consumed in
step 3 immediately, so land steps 2+3 in one change or guard the builders with their first
consumer.

### Step 3 — thread structs through `MountExecutor`

- Extend the `MountExecutor` constructor to take the new structs it needs (or a single
  `core::MountBuildConfig` bundle holding `MountConfig` + the new ones — decide once; the bundle is
  less churn). Pass them through to the `_readout` and `_persisted` members in the initialiser list
  exactly as `MountConfig` is passed today
  ([`MountExecutor.cpp`](src/app/MountExecutor.cpp:87)).
- Replace these reads with struct fields / `BoardPresence` facts:
  - [`MountExecutor.cpp`](src/app/MountExecutor.cpp): `NORTHERN_HEMISPHERE` (line 112) → `siteConfig.northernHemisphere`; `RA_TRACKING_LIMIT` (line 215) → `limitConfig.raTrackingLimitHours`.
  - [`MountExecutorConfig.cpp`](src/app/MountExecutorConfig.cpp): `DEC_LIMIT_DOWN/UP` (line 92) → `limitConfig.*`; `RA_INVERT_DIR` (hemisphere flip) → `mechanics.ra.invertDirection`; `USE_GYRO_LEVEL` → existing `kHasGyro`.
  - [`MountExecutorGuide.cpp`](src/app/MountExecutorGuide.cpp): `RA/DEC_PULSE_MULTIPLIER` → `guideConfig.*`; `SUPPORT_DRIFT_ALIGNMENT` → a `kHasDriftAlignment` fact.
  - [`MountExecutorSlew.cpp`](src/app/MountExecutorSlew.cpp): `RA_PHYSICAL_LIMIT`, `RA_TRACKING_LIMIT`, `DEC_LIMIT_UP/DOWN` (lines 303–306) → `limitConfig.*`; `POLARIS_RA_*` (line 437) → `siteConfig.polarisRa`.
  - [`MountExecutorHoming.cpp`](src/app/MountExecutorHoming.cpp): `USE_HALL_SENSOR_*` → existing `kHasRaHoming` / `kHasDecHoming`.
  - [`MountExecutorOptionalAxes.cpp`](src/app/MountExecutorOptionalAxes.cpp): `AZ/ALT/FOCUSER_ALWAYS_ON`, `*_STEPS_PER_ARC_MINUTE` → `mechanics.*`.
  - [`MountExecutorTick.cpp`](src/app/MountExecutorTick.cpp): `SUPPORT_DRIFT_ALIGNMENT` → `kHasDriftAlignment`; `DEBUG_LEVEL` → `commsConfig.debugLevel`.
- [`MountReadout.cpp`](src/app/MountReadout.cpp): `getStepperInfo()` driver codes
  (`#if RA_DRIVER_TYPE …`) → `if (kHasRaTmcUart) …` over the `constexpr` facts; `getMountHardwareInfo()`
  pulley teeth/stepper SPR/`USE_GPS`/presence/display strings → `mechanics.*`, `kHas*`, and a
  `constexpr int kDisplayType` / `kInfoDisplayType` (§5.2); `RA_LIMIT_LEFT/RIGHT` (lines 686–687) →
  `limitConfig.*`; `POLARIS_RA_*` (lines 587, 792) → `siteConfig.polarisRa`.
- [`MountSettings.cpp`](src/app/MountSettings.cpp): `DEC_LIMIT_UP/DOWN` (lines 229–240) →
  `limitConfig.*`; `USE_GYRO_LEVEL` → `kHasGyro`.

**Gate:** `ramps`, `esp32`, `mksgenlv21`, `rp2040_skr_pico` build; `pio test -e native` green.

### Step 4 — rewire the three wiring modules

- [`MotorWiring.cpp`](src/app/MotorWiring.cpp): `attachSteppers()` takes
  `const core::AxisMechanicsConfig &` and `const core::MountConfig &` and fills `core::AxisLimits`
  from `mechanics.<axis>.speed/acceleration`; the `#if *_INVERT_DIR` blocks become
  `if (mechanics.<axis>.invertDirection)`. The `LOG` diagnostics read `mechanics.*` instead of
  macros. `RA/DEC_STEPPER_SPEED/ACCELERATION` under `NEW_STEPPER_LIB` already use
  `config::Ra::*`/`config::Dec::*` — leave that AVR path untouched.
- [`DriverWiring.cpp`](src/app/DriverWiring.cpp): the five axis builders take
  `const core::DriverConfig &` (plus `MountConfig`/`AxisMechanicsConfig` for microsteps) and fill
  `Tmc2209Axis.rmsCurrent/holdMultiplier/stallValue/…` from `driverConfig.<axis>.*`; the
  `UART_CONNECTION_TEST_TXRX` and `USE_VREF` guards read `kHasUartTest` / `driverConfig.useVref`.
  The `Tmc2209Traits` (pdnDisable/standstillPowerDown/trimClock) are per-axis *derived* facts, not
  macro reads — keep them local.
- [`SensorWiring.cpp`](src/app/SensorWiring.cpp): `attachSensors()` takes
  `const core::SensorConfig &`; the active-state and search-degree reads come from the struct; the
  presence `#if USE_*` reads `kHasRaHoming` / `kHasDecHoming` / `kHasRaEndSwitch` / `kHasDecEndSwitch`.
- [`EndSwitchMonitor.cpp`](src/app/EndSwitchMonitor.cpp): `RA/DEC_ENDSWITCH_BACKSLEW_DEG` →
  `sensorConfig.*` (inject via its constructor); `DEBUG_LEVEL` → `commsConfig.debugLevel`.
- [`AxisBank.cpp`](src/app/AxisBank.cpp) / [`AxisBank.hpp`](src/app/AxisBank.hpp):
  `AZIMUTH_STEPS_PER_REV` / `ALTITUDE_STEPS_PER_REV` → `mechanics.*`.

**Gate:** matrix build of `ramps` (192 combinations) — this is where the optional-axis and
driver-type combinations are all exercised.

### Step 5 — rewire the boot path and composition root

- [`AppBootstrap.cpp`](src/app/AppBootstrap.cpp): `startSerial()` reads
  `commsConfig.serialBaudrate/debugLevel/debugSeparateSerial/debugSerialBaudrate`; `startGps()` reads
  `commsConfig.gpsBaudrate`; `startInfoDisplay()` keeps its `INFO_DISPLAY_TYPE` `#if`s (§5.2) but
  reads `kHasMenu`-style facts where possible.
- [`BoardWiring.cpp`](src/app/BoardWiring.cpp): build the structs once at namespace scope (they
  constant-fold) and pass them down:
  - `loggerPort(..., commsConfig.debugLevel)`; the `#if DEBUG_LEVEL != 0` guard reads
    `commsConfig.debugLevel != 0` (still a constant — the `#if` can stay or become `if`).
  - `mountCoordinator(..., siteConfig.siderealSecondsPerDay, buildMountConfig().trackingMicrostepRatio())`
    replacing `SIDEREAL_SECONDS_PER_DAY`.
  - `SiteTimeService(..., {siteConfig.raZeroPointShiftHours, siteConfig.polarisRa})`.
  - `BACKLASH_STEPS` → a field or the existing default-store call reads the builder once.
  - `STEPPER_CONTROL_INTERVAL_MS` → `commsConfig`-adjacent field (it is a motion cadence, not a
    comms value — put it in `SiteConfig` or `AxisMechanicsConfig`; decide once).
- [`SerialPump.cpp`](src/app/SerialPump.cpp): `DEBUG_SEPARATE_SERIAL` / `DEBUG_LEVEL` /
  `SUPPORT_SERIAL_CONTROL` → `commsConfig.*` + `kHasSerialControl`.
- [`main.cpp`](src/app/main.cpp) is unchanged (it already contains no config reads).

**Gate:** all six board envs build; `check_architecture.py` clean.

### Step 6 — finish the `BoardPresence` facts

Add the `constexpr` driver/display facts from §3.3; sweep the remaining raw presence tests in
[`BoardWiring.cpp`](src/app/BoardWiring.cpp), [`MenuHost.cpp`](src/app/MenuHost.cpp),
[`UiScreens.cpp`](src/app/UiScreens.cpp) and [`test/TestMenu*`](src/app/test/) onto them, leaving
only the backend-selector and object-existence `#if`s from §5.2.

**Gate:** `grep -c '#if' src/app` is at or below the §0 census baseline and no `Configuration*.hpp`
macro appears outside `BoardPresence.hpp`, the builders and the §5 exceptions.

---

## 5. What stays compile-time (and why)

### 5.1 Object-existence / header-inclusion gates — keep as `#if`

`WIFI_ENABLED` (`#include "adapters/WifiControl.hpp"` + the `wifiControl` object),
`INFO_DISPLAY_TYPE` (the `Ssd1306InfoDisplay` object), `NEW_STEPPER_LIB`, `ESP32`, `TEST_VERIFY_MODE`
and the `*_DRIVER_TYPE` gates that add `hal::IGpioPin *raEnablePin`-style members to `MountDeps` /
`MountExecutor` all **change the class layout or what links**. A runtime `if` cannot remove a member
or an object; on AVR it can even cost flash (the prior plan measured +502 B for a runtime gyro query).
They stay preprocessor, but their *predicate* can be a `constexpr` name (§3.3).

### 5.2 Backend selector — keep as `#if`, centralise the fact

`#if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD` … selects **which panel object to instantiate and which
header to include** in [`BoardWiring.cpp`](src/app/BoardWiring.cpp:160) — inherently compile-time.
Keep the `#if` chain, but expose `constexpr int kDisplayType = DISPLAY_TYPE;` /
`constexpr int kInfoDisplayType = INFO_DISPLAY_TYPE;` so readout strings can `if (kDisplayType ==
DISPLAY_TYPE_LCD_KEYPAD)` and drop their own `#if` chains in
[`MountReadout.cpp`](src/app/MountReadout.cpp) (the compiler folds the `if`).

### 5.3 Adapter-local config — out of scope (already localised)

- Wi-Fi (`WIFI_MODE`, `WIFI_HOSTNAME`, `*_SSID`, `*_WPAKEY`, `WIFI_PORT`) is read only in
  [`src/adapters/WifiControl.cpp`](src/adapters/WifiControl.cpp); `adapters/` must not include
  `core/`, so it cannot take a `core::` struct. Leave it; a future `adapters::WifiConfig` is its own
  change.
- `DEBUG_LEVEL` is also read by [`SerialLogger.hpp`](src/adapters/SerialLogger.hpp) /
  [`NullLogger.hpp`](src/adapters/NullLogger.hpp) / [`EepromSettingsFormat.cpp`](src/adapters/EepromSettingsFormat.cpp)
  and `USE_GPS` by [`MeadeCommandAdapter.cpp`](src/adapters/MeadeCommandAdapter.cpp:384) — both stay.
- [`src/hal/avr/StepperConfiguration.hpp`](src/hal/avr/StepperConfiguration.hpp) reads
  `SIDEREAL_SECONDS_PER_DAY`; that is the AVR `NEW_STEPPER_LIB` backend, out of scope.

### 5.4 Constexpr data table — keep as compile-time

[`src/app/ui/PointOfInterestTable.hpp`](src/app/ui/PointOfInterestTable.hpp) is a `constexpr` table;
its Polaris row is a compile-time substitution (`#if defined(POLARIS_RA_HOUR)`). Turning it into a
runtime-injected value would move a flash-resident table into RAM on AVR for no gain. Keep it; it is
already guarded to compile on the host (which has no board config).

### 5.5 Documentation-only constants

`DISPLAY_UPDATE_TIME` no longer has a code reader in `app/ui/` — the screens spell the 200 ms out as
literals (it survives only in comments). Leave the macro as documentation, or delete it and keep the
comments; no struct field is needed.

---

## 6. Verification

| Check | Command | What it proves |
|-------|---------|----------------|
| Architecture | `python scripts/check_architecture.py` | new `core/` structs and `app/ui/` carry no macros/Arduino |
| Host tests | `pio test -e native` | `core/` structs behave; nothing else regressed |
| Full board build | `pio run -e ramps -e mksgenlv21 -e mksgenlv2 -e mksgenlv1 -e esp32 -e oaeboardv1` (RP2040 envs if configured) | builders + consumers compile under `-Werror` on every toolchain |
| Config sweep | `python matrix_build.py -b ramps` | every optional-axis/driver/display combination still compiles and folds the `if` facts correctly |
| Format | `bash -c 'shopt -s nullglob globstar;GLOBIGNORE=./src/libs/TimerInterrupt/*; for i in ./{.,src/**,unit_tests,boards/**}/*.{c,cpp,h,hpp}; do clang-format -i $i; done'` (then `git diff --check`) | style gate |

Success = all six envs build, matrix passes, `pio test -e native` green, `check_architecture.py`
clean, and the §0 census shows each `Configuration*.hpp` macro read by `app/` in exactly one
builder or `BoardPresence` fact (with the §5 exceptions).

---

## 7. Risks and mitigations

| Risk | Mitigation |
|------|------------|
| AVR flash/RAM regressions from the new structs | Structs are aggregates filled once and constant-folded; verify size on `ramps` before/after (the prior plan's §0 did this per phase) |
| `-Werror` unused-field warnings when an axis is absent | Only pass/consume fields behind the existing `kHas*` facts; builders may need the same `#if` guards the consumers already carry |
| Builders dead-code warnings before first consumer | Land steps 2+3 as one change (§4 step 2) |
| `core/` accidentally names a macro and trips `check_architecture.py` | The check is the gate in step 1; keep all macro reads in `app/` builders |
| `adapters/` boundary (Wi-Fi) | Explicitly out of scope (§5.3) |
| POI table constexpr → RAM | Explicitly kept compile-time (§5.4) |
