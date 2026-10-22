#include <gtest/gtest.h>

#include "core/MountConfig.hpp"

using core::MountConfig;

namespace
{
/// A TMC2209 UART board: 256x tracking microsteps, 8x for slewing, 256x for guiding.
MountConfig tmcBoard()
{
    MountConfig config;
    config.raSlewMicrosteps         = 8;
    config.raTrackMicrosteps        = 256;
    config.decSlewMicrosteps        = 16;
    config.decGuideMicrosteps       = 256;
    config.defaultRaStepsPerDegree  = 720.0f;
    config.defaultDecStepsPerDegree = 360.0f;
    return config;
}
}  // namespace

// The steps-per-degree values are the slew-mode ones, so tracking runs at a finer rate and the
// two are related by this integer ratio. Everything in the rate math depends on it.
TEST(MountConfigTest, TrackingRatioIsTheMicrostepRatio)
{
    MountConfig config = tmcBoard();

    EXPECT_EQ(32u, config.trackingMicrostepRatio());
    EXPECT_EQ(256, static_cast<int>(config.raTrackingMicrosteps()));
    EXPECT_EQ(256, static_cast<int>(config.decGuidingMicrosteps()));
}

// A board that straps one microstep mode for everything (A4988) must report a ratio of 1, not
// trap on a division of equals or divide by zero.
TEST(MountConfigTest, StrapOnlyBoardHasRatioOne)
{
    MountConfig config       = tmcBoard();
    config.raSlewMicrosteps  = 16;
    config.raTrackMicrosteps = 16;

    EXPECT_EQ(1u, config.trackingMicrostepRatio());
}

// A zero slew rate cannot happen in a real configuration, but a divide by it would be a crash,
// so the guard is pinned.
TEST(MountConfigTest, ZeroSlewMicrostepsDoesNotDivide)
{
    MountConfig config      = tmcBoard();
    config.raSlewMicrosteps = 0;

    EXPECT_EQ(1u, config.trackingMicrostepRatio());
}
