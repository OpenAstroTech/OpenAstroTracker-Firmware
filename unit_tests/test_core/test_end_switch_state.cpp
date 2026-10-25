#include <gtest/gtest.h>

#include "core/EndSwitchState.hpp"

using core::LimitAction;
using core::LimitEvent;
using core::LimitPoll;
using core::LimitState;
using core::pollLimit;

namespace
{

LimitPoll poll(bool minimum, bool maximum)
{
    return LimitPoll {minimum, maximum};
}

}  // namespace

TEST(EndSwitchStateTest, AnIdleAxisStaysIdle)
{
    LimitState state = LimitState::NotActive;

    EXPECT_EQ(LimitEvent::None, pollLimit(state, poll(false, false)));
    EXPECT_EQ(LimitState::NotActive, state);
}

TEST(EndSwitchStateTest, ReachingTheMinimumLatchesAtMinimum)
{
    LimitState state = LimitState::NotActive;

    EXPECT_EQ(LimitEvent::MinimumReached, pollLimit(state, poll(true, false)));
    EXPECT_EQ(LimitState::AtMinimum, state);

    // Still on the switch: no further edge, no further transition.
    EXPECT_EQ(LimitEvent::None, pollLimit(state, poll(true, false)));
    EXPECT_EQ(LimitState::AtMinimum, state);
}

TEST(EndSwitchStateTest, ReachingTheMaximumLatchesAtMaximum)
{
    LimitState state = LimitState::NotActive;

    EXPECT_EQ(LimitEvent::MaximumReached, pollLimit(state, poll(false, true)));
    EXPECT_EQ(LimitState::AtMaximum, state);
}

TEST(EndSwitchStateTest, BothSwitchesActiveEndsUpAtMaximum)
{
    // The original code sampled the minimum first and the maximum second, so the maximum won.
    LimitState state = LimitState::NotActive;

    EXPECT_EQ(LimitEvent::MaximumReached, pollLimit(state, poll(true, true)));
    EXPECT_EQ(LimitState::AtMaximum, state);
}

TEST(EndSwitchStateTest, BackingOffClearsTheLatchOnlyWhenTheSwitchReleases)
{
    LimitState state = LimitState::NotActive;
    ASSERT_EQ(LimitEvent::MinimumReached, pollLimit(state, poll(true, false)));
    EXPECT_EQ(LimitState::AtMinimum, state);

    // Once the back-off starts, the switch is still held until the axis has moved clear.
    state = LimitState::SlewingOffMinimum;
    EXPECT_EQ(LimitEvent::None, pollLimit(state, poll(true, false)));
    EXPECT_EQ(LimitState::SlewingOffMinimum, state);

    EXPECT_EQ(LimitEvent::LeftMinimum, pollLimit(state, poll(false, false)));
    EXPECT_EQ(LimitState::NotActive, state);
}

TEST(EndSwitchStateTest, BackingOffTheMaximumClearsItsOwnLatch)
{
    LimitState state = LimitState::SlewingOffMaximum;

    EXPECT_EQ(LimitEvent::None, pollLimit(state, poll(false, true)));
    EXPECT_EQ(LimitState::SlewingOffMaximum, state);

    EXPECT_EQ(LimitEvent::LeftMaximum, pollLimit(state, poll(false, false)));
    EXPECT_EQ(LimitState::NotActive, state);
}

TEST(EndSwitchStateTest, TheWaitStatesDoNotReactToTheSwitches)
{
    LimitState state = LimitState::WaitForStopAtMaximum;

    EXPECT_EQ(LimitEvent::None, pollLimit(state, poll(false, false)));
    EXPECT_EQ(LimitState::WaitForStopAtMaximum, state);
}

TEST(EndSwitchStateTest, OnlyTheAtStatesAskForAction)
{
    EXPECT_EQ(LimitAction::None, core::checkLimit(LimitState::NotActive, true, true));
    EXPECT_EQ(LimitAction::None, core::checkLimit(LimitState::SlewingOffMinimum, true, true));
    EXPECT_EQ(LimitAction::None, core::checkLimit(LimitState::WaitForStopAtMaximum, true, true));

    EXPECT_EQ(LimitAction::StopSlew, core::checkLimit(LimitState::AtMinimum, true, false));
    EXPECT_EQ(LimitAction::StopSlew, core::checkLimit(LimitState::AtMaximum, true, false));
}

TEST(EndSwitchStateTest, TrackingIntoTheMaximumStopsTrackingAndLeavesTheAxisThere)
{
    EXPECT_EQ(LimitAction::StopTracking, core::checkLimit(LimitState::AtMaximum, false, true));
}

TEST(EndSwitchStateTest, AtTheMinimumWithoutMotionOnlyResetsTheState)
{
    EXPECT_EQ(LimitAction::ResetAfterMinimum, core::checkLimit(LimitState::AtMinimum, false, false));
}

TEST(EndSwitchStateTest, AtTheMaximumWithNothingMovingAsksForNothing)
{
    EXPECT_EQ(LimitAction::None, core::checkLimit(LimitState::AtMaximum, false, false));
}

TEST(EndSwitchStateTest, BackOffDistanceAddsTheHysteresisInTheDirectionOfTravel)
{
    // Ran 100 steps past the switch, with 2 degrees of hysteresis at 50 steps/degree.
    EXPECT_EQ(200, core::backOffDistance(1000, 900, 2, 50.0f));

    // Overshot the other way: the correction still points back.
    EXPECT_EQ(-200, core::backOffDistance(900, 1000, 2, 50.0f));
}
