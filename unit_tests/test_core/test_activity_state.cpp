#include <gtest/gtest.h>

#include "core/ActivityState.hpp"

namespace
{

core::MountState stateWith(core::StatusBit bit)
{
    core::MountState state;
    state.reset(0);
    state.set(static_cast<uint16_t>(bit));
    return state;
}

}  // namespace

TEST(ActivityStateTest, IdleWhenAFlagIsSetButNothingMoves)
{
    // Zero is "parked" (the all-clear value), so idleness is a non-zero state with no axis
    // running and no activity flag that classifies it.
    core::MountState state = stateWith(core::StatusBit::Slewing);
    EXPECT_EQ(core::ActivityState::Idle, core::classifyActivity(state, false, false, false, false));
}

TEST(ActivityStateTest, ParkedWinsOverEverything)
{
    core::MountState state;
    state.reset(0);  // parked is the all-clear value
    EXPECT_EQ(core::ActivityState::Parked, core::classifyActivity(state, true, true, true, true));
}

TEST(ActivityStateTest, ParkingBeatsHomingAndGuiding)
{
    const core::MountState parking = stateWith(core::StatusBit::Parking);
    EXPECT_EQ(core::ActivityState::Parking, core::classifyActivity(parking, true, true, true, true));

    const core::MountState parkingPos = stateWith(core::StatusBit::ParkingPos);
    EXPECT_EQ(core::ActivityState::Parking, core::classifyActivity(parkingPos, true, false, false, false));
}

TEST(ActivityStateTest, HomingBeatsGuiding)
{
    core::MountState state = stateWith(core::StatusBit::FindingHome);
    EXPECT_EQ(core::ActivityState::Homing, core::classifyActivity(state, true, true, false, false));
}

TEST(ActivityStateTest, GuidingIsReportedWhileAPulseRuns)
{
    const core::MountState state = stateWith(core::StatusBit::GuidePulse);
    EXPECT_EQ(core::ActivityState::Guiding, core::classifyActivity(state, false, true, false, false));
}

TEST(ActivityStateTest, SlewKindsAreDistinguished)
{
    const core::MountState toTarget = stateWith(core::StatusBit::SlewingToTarget);
    EXPECT_EQ(core::ActivityState::SlewToTarget, core::classifyActivity(toTarget, false, false, true, false));

    const core::MountState freeSlew = stateWith(core::StatusBit::SlewingFree);
    EXPECT_EQ(core::ActivityState::FreeSlew, core::classifyActivity(freeSlew, false, false, true, false));

    const core::MountState manual = stateWith(core::StatusBit::SlewingManual);
    EXPECT_EQ(core::ActivityState::ManualSlew, core::classifyActivity(manual, false, false, true, false));
}

TEST(ActivityStateTest, TrackingIsReportedWhenOnlyTheTrackingAxisMoves)
{
    const core::MountState state = stateWith(core::StatusBit::Tracking);
    EXPECT_EQ(core::ActivityState::Tracking, core::classifyActivity(state, false, false, false, true));
}

TEST(ActivityStateTest, AMovingAxisWithNoSlewBitsIsUnknown)
{
    // The legacy status string printed nothing in this window; the classification says so
    // explicitly instead of inventing "Idle". 0x0400 is not one of the activity bits.
    core::MountState state;
    state.reset(0x0400);
    EXPECT_EQ(core::ActivityState::Unknown, core::classifyActivity(state, false, false, true, false));
}
