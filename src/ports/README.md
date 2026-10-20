# ports/

Domain-level interfaces consumed by [`../core/`](../core/): what the domain *needs*
(axis position, persistent value, "now", a log sink, a transport, …).

Pure C++ interfaces only. No Arduino, no hardware libraries, no `#ifdef` feature flags.
Adapters in [`../adapters/`](../adapters/) implement these ports on top of [`../hal/`](../hal/).

## Catalogue

| Header | What the domain needs |
|--------|----------------------|
| `IClock.hpp` | "now" in milliseconds, plus a blocking delay |
| `ILogger.hpp` | a log sink taking the firmware's `DEBUG_*` bit-mask |
| `LogFormat.hpp` | the opaque, PROGMEM-safe format handle that `ILogger` takes |
| `IPersistentStore.hpp` | durable bytes at an address |
| `IStepperAxis.hpp` | one stepper axis; `AxisSnapshot` is the ISR-safe read |
| `IMotorDriver.hpp` | driver current/chopper/stall-guard configuration |
| `IHomingSensor.hpp` | "the magnet is here" |
| `IEndSwitch.hpp` | "the carriage hit this end" |
| `IDisplay.hpp` | the menu's text grid and backlight |
| `IInfoDisplay.hpp` | the status panel, fed an `InfoDisplayState` snapshot |
| `ITransport.hpp` | line-oriented command input and reply output |
| `IGps.hpp` | the receiver's `GpsFix` |
| `IGyro.hpp` | the base's `GyroAngles` |

## Two contracts worth knowing

- **`IStepperAxis` threading.** `snapshot()` is the only method an ISR may call; every
  other method is main-loop-only. Timers are shared between axes, so ownership is
  granted by the coordinator rather than assumed by an axis.
- **`ILogger` and flash.** Format strings reach the logger as an opaque `LogFormat`
  built with `OAT_LOG_FORMAT()`, which is `PSTR()` on AVR. That single platform-specific
  line in `LogFormat.hpp` is what keeps the firmware's 273 format literals (13.9 KB) out
  of the ATmega2560's 8 KB of SRAM.

See [PLAN.md](../../PLAN.md) for the target architecture.
