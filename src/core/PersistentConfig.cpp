#include "PersistentConfig.hpp"

namespace core
{

PersistedSettings loadPersistedSettings(ports::IMountSettings &store)
{
    PersistedSettings settings;

    settings.raStepsPerDegree        = store.getRAStepsPerDegree();
    settings.decStepsPerDegree       = store.getDECStepsPerDegree();
    settings.azStepsPerDegree        = store.getAZStepsPerDegree();
    settings.altStepsPerDegree       = store.getALTStepsPerDegree();
    settings.speedFactor             = store.getSpeedFactor();
    settings.backlashCorrectionSteps = store.getBacklashCorrectionSteps();
    settings.latitude                = store.getLatitude();
    settings.longitude               = store.getLongitude();
    settings.utcOffset               = store.getUtcOffset();
    settings.pitchCalibrationAngle   = store.getPitchCalibrationAngle();
    settings.rollCalibrationAngle    = store.getRollCalibrationAngle();
    settings.decLowerLimitDegrees    = store.getDECLowerLimit();
    settings.decUpperLimitDegrees    = store.getDECUpperLimit();
    settings.azPosition              = store.getAZPosition();
    settings.altPosition             = store.getALTPosition();
    settings.lastFlashedVersion      = store.getLastFlashedVersion();

    return settings;
}

MigrationOutcome applyVersionMigration(ports::IMountSettings &store, int16_t currentVersion)
{
    MigrationOutcome outcome;

    const int16_t lastFlashed = store.getLastFlashedVersion();
    if (lastFlashed == currentVersion)
    {
        return outcome;
    }

    outcome.versionChanged = true;
    store.storeLastFlashedVersion(currentVersion);

    if (lastFlashed < kFirstFirmwareWithAzAltHome)
    {
        // Those two words were unused before 1.13.7, so whatever is there is not a position.
        store.storeALTPosition(0);
        store.storeAZPosition(0);
        outcome.homePositionsReset = true;
    }

    return outcome;
}

DecLimits decLimitsFromStored(
    float storedLowerDegrees, float storedUpperDegrees, float decStepsPerDegree, float defaultLowerDegrees, float defaultUpperDegrees)
{
    DecLimits limits;

    limits.lowerSteps = static_cast<long>(-(storedLowerDegrees * decStepsPerDegree));
    if (limits.lowerSteps == 0 && defaultLowerDegrees != 0)
    {
        limits.lowerSteps = static_cast<long>(-(defaultLowerDegrees * decStepsPerDegree));
    }

    limits.upperSteps = static_cast<long>(storedUpperDegrees * decStepsPerDegree);
    if (limits.upperSteps == 0 && defaultUpperDegrees != 0)
    {
        limits.upperSteps = static_cast<long>(defaultUpperDegrees * decStepsPerDegree);
    }

    return limits;
}

}  // namespace core
