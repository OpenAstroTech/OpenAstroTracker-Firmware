#include <gtest/gtest.h>

#include "core/MountRuntimeSettings.hpp"

TEST(MountRuntimeSettingsTest, ResetRestoresBuildTimeDefaults)
{
    core::MountRuntimeSettings settings;
    settings.setStepsPerDegree(RA_STEPS, 100.0f);
    settings.setStepsPerDegree(DEC_STEPS, 200.0f);
    settings.setDecLowerLimitSteps(-50);
    settings.setDecUpperLimitSteps(60);
    settings.setBacklashCorrectionSteps(12);
    settings.setMoveRate(1);

    settings.reset(30.0f, 40.0f);

    EXPECT_FLOAT_EQ(30.0f, settings.stepsPerDegree(RA_STEPS));
    EXPECT_FLOAT_EQ(40.0f, settings.stepsPerDegree(DEC_STEPS));
    EXPECT_EQ(0, settings.decLowerLimitSteps());
    EXPECT_EQ(0, settings.decUpperLimitSteps());
    EXPECT_EQ(0, settings.backlashCorrectionSteps());
    EXPECT_EQ(4, settings.moveRate());
}

TEST(MountRuntimeSettingsTest, AxesAreIndependent)
{
    core::MountRuntimeSettings settings;
    settings.reset(10.0f, 20.0f);

    EXPECT_FLOAT_EQ(10.0f, settings.stepsPerDegree(RA_STEPS));
    EXPECT_FLOAT_EQ(20.0f, settings.stepsPerDegree(DEC_STEPS));

    settings.setStepsPerDegree(DEC_STEPS, 25.0f);

    EXPECT_FLOAT_EQ(10.0f, settings.stepsPerDegree(RA_STEPS));
    EXPECT_FLOAT_EQ(25.0f, settings.stepsPerDegree(DEC_STEPS));
}
