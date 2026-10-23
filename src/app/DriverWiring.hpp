#pragma once

#include "../inc/Globals.hpp"
#include "../../Configuration.hpp"
#include "../Mount.hpp"

namespace app
{

/// Builds this board's TMC2209 UART drivers and programs their registers.
///
/// Driver registers depend on board macros (currents, stall thresholds, stealth mode, microstep
/// seeding), so this is composition-root knowledge, not domain behaviour. `Mount` receives the
/// finished drivers through `attachDrivers()` and only ever talks to them through
/// `ports::IMotorDriver`.
void attachDrivers(Mount &mount);

}  // namespace app
