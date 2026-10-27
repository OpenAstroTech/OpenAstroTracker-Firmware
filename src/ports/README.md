# ports/

The boundary between the firmware's policy and its hardware: the interfaces that
[`../app/`](../app/) and [`../core/`](../core/) depend on, expressed as what they
*need* — an axis position, a persistent value, "now", a log sink, a transport, …
Adapters in [`../adapters/`](../adapters/) implement these ports on top of [`../hal/`](../hal/).

Pure C++ interfaces only. No Arduino, no hardware libraries, no `#ifdef` feature flags.
A port may name a value type from [`../types/`](../types/) when that type is the
vocabulary it speaks (`IMountSettings` and `types::DayTime`); nothing else from `core/` is allowed.

## Catalogue

| Header | What the domain needs |
|--------|----------------------|
| `IClock.hpp` | "now" in milliseconds, plus a blocking delay |
| `IScheduler.hpp` | handing control back to the platform while waiting |
| `ILogger.hpp` | a log sink taking the firmware's `DEBUG_*` bit-mask |
| `LogFormat.hpp` | the opaque, PROGMEM-safe format handle that `ILogger` takes |
| `ILogRetention.hpp` | the retained log tail a host reads back over `:D#` |
| `IPersistentStore.hpp` | durable bytes at an address |
| `IMountSettings.hpp` | the persisted settings as typed values (HA time, steps/degree, latitude, limits, …) |
| `IStepperAxis.hpp` | one stepper axis; `AxisSnapshot` is the ISR-safe read |
| `IMotorDriver.hpp` | driver current/chopper/stall-guard configuration |
| `IMountMotion.hpp` | the motion requests the command layer makes (slew, stop, park, guide, move-by) |
| `IMountReadModel.hpp` | what the command layer reads back (positions, targets, status strings) |
| `IMountConfiguration.hpp` | the adjustable, persisted values a host or menu calibrates |
| `IMountOptionalAxes.hpp` | the focuser and the AZ/ALT pair, present or not |
| `IMountSiteTime.hpp` | the site and the clock every coordinate is derived from |
| `IHomingSensor.hpp` | "the magnet is here" |
| `IEndSwitch.hpp` | "the carriage hit this end" |
| `IDisplay.hpp` | the menu's text grid and backlight |
| `IButtons.hpp` | the navigation keys, debounced and edge-reported |
| `IInfoDisplay.hpp` | the status panel, fed an `InfoDisplayState` snapshot |
| `ITransport.hpp` | line-oriented command input and reply output |
| `IGps.hpp` | the receiver's `GpsFix` |
| `IGyro.hpp` | the base's `GyroAngles` |
| `IMountEvents.hpp` | subscribe/unsubscribe to the mount's event stream |
| `IEventSink.hpp` | anything that wants to observe `types::MountEvent`s (the subscription contract) |
| `PortVocabulary.hpp` | the words the ports are written in: direction bits, coordinate-format selectors, `StepperAxis`, `LocalDate` |

## Two contracts worth knowing

- **`IStepperAxis` threading.** `snapshot()` is the only method an ISR may call; every
  other method is main-loop-only. How steps are generated is the backend's business — a
  timer ISR, a task, a polled engine, or a driver that generates them itself — so no
  controller may assume a timer. What the coordinator arbitrates is the *actuator*: RA
  and DEC each carry two logical roles, and each role holds the actuator to the exclusion
  of the other.
- **`ILogger` and flash.** Format strings reach the logger as an opaque `LogFormat`
  built with `OAT_LOG_FORMAT()`, which is `PSTR()` on AVR. That single platform-specific
  line in `LogFormat.hpp` is what keeps the firmware's several hundred format literals
  (well over 10 KB) out of the ATmega2560's 8 KB of SRAM. Call sites use the
  `OAT_LOG(logger, level, format, …)` macro in [`ILogger.hpp`](ILogger.hpp), which also
  compiles the whole call out when `DEBUG_LEVEL` is `DEBUG_NONE`.

See [ARCHITECTURE.md](../../ARCHITECTURE.md) for how the layers fit together.
