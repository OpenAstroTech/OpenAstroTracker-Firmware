#pragma once

#include "../inc/Globals.hpp"
#include "../../Configuration.hpp"
#include "../Mount.hpp"

namespace app
{

/// Builds every axis engine this board has and hands them to the mount.
///
/// Engine construction is the last place the platform `#ifdef`s live: on AVR the interrupt
/// steppers are created through `hal/avr/`, everywhere else through `hal::AccelStepperMotor`.
/// That is the composition root's job, so it lives in `app/` rather than in `Mount`.
void attachSteppers(Mount &mount);

}  // namespace app
