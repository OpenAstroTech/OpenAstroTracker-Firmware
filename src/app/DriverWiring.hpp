#pragma once

#include "../inc/Globals.hpp"
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

}  // namespace app
