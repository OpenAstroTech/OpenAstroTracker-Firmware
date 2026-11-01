#pragma once

namespace core
{
/// The driver-side decisions a build makes that are *not* register values.
///
/// The `*AlwaysOn` flags are the power policy: a board that wants a coil energised while idle keeps
/// the enable pin asserted instead of releasing it when the axis stops. `uartConnectionTestTxDeg` is
/// the sweep the bring-up screen uses for the VACTUAL self-test.
///
/// Deliberately **not** here: the per-axis TMC2209 register values (run current, SGTHRS, hold
/// multiplier, stealthChop, microstep count). Those stay in `DriverWiring.cpp`, which reads each of
/// its `Configuration*.hpp` macros exactly once per axis. Routing them through a struct was
/// measured on `ramps` and costs ~1 kB of flash: the values feed a non-inlined register-programming
/// sequence, so the struct is materialised instead of constant-folded. See PLAN.md (Phase 11).
struct DriverConfig {
    bool azAlwaysOn    = false;
    bool altAlwaysOn   = false;
    bool focusAlwaysOn = false;

    float uartConnectionTestTxDeg = 5.0f;
};

}  // namespace core
