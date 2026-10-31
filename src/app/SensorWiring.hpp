#pragma once

#include "../../Configuration.hpp"
#include "MountExecutor.hpp"

namespace app
{
/// Builds the end switches and hall sensors this board has, and hands them to the mount.
///
/// Which pins exist, and whether a switch reads high or low, is board knowledge — the same
/// reason the axes and the drivers are built here. The devices are built once: the homing state
/// machine re-starts itself on every run, so it does not need a fresh object per attempt.
void attachSensors(app::MountExecutor &mount);

/// Puts the hall sensors' pins into input mode before `attachSensors()` builds the devices that
/// read them. Empty on a board without either homing sensor.
void initHomingSensorPins();

}  // namespace app
