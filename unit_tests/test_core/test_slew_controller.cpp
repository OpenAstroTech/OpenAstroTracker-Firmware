#include <gtest/gtest.h>

#include "core/SlewController.hpp"

TEST(SlewControllerTest, StartsIdle)
{
    core::SlewController slew;

    EXPECT_FALSE(slew.isActive());
    EXPECT_EQ(core::SlewIntent::None, slew.intent());
    EXPECT_FALSE(slew.wasRunning());
    EXPECT_FALSE(slew.backlashOwed());
    EXPECT_FLOAT_EQ(0.0f, slew.totalRaSteps());
    EXPECT_FLOAT_EQ(0.0f, slew.totalDecSteps());
}

TEST(SlewControllerTest, ABegunSlewIsActiveUntilItEnds)
{
    core::SlewController slew;

    slew.begin(core::SlewIntent::Target);
    EXPECT_TRUE(slew.isActive());
    EXPECT_EQ(core::SlewIntent::Target, slew.intent());

    slew.end();
    EXPECT_FALSE(slew.isActive());
    EXPECT_EQ(core::SlewIntent::None, slew.intent());
}

TEST(SlewControllerTest, TotalMoveIsKeptForProgressReporting)
{
    core::SlewController slew;

    slew.setTotalMove(1200.0f, -340.0f);
    EXPECT_FLOAT_EQ(1200.0f, slew.totalRaSteps());
    EXPECT_FLOAT_EQ(-340.0f, slew.totalDecSteps());

    slew.clearTotalMove();
    EXPECT_FLOAT_EQ(0.0f, slew.totalRaSteps());
    EXPECT_FLOAT_EQ(0.0f, slew.totalDecSteps());
}

TEST(SlewControllerTest, SettingOneTotalKeepsTheOther)
{
    core::SlewController slew;
    slew.setTotalMove(1200.0f, -340.0f);

    // moveStepperBy() only knows about one axis and passes the other back unchanged.
    slew.setTotalMove(500.0f, slew.totalDecSteps());

    EXPECT_FLOAT_EQ(500.0f, slew.totalRaSteps());
    EXPECT_FLOAT_EQ(-340.0f, slew.totalDecSteps());
}

TEST(SlewControllerTest, RunningLatchTracksTheLastTick)
{
    core::SlewController slew;

    slew.noteRunning(true);
    EXPECT_TRUE(slew.wasRunning());

    slew.noteRunning(false);
    EXPECT_FALSE(slew.wasRunning());
}

TEST(SlewControllerTest, BacklashIsOwedUntilItIsPaid)
{
    core::SlewController slew;

    slew.requestBacklashCorrection();
    EXPECT_TRUE(slew.backlashOwed());

    slew.clearBacklashCorrection();
    EXPECT_FALSE(slew.backlashOwed());
}

TEST(SlewControllerTest, SnapshotAndRestoreSurviveSetHome)
{
    // setHome() invalidates the slew; the arrival sequence runs during a parking slew and
    // needs its intent back afterwards, which is what the snapshot is for.
    core::SlewController slew;
    slew.begin(core::SlewIntent::Home);
    const core::SlewSnapshot saved = slew.snapshot();

    slew.end();

    slew.restore(saved);

    EXPECT_EQ(core::SlewIntent::Home, slew.intent());
}

TEST(SlewControllerTest, SnapshotDoesNotCarryTheMoveTotals)
{
    // Totals belong to the move that just finished, not to the intent that outlives it.
    core::SlewController slew;
    slew.begin(core::SlewIntent::Home);
    slew.setTotalMove(100.0f, 50.0f);

    const core::SlewSnapshot saved = slew.snapshot();
    slew.clearTotalMove();
    slew.restore(saved);

    EXPECT_FLOAT_EQ(0.0f, slew.totalRaSteps());
}
