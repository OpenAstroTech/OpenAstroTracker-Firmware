# app/

Per-board composition root. The only place that:
- reads `Configuration*.hpp` macros and builds a runtime `MountConfig`,
- selects HAL backends,
- constructs adapters and wires them into [`../core/`](../core/) controllers,
- owns the program entry point (eventually replacing the legacy `.ino`).

Feature `#ifdef`s survive here and in [`../hal/`](../hal/) backend selection only.

See [specs/plan.md](../../specs/plan.md) for the target architecture.
