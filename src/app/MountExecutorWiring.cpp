#include "core/MathUtil.hpp"
#include "MountExecutor.hpp"

#include "ports/ILogger.hpp"
#include "../adapters/Tmc2209Driver.hpp"
#include "../hal/arduino/ArduinoGpioPin.hpp"
#include "../libs/MappedDict/MappedDict.hpp"

/// How many times the UART self-test asks a driver to answer before giving up on it.
#define UART_CONNECTION_TEST_RETRIES 5

namespace app
{

void MountExecutor::attachSteppers(const AxisBank::Steppers &steppers, const core::AxisLimits &limits)
{
    _axes.attachSteppers(steppers, limits);

    if (_axes.tracking() != nullptr)
    {
        _coordinator.tracking().attach(_axes.tracking());
    }

    if (_axes.focus() != nullptr)
    {
        _coordinator.focus().attach(_axes.focus());
        _coordinator.focus().initialize(limits.focusSpeed);
    }
}

void MountExecutor::attachDrivers(const AxisBank::Drivers &drivers)
{
    _axes.attachDrivers(drivers);
}

void MountExecutor::attachSensors(const AxisBank::Sensors &sensors)
{
    _axes.attachSensors(sensors);
}

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                              \
    || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                           \
    || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
bool MountExecutor::connectToDriver(const String &driverKind, uint16_t *rmsCurrent)
{
    MappedDict<String, ports::IMotorDriver *>::DictEntry_t lookupTable[] = {
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"RA", _axes.raDriver()},
    #endif
    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"DEC", _axes.decDriver()},
    #endif
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"ALT", _axes.altDriver()},
    #endif
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"AZ", _axes.azDriver()},
    #endif
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"FOC", _axes.focusDriver()},
    #endif
    };
    auto driverLookup = MappedDict<String, ports::IMotorDriver *>(lookupTable, core::arraySize(lookupTable));

    ports::IMotorDriver *driver = nullptr;
    driverLookup.tryGet(driverKind, &driver);

    if (driver != nullptr)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: Testing UART Connection to %s driver...", driverKind.c_str());
        for (int i = 0; i < UART_CONNECTION_TEST_RETRIES; i++)
        {
            if (driver->testConnection())
            {
                OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: UART connection to %s driver successful.", driverKind.c_str());
                if (rmsCurrent != nullptr)
                {
                    *rmsCurrent = driver->rms_current();
                }
                return true;
            }
            else
            {
                delay(500);
            }
        }
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: UART connection to %s driver failed.", driverKind.c_str());
    }
    if (rmsCurrent != nullptr)
    {
        *rmsCurrent = 0;
    }
    return false;
}
#endif

#if UART_CONNECTION_TEST_TX == 1
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void MountExecutor::testRA_UART_TX()
{
    //microsteps per driver clock tick (From TMC2209 datasheet: v[Hz] (microstep/s) = VACTUAL[2209] * 0.715Hz)
    const int speed = (RA_STEPPER_SPEED / 2) / 0.715255737f;
    //Duration in ms to move X degrees at half of the max speed
    const int duration = UART_CONNECTION_TEST_TX_DEG * (_settings.stepsPerDegree(RA_STEPS) / (RA_STEPPER_SPEED / 2)) * 1000;
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) for UART test",
            _config.raSlewMicrosteps);
    setRaMicrosteps(_config.raSlewMicrosteps);
    testUART_vactual(_axes.raDriver(), speed, duration);
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) after UART test",
            _config.raTrackingMicrosteps());
    setRaMicrosteps(_config.raTrackingMicrosteps());
}

    #endif
    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void MountExecutor::testDEC_UART_TX()
{
    //microsteps per driver clock tick (From TMC2209 datasheet: v[Hz] (microstep/s) = VACTUAL[2209] * 0.715Hz)
    const int speed = (DEC_STEPPER_SPEED / 2) / 0.715255737f;
    //Duration in ms to move X degrees at half of the max speed
    const int duration = UART_CONNECTION_TEST_TX_DEG * (_settings.stepsPerDegree(DEC_STEPS) / (DEC_STEPPER_SPEED / 2)) * 1000;
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching DEC driver to microsteps(%d) for UART test",
            _config.decSlewMicrosteps);
    setDecMicrosteps(_config.decSlewMicrosteps);
    testUART_vactual(_axes.decDriver(), speed, duration);
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching DEC driver to microsteps(%d) after UART test",
            _config.decGuidingMicrosteps());
    setDecMicrosteps(_config.decGuidingMicrosteps());
}

void MountExecutor::testUART_vactual(ports::IMotorDriver *driver, int _speed, int _duration)
{
    driver->VACTUAL(_speed);
    delay(_duration);
    driver->VACTUAL(0);
    driver->shaft(1);
    delay(500);
    driver->VACTUAL(_speed);
    delay(_duration);
    driver->VACTUAL(0);
    driver->shaft(0);
    delay(500);
}
    #endif
#endif

}  // namespace app
