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
| `InMemoryEeprom.hpp` | a RAM-backed `IEeprom` (for `USE_DUMMY_EEPROM` builds) |

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
| `arduino/` | Generic Arduino backends: `ArduinoSystemClock`, `ArduinoGpioPin`, `ArduinoSerialPort` (wraps any `Stream`), `ArduinoEeprom`, `AccelStepperMotor`. Compiles on every platform; the stepper backend is used by ESP32 and RP2040 |
| `esp32/` | `Esp32TimerService` (over `esp_timer`), `Esp32WifiStack` (over `WiFi`, `WiFiServer`, `WiFiUDP`) |
| `rp2040/` | `Rp2040TimerService` (over `add_repeating_timer_us` / `add_alarm_in_us`) |
| `avr/` | `InterruptStepperMotor` (engine → `IStepperMotor`, used for the runtime-optional AZ/ALT/Focus axes), the vendored `InterruptAccelStepper` engine, and `TimerIsrVectors.cpp` (the TIMER1/3/4/5 vectors the engines own) |

`InterruptCallback` (the pre-`NEW_STEPPER_LIB` timer abstraction, one branch per platform)
also lives here rather than in `src/`: it is backend selection in one file, which is what
this layer is for.

The AVR backend is deliberately thin, for two measured reasons:

- **Only the optional axes are wrapped in `IStepperMotor`.** A polymorphic wrapper per engine
  costs ~2.6 KB of flash because its methods can no longer inline, so RA/DEC/TRK/GUIDE — always
  present, and the hot stepping path — keep their concrete engines and `Mount` holds them
  directly. The three runtime-optional axes need a type that exists on every board, so they do
  go through [`avr/InterruptStepperMotor.hpp`](avr/InterruptStepperMotor.hpp); on AVR the step
  generator is the engine's own ISR, so no virtual call reaches it.
- **No `ITimerService` backend.** AVR timers are owned by the interrupt stepper and by
  `IntervalInterrupt`, so there is nothing to arbitrate.

## Rules

Feature `#ifdef`s are allowed only here, for platform/backend selection.
Backends may include hardware libraries; interface headers may not.
HAL knows about hardware capabilities and platform backends, never about go-to policy,
RA/DEC semantics, or domain state machines.

See [PLAN.md](../../PLAN.md) for the target architecture.
