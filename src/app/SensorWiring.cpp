#include "SensorWiring.hpp"

#include "BoardWiring.hpp"

#include "../Utility.hpp"
#include "../EndSwitches.hpp"
#include "../HallSensorHoming.hpp"
#include "../adapters/GpioEndSwitch.hpp"
#include "../adapters/HallHomingSensor.hpp"
#include "../adapters/StatusFlags.hpp"
#include "../hal/arduino/ArduinoGpioPin.hpp"

namespace
{
#if (USE_RA_END_SWITCH == 1) || (USE_DEC_END_SWITCH == 1) || (USE_HALL_SENSOR_RA_AUTOHOME == 1) || (USE_HALL_SENSOR_DEC_AUTOHOME == 1)
/// The status bits the homing state machine writes. The coordinator owns the bitfield, so the
/// homing code no longer needs the mount object for it.
adapters::StatusFlags statusFlags(mountCoordinator.state());

hal::PinLevel toPinLevel(int activeState)
{
    return (activeState == LOW) ? hal::PinLevel::Low : hal::PinLevel::High;
}
#endif

#if (USE_RA_END_SWITCH == 1) || (USE_DEC_END_SWITCH == 1)
/// One input pin as a domain switch. The pin object lives for the lifetime of the program;
/// polarity is the adapter's business.
ports::IEndSwitch &makeEndSwitch(uint8_t pin, int activeState)
{
    hal::ArduinoGpioPin *gpio = new hal::ArduinoGpioPin(pin);
    gpio->configure(hal::PinMode::InputPullup);
    return *new adapters::GpioEndSwitch(*gpio, toPinLevel(activeState));
}
#endif

#if (USE_HALL_SENSOR_RA_AUTOHOME == 1) || (USE_HALL_SENSOR_DEC_AUTOHOME == 1)
ports::IHomingSensor &makeHomingSensor(uint8_t pin, int activeState)
{
    hal::ArduinoGpioPin *gpio = new hal::ArduinoGpioPin(pin);
    gpio->configure(hal::PinMode::InputPullup);
    return *new adapters::HallHomingSensor(*gpio, toPinLevel(activeState));
}
#endif
}  // namespace

namespace app
{

void attachSensors(Mount &mount)
{
    Mount::Sensors sensors;

#if USE_RA_END_SWITCH == 1
    sensors.raEndSwitch = new EndSwitch(ui,
                                        StepperAxis::RA_STEPS,
                                        makeEndSwitch(RA_ENDSWITCH_EAST_SENSOR_PIN, RA_END_SWITCH_ACTIVE_STATE),
                                        makeEndSwitch(RA_ENDSWITCH_WEST_SENSOR_PIN, RA_END_SWITCH_ACTIVE_STATE));
#endif

#if USE_DEC_END_SWITCH == 1
    sensors.decEndSwitch = new EndSwitch(ui,
                                         StepperAxis::DEC_STEPS,
                                         makeEndSwitch(DEC_ENDSWITCH_DOWN_SENSOR_PIN, DEC_END_SWITCH_ACTIVE_STATE),
                                         makeEndSwitch(DEC_ENDSWITCH_UP_SENSOR_PIN, DEC_END_SWITCH_ACTIVE_STATE));
#endif

#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    sensors.raHoming = new HallSensorHoming(
        ui, statusFlags, StepperAxis::RA_STEPS, makeHomingSensor(RA_HOMING_SENSOR_PIN, RA_HOMING_SENSOR_ACTIVE_STATE));
#endif

#if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    sensors.decHoming = new HallSensorHoming(
        ui, statusFlags, StepperAxis::DEC_STEPS, makeHomingSensor(DEC_HOMING_SENSOR_PIN, DEC_HOMING_SENSOR_ACTIVE_STATE));
#endif

    mount.attachSensors(sensors);
}

}  // namespace app
