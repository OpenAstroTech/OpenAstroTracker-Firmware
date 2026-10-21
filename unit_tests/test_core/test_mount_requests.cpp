#include <gtest/gtest.h>

#include "core/MountRequests.hpp"

using core::Admission;
using core::MountActivityFacts;
using core::MountRequest;
using core::MountRequests;

namespace
{
MountActivityFacts idleFacts()
{
    MountActivityFacts facts;
    facts.parking     = false;
    facts.tracking    = false;
    facts.guiding     = false;
    facts.slewing     = false;
    facts.findingHome = false;
    return facts;
}

MountActivityFacts trackingFacts()
{
    MountActivityFacts facts = idleFacts();
    facts.tracking           = true;
    return facts;
}

MountActivityFacts parkingFacts()
{
    MountActivityFacts facts = idleFacts();
    facts.parking            = true;
    return facts;
}
}  // namespace

TEST(MountRequestsTest, IdleAcceptsEverySlewAndTracking)
{
    const MountActivityFacts idle = idleFacts();

    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::Track, idle));
    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::GoTo, idle));
    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::Home, idle));
    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::ManualSlew, idle));
}

TEST(MountRequestsTest, ParkingRefusesEveryMoveIncludingAGoTo)
{
    const MountActivityFacts parking = parkingFacts();

    EXPECT_EQ(Admission::RefusedWhileParking, MountRequests::admit(MountRequest::Track, parking));
    EXPECT_EQ(Admission::RefusedWhileParking, MountRequests::admit(MountRequest::GoTo, parking));
    EXPECT_EQ(Admission::RefusedWhileParking, MountRequests::admit(MountRequest::Home, parking));
    EXPECT_EQ(Admission::RefusedWhileParking, MountRequests::admit(MountRequest::ManualSlew, parking));
    EXPECT_EQ(Admission::RefusedWhileParking, MountRequests::admit(MountRequest::Guide, parking));
}

TEST(MountRequestsTest, ParkingIsAlwaysAcceptedEvenWhileParking)
{
    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::Park, idleFacts()));
    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::Park, parkingFacts()));
    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::Park, trackingFacts()));
}

TEST(MountRequestsTest, AGuidePulseNeedsTracking)
{
    EXPECT_EQ(Admission::RefusedWhileNotTracking, MountRequests::admit(MountRequest::Guide, idleFacts()));
    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::Guide, trackingFacts()));
}

TEST(MountRequestsTest, SlewRequestsDoNotCareAboutTrackingOrGuiding)
{
    MountActivityFacts facts = trackingFacts();
    facts.guiding            = true;

    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::GoTo, facts));
    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::Home, facts));
    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::ManualSlew, facts));
}

TEST(MountRequestsTest, HomingDoesNotBlockRequestsByItself)
{
    // The hall-sensor homing runs with the park flags clear when it was started from the
    // menu; only parking owns the axes outright.
    MountActivityFacts facts = idleFacts();
    facts.findingHome        = true;

    EXPECT_EQ(Admission::Allowed, MountRequests::admit(MountRequest::GoTo, facts));
}

TEST(MountRequestsTest, EverySlewShapeStopsGuidingFirst)
{
    EXPECT_TRUE(MountRequests::slewStopsGuiding(MountRequest::GoTo));
    EXPECT_TRUE(MountRequests::slewStopsGuiding(MountRequest::Home));
    EXPECT_TRUE(MountRequests::slewStopsGuiding(MountRequest::ManualSlew));

    EXPECT_FALSE(MountRequests::slewStopsGuiding(MountRequest::Track));
    EXPECT_FALSE(MountRequests::slewStopsGuiding(MountRequest::Guide));
    EXPECT_FALSE(MountRequests::slewStopsGuiding(MountRequest::Park));
}
