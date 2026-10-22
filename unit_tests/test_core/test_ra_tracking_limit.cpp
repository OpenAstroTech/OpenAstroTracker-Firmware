#include <gtest/gtest.h>

#include "core/RaTrackingLimit.hpp"

using core::RaLimitFacts;
using core::RaLimitResult;
using core::RaTrackingLimit;

namespace
{
RaLimitFacts facts(float homeRa, float currentRa, float decAxisDegrees, bool northern = true, float limit = 12.0f)
{
    RaLimitFacts f;
    f.homeRaHours        = homeRa;
    f.currentRaHours     = currentRa;
    f.decAxisDegrees     = decAxisDegrees;
    f.northernHemisphere = northern;
    f.limitHours         = limit;
    return f;
}
}  // namespace

TEST(RaTrackingLimitTest, JustStartedTrackingHasTheWholeLimitAvailable)
{
    const RaLimitResult r = RaTrackingLimit::evaluate(facts(6.0f, 6.0f, 45.0f));

    EXPECT_FLOAT_EQ(12.0f, r.remainingHours);
    EXPECT_FALSE(r.limitReached);
}

TEST(RaTrackingLimitTest, TrackingMovesTheChainTowardTheLimit)
{
    // Six hours of tracking: six hours of budget left.
    const RaLimitResult r = RaTrackingLimit::evaluate(facts(12.0f, 6.0f, 45.0f));

    EXPECT_FLOAT_EQ(6.0f, r.remainingHours);
    EXPECT_FALSE(r.limitReached);
}

TEST(RaTrackingLimitTest, TheLimitIsReachedOnlyOnceItIsExceeded)
{
    // delta == limit is still allowed; the original used a strict greater-than. The limit has to
    // be below 12 h for this to be reachable at all, because the delta is folded into ±12 — which
    // is why the configured value is 7 h.
    const RaLimitResult atTheLimit = RaTrackingLimit::evaluate(facts(13.0f, 6.0f, 45.0f, true, 7.0f));
    EXPECT_FALSE(atTheLimit.limitReached);
    EXPECT_FLOAT_EQ(0.0f, atTheLimit.remainingHours);

    const RaLimitResult past = RaTrackingLimit::evaluate(facts(13.5f, 6.0f, 45.0f, true, 7.0f));
    EXPECT_TRUE(past.limitReached);
    EXPECT_FLOAT_EQ(-0.5f, past.remainingHours);
}

TEST(RaTrackingLimitTest, ADeltaBeyondTwelveHoursFoldsBackIntoRange)
{
    // 20 hours of apparent delta is really 4 hours the other way round the hour circle, so the
    // budget reads 12 + 4 rather than 12 - 20.
    const RaLimitResult r = RaTrackingLimit::evaluate(facts(22.0f, 2.0f, 45.0f, true, 12.0f));

    EXPECT_FLOAT_EQ(16.0f, r.remainingHours);
    EXPECT_FALSE(r.limitReached);
}

TEST(RaTrackingLimitTest, ANegativeDeltaBeyondTwelveHoursFoldsTheOtherWay)
{
    // Home 2 h, current 22 h: raw delta -20 h, folded to +4 h, i.e. 8 h of budget left.
    const RaLimitResult r = RaTrackingLimit::evaluate(facts(2.0f, 22.0f, 45.0f, true, 12.0f));

    EXPECT_FLOAT_EQ(8.0f, r.remainingHours);
    EXPECT_FALSE(r.limitReached);
}

TEST(RaTrackingLimitTest, NorthOfThePoleBelowZeroDecShiftsTheHourCircle)
{
    // In the north, a DEC axis position below zero is across the pole, so the current RA is
    // reported 12 hours off: a 6 h vs 6 h pair becomes 6 h vs 18 h, i.e. a +12 h delta, which
    // folds to -12 h and leaves the full budget.
    const RaLimitResult acrossThePole = RaTrackingLimit::evaluate(facts(6.0f, 6.0f, -10.0f, true, 12.0f));
    const RaLimitResult sameSide      = RaTrackingLimit::evaluate(facts(6.0f, 6.0f, 10.0f, true, 12.0f));

    EXPECT_FLOAT_EQ(24.0f, acrossThePole.remainingHours);  // folded -12 - 12 → 24
    EXPECT_FLOAT_EQ(12.0f, sameSide.remainingHours);
}

TEST(RaTrackingLimitTest, TheHemisphereDecidesWhichSideIsAcrossThePole)
{
    const float northernPositiveDec = RaTrackingLimit::evaluate(facts(6.0f, 6.0f, 10.0f, true, 12.0f)).remainingHours;
    const float southernPositiveDec = RaTrackingLimit::evaluate(facts(6.0f, 6.0f, 10.0f, false, 12.0f)).remainingHours;

    // The same facts are "same side" in the north and "across the pole" in the south.
    EXPECT_FLOAT_EQ(12.0f, northernPositiveDec);
    EXPECT_FLOAT_EQ(24.0f, southernPositiveDec);
}

TEST(RaTrackingLimitTest, TheShiftWrapsPastMidnight)
{
    // currentRA just below 12 h would subtract to negative; the original wrapped it by adding 24.
    const RaLimitResult r = RaTrackingLimit::evaluate(facts(10.0f, 5.0f, -1.0f, true, 12.0f));

    // 5 - 12 = -7 -> +17; delta = 10 - 17 = -7, inside the fold. Budget is 12 - (-7) = 19.
    EXPECT_FLOAT_EQ(19.0f, r.remainingHours);
    EXPECT_FALSE(r.limitReached);
}

TEST(RaTrackingLimitTest, TheLimitIsTheCallerSuppliedOne)
{
    // The caller reads RA_TRACKING_LIMIT; the rule must not bake in a value.
    const RaLimitResult tight = RaTrackingLimit::evaluate(facts(6.0f, 2.0f, 45.0f, true, 2.0f));

    EXPECT_TRUE(tight.limitReached);
    EXPECT_FLOAT_EQ(-2.0f, tight.remainingHours);
}
