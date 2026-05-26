# app/

Per-board composition root. The only place that:
- reads `Configuration*.hpp` macros and builds a runtime `MountConfig`,
- selects HAL backends,
- constructs adapters and wires them into [`../core/`](../core/) controllers,
- owns the program entry point (eventually replacing the legacy `.ino`).

Feature `#ifdef`s survive here and in [`../hal/`](../hal/) backend selection only.

Note: the snippets below are illustrative only. They demonstrate layer responsibilities and dependency direction, not the final refactored code shape; actual code can and will differ.

Minimal go-to example:

```cpp
std::unique_ptr<hal::IStepperMotor> raMotor = makeRaStepperMotor(config);
std::unique_ptr<hal::IStepperMotor> decMotor = makeDecStepperMotor(config);

RaAxis raAxis(*raMotor, config.raStepsPerArcSecondX1000);
DecAxis decAxis(*decMotor, config.decStepsPerArcSecondX1000);

GoToController goToController(raAxis, decAxis);

MeadeCommandAdapter meade(goToController);
```

Responsibility: choose concrete implementations and wire the go-to slice together for the selected board.
Dependency rule: app may depend on every inner layer because it is the composition root; the inner layers never depend back on app.

See [specs/plan.md](../../specs/plan.md) for the target architecture.
