PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

#include "AxisBank.hpp"

#include "../adapters/NullMotorDriver.hpp"

namespace
{
/// Stands in for a driver that has no registers to talk to (A4988, or a TMC2209 strapped to
/// standalone), so every driver accessor is non-null and its call sites stay unguarded.
adapters::NullMotorDriver nullDriver;
}  // namespace

namespace app
{
AxisBank::AxisBank()
{
    _stepsPerAzDegree  = AZIMUTH_STEPS_PER_REV / 360.0f;
    _stepsPerAltDegree = ALTITUDE_STEPS_PER_REV / 360.0f;
    reset();
}

void AxisBank::attachSteppers(const Steppers &steppers, const core::AxisLimits &limits)
{
    _ra       = steppers.ra;
    _tracking = steppers.tracking;
    _dec      = steppers.dec;
    _guide    = steppers.guide;
    _az       = steppers.az;
    _alt      = steppers.alt;
    _focus    = steppers.focus;
    _limits   = limits;
}

void AxisBank::attachDrivers(const Drivers &drivers)
{
    _raDriver    = drivers.ra != nullptr ? drivers.ra : &nullDriver;
    _decDriver   = drivers.dec != nullptr ? drivers.dec : &nullDriver;
    _azDriver    = drivers.az != nullptr ? drivers.az : &nullDriver;
    _altDriver   = drivers.alt != nullptr ? drivers.alt : &nullDriver;
    _focusDriver = drivers.focus != nullptr ? drivers.focus : &nullDriver;
}

void AxisBank::attachSensors(const Sensors &sensors)
{
    _raEndSwitch  = sensors.raEndSwitch;
    _decEndSwitch = sensors.decEndSwitch;
    _raHoming     = sensors.raHoming;
    _decHoming    = sensors.decHoming;
}

void AxisBank::reset()
{
    _ra       = nullptr;
    _tracking = nullptr;
    _dec      = nullptr;
    _guide    = nullptr;
    _az       = nullptr;
    _alt      = nullptr;
    _focus    = nullptr;

    _raDriver    = &nullDriver;
    _decDriver   = &nullDriver;
    _azDriver    = &nullDriver;
    _altDriver   = &nullDriver;
    _focusDriver = &nullDriver;

    _raEndSwitch  = nullptr;
    _decEndSwitch = nullptr;
    _raHoming     = nullptr;
    _decHoming    = nullptr;

    _limits = core::AxisLimits {};
}

}  // namespace app
