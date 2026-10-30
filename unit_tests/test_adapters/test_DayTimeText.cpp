#include <gtest/gtest.h>

#include <string.h>

#include "adapters/DayTimeText.hpp"

namespace
{
/// Round-trips both Meade wire forms through the pure parsers and checks the axis mapping that
/// `calculateStepperPositions()` depends on. This file exists because the four overlay classes this
/// replaced were `String`-based and therefore excluded from the host build: the value formatting
/// was the only code of its kind in the tree with no test.

TEST(DayTimeText, dayTimeRendersHoursMinutesSecondsAndDecimalHours)
{
    const types::DayTime t(14, 45, 6);
    EXPECT_STREQ("14:45:06 (14.75167)", adapters::formatDayTime(t));
}

TEST(DayTimeText, dayTimeRendersASignForNegativeValues)
{
    const types::DayTime t(-1, 30, 0);
    EXPECT_STREQ("-01:30:00 (-1.50000)", adapters::formatDayTime(t));
}

TEST(DayTimeText, latitudeAlwaysCarriesASign)
{
    // `{+}` emits the sign and `{d}` emits it again for these values. That double sign predates
    // this file and is pinned here so a later cleanup has a failing test to update on purpose.
    EXPECT_STREQ("++45:00:00", adapters::formatLatitude(types::Latitude(45.0f)));
    EXPECT_STREQ("+-45:00:00", adapters::formatLatitude(types::Latitude(-45.0f)));
}

TEST(DayTimeText, longitudeRendersTheEastCountAndAHemisphereLetter)
{
    // The second field is the 0..360 count Meade wants, the letter says which side of Greenwich.
    EXPECT_STREQ("100.00 (100.00E)", adapters::formatLongitude(types::Longitude(100.0f)));
    EXPECT_STREQ("260.00 (100.00W)", adapters::formatLongitude(types::Longitude(-100.0f)));
}

TEST(DayTimeText, declinationRendersCelestialDegreesInTheNorthernHemisphere)
{
    // Axis +90 is the pole above the mount, which is celestial 0 in the north.
    const types::Declination axisPole = types::Declination::fromTotalSeconds(324000);
    EXPECT_STREQ("+00*00:00 (0.0000, 90.0000)", adapters::formatDeclination(axisPole, true));
}

TEST(DayTimeText, declinationBelowOneDegreeSurvivesTheRoundTrip)
{
    // "-00*30:00" is the coordinate the old signed-degrees join used to lose.
    const long celestial          = types::Declination::celestialSecondsFrom(0, 30, 0, /*negative=*/true);
    const types::Declination axis = adapters::decFromCelestialSeconds(celestial, /*northernHemisphere=*/true);
    const long back               = types::Declination::axisToCelestialSeconds(axis.getTotalSeconds(), true);
    EXPECT_EQ(celestial, back);
}

TEST(DayTimeText, theTwoCelestialFactoriesAgreeOnTheNorthernBranch)
{
    // decFromCelestialArcSeconds is the mapping calculateStepperPositions() takes, and it has to
    // stay on the same branch as the wire-side join.
    for (long seconds : {0L, 900L, -900L, 3600L, -3600L})
    {
        const types::Declination fromArcSeconds = adapters::decFromCelestialArcSeconds(seconds, true);
        const types::Declination fromCelestial  = adapters::decFromCelestialSeconds(seconds, true);
        EXPECT_EQ(fromArcSeconds.getTotalSeconds(), fromCelestial.getTotalSeconds()) << "seconds=" << seconds;
    }
}

TEST(DayTimeText, theTwoCelestialFactoriesMirrorInTheSouthernHemisphere)
{
    // They deliberately differ in sign here; this pins the behaviour rather than asserting the
    // southern mapping is symmetric, which it has never been.
    for (long seconds : {0L, 900L, -900L})
    {
        const types::Declination fromArcSeconds = adapters::decFromCelestialArcSeconds(seconds, false);
        const types::Declination fromCelestial  = adapters::decFromCelestialSeconds(seconds, false);
        EXPECT_EQ(-fromArcSeconds.getTotalSeconds(), fromCelestial.getTotalSeconds()) << "seconds=" << seconds;
    }
}

TEST(DayTimeText, eachRendererKeepsItsOwnBuffer)
{
    // A log line carries two of these at once, so the four renderers must not share scratch.
    const char *dayTime   = adapters::formatDayTime(types::DayTime(1, 2, 3));
    const char *latitude  = adapters::formatLatitude(types::Latitude(45.0f));
    const char *longitude = adapters::formatLongitude(types::Longitude(100.0f));
    EXPECT_STREQ("01:02:03 (1.03417)", dayTime);
    EXPECT_STREQ("++45:00:00", latitude);
    EXPECT_STREQ("100.00 (100.00E)", longitude);
}

}  // namespace
