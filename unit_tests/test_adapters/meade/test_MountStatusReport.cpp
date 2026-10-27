#include <gtest/gtest.h>

#include <string>

#include "adapters/meade/MountStatusReport.hpp"

namespace
{

adapters::meade::MountStatusFacts idleFacts()
{
    adapters::meade::MountStatusFacts facts {};
    facts.activity = "Idle";
    facts.raText   = "120000";
    facts.decText  = "-051500";
    return facts;
}

std::string render(const adapters::meade::MountStatusFacts &facts)
{
    char buffer[160] = {0};
    const bool ok    = adapters::meade::formatMountStatus(buffer, sizeof(buffer), facts);
    EXPECT_TRUE(ok);
    return std::string(buffer);
}

}  // namespace

TEST(MountStatusReportTest, IdleReportHasDashesAndEmptyFocusField)
{
    // The trailing double comma is the legacy shape: the DEC field ends with its own separator and
    // the empty focus field contributes nothing but its trailing one.
    EXPECT_EQ("Idle,------,0,0,0,120000,-051500,,", render(idleFacts()));
}

TEST(MountStatusReportTest, RunningAxesReportTheirDirectionInCase)
{
    auto facts                  = idleFacts();
    facts.raRunning             = true;
    facts.raTargetBelowCurrent  = true;
    facts.decRunning            = true;
    facts.decTargetBelowCurrent = false;
    EXPECT_EQ("Idle,Rd----,0,0,0,120000,-051500,,", render(facts));
}

TEST(MountStatusReportTest, TrackingIsShownWhileTheTrackingAxisRuns)
{
    auto facts                = idleFacts();
    facts.trackingAxisRunning = true;
    EXPECT_EQ("Idle,--T---,0,0,0,120000,-051500,,", render(facts));
}

TEST(MountStatusReportTest, TrackingOnlyFillsTheSamePosition)
{
    auto facts         = idleFacts();
    facts.trackingOnly = true;
    EXPECT_EQ("Idle,--T---,0,0,0,120000,-051500,,", render(facts));
}

TEST(MountStatusReportTest, OptionalAxesOccupyTheirPositions)
{
    auto facts                    = idleFacts();
    facts.azRunning               = true;
    facts.azTargetBelowCurrent    = true;
    facts.altRunning              = true;
    facts.altTargetBelowCurrent   = false;
    facts.focusRunning            = true;
    facts.focusTargetBelowCurrent = true;
    facts.hasFocus                = true;
    facts.focusSteps              = 50000;
    EXPECT_EQ("Idle,---ZaF,0,0,0,120000,-051500,50000,", render(facts));
}

TEST(MountStatusReportTest, StepperPositionsAreReportedVerbatim)
{
    auto facts     = idleFacts();
    facts.raSteps  = -1234;
    facts.decSteps = 5678;
    facts.trkSteps = 90;
    EXPECT_EQ("Idle,------,-1234,5678,90,120000,-051500,,", render(facts));
}

TEST(MountStatusReportTest, AnEmptyActivityWordStartsWithTheSeparator)
{
    auto facts     = idleFacts();
    facts.activity = "";
    EXPECT_EQ(",------,0,0,0,120000,-051500,,", render(facts));
}

TEST(MountStatusReportTest, ASmallBufferIsReportedRatherThanTruncatedSilently)
{
    char buffer[8] = {0};
    EXPECT_FALSE(adapters::meade::formatMountStatus(buffer, sizeof(buffer), idleFacts()));
}
