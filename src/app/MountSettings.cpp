PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

#include "MountSettings.hpp"

#include "core/MathUtil.hpp"

#include "ports/ILogger.hpp"

#if USE_GYRO_LEVEL == 1
    #include "../core/DecLimitClamp.hpp"
#endif

namespace app
{
MountSettings::MountSettings(core::MountRuntimeSettings &settings,
                             ports::IMountSettings &store,
                             AxisBank &axes,
                             core::MountCoordinator &coordinator,
                             ports::ILogger &logger,
                             TrackingQuery tracking)
    : _settings(settings), _store(store), _axes(axes), _coordinator(coordinator), _logger(logger), _tracking(tracking)
{
}

void MountSettings::reset()
{
#if USE_GYRO_LEVEL == 1
    _pitchCalibrationAngle = 0.0f;
    _rollCalibrationAngle  = 0.0f;
#endif
}

#if USE_GYRO_LEVEL == 1
void MountSettings::applyLoadedPitchCalibration(float angle)
{
    _pitchCalibrationAngle = angle;
}

void MountSettings::applyLoadedRollCalibration(float angle)
{
    _rollCalibrationAngle = angle;
}
#endif

float MountSettings::getSpeedCalibration()
{
    return _coordinator.tracking().calibration();
}

void MountSettings::setSpeedCalibration(float val, bool saveToStorage)
{
    OAT_LOG(&_logger, DEBUG_MOUNT, "[MOUNT]: Updating speed calibration from %f to %f", _coordinator.tracking().calibration(), val);
    _coordinator.tracking().setCalibration(val);

    OAT_LOG(&_logger, DEBUG_MOUNT, "[MOUNT]: Current tracking speed is %f steps/sec", _coordinator.tracking().speed());

    // The tracking rate is derived from the slewing geometry; see core/TrackingController.hpp.
    _coordinator.tracking().setStepsPerDegree(_settings.stepsPerDegree(RA_STEPS));
    OAT_LOG(&_logger, DEBUG_MOUNT, "[MOUNT]: RA steps per degree is %f steps/deg", _settings.stepsPerDegree(RA_STEPS));
    OAT_LOG(&_logger, DEBUG_MOUNT, "[MOUNT]: New tracking speed is %f steps/sec", _coordinator.tracking().speed());

    OAT_LOG(&_logger,
            DEBUG_MOUNT,
            "[MOUNT]: FactorToSpeed : %s, %s",
            String(val, 6).c_str(),
            String(_coordinator.tracking().speed(), 6).c_str());

    if (saveToStorage)
        _store.storeSpeedFactor(_coordinator.tracking().calibration());

    // If we are currently tracking, update the speed. No need to update microstepping mode
    if (_tracking.valid() && _tracking.isTracking(_tracking.context))
    {
        OAT_LOG(&_logger, DEBUG_STEPPERS, "[MOUNT]: SpeedCalibration TRK.setSpeed(%f)", _coordinator.tracking().speed());
        _coordinator.tracking().applySpeed();
    }
}

#if USE_GYRO_LEVEL == 1
float MountSettings::getPitchCalibrationAngle()
{
    return _pitchCalibrationAngle;
}

void MountSettings::setPitchCalibrationAngle(float angle)
{
    _pitchCalibrationAngle = angle;
    _store.storePitchCalibrationAngle(_pitchCalibrationAngle);
}

float MountSettings::getRollCalibrationAngle()
{
    return _rollCalibrationAngle;
}

void MountSettings::setRollCalibrationAngle(float angle)
{
    _rollCalibrationAngle = angle;
    _store.storeRollCalibrationAngle(_rollCalibrationAngle);
}
#endif

void MountSettings::setStepsPerDegree(StepperAxis which, float steps)
{
    if (which == DEC_STEPS)
    {
        _settings.setStepsPerDegree(DEC_STEPS, steps);
        _store.storeDECStepsPerDegree(_settings.stepsPerDegree(DEC_STEPS));
    }
    else if (which == RA_STEPS)
    {
        _settings.setStepsPerDegree(RA_STEPS, steps);
        _store.storeRAStepsPerDegree(_settings.stepsPerDegree(RA_STEPS));
        setSpeedCalibration(_coordinator.tracking().calibration(), false);
    }
    else if (kHasAzStepper && which == AZIMUTH_STEPS)
    {
        _axes.setAzStepsPerDegree(steps);
        _store.storeAZStepsPerDegree(_axes.azStepsPerDegree());
    }
    else if (kHasAltStepper && which == ALTITUDE_STEPS)
    {
        _axes.setAltStepsPerDegree(steps);
        _store.storeALTStepsPerDegree(_axes.altStepsPerDegree());
    }
}

int MountSettings::getBacklashCorrection()
{
    return _settings.backlashCorrectionSteps();
}

void MountSettings::setBacklashCorrection(int steps)
{
    _settings.setBacklashCorrectionSteps(steps);
    _store.storeBacklashCorrectionSteps(_settings.backlashCorrectionSteps());
}

void MountSettings::setSlewRate(int rate)
{
    _settings.setMoveRate(core::clamp(rate, 1, 4));
    float speedFactor[] = {0, 0.05, 0.2, 0.5, 1.0};
    OAT_LOG(&_logger, DEBUG_MOUNT, "[MOUNT]: setSlewRate, rate is %d -> %f", _settings.moveRate(), speedFactor[_settings.moveRate()]);
    _axes.dec()->setMaxSpeed(speedFactor[_settings.moveRate()] * _axes.limits().decSpeed);
    _axes.ra()->setMaxSpeed(speedFactor[_settings.moveRate()] * _axes.limits().raSpeed);
    OAT_LOG(&_logger, DEBUG_MOUNT, "[MOUNT]: setSlewRate, new speeds are RA: %f  DEC: %f", _axes.ra()->maxSpeed(), _axes.dec()->maxSpeed());
}

int MountSettings::getSlewRate()
{
    return _settings.moveRate();
}

void MountSettings::setHomingOffset(StepperAxis axis, long offset)
{
    if (axis == StepperAxis::RA_STEPS)
    {
        _store.storeRAHomingOffset(offset);
        OAT_LOG(&_logger, DEBUG_MOUNT, "[MOUNT]: setHomingOffset(RA): Offset: %ld", offset);
    }
    if (axis == StepperAxis::DEC_STEPS)
    {
        _store.storeDECHomingOffset(offset);
        OAT_LOG(&_logger, DEBUG_MOUNT, "[MOUNT]: setDECHomingOffset(DEC): Offset: %ld", offset);
    }
}

long MountSettings::getHomingOffset(StepperAxis axis)
{
    if (axis == StepperAxis::RA_STEPS)
    {
        return _store.getRAHomingOffset();
    }
    else if (axis == StepperAxis::DEC_STEPS)
    {
        return _store.getDECHomingOffset();
    }
    return 0;
}

void MountSettings::setDecLimitPosition(bool upper, float limitAngle)
{
    if (upper)
    {
        if (limitAngle == 0)
        {
            _settings.setDecUpperLimitSteps(_axes.dec()->currentPosition());
            _store.storeDECUpperLimit(fabsf(_settings.decUpperLimitSteps() / _settings.stepsPerDegree(DEC_STEPS)));
        }
        else
        {
            _settings.setDecUpperLimitSteps((limitAngle * _settings.stepsPerDegree(DEC_STEPS)));
            _store.storeDECUpperLimit(limitAngle);
        }
        OAT_LOG(&_logger,
                DEBUG_MOUNT,
                "[MOUNT]: setDecLimitPosition(Upper) to %f: limit DEC: %ld -> %ld",
                limitAngle,
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
    }
    else
    {
        if (limitAngle == 0)
        {
            _settings.setDecLowerLimitSteps(_axes.dec()->currentPosition());
            _store.storeDECLowerLimit(fabsf(_settings.decLowerLimitSteps() / _settings.stepsPerDegree(DEC_STEPS)));
        }
        else
        {
            _settings.setDecLowerLimitSteps(-(limitAngle * _settings.stepsPerDegree(DEC_STEPS)));
            _store.storeDECLowerLimit(limitAngle);
        }
        OAT_LOG(&_logger,
                DEBUG_MOUNT,
                "[MOUNT]: setDecLimitPosition(Lower) to %f: limit DEC: %ld -> %ld",
                limitAngle,
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
    }
}

void MountSettings::clearDecLimitPosition(bool upper)
{
    if (upper)
    {
        _settings.setDecUpperLimitSteps(DEC_LIMIT_UP * _settings.stepsPerDegree(DEC_STEPS));
        _store.storeDECUpperLimit(DEC_LIMIT_UP);
        OAT_LOG(&_logger,
                DEBUG_MOUNT,
                "[MOUNT]: clearDecLimitPosition(Upper): limit DEC: %ld -> %ld",
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
    }
    else
    {
        _settings.setDecLowerLimitSteps(-(DEC_LIMIT_DOWN * _settings.stepsPerDegree(DEC_STEPS)));
        _store.storeDECLowerLimit(DEC_LIMIT_DOWN);
        OAT_LOG(&_logger,
                DEBUG_MOUNT,
                "[MOUNT]: clearDecLimitPosition(Lower): limit DEC: %ld -> %ld",
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
    }
}

void MountSettings::getDecLimitPositions(float &lowerLimit, float &upperLimit)
{
    lowerLimit = -1.0f * _settings.decLowerLimitSteps() / _settings.stepsPerDegree(DEC_STEPS);
    upperLimit = 1.0f * _settings.decUpperLimitSteps() / _settings.stepsPerDegree(DEC_STEPS);
    OAT_LOG(&_logger,
            DEBUG_MOUNT,
            "[MOUNT]: getDecLimitPositions: limit DEC: %ld -> %ld (%f -> %f)",
            _settings.decLowerLimitSteps(),
            _settings.decUpperLimitSteps(),
            lowerLimit,
            upperLimit);
}

}  // namespace app
