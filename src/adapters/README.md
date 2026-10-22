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
| `MeadeCommandAdapter` | `oat::core::meade::IMeadeHandlers` | the parser in [`../core/meade/`](../core/meade/) |
| `SerialTransport` | `ITransport` | `hal::ISerialPort` + `core::meade::CommandAssembler` |

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
- `WifiTransport` — Phase 5. `SerialTransport` is here and host-tested; the Wi-Fi path still
  frames commands inline in [`../WifiControl.cpp`](../WifiControl.cpp) over `WiFiClient`, and
  moving it needs the same `CommandAssembler` plus a decision about the UDP side.
- `MeadeCommandAdapter` still takes a `Mount*` and reads `String`, so it is not reachable from
  `native`; the transports are, because they only touch `ITransport` and the HAL.
- `StepperAxis` is one adapter for every platform: the AVR difference (`run()`/
  `runSpeed()` are stubs, the engine steps from its own timer ISR) lives in the engine,
  not here. There is deliberately no `AccelStepperAxis` and no
  `InterruptAccelStepperAxis`.

No domain logic lives here — adapters translate, they don't decide.

See [PLAN.md](../../PLAN.md) for the target architecture.
