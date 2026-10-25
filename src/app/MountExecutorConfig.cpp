#include "MountExecutor.hpp"

#include "../Utility.hpp"
#include "../core/FirmwareVersion.hpp"
#include "../core/Microsteps.hpp"
#include "../core/PersistentConfig.hpp"

namespace app
{

/////////////////////////////////
void MountExecutor::clearConfiguration()
{
    _deps.settings->clearConfiguration();
    initializeVariables();
    readConfiguration();
}

/////////////////////////////////
void MountExecutor::readConfiguration()
{
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: Reading configuration data from EEPROM");
    readPersistentData();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: Done reading configuration data from EEPROM");
}

/////////////////////////////////
void MountExecutor::readPersistentData()
{
    // The upgrade check runs before anything is read: it may zero the AZ/ALT home positions,
    // and those are read below. The store always returns valid data, even on a blank EEPROM.
    const int16_t version                = core::firmwareVersionCode(VERSION);
    const core::MigrationOutcome outcome = core::applyVersionMigration(*_deps.settings, version);

    if (!outcome.versionChanged)
    {
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Same firmware version as last boot %d.", version);
    }
    else
    {
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: New Flash detected! Flashed to %d.", version);
        if (outcome.homePositionsReset)
        {
            OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: First time post 1.13.6, setting AZ and ALT home to 0");
        }
    }

    const core::PersistedSettings stored = core::loadPersistedSettings(*_deps.settings);

    _settings.setStepsPerDegree(RA_STEPS, stored.raStepsPerDegree);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: RA steps/deg is %f", _settings.stepsPerDegree(RA_STEPS));

    _settings.setStepsPerDegree(DEC_STEPS, stored.decStepsPerDegree);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: DEC steps/deg is %f", _settings.stepsPerDegree(DEC_STEPS));

    if (kHasAzStepper)
    {
        _axes.setAzStepsPerDegree(stored.azStepsPerDegree);
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: AZ steps/deg is %f", _axes.azStepsPerDegree());
    }

    if (kHasAltStepper)
    {
        _axes.setAltStepsPerDegree(stored.altStepsPerDegree);
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: ALT steps/deg is %f", _axes.altStepsPerDegree());
    }

    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Speed factor is %f", stored.speedFactor);
    setSpeedCalibration(stored.speedFactor, false);

    _settings.setBacklashCorrectionSteps(stored.backlashCorrectionSteps);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Backlash correction is %d", _settings.backlashCorrectionSteps());

    _siteTime.setLatitude(stored.latitude);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Latitude is %s", Latitude(_siteTime.latitude()).ToString());

    _siteTime.setLongitude(stored.longitude);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Longitude is %s", Longitude(_siteTime.longitude()).ToString());

    _siteTime.setLocalUtcOffset(stored.utcOffset);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: UTC offset is %d", _siteTime.localUtcOffset());

#if USE_GYRO_LEVEL == 1
    _pitchCalibrationAngle = stored.pitchCalibrationAngle;
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Pitch Offset is %f", _pitchCalibrationAngle);

    _rollCalibrationAngle = stored.rollCalibrationAngle;
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Roll Offset is %f", _rollCalibrationAngle);
#endif

    const core::DecLimits limits = core::decLimitsFromStored(
        stored.decLowerLimitDegrees, stored.decUpperLimitDegrees, _settings.stepsPerDegree(DEC_STEPS), DEC_LIMIT_DOWN, DEC_LIMIT_UP);
    _settings.setDecLowerLimitSteps(limits.lowerSteps);
    _settings.setDecUpperLimitSteps(limits.upperSteps);
    OAT_LOG(_deps.logger,
            DEBUG_INFO,
            "[MOUNT]: EEPROM: DEC limits read as %ld -> %ld",
            _settings.decLowerLimitSteps(),
            _settings.decUpperLimitSteps());

    if (kHasAltStepper)
    {
        _axes.alt()->setCurrentPosition(stored.altPosition);
    }

    if (kHasAzStepper)
    {
        _axes.az()->setCurrentPosition(stored.azPosition);
    }

    configureHemisphere(_siteTime.latitude().getTotalHours() > 0);
}

/////////////////////////////////
void MountExecutor::configureHemisphere(bool inNorthern, bool force)
{
    if ((inNorthernHemisphere != inNorthern) || force)
    {
        bool wasTracking = isSlewingTRK();
        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Hemisphere changed (or forced update) to %s.", inNorthern ? "northern" : "southern");
        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Stopping all steppers.");
        stopSlewing(ALL_DIRECTIONS | TRACKING);
        waitUntilStopped(ALL_DIRECTIONS);
        inNorthernHemisphere = inNorthern;
        bool invertDir       = inNorthernHemisphere ? (RA_INVERT_DIR == 1) : (RA_INVERT_DIR != 1);
        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Configured RA steppers, DIR Invert is %d", invertDir);
        _axes.ra()->setPinsInverted(invertDir, false, false);
        _axes.tracking()->setPinsInverted(invertDir, false, false);

        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Reset RA and TRK positions to 0");
        _axes.tracking()->setCurrentPosition(0);
        _axes.ra()->setCurrentPosition(0);
        if (wasTracking)
        {
            OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Restarting TRK since it was on.");
            startSlewing(TRACKING);
        }
    }
    else
    {
        OAT_LOG(_deps.logger,
                DEBUG_ANY,
                "[SYSTEM]: Already in %s hemisphere, no action taken.",
                inNorthernHemisphere ? "northern" : "southern");
    }
}

float MountExecutor::getSpeedCalibration()
{
    return _coordinator.tracking().calibration();
}

/////////////////////////////////
void MountExecutor::setSpeedCalibration(float val, bool saveToStorage)
{
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Updating speed calibration from %f to %f", _coordinator.tracking().calibration(), val);
    _coordinator.tracking().setCalibration(val);

    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Current tracking speed is %f steps/sec", _coordinator.tracking().speed());

    // The tracking rate is derived from the slewing geometry; see core/TrackingController.hpp.
    _coordinator.tracking().setStepsPerDegree(_settings.stepsPerDegree(RA_STEPS));
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: RA steps per degree is %f steps/deg", _settings.stepsPerDegree(RA_STEPS));
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: New tracking speed is %f steps/sec", _coordinator.tracking().speed());

    OAT_LOG(_deps.logger,
            DEBUG_MOUNT,
            "[MOUNT]: FactorToSpeed : %s, %s",
            String(val, 6).c_str(),
            String(_coordinator.tracking().speed(), 6).c_str());

    if (saveToStorage)
        _deps.settings->storeSpeedFactor(_coordinator.tracking().calibration());

    // If we are currently tracking, update the speed. No need to update microstepping mode
    if (isSlewingTRK())
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: SpeedCalibration TRK.setSpeed(%f)", _coordinator.tracking().speed());
        _coordinator.tracking().applySpeed();
    }
}

/////////////////////////////////
#if USE_GYRO_LEVEL == 1
float MountExecutor::getPitchCalibrationAngle()
{
    return _pitchCalibrationAngle;
}

/////////////////////////////////
void MountExecutor::setPitchCalibrationAngle(float angle)
{
    _pitchCalibrationAngle = angle;
    _deps.settings->storePitchCalibrationAngle(_pitchCalibrationAngle);
}

/////////////////////////////////
float MountExecutor::getRollCalibrationAngle()
{
    return _rollCalibrationAngle;
}

/////////////////////////////////
void MountExecutor::setRollCalibrationAngle(float angle)
{
    _rollCalibrationAngle = angle;
    _deps.settings->storeRollCalibrationAngle(_rollCalibrationAngle);
}
#endif

void MountExecutor::setStepsPerDegree(StepperAxis which, float steps)
{
    if (which == DEC_STEPS)
    {
        _settings.setStepsPerDegree(DEC_STEPS, steps);
        _deps.settings->storeDECStepsPerDegree(_settings.stepsPerDegree(DEC_STEPS));
    }
    else if (which == RA_STEPS)
    {
        _settings.setStepsPerDegree(RA_STEPS, steps);
        _deps.settings->storeRAStepsPerDegree(_settings.stepsPerDegree(RA_STEPS));
        setSpeedCalibration(_coordinator.tracking().calibration(), false);
    }
    else if (kHasAzStepper && which == AZIMUTH_STEPS)
    {
        _axes.setAzStepsPerDegree(steps);
        _deps.settings->storeAZStepsPerDegree(_axes.azStepsPerDegree());
    }
    else if (kHasAltStepper && which == ALTITUDE_STEPS)
    {
        _axes.setAltStepsPerDegree(steps);
        _deps.settings->storeALTStepsPerDegree(_axes.altStepsPerDegree());
    }
}

int MountExecutor::getBacklashCorrection()
{
    return _settings.backlashCorrectionSteps();
}

void MountExecutor::setBacklashCorrection(int steps)
{
    _settings.setBacklashCorrectionSteps(steps);
    _deps.settings->storeBacklashCorrectionSteps(_settings.backlashCorrectionSteps());
}

/////////////////////////////////
void MountExecutor::setSlewRate(int rate)
{
    _settings.setMoveRate(clamp(rate, 1, 4));
    float speedFactor[] = {0, 0.05, 0.2, 0.5, 1.0};
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setSlewRate, rate is %d -> %f", _settings.moveRate(), speedFactor[_settings.moveRate()]);
    _axes.dec()->setMaxSpeed(speedFactor[_settings.moveRate()] * _axes.limits().decSpeed);
    _axes.ra()->setMaxSpeed(speedFactor[_settings.moveRate()] * _axes.limits().raSpeed);
    OAT_LOG(
        _deps.logger, DEBUG_MOUNT, "[MOUNT]: setSlewRate, new speeds are RA: %f  DEC: %f", _axes.ra()->maxSpeed(), _axes.dec()->maxSpeed());
}

/////////////////////////////////
int MountExecutor::getSlewRate()
{
    return _settings.moveRate();
}

/////////////////////////////////
void MountExecutor::setHomingOffset(StepperAxis axis, long offset)
{
    if (axis == StepperAxis::RA_STEPS)
    {
        _deps.settings->storeRAHomingOffset(offset);
        OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setHomingOffset(RA): Offset: %ld", offset);
    }
    if (axis == StepperAxis::DEC_STEPS)
    {
        _deps.settings->storeDECHomingOffset(offset);
        OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setDECHomingOffset(DEC): Offset: %ld", offset);
    }
}

long MountExecutor::getHomingOffset(StepperAxis axis)
{
    if (axis == StepperAxis::RA_STEPS)
    {
        return _deps.settings->getRAHomingOffset();
    }
    else if (axis == StepperAxis::DEC_STEPS)
    {
        return _deps.settings->getDECHomingOffset();
    }
    return 0;
}

/////////////////////////////////
void MountExecutor::setDecLimitPosition(bool upper, float limitAngle)
{
    if (upper)
    {
        if (limitAngle == 0)
        {
            _settings.setDecUpperLimitSteps(_axes.dec()->currentPosition());
            _deps.settings->storeDECUpperLimit(fabsf(_settings.decUpperLimitSteps() / _settings.stepsPerDegree(DEC_STEPS)));
        }
        else
        {
            _settings.setDecUpperLimitSteps((limitAngle * _settings.stepsPerDegree(DEC_STEPS)));
            _deps.settings->storeDECUpperLimit(limitAngle);
        }
        OAT_LOG(_deps.logger,
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
            _deps.settings->storeDECLowerLimit(fabsf(_settings.decLowerLimitSteps() / _settings.stepsPerDegree(DEC_STEPS)));
        }
        else
        {
            _settings.setDecLowerLimitSteps(-(limitAngle * _settings.stepsPerDegree(DEC_STEPS)));
            _deps.settings->storeDECLowerLimit(limitAngle);
        }
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: setDecLimitPosition(Lower) to %f: limit DEC: %ld -> %ld",
                limitAngle,
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
    }
}

/////////////////////////////////
void MountExecutor::clearDecLimitPosition(bool upper)
{
    if (upper)
    {
        _settings.setDecUpperLimitSteps(DEC_LIMIT_UP * _settings.stepsPerDegree(DEC_STEPS));
        _deps.settings->storeDECUpperLimit(DEC_LIMIT_UP);
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: clearDecLimitPosition(Upper): limit DEC: %ld -> %ld",
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
    }
    else
    {
        _settings.setDecLowerLimitSteps(-(DEC_LIMIT_DOWN * _settings.stepsPerDegree(DEC_STEPS)));
        _deps.settings->storeDECLowerLimit(DEC_LIMIT_DOWN);
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: clearDecLimitPosition(Lower): limit DEC: %ld -> %ld",
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
    }
}

void MountExecutor::getDecLimitPositions(float &lowerLimit, float &upperLimit)
{
    lowerLimit = -1.0f * _settings.decLowerLimitSteps() / _settings.stepsPerDegree(DEC_STEPS);
    upperLimit = 1.0f * _settings.decUpperLimitSteps() / _settings.stepsPerDegree(DEC_STEPS);
    OAT_LOG(_deps.logger,
            DEBUG_MOUNT,
            "[MOUNT]: getDecLimitPositions: limit DEC: %ld -> %ld (%f -> %f)",
            _settings.decLowerLimitSteps(),
            _settings.decUpperLimitSteps(),
            lowerLimit,
            upperLimit);
}

/////////////////////////////////
float MountExecutor::getSpeed(int direction)
{
    if (direction & TRACKING)
    {
        return _coordinator.tracking().speed();
    }
    if (direction & (NORTH | SOUTH))
    {
        return _axes.dec()->speed();
    }
    if (direction & (EAST | WEST))
    {
        return _axes.ra()->speed();
    }
    return 0;
}

}  // namespace app
