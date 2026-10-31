#include "SensorWiring.hpp"

#include "BoardWiring.hpp"

#include "ports/ILogger.hpp"
#include "EndSwitchMonitor.hpp"
#include "HomingRunner.hpp"
#include "../adapters/GpioEndSwitch.hpp"
#include "../adapters/HallHomingSensor.hpp"
#include "../hal/arduino/ArduinoGpioPin.hpp"

namespace
{
#if (USE_RA_END_SWITCH == 1) || (USE_DEC_END_SWITCH == 1) || (USE_HALL_SENSOR_RA_AUTOHOME == 1) || (USE_HALL_SENSOR_DEC_AUTOHOME == 1)
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

void attachSensors(app::MountExecutor &mount)
{
    app::AxisBank::Sensors sensors;

#if USE_RA_END_SWITCH == 1
    sensors.raEndSwitch = new EndSwitchMonitor(ui,
                                        StepperAxis::RA_STEPS,
                                        makeEndSwitch(RA_ENDSWITCH_EAST_SENSOR_PIN, RA_END_SWITCH_ACTIVE_STATE),
                                        makeEndSwitch(RA_ENDSWITCH_WEST_SENSOR_PIN, RA_END_SWITCH_ACTIVE_STATE));
#endif

#if USE_DEC_END_SWITCH == 1
    sensors.decEndSwitch = new EndSwitchMonitor(ui,
                                         StepperAxis::DEC_STEPS,
                                         makeEndSwitch(DEC_ENDSWITCH_DOWN_SENSOR_PIN, DEC_END_SWITCH_ACTIVE_STATE),
                                         makeEndSwitch(DEC_ENDSWITCH_UP_SENSOR_PIN, DEC_END_SWITCH_ACTIVE_STATE));
#endif

#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    sensors.raHoming = new HallSensorHoming(
        ui, mountCoordinator.state(), StepperAxis::RA_STEPS, makeHomingSensor(RA_HOMING_SENSOR_PIN, RA_HOMING_SENSOR_ACTIVE_STATE));
#endif

#if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    sensors.decHoming = new HallSensorHoming(
        ui, mountCoordinator.state(), StepperAxis::DEC_STEPS, makeHomingSensor(DEC_HOMING_SENSOR_PIN, DEC_HOMING_SENSOR_ACTIVE_STATE));
#endif

    mount.attachSensors(sensors);
}

void initHomingSensorPins()
{
#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    pinMode(RA_HOMING_SENSOR_PIN, INPUT);
#endif
#if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    pinMode(DEC_HOMING_SENSOR_PIN, INPUT);
#endif
}

}  // namespace app