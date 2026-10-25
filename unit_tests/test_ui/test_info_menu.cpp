#include <gtest/gtest.h>

#include <string>

#include "app/ui/screens/InfoMenu.hpp"
#include "../test_common/ports_fakes/FakeDisplay.hpp"
#include "../test_common/ports_fakes/FakeStatusSources.hpp"

using app::InfoMenu;
using app::InfoMenuEnvironment;
using app::MenuKeyInput;
using app::MenuKeyResult;
using core::Button;
using fakes::FakeDisplay;
using fakes::FakeMountConfiguration;
using fakes::FakeMountReadModel;
using fakes::FakeSiteTime;

namespace
{

/// The screen paints at most once per 200 ms, so every paint in a test uses a time far past that.
constexpr unsigned long kPaintTime = 10000;

/// The three ports the screen reads, bundled so a test arranges only the one it cares about.
struct Sources {
    FakeMountReadModel read;
    FakeMountConfiguration config;
    FakeSiteTime siteTime;
};

MenuKeyInput pressed(Button key)
{
    return MenuKeyInput {key, key};
}

std::string paint(InfoMenu &menu, Sources &sources, const InfoMenuEnvironment &environment, unsigned long now = kPaintTime)
{
    FakeDisplay display;
    menu.render(sources.read, sources.config, sources.siteTime, environment, now, display);
    return std::string(display.row(1));
}

InfoMenuEnvironment environmentWithoutGyro()
{
    InfoMenuEnvironment environment;
    environment.version         = "1.9.9";
    environment.freeMemoryBytes = 1234;
    return environment;
}

InfoMenuEnvironment environmentWithGyro(float celsius)
{
    InfoMenuEnvironment environment = environmentWithoutGyro();
    environment.gyroTemperatureC    = celsius;
    environment.hasGyro             = true;
    return environment;
}

/// Walks the list down to `entry`, from a freshly entered screen.
void selectEntry(InfoMenu &menu, int entry)
{
    for (int i = 0; i < entry; i++)
    {
        menu.handleKey(pressed(Button::Down));
    }
}

}  // namespace

TEST(InfoMenuTest, TheFirstEntryIsTheRaStepperPosition)
{
    InfoMenu menu;
    Sources sources;
    sources.read.raStepperPosition = 4321;
    menu.enter();

    EXPECT_EQ("RA Stpr: 4321   ", paint(menu, sources, environmentWithGyro(20.0F)));
    EXPECT_EQ(static_cast<long>(WEST), sources.read.lastPositionAxis);
}

TEST(InfoMenuTest, SelectWalksTheSubPagesOfTheCoordinateEntries)
{
    InfoMenu menu;
    Sources sources;
    sources.read.raTargetText             = "12:34:56";
    sources.read.raCurrentText            = "01:02:03";
    const InfoMenuEnvironment environment = environmentWithGyro(20.0F);
    menu.enter();

    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Select)));
    EXPECT_EQ("RTrg: 12:34:56  ", paint(menu, sources, environment));
    EXPECT_EQ(static_cast<uint8_t>(LCD_STRING | TARGET_STRING), sources.read.lastRaType);

    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Select)));
    EXPECT_EQ("RCur: 01:02:03  ", paint(menu, sources, environment));
    EXPECT_EQ(static_cast<uint8_t>(LCD_STRING | CURRENT_STRING), sources.read.lastRaType);

    // A third press wraps back to the first sub-page.
    menu.handleKey(pressed(Button::Select));
    EXPECT_EQ("RA Stpr: 0      ", paint(menu, sources, environment));
}

TEST(InfoMenuTest, TheDecEntryUsesItsOwnStepperAxis)
{
    InfoMenu menu;
    Sources sources;
    sources.read.decStepperPosition = -987;
    menu.enter();
    selectEntry(menu, 1);

    EXPECT_EQ("DEC Stpr:-987   ", paint(menu, sources, environmentWithGyro(20.0F)));
    EXPECT_EQ(static_cast<long>(NORTH), sources.read.lastPositionAxis);
}

TEST(InfoMenuTest, TheTrackingEntryShowsTheStepperOrTheSpeed)
{
    InfoMenu menu;
    Sources sources;
    sources.read.trkStepperPosition       = 55;
    sources.config.trackingSpeed          = 1.002738F;
    const InfoMenuEnvironment environment = environmentWithGyro(20.0F);
    menu.enter();
    selectEntry(menu, 2);

    EXPECT_EQ("TRK Stpr:55     ", paint(menu, sources, environment));

    menu.handleKey(pressed(Button::Select));
    EXPECT_EQ("TRK Spd:1.002738", paint(menu, sources, environment));
}

TEST(InfoMenuTest, TheLocationRowCarriesTheHemispheres)
{
    InfoMenu menu;
    Sources sources;
    sources.siteTime.siteLatitude  = core::Latitude(-33.9F);
    sources.siteTime.siteLongitude = core::Longitude(151.2F);
    menu.enter();
    selectEntry(menu, 3);

    EXPECT_EQ("Loc 33.9S 151.2E", paint(menu, sources, environmentWithGyro(20.0F)));
}

TEST(InfoMenuTest, TheGyroRowConvertsToFahrenheit)
{
    InfoMenu menu;
    Sources sources;
    menu.enter();
    selectEntry(menu, 4);

    EXPECT_EQ("Temp: 20@C 68@F ", paint(menu, sources, environmentWithGyro(20.0F)));
}

TEST(InfoMenuTest, WithoutAGyroTheEntryIsSkippedInsteadOfPainted)
{
    InfoMenu menu;
    Sources sources;
    const InfoMenuEnvironment environment = environmentWithoutGyro();
    menu.enter();
    selectEntry(menu, 4);

    // The skipped paint draws nothing at all, and the selection has moved on to the DEC limits.
    FakeDisplay display;
    menu.render(sources.read, sources.config, sources.siteTime, environment, kPaintTime, display);
    EXPECT_EQ(0, display.lineWrites);
    EXPECT_EQ("DEC Lo: -10.0   ", paint(menu, sources, environment));
}

TEST(InfoMenuTest, TheDeclinationLimitsComeFromTheConfiguration)
{
    InfoMenu menu;
    Sources sources;
    sources.config.decLowerLimit          = -5.5F;
    sources.config.decUpperLimit          = 75.5F;
    const InfoMenuEnvironment environment = environmentWithGyro(20.0F);
    menu.enter();
    selectEntry(menu, 5);

    EXPECT_EQ("DEC Lo: -5.5    ", paint(menu, sources, environment));

    menu.handleKey(pressed(Button::Down));
    EXPECT_EQ("DEC Up: 75.5    ", paint(menu, sources, environment));
}

TEST(InfoMenuTest, TheFreeMemoryRowShowsWhatTheBoardReported)
{
    InfoMenu menu;
    Sources sources;
    menu.enter();
    selectEntry(menu, 7);

    EXPECT_EQ("MemAvail: 1234  ", paint(menu, sources, environmentWithGyro(20.0F)));
}

TEST(InfoMenuTest, TheUptimeRowSplitsDaysAndHours)
{
    InfoMenu menu;
    Sources sources;
    menu.enter();
    selectEntry(menu, 8);

    // 1 day, 2 hours, 3 minutes and 4 seconds of uptime.
    const unsigned long uptime = ((24UL + 2UL) * 60UL * 60UL) + (3UL * 60UL) + 4UL;
    EXPECT_EQ("Up: 1d 02:03:04 ", paint(menu, sources, environmentWithGyro(20.0F), uptime * 1000UL));
}

TEST(InfoMenuTest, TheClockRowsReadTheSiteTime)
{
    InfoMenu menu;
    Sources sources;
    sources.siteTime.localDate            = {2024, 7, 9};
    sources.siteTime.utcTime              = core::DayTime(20, 30, 15);
    sources.siteTime.localTime            = core::DayTime(21, 30, 15);
    sources.siteTime.utcOffset            = 1;
    const InfoMenuEnvironment environment = environmentWithGyro(20.0F);
    menu.enter();
    selectEntry(menu, 9);

    EXPECT_EQ("Date: 2024-07-09", paint(menu, sources, environment));
    menu.handleKey(pressed(Button::Down));
    EXPECT_EQ("UTC: 20:30:15   ", paint(menu, sources, environment));
    menu.handleKey(pressed(Button::Down));
    EXPECT_EQ("Time: 21:30:15  ", paint(menu, sources, environment));
    menu.handleKey(pressed(Button::Down));
    EXPECT_EQ("Timezone: 1     ", paint(menu, sources, environment));
}

TEST(InfoMenuTest, TheDerivedSkyRowsComeFromTheReadModel)
{
    InfoMenu menu;
    Sources sources;
    sources.read.lstTime                  = core::DayTime(11, 22, 33);
    sources.read.haTime                   = core::DayTime(1, 2, 3);
    const InfoMenuEnvironment environment = environmentWithGyro(20.0F);
    menu.enter();
    selectEntry(menu, 13);

    EXPECT_EQ("LST: 11:22:33   ", paint(menu, sources, environment));
    menu.handleKey(pressed(Button::Down));
    EXPECT_EQ("HA: 01:02:03    ", paint(menu, sources, environment));
    menu.handleKey(pressed(Button::Down));
    EXPECT_EQ("Firmw.: 1.9.9   ", paint(menu, sources, environment));
}

TEST(InfoMenuTest, TheListWrapsAtBothEnds)
{
    InfoMenu menu;
    Sources sources;
    const InfoMenuEnvironment environment = environmentWithGyro(20.0F);
    menu.enter();

    menu.handleKey(pressed(Button::Up));
    EXPECT_EQ("Firmw.: 1.9.9   ", paint(menu, sources, environment));

    menu.handleKey(pressed(Button::Down));
    EXPECT_EQ("RA Stpr: 0      ", paint(menu, sources, environment));
}

TEST(InfoMenuTest, RightLeavesTheScreenAndNothingElseDoes)
{
    InfoMenu menu;
    menu.enter();

    EXPECT_EQ(MenuKeyResult::NextScreen, menu.handleKey(pressed(Button::Right)));
    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Down)));
    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Up)));
    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Select)));
    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Left)));
    EXPECT_EQ(MenuKeyResult::Ignored, menu.handleKey(MenuKeyInput {Button::None, Button::None}));
}

TEST(InfoMenuTest, ThePaintIsThrottledUntilAKeyIsPressed)
{
    InfoMenu menu;
    Sources sources;
    const InfoMenuEnvironment environment = environmentWithGyro(20.0F);
    FakeDisplay display;
    menu.enter();

    menu.render(sources.read, sources.config, sources.siteTime, environment, kPaintTime, display);
    EXPECT_EQ(1, display.lineWrites);

    // The interval is exclusive, so exactly one interval later is still inside the window.
    menu.render(sources.read, sources.config, sources.siteTime, environment, kPaintTime + 200, display);
    EXPECT_EQ(1, display.lineWrites);

    menu.render(sources.read, sources.config, sources.siteTime, environment, kPaintTime + 201, display);
    EXPECT_EQ(2, display.lineWrites);

    // A key press clears the timer, so the next pass repaints straight away.
    menu.handleKey(pressed(Button::Down));
    menu.render(sources.read, sources.config, sources.siteTime, environment, kPaintTime + 202, display);
    EXPECT_EQ(3, display.lineWrites);
}
