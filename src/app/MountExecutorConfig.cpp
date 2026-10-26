#include "MountExecutor.hpp"

#include "ports/ILogger.hpp"
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
    _persisted.applyLoadedPitchCalibration(stored.pitchCalibrationAngle);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Pitch Offset is %f", _persisted.getPitchCalibrationAngle());

    _persisted.applyLoadedRollCalibration(stored.rollCalibrationAngle);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Roll Offset is %f", _persisted.getRollCalibrationAngle());
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
    if ((_coordinator.state().isNorthernHemisphere() != inNorthern) || force)
    {
        bool wasTracking = isSlewingTRK();
        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Hemisphere changed (or forced update) to %s.", inNorthern ? "northern" : "southern");
        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Stopping all steppers.");
        stopSlewing(ALL_DIRECTIONS | TRACKING);
        waitUntilStopped(ALL_DIRECTIONS);
        _coordinator.state().setNorthernHemisphere(inNorthern);
        bool invertDir = inNorthern ? (RA_INVERT_DIR == 1) : (RA_INVERT_DIR != 1);
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
        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Already in %s hemisphere, no action taken.", inNorthern ? "northern" : "southern");
    }
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
