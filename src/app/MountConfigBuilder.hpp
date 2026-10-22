#pragma once

#include "../Configuration.hpp"
#include "../Configuration_adv.hpp"
#include "core/MountConfig.hpp"

namespace app
{

/// Reads the board's `Configuration*.hpp` macros once, at start-up, and returns the motion
/// shape they describe.
///
/// This is the only place the microstep and steps-per-degree macros are read for motion
/// purposes. `Mount` takes the result and never looks at them again, which is what makes the
/// values runtime data.
///
/// Constant-folded wherever the macros allow it: every field is a literal or a product of
/// literals, so the result costs no start-up work.
inline core::MountConfig buildMountConfig()
{
    core::MountConfig config;

    config.raSlewMicrosteps   = RA_SLEW_MICROSTEPPING;
    config.raTrackMicrosteps  = RA_TRACKING_MICROSTEPPING;
    config.decSlewMicrosteps  = DEC_SLEW_MICROSTEPPING;
    config.decGuideMicrosteps = DEC_GUIDE_MICROSTEPPING;

    config.defaultRaStepsPerDegree  = RA_STEPS_PER_DEGREE;
    config.defaultDecStepsPerDegree = DEC_STEPS_PER_DEGREE;

    return config;
}

}  // namespace app
