# adapters/

Thin glue binding domain [`../ports/`](../ports/) to [`../hal/`](../hal/) backends
(and to non-HAL libraries such as TinyGPS data structs).

Examples (planned): `AccelStepperAxis`, `Tmc2209Driver`, `EepromPersistentStore`,
`SerialLogger`, `LcdMenuDisplay`, `Ssd1306InfoDisplay`, `SerialTransport`,
`WifiTransport`, `TinyGpsAdapter`.

No domain logic lives here — adapters translate, they don't decide.

See [specs/plan.md](../../specs/plan.md) for the target architecture.
