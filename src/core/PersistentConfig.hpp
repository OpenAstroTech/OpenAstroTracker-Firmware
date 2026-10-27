#pragma once

#include <stdint.h>

#include "ports/IMountSettings.hpp"
#include "types/Latitude.hpp"
#include "types/Longitude.hpp"

namespace core
{

/// Everything the mount reads out of the store at boot, as one value.
///
/// The values live in the EEPROM image in their own encodings and are read one accessor at a
/// time; gathering them here is what makes "does a fresh boot apply the stored calibration"
/// answerable on the host, against a fake store, instead of only on hardware.
struct PersistedSettings {
    float raStepsPerDegree      = 0.0f;
    float decStepsPerDegree     = 0.0f;
    float azStepsPerDegree      = 0.0f;
    float altStepsPerDegree     = 0.0f;
    float speedFactor           = 0.0f;
    int backlashCorrectionSteps = 0;

    types::Latitude latitude;
    types::Longitude longitude;
    int utcOffset = 0;

    float pitchCalibrationAngle = 0.0f;
    float rollCalibrationAngle  = 0.0f;

    float decLowerLimitDegrees = 0.0f;
    float decUpperLimitDegrees = 0.0f;

    int32_t azPosition  = 0;
    int32_t altPosition = 0;

    int16_t lastFlashedVersion = 0;
};

/// Reads the stored settings. Runs at boot, so it reads every field in one pass rather than
/// making each consumer ask the store again.
PersistedSettings loadPersistedSettings(ports::IMountSettings &store);

/// What the boot-time "a different firmware is running" check decided.
struct MigrationOutcome {
    /// The stored version differs from this build's, so the migration path ran.
    bool versionChanged = false;

    /// The AZ/ALT home positions were reset because the stored version predates them.
    bool homePositionsReset = false;
};

/// The first firmware version whose store holds meaningful AZ/ALT home positions. Anything older
/// kept those two words as unused space, so an upgrade from it has to zero them.
constexpr int16_t kFirstFirmwareWithAzAltHome = 11307;

/// Compares the firmware that last wrote the store with the one now running and performs the
/// upgrade steps that difference implies.
///
/// Always records the running version. Kept separate from `loadPersistedSettings()` because it
/// writes, and because the order matters: the positions it may zero are read afterwards.
MigrationOutcome applyVersionMigration(ports::IMountSettings &store, int16_t currentVersion);

/// A declination travel limit in slewing microsteps.
struct DecLimits {
    long lowerSteps = 0;
    long upperSteps = 0;
};

/// The DEC travel limits, in microsteps, from the stored degree values.
///
/// A stored value of 0 means "never calibrated", in which case the build-time default applies;
/// a default of 0 means "no limit", which the mount has always encoded as 0. The conversion
/// mirrors the original expression exactly: the negation happens in float and only the result is
/// truncated to a step count.
DecLimits decLimitsFromStored(
    float storedLowerDegrees, float storedUpperDegrees, float decStepsPerDegree, float defaultLowerDegrees, float defaultUpperDegrees);

}  // namespace core
