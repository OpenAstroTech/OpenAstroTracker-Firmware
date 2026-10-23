#include <gtest/gtest.h>

#include "../test_common/ports_fakes/FakeMountSettings.hpp"
#include "core/PersistentConfig.hpp"

namespace
{
/// A store with every field distinguishable from every other, so a mixed-up accessor shows up.
fakes::FakeMountSettings populatedStore()
{
    fakes::FakeMountSettings store;
    store.raStepsPerDegree        = 11.0f;
    store.decStepsPerDegree       = 22.0f;
    store.azStepsPerDegree        = 33.0f;
    store.altStepsPerDegree       = 44.0f;
    store.speedFactor             = 1.5f;
    store.backlashCorrectionSteps = 7;
    store.latitude                = core::Latitude(52.5f);
    store.longitude               = core::Longitude(13.25f);
    store.utcOffset               = 2;
    store.pitchCalibrationAngle   = 0.75f;
    store.rollCalibrationAngle    = -0.25f;
    store.decLowerLimit           = 60.0f;
    store.decUpperLimit           = 70.0f;
    store.azPosition              = 1234;
    store.altPosition             = -5678;
    store.lastFlashedVersion      = 11322;
    return store;
}
}  // namespace

TEST(PersistentConfigTest, LoadReadsEveryStoredValue)
{
    fakes::FakeMountSettings store = populatedStore();

    const core::PersistedSettings settings = core::loadPersistedSettings(store);

    EXPECT_FLOAT_EQ(11.0f, settings.raStepsPerDegree);
    EXPECT_FLOAT_EQ(22.0f, settings.decStepsPerDegree);
    EXPECT_FLOAT_EQ(33.0f, settings.azStepsPerDegree);
    EXPECT_FLOAT_EQ(44.0f, settings.altStepsPerDegree);
    EXPECT_FLOAT_EQ(1.5f, settings.speedFactor);
    EXPECT_EQ(7, settings.backlashCorrectionSteps);
    EXPECT_FLOAT_EQ(52.5f, settings.latitude.getTotalHours());
    EXPECT_FLOAT_EQ(13.25f, settings.longitude.getTotalHours());
    EXPECT_EQ(2, settings.utcOffset);
    EXPECT_FLOAT_EQ(0.75f, settings.pitchCalibrationAngle);
    EXPECT_FLOAT_EQ(-0.25f, settings.rollCalibrationAngle);
    EXPECT_FLOAT_EQ(60.0f, settings.decLowerLimitDegrees);
    EXPECT_FLOAT_EQ(70.0f, settings.decUpperLimitDegrees);
    EXPECT_EQ(1234, settings.azPosition);
    EXPECT_EQ(-5678, settings.altPosition);
    EXPECT_EQ(11322, settings.lastFlashedVersion);
}

TEST(PersistentConfigTest, LoadDoesNotWriteToTheStore)
{
    fakes::FakeMountSettings store = populatedStore();

    core::loadPersistedSettings(store);

    EXPECT_EQ(11322, store.lastFlashedVersion);
    EXPECT_EQ(1234, store.azPosition);
    EXPECT_EQ(-5678, store.altPosition);
}

TEST(PersistentConfigTest, SameVersionDoesNothing)
{
    fakes::FakeMountSettings store;
    store.lastFlashedVersion = 11322;
    store.azPosition         = 100;
    store.altPosition        = 200;

    const core::MigrationOutcome outcome = core::applyVersionMigration(store, 11322);

    EXPECT_FALSE(outcome.versionChanged);
    EXPECT_FALSE(outcome.homePositionsReset);
    EXPECT_EQ(100, store.azPosition);
    EXPECT_EQ(200, store.altPosition);
}

TEST(PersistentConfigTest, NewVersionIsRecorded)
{
    fakes::FakeMountSettings store;
    store.lastFlashedVersion = 11100;

    const core::MigrationOutcome outcome = core::applyVersionMigration(store, 11322);

    EXPECT_TRUE(outcome.versionChanged);
    EXPECT_EQ(11322, store.lastFlashedVersion);
}

TEST(PersistentConfigTest, UpgradeFromBeforeTheHomePositionsResetsThem)
{
    fakes::FakeMountSettings store;
    store.lastFlashedVersion = 11306;  // one release before the fields appeared
    store.azPosition         = 54321;
    store.altPosition        = 98765;

    const core::MigrationOutcome outcome = core::applyVersionMigration(store, 11322);

    EXPECT_TRUE(outcome.versionChanged);
    EXPECT_TRUE(outcome.homePositionsReset);
    EXPECT_EQ(0, store.azPosition);
    EXPECT_EQ(0, store.altPosition);
}

TEST(PersistentConfigTest, UpgradeFromTheFirstVersionThatHasThemKeepsThem)
{
    fakes::FakeMountSettings store;
    store.lastFlashedVersion = core::kFirstFirmwareWithAzAltHome;
    store.azPosition         = 54321;
    store.altPosition        = 98765;

    const core::MigrationOutcome outcome = core::applyVersionMigration(store, 11322);

    EXPECT_TRUE(outcome.versionChanged);
    EXPECT_FALSE(outcome.homePositionsReset);
    EXPECT_EQ(54321, store.azPosition);
    EXPECT_EQ(98765, store.altPosition);
}

TEST(PersistentConfigTest, AFirstBootWithAnEmptyStoreResetsThePositions)
{
    fakes::FakeMountSettings store;  // everything zero

    const core::MigrationOutcome outcome = core::applyVersionMigration(store, 11322);

    EXPECT_TRUE(outcome.versionChanged);
    EXPECT_TRUE(outcome.homePositionsReset);
    EXPECT_EQ(11322, store.lastFlashedVersion);
}

TEST(PersistentConfigTest, DecLimitsConvertStoredDegreesToSteps)
{
    const core::DecLimits limits = core::decLimitsFromStored(60.0f, 70.0f, 10.0f, 0.0f, 0.0f);

    EXPECT_EQ(-600, limits.lowerSteps);
    EXPECT_EQ(700, limits.upperSteps);
}

TEST(PersistentConfigTest, DecLimitsFallBackToTheBuildDefaultWhenUncalibrated)
{
    const core::DecLimits limits = core::decLimitsFromStored(0.0f, 0.0f, 10.0f, 45.0f, 30.0f);

    EXPECT_EQ(-450, limits.lowerSteps);
    EXPECT_EQ(300, limits.upperSteps);
}

TEST(PersistentConfigTest, DecLimitsStayUnlimitedWhenNothingIsConfigured)
{
    // "0 means no limit" is how the mount has always encoded an uncalibrated axis.
    const core::DecLimits limits = core::decLimitsFromStored(0.0f, 0.0f, 10.0f, 0.0f, 0.0f);

    EXPECT_EQ(0, limits.lowerSteps);
    EXPECT_EQ(0, limits.upperSteps);
}

TEST(PersistentConfigTest, DecLimitsTruncateTowardsZeroLikeTheOriginal)
{
    // 1.5 steps is one step: the cast happens after the float negation, as it always did.
    const core::DecLimits limits = core::decLimitsFromStored(0.15f, 0.15f, 10.0f, 0.0f, 0.0f);

    EXPECT_EQ(-1, limits.lowerSteps);
    EXPECT_EQ(1, limits.upperSteps);
}
