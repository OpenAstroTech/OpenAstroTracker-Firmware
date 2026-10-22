#include <gtest/gtest.h>

#include "core/MountState.hpp"
#include "core/ParkingController.hpp"

using core::ParkingController;
using core::ParkingPhase;

TEST(ParkingControllerTest, StartsNotParking)
{
    ParkingController parking;

    EXPECT_EQ(ParkingPhase::NotParking, parking.phase());
    EXPECT_FALSE(parking.isParking());
    EXPECT_FALSE(parking.headingToPosition());
    EXPECT_FALSE(parking.isParked());
    EXPECT_EQ(0, ParkingController::statusBitFor(parking.phase()));
}

TEST(ParkingControllerTest, ParkingHomesFirst)
{
    ParkingController parking;

    parking.beginHoming();

    EXPECT_EQ(ParkingPhase::Homing, parking.phase());
    EXPECT_TRUE(parking.isParking());
    EXPECT_FALSE(parking.headingToPosition());
    EXPECT_FALSE(parking.isParked());
    EXPECT_EQ(ParkingController::statusBitFor(ParkingPhase::Homing), static_cast<uint16_t>(core::StatusBit::Parking));
}

TEST(ParkingControllerTest, TheSecondLegMarksTheParkPositionBit)
{
    ParkingController parking;
    parking.beginHoming();

    parking.beginPositionLeg();

    EXPECT_EQ(ParkingPhase::MovingToPosition, parking.phase());
    EXPECT_TRUE(parking.isParking());  // a park still owns the axes
    EXPECT_TRUE(parking.headingToPosition());
    EXPECT_EQ(ParkingController::statusBitFor(ParkingPhase::MovingToPosition), static_cast<uint16_t>(core::StatusBit::ParkingPos));
}

TEST(ParkingControllerTest, AParkedMountIsNoLongerParking)
{
    // "Parked" releases the axes: that is what lets unpark command tracking.
    ParkingController parking;
    parking.beginHoming();
    parking.beginPositionLeg();

    parking.markParked();

    EXPECT_TRUE(parking.isParked());
    EXPECT_FALSE(parking.isParking());
    EXPECT_FALSE(parking.headingToPosition());
    EXPECT_EQ(0, ParkingController::statusBitFor(parking.phase()));
}

TEST(ParkingControllerTest, ClearingForgetsThePark)
{
    ParkingController parking;
    parking.beginHoming();

    parking.clear();

    EXPECT_EQ(ParkingPhase::NotParking, parking.phase());
    EXPECT_FALSE(parking.isParking());
}

TEST(ParkingControllerTest, ReRequestingARestartsFromHoming)
{
    ParkingController parking;
    parking.beginHoming();
    parking.beginPositionLeg();

    parking.beginHoming();

    EXPECT_EQ(ParkingPhase::Homing, parking.phase());
    EXPECT_FALSE(parking.headingToPosition());
}

TEST(ParkingControllerTest, OnlyTheTwoLegsSetAStatusBit)
{
    EXPECT_EQ(0, ParkingController::statusBitFor(ParkingPhase::NotParking));
    EXPECT_NE(0, ParkingController::statusBitFor(ParkingPhase::Homing));
    EXPECT_NE(0, ParkingController::statusBitFor(ParkingPhase::MovingToPosition));
    EXPECT_EQ(0, ParkingController::statusBitFor(ParkingPhase::Parked));

    // The two legs must be distinguishable, since the status string reports them apart.
    EXPECT_NE(ParkingController::statusBitFor(ParkingPhase::Homing), ParkingController::statusBitFor(ParkingPhase::MovingToPosition));
}
