#include <gtest/gtest.h>

#include "core/CoordinateMath.hpp"

namespace
{
core::SlewGeometryFacts facts()
{
    core::SlewGeometryFacts f;
    f.targetRaHours      = 3.0f;
    f.targetDecDegrees   = 40.0f;
    f.zeroPosRaHours     = 1.0f;
    f.zeroPosDecDegrees  = 0.5f;
    f.trackedHours       = 0.0f;
    f.raStepsPerDegree   = 100.0f;
    f.decStepsPerDegree  = 20.0f;
    f.raLimitLeftHours   = 5.0f;
    f.raLimitRightHours  = 7.0f;
    f.northernHemisphere = true;
    return f;
}
}  // namespace

TEST(SlewGeometryTest, RaIsTheTargetRelativeToTheZeroPoint)
{
    const core::SlewGeometry g = core::buildSlewGeometry(facts());

    EXPECT_FLOAT_EQ(2.0f, g.moveRA);
}

TEST(SlewGeometryTest, RaTakesTheShorterWayRound)
{
    core::SlewGeometryFacts f = facts();
    f.zeroPosRaHours          = 22.0f;  // 3h target is 5h past 22h, not 19h before it
    f.targetRaHours           = 3.0f;

    EXPECT_FLOAT_EQ(5.0f, core::buildSlewGeometry(f).moveRA);
}

TEST(SlewGeometryTest, RaWrapsNegativeDeltasToo)
{
    core::SlewGeometryFacts f = facts();
    f.zeroPosRaHours          = 1.0f;
    f.targetRaHours           = 0.5f;

    EXPECT_FLOAT_EQ(-0.5f, core::buildSlewGeometry(f).moveRA);
}

TEST(SlewGeometryTest, DeltaFromHomeIncludesTheTrackedHours)
{
    core::SlewGeometryFacts f = facts();
    f.trackedHours            = 4.0f;  // home has advanced to 1h + 4h = 5h

    // Target 3h - home 5h = -2h.
    EXPECT_FLOAT_EQ(-2.0f, core::buildSlewGeometry(f).homeTargetDeltaRA);
}

TEST(SlewGeometryTest, DeltaFromHomeWrapsInBothDirections)
{
    core::SlewGeometryFacts f = facts();
    f.zeroPosRaHours          = 20.0f;
    f.targetRaHours           = 2.0f;
    f.trackedHours            = 10.0f;  // home is at 30h == 6h

    // 2h - 6h = -4h, not +20h.
    EXPECT_FLOAT_EQ(-4.0f, core::buildSlewGeometry(f).homeTargetDeltaRA);
}

TEST(SlewGeometryTest, SouthernHemisphereMirrorsTheDeclination)
{
    core::SlewGeometryFacts f = facts();
    f.northernHemisphere      = false;

    EXPECT_FLOAT_EQ(-40.0f, core::buildSlewGeometry(f).moveDEC);
}

TEST(SlewGeometryTest, SouthernHemisphereSwapsTheRaRingLimits)
{
    const core::SlewGeometry north = core::buildSlewGeometry(facts());

    core::SlewGeometryFacts f      = facts();
    f.northernHemisphere           = false;
    const core::SlewGeometry south = core::buildSlewGeometry(f);

    EXPECT_FLOAT_EQ(-5.0f, north.raLimitL);
    EXPECT_FLOAT_EQ(7.0f, north.raLimitR);
    EXPECT_FLOAT_EQ(-7.0f, south.raLimitL);
    EXPECT_FLOAT_EQ(5.0f, south.raLimitR);
}

TEST(SlewGeometryTest, StepsPerSiderealHourIsStepsPerDegreeTimesTheSiderealRate)
{
    const core::SlewGeometry g = core::buildSlewGeometry(facts());

    EXPECT_NEAR(100.0f * 14.95904348958f, g.stepsPerSiderealHour, 0.001f);
}

TEST(SlewGeometryTest, PassthroughValuesAreCopied)
{
    const core::SlewGeometry g = core::buildSlewGeometry(facts());

    EXPECT_FLOAT_EQ(20.0f, g.stepsPerDECDegree);
    EXPECT_FLOAT_EQ(0.5f, g.zeroPosDEC);
}
