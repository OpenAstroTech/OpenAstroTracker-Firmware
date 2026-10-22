#include <gtest/gtest.h>

#include "core/AzAltController.hpp"

using core::AzAltController;
using core::AzAltUpdate;

TEST(AzAltControllerTest, StartsIdle)
{
    AzAltController azAlt;

    EXPECT_FALSE(azAlt.wasRunning());
    EXPECT_EQ(AzAltUpdate::None, azAlt.update(false));
}

TEST(AzAltControllerTest, ReportsTheStopExactlyOnce)
{
    AzAltController azAlt;

    EXPECT_EQ(AzAltUpdate::None, azAlt.update(true));
    EXPECT_TRUE(azAlt.wasRunning());

    EXPECT_EQ(AzAltUpdate::JustStopped, azAlt.update(false));
    EXPECT_FALSE(azAlt.wasRunning());

    // The outputs are already off; a second idle tick must not disable again.
    EXPECT_EQ(AzAltUpdate::None, azAlt.update(false));
}

TEST(AzAltControllerTest, RepeatedRunningTicksDoNotReportAnything)
{
    AzAltController azAlt;

    for (int i = 0; i < 5; i++)
    {
        EXPECT_EQ(AzAltUpdate::None, azAlt.update(true));
    }
    EXPECT_TRUE(azAlt.wasRunning());
}

TEST(AzAltControllerTest, AStopThenRestartReportsEachEdge)
{
    AzAltController azAlt;
    azAlt.update(true);

    EXPECT_EQ(AzAltUpdate::JustStopped, azAlt.update(false));
    EXPECT_EQ(AzAltUpdate::None, azAlt.update(true));
    EXPECT_TRUE(azAlt.wasRunning());
    EXPECT_EQ(AzAltUpdate::JustStopped, azAlt.update(false));
}

TEST(AzAltControllerTest, ResetForgetsTheLatchWithoutReportingAStop)
{
    AzAltController azAlt;
    azAlt.update(true);

    azAlt.reset();

    EXPECT_FALSE(azAlt.wasRunning());
    EXPECT_EQ(AzAltUpdate::None, azAlt.update(false));
}
