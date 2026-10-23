#pragma once

#include <stdint.h>

#include "hal/WarningGuard.hpp"

// `StepperConfiguration.hpp` names the vendored engine types (`config::Ra::stepper_slew`, …),
// which are the only place the AVR interrupt-stepper engines are known. They stay private to this
// directory: the functions below return `IStepperMotor *`, so no caller above `hal/` ever names an
// AVR engine type.
PUSH_NO_WARNINGS
    #include "StepperConfiguration.hpp"
POP_NO_WARNINGS

#include "InterruptStepperMotor.hpp"

namespace hal
{
namespace avr
{

/// The engine (`InterruptAccelStepper<Engine>`) is constructed with the same arguments the old
/// `Mount` call sites passed; the interrupt engine's constructor ignores them beyond `init()`, so
/// the interface/pin arguments are kept for symmetry with `hal::AccelStepperMotor`.
inline IStepperMotor *makeRaSlewMotor(uint8_t interfaceType, uint8_t pin1, uint8_t pin2)
{
    using Engine = InterruptAccelStepper<config::Ra::stepper_slew>;
    return new InterruptStepperMotor<Engine>(*new Engine(interfaceType, pin1, pin2));
}

inline IStepperMotor *makeRaTrkMotor(uint8_t interfaceType, uint8_t pin1, uint8_t pin2)
{
    using Engine = InterruptAccelStepper<config::Ra::stepper_trk>;
    return new InterruptStepperMotor<Engine>(*new Engine(interfaceType, pin1, pin2));
}

inline IStepperMotor *makeDecSlewMotor(uint8_t interfaceType, uint8_t pin1, uint8_t pin2)
{
    using Engine = InterruptAccelStepper<config::Dec::stepper_slew>;
    return new InterruptStepperMotor<Engine>(*new Engine(interfaceType, pin1, pin2));
}

inline IStepperMotor *makeDecTrkMotor(uint8_t interfaceType, uint8_t pin1, uint8_t pin2)
{
    using Engine = InterruptAccelStepper<config::Dec::stepper_trk>;
    return new InterruptStepperMotor<Engine>(*new Engine(interfaceType, pin1, pin2));
}

    #if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
inline IStepperMotor *makeAzSlewMotor(uint8_t interfaceType, uint8_t pin1, uint8_t pin2)
{
    using Engine = InterruptAccelStepper<config::Az::stepper_slew>;
    return new InterruptStepperMotor<Engine>(*new Engine(interfaceType, pin1, pin2));
}
    #endif

    #if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
inline IStepperMotor *makeAltSlewMotor(uint8_t interfaceType, uint8_t pin1, uint8_t pin2)
{
    using Engine = InterruptAccelStepper<config::Alt::stepper_slew>;
    return new InterruptStepperMotor<Engine>(*new Engine(interfaceType, pin1, pin2));
}
    #endif

    #if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
inline IStepperMotor *makeFocusSlewMotor(uint8_t interfaceType, uint8_t pin1, uint8_t pin2)
{
    using Engine = InterruptAccelStepper<config::Focus::stepper_slew>;
    return new InterruptStepperMotor<Engine>(*new Engine(interfaceType, pin1, pin2));
}
    #endif

}  // namespace avr
}  // namespace hal
