#include "StepperMotorFactory.hpp"

#include "../../Configuration.hpp"
#include "../Utility.hpp"

#ifdef ARDUINO_AVR_ATmega2560
    #include "avr/InterruptStepperFactory.hpp"

/// `AccelStepper::DRIVER`, the interface style the engines were always built with. The interrupt
/// engine ignores it beyond `init()`, so it is named once here rather than at every call site.
namespace
{
constexpr uint8_t kDriverInterface = 1;
}
#else
    #include "arduino/AccelStepperMotor.hpp"
#endif

namespace hal
{

IStepperMotor *makeAxisMotor(AxisRole role, uint8_t pin1, uint8_t pin2)
{
#ifdef ARDUINO_AVR_ATmega2560
    switch (role)
    {
        case AxisRole::RaSlew:
            return avr::makeRaSlewMotor(kDriverInterface, pin1, pin2);
        case AxisRole::RaTracking:
            return avr::makeRaTrkMotor(kDriverInterface, pin1, pin2);
        case AxisRole::DecSlew:
            return avr::makeDecSlewMotor(kDriverInterface, pin1, pin2);
        case AxisRole::DecGuide:
            return avr::makeDecTrkMotor(kDriverInterface, pin1, pin2);
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        case AxisRole::Az:
            return avr::makeAzSlewMotor(kDriverInterface, pin1, pin2);
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        case AxisRole::Alt:
            return avr::makeAltSlewMotor(kDriverInterface, pin1, pin2);
    #endif
    #if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
        case AxisRole::Focus:
            return avr::makeFocusSlewMotor(kDriverInterface, pin1, pin2);
    #endif
    }
    return nullptr;
#else
    (void) role;
    return new AccelStepperMotor(*new AccelStepper(AccelStepper::DRIVER, pin1, pin2));
#endif
}

}  // namespace hal
