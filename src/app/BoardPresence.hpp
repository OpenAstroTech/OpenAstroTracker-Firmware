#pragma once

#include "../../Configuration.hpp"

namespace app
{

/// What this board has, as compile-time facts.
///
/// The compile-time half of the `kHasX && ptr != nullptr` test every optional axis, switch and
/// sensor uses: a board that cannot have the part still pays for the pointer (2 B on AVR) but not
/// for the code behind it, because the constant lets the compiler drop the branch entirely.
///
/// Free constants rather than members of one class, because three classes ask the same questions —
/// the executor, the step pump and the axis bank. One definition is the only way they cannot
/// disagree, and `constexpr` at namespace scope is internal linkage, so each translation unit gets
/// its own copy and there is nothing to link.
constexpr bool kHasAzStepper    = (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE);
constexpr bool kHasAltStepper   = (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE);
constexpr bool kHasFocusStepper = (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE);
constexpr bool kHasRaEndSwitch  = (USE_RA_END_SWITCH == 1);
constexpr bool kHasDecEndSwitch = (USE_DEC_END_SWITCH == 1);
constexpr bool kHasRaHoming     = (USE_HALL_SENSOR_RA_AUTOHOME == 1);
constexpr bool kHasDecHoming    = (USE_HALL_SENSOR_DEC_AUTOHOME == 1);

}  // namespace app
