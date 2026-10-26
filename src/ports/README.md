# ports/

Domain-level interfaces consumed by [`../core/`](../core/): what the domain *needs*
(axis position, persistent value, "now", a log sink, a transport, …).

Pure C++ interfaces only. No Arduino, no hardware libraries, no `#ifdef` feature flags.
A port may name a value type from [`../core/types/`](../core/types/) when that type is the
vocabulary it speaks (`IMountSettings` and `core::DayTime`); nothing else from `core/` is allowed.
Adapters in [`../adapters/`](../adapters/) implement these ports on top of [`../hal/`](../hal/).

## Catalogue

| Header | What the domain needs |
|--------|----------------------|
| `IClock.hpp` | "now" in milliseconds, plus a blocking delay |
| `IScheduler.hpp` | handing control back to the platform while waiting |
| `ILogger.hpp` | a log sink taking the firmware's `DEBUG_*` bit-mask |
| `LogFormat.hpp` | the opaque, PROGMEM-safe format handle that `ILogger` takes |
| `IPersistentStore.hpp` | durable bytes at an address |
| `IMountSettings.hpp` | the persisted settings as typed values (HA time, steps/degree, latitude, limits, …) |
| `IStepperAxis.hpp` | one stepper axis; `AxisSnapshot` is the ISR-safe read |
| `IMotorDriver.hpp` | driver current/chopper/stall-guard configuration |
| `IMountMotion.hpp` | the motion requests the command layer makes (slew, stop, park, guide, move-by) |
| `IHomingSensor.hpp` | "the magnet is here" |
| `IEndSwitch.hpp` | "the carriage hit this end" |
| `IDisplay.hpp` | the menu's text grid and backlight |
| `IInfoDisplay.hpp` | the status panel, fed an `InfoDisplayState` snapshot |
| `ITransport.hpp` | line-oriented command input and reply output |
| `IGps.hpp` | the receiver's `GpsFix` |
| `IGyro.hpp` | the base's `GyroAngles` |

## Two contracts worth knowing

- **`IStepperAxis` threading.** `snapshot()` is the only method an ISR may call; every
  other method is main-loop-only. How steps are generated is the backend's business — a
  timer ISR, a task, a polled engine, or a driver that generates them itself — so no
  controller may assume a timer. What the coordinator arbitrates is the *actuator*: RA
  and DEC each carry two logical roles, so a role holds the axis to the exclusion of the
  other.
- **`ILogger` and flash.** Format strings reach the logger as an opaque `LogFormat`
  built with `OAT_LOG_FORMAT()`, which is `PSTR()` on AVR. That single platform-specific
  line in `LogFormat.hpp` is what keeps the firmware's 273 format literals (13.9 KB) out
  of the ATmega2560's 8 KB of SRAM. Call sites use the `OAT_LOG(logger, level, format, …)`
  macro in [`ILogger.hpp`](ILogger.hpp), which also compiles the whole call out when
  `DEBUG_LEVEL` is `DEBUG_NONE`.

See [PLAN.md](../../PLAN.md) for the target architecture.
