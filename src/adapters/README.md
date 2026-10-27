# adapters/

Thin glue binding domain [`../ports/`](../ports/) to [`../hal/`](../hal/) backends
(and to non-HAL libraries such as TMCStepper and TinyGPS++).

| Adapter | Port | Backed by |
|---------|------|-----------|
| `ArduinoClock` | `IClock` | `hal::ISystemClock` |
| `ArduinoScheduler` | `IScheduler` | the Arduino `yield()` |
| `SerialLogger` | `ILogger` | `hal::ISerialPort` + `hal::ISystemClock` |
| `NullLogger` | `ILogger` | nothing — the null object for `DEBUG_NONE` builds |
| `EepromPersistentStore` | `IPersistentStore` | `hal::IEeprom` |
| `MountSettingsStore` | `IMountSettings` | the legacy `EEPROMStore` facade |
| `StepperAxis` | `IStepperAxis` | `hal::IStepperMotor` |
| `Tmc2209Driver` | `IMotorDriver` | `TMC2209Stepper` (UART or software serial) |
| `NullMotorDriver` | `IMotorDriver` | nothing — the null object for non-UART drivers |
| `TinyGpsAdapter` | `IGps` | `TinyGPSPlus` + a `Stream` |
| `MeadeCommandAdapter` | `adapters::meade::IMeadeHandlers` | the parser in [`../adapters/meade/`](../adapters/meade/) |
| `SerialTransport<Capacity>` | `ITransport` | `hal::ISerialPort` + `adapters::meade::CommandAssembler` |

Four of them (`Tmc2209Driver`, `TinyGpsAdapter`, `ArduinoScheduler`, `MeadeCommandAdapter`)
need Arduino or a hardware library, so the `native` env excludes them from its
`build_src_filter` and they are compile-verified by the board builds instead. Everything else is host-tested
in `unit_tests/test_adapters/` against the fakes in
[`../../unit_tests/test_common/hal_fakes/`](../../unit_tests/test_common/hal_fakes/).

`MountSettingsStore` is the exception: it is header-only and forwards every method to
the legacy `EEPROMStore` facade, which still owns the on-device encoding. That makes it
the only adapter that names a `src/` root class, and it has nothing of its own to test.
It disappears when the remaining `EEPROMStore` consumers move in Phase 5.

Still missing on purpose:

- `HallHomingSensor`, `GpioEndSwitch`, `LcdMenuDisplay`, `Ssd1306InfoDisplay` — they
  call `Mount` today, so they would violate the "adapters depend on ports + HAL only"
  rule. They arrive with Phase 3's `HomingController`/`EventBus` read model.
- There is no `WifiTransport` and does not need to be: the Wi-Fi TCP path is
  `SerialTransport<128>` over a `hal::ArduinoSerialPort` wrapped around the `WiFiClient`, so
  both transports share one tested implementation instead of two copies of the framing. The
  UDP discovery responder stays in [`../WifiControl.cpp`](../WifiControl.cpp) — it answers a
  whole datagram, which is not a byte stream.
- `MeadeCommandAdapter` still takes a `Mount*` and reads `String`, so it is not reachable from
  `native`; the transports are, because they only touch `ITransport` and the HAL.
- `StepperAxis` is one adapter for every platform: the AVR difference (`run()`/
  `runSpeed()` are stubs, the engine steps from its own timer ISR) lives in the engine,
  not here. There is deliberately no `AccelStepperAxis` and no
  `InterruptAccelStepperAxis`.

No domain logic lives here — adapters translate, they don't decide.

See [PLAN.md](../../PLAN.md) for the target architecture.
