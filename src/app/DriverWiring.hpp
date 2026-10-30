#pragma once

#include "../../Configuration.hpp"
#include "MountExecutor.hpp"

namespace app
{
/// Builds this board's TMC2209 UART drivers and programs their registers.
///
/// Driver registers depend on board macros (currents, stall thresholds, stealth mode, microstep
/// seeding), so this is composition-root knowledge, not domain behaviour. The executor receives the
/// finished drivers through `attachDrivers()` and only ever talks to them through
/// `ports::IMotorDriver`.
void attachDrivers(app::MountExecutor &mount);

/// The axes whose enable, microstep-select, DIAG and hardware-UART pins `setup()` brings up.
/// Named after the axis, not its abbreviation: `DEC` is a macro in Arduino's `Print.h`.
enum class DriverAxis
{
    RightAscension,
    Declination,
    Azimuth,
    Altitude,
    Focuser,
};

/// Brings up one axis's driver pins: the enable line in its idle state, the MS0/MS1/MS2 select
/// lines for the standalone drivers that have them, the DIAG input, and the TMC2209 UART at the
/// baud that platform wants.
///
/// This is board knowledge, so it lives here with the pin maps. The axis name is a value rather
/// than a template parameter because every board-macro decision below is a compile-time `#if`
/// already: the switch picks which set of those was compiled in.
void initDriverPins(DriverAxis axis);

}  // namespace app
