# hal/

Hardware Abstraction Layer: pure C++ interfaces describing *what the hardware can do*
(pin toggles, UART bytes, timer ticks, EEPROM cells, OLED panels, …) plus one backend
per platform.

## Interfaces

| Header | Capability |
|--------|-----------|
| `IGpioPin.hpp` | one digital/analog/PWM pin |
| `ISerialPort.hpp` | an open byte stream (UART, USB CDC, software serial) |
| `ISpiBus.hpp` | SPI master bus (contract only — no backend yet) |
| `II2cBus.hpp` | I2C master bus (contract only — no backend yet) |
| `IEeprom.hpp` | non-volatile byte storage |
| `IStepperMotor.hpp` | the motion engine of one stepper motor (not step/dir pulses) |
| `ITmcDriver.hpp` | TMC driver register access over UART |
| `IOledPanel.hpp` | monochrome pixel panel |
| `ICharLcd.hpp` | character LCD, raw or behind an I2C expander |
| `IButtonMatrix.hpp` | the five navigation keys |
| `ITimerService.hpp` | periodic/one-shot callbacks from one hardware timer |
| `ISystemClock.hpp` | `millis`/`micros`/`delay` |
| `IWifiStack.hpp` | network stack plus TCP/UDP socket factories |

Two more headers are not capabilities:

- `EepromLayout.hpp` — the on-device storage format (magic markers, addresses, value
  encodings) shared with the EEPROM adapter. Host-tested in `unit_tests/test_hal/`.
- `WarningGuard.hpp` — fallbacks for the warning pragmas that PlatformIO force-includes,
  so backends can include third-party headers without tripping `-Werror`.

## Backends

Each directory compiles only for its platform, enforced by per-env
`build_src_filter` in `platformio.ini`.

| Directory | Contents |
|-----------|----------|
| `arduino/` | Generic Arduino backends: `ArduinoSystemClock`, `ArduinoGpioPin`, `ArduinoSerialPort` (wraps any `Stream`), `ArduinoEeprom`, `AccelStepperMotor`. Compiles on every platform; the stepper backend is used by ESP32 and RP2040 only |
| `avr/` | `InterruptStepperMotor.hpp` — a header-only template over the timer-ISR engine in [`../StepperConfiguration.hpp`](../StepperConfiguration.hpp), parameterised by `config::<Axis>::stepper_*` |
| `esp32/` | `Esp32TimerService` (over `esp_timer`), `Esp32WifiStack` (over `WiFi`, `WiFiServer`, `WiFiUDP`) |
| `rp2040/` | `Rp2040TimerService` (over `add_repeating_timer_us` / `add_alarm_in_us`) |

AVR has no `ITimerService` backend on purpose: its timers are owned by the interrupt
stepper and by `IntervalInterrupt`, so timer arbitration is a Phase 3 concern.

## Rules

Feature `#ifdef`s are allowed only here, for platform/backend selection.
Backends may include hardware libraries; interface headers may not.
HAL knows about hardware capabilities and platform backends, never about go-to policy,
RA/DEC semantics, or domain state machines.

See [PLAN.md](../../PLAN.md) for the target architecture.
