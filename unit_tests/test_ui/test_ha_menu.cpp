#include <gtest/gtest.h>

#include <string>

#include "app/ui/screens/HaMenu.hpp"
#include "../test_common/ports_fakes/FakeDisplay.hpp"
#include "../test_common/ports_fakes/FakeMountSettings.hpp"
#include "../test_common/ports_fakes/FakeStatusSources.hpp"

using app::HaMenu;
using app::HaMenuOutcome;
using app::MenuKeyInput;
using app::MenuKeyResult;
using core::Button;
using fakes::FakeDisplay;
using fakes::FakeMountReadModel;
using fakes::FakeMountSettings;
using fakes::FakeSiteTime;

namespace
{

MenuKeyInput pressed(Button key)
{
    return MenuKeyInput {key, key};
}

MenuKeyInput heldOnly(Button key)
{
    return MenuKeyInput {Button::None, key};
}

/// A receiver the test drives: no fix until `hasFix` is set.
class FakeGps final : public ports::IGps
{
  public:
    bool begin() override
    {
        beginCalls++;
        return true;
    }

    void poll() override
    {
        pollCalls++;
    }

    bool hasLocation() override
    {
        return hasFix;
    }

    ports::GpsFix fix() override
    {
        return currentFix;
    }

    bool hasFix    = false;
    int beginCalls = 0;
    int pollCalls  = 0;
    ports::GpsFix currentFix {};
};

/// The ports the screen needs, wired to the fakes and to a clock the test controls.
struct Harness {
    FakeMountReadModel read;
    FakeMountSettings settings;
    FakeSiteTime siteTime;
    FakeDisplay display;
    app::UiPorts ports;

    Harness()
    {
        ports.motion    = nullptr;
        ports.config    = nullptr;
        ports.read      = &read;
        ports.settings  = &settings;
        ports.siteTime  = &siteTime;
        ports.axes      = nullptr;
        ports.clock     = &clock;
        ports.scheduler = &scheduler;
    }

    /// A clock that moves a millisecond per read, so `UiPorts::delay()` — which spins on the clock
    /// and yields — always terminates.
    struct TestClock final : public ports::IClock {
        uint32_t nowValue = 10000;

        uint32_t now() override
        {
            nowValue += 1;
            return nowValue;
        }

        void delay(uint32_t) override
        {
        }
    } clock;

    struct TestScheduler final : public ports::IScheduler {
        void yield() override
        {
        }
    } scheduler;
};

}  // namespace

TEST(HaMenuTest, WithoutAReceiverTheScreenIsTheManualEditor)
{
    HaMenu menu;
    Harness harness;
    menu.enter(false);

    menu.render(harness.read, harness.display);

    EXPECT_EQ(app::HaMenu::State::EnterManually, menu.state());
    EXPECT_EQ(">00h 00m        ", std::string(harness.display.row(1)));
}

TEST(HaMenuTest, TheFieldCursorMovesOnLeft)
{
    HaMenu menu;
    Harness harness;
    menu.enter(false);

    HaMenuOutcome outcome;
    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Left), harness.ports, nullptr, false, outcome));
    menu.render(harness.read, harness.display);

    // The cursor marks the minutes now; the legacy row is " hh mm" with '>' on a four-column field.
    EXPECT_EQ(" 00h>00m        ", std::string(harness.display.row(1)));
}

TEST(HaMenuTest, StoringReportsBackAndResumesStartupWhenAsked)
{
    HaMenu menu;
    Harness harness;
    HaMenuOutcome outcome;
    menu.enter(false);

    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Select), harness.ports, nullptr, true, outcome));
    EXPECT_TRUE(outcome.haStored);
    EXPECT_TRUE(outcome.resumeStartup);
}

TEST(HaMenuTest, RightLeavesUnlessTheStartupIsWaiting)
{
    HaMenu menu;
    Harness harness;
    HaMenuOutcome outcome;
    menu.enter(false);

    EXPECT_EQ(MenuKeyResult::NextScreen, menu.handleKey(pressed(Button::Right), harness.ports, nullptr, false, outcome));

    // While the wizard waits, RIGHT is swallowed: only storing the hour angle confirms it.
    HaMenuOutcome waiting;
    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Right), harness.ports, nullptr, true, waiting));
    EXPECT_FALSE(waiting.resumeStartup);
    EXPECT_FALSE(waiting.haStored);
}

TEST(HaMenuTest, TheReceiverStartsInTheFoundSatsState)
{
    HaMenu menu;
    Harness harness;
    FakeGps gps;
    menu.enter(true);

    menu.render(harness.read, harness.display);

    EXPECT_EQ(app::HaMenu::State::StartingGps, menu.state());
    EXPECT_EQ("* Found 0 sats  ", std::string(harness.display.row(1)));
}

TEST(HaMenuTest, AFreshFixSetsTheSiteAndTheClock)
{
    HaMenu menu;
    Harness harness;
    FakeGps gps;
    HaMenuOutcome outcome;
    menu.enter(true);

    gps.hasFix                   = true;
    gps.currentFix.locationValid = true;
    gps.currentFix.longitude     = -1.25;
    gps.currentFix.latitude      = 52.5;
    gps.currentFix.locationAgeMs = 1000;
    gps.currentFix.year          = 2024;
    gps.currentFix.month         = 7;
    gps.currentFix.day           = 9;
    gps.currentFix.hour          = 20;
    gps.currentFix.minute        = 30;
    gps.currentFix.second        = 15;
    gps.currentFix.satellites    = 7;

    EXPECT_EQ(MenuKeyResult::WaitForRelease,
              menu.handleKey(MenuKeyInput {Button::None, Button::None}, harness.ports, &gps, false, outcome));
    EXPECT_EQ(app::HaMenu::State::ShowingSync, menu.state());
    EXPECT_EQ(1, gps.pollCalls);
    EXPECT_NEAR(52.5, static_cast<double>(harness.siteTime.latitude().getTotalHours()), 0.001);
    EXPECT_NEAR(-1.25, static_cast<double>(harness.siteTime.longitude().getTotalHours()), 0.001);
    // UTC 20:30:15 plus the local offset of one hour.
    EXPECT_EQ(21, harness.siteTime.localTime.getHours());
    EXPECT_EQ(30, harness.siteTime.localTime.getMinutes());
    EXPECT_EQ(15, harness.siteTime.localTime.getSeconds());
}

TEST(HaMenuTest, AStaleFixIsNotApplied)
{
    HaMenu menu;
    Harness harness;
    FakeGps gps;
    HaMenuOutcome outcome;
    menu.enter(true);

    gps.hasFix                   = true;
    gps.currentFix.locationValid = true;
    gps.currentFix.longitude     = -1.25;
    gps.currentFix.locationAgeMs = 31000;  // older than the 30 s window

    menu.handleKey(MenuKeyInput {Button::None, Button::None}, harness.ports, &gps, false, outcome);

    EXPECT_EQ(app::HaMenu::State::StartingGps, menu.state());
    EXPECT_EQ(0.0, static_cast<double>(harness.siteTime.latitude().getTotalHours() - 52.5));
}

TEST(HaMenuTest, UpAndDownWalkTheThreeGpsStates)
{
    HaMenu menu;
    Harness harness;
    FakeGps gps;
    HaMenuOutcome outcome;
    menu.enter(true);

    menu.handleKey(pressed(Button::Down), harness.ports, &gps, false, outcome);
    EXPECT_EQ(app::HaMenu::State::ShowingSet, menu.state());

    menu.handleKey(pressed(Button::Down), harness.ports, &gps, false, outcome);
    EXPECT_EQ(app::HaMenu::State::ShowingSync, menu.state());

    menu.handleKey(pressed(Button::Up), harness.ports, &gps, false, outcome);
    EXPECT_EQ(app::HaMenu::State::ShowingSet, menu.state());
}

TEST(HaMenuTest, TheSyncAndSetRowsSayWhichOneIsShowing)
{
    HaMenu menu;
    Harness harness;
    FakeGps gps;
    HaMenuOutcome outcome;
    menu.enter(true);
    menu.handleKey(pressed(Button::Down), harness.ports, &gps, false, outcome);

    menu.render(harness.read, harness.display);
    EXPECT_EQ("00h 00m >Set    ", std::string(harness.display.row(1)));

    menu.handleKey(pressed(Button::Down), harness.ports, &gps, false, outcome);
    menu.render(harness.read, harness.display);
    EXPECT_EQ("00h 00m >Sync   ", std::string(harness.display.row(1)));
}

TEST(HaMenuTest, ManualEntryAdjustsTheHourOrTheMinuteOnEachPress)
{
    HaMenu menu;
    Harness harness;
    FakeGps gps;
    HaMenuOutcome outcome;
    harness.read.haTime     = core::DayTime(5, 30, 0);
    harness.siteTime.lastHa = harness.read.haTime;
    menu.enter(true);

    // StartingGps -> ShowingSet -> EnterManually.
    menu.handleKey(pressed(Button::Down), harness.ports, &gps, false, outcome);
    menu.handleKey(pressed(Button::Select), harness.ports, &gps, false, outcome);
    EXPECT_EQ(app::HaMenu::State::EnterManually, menu.state());

    menu.handleKey(pressed(Button::Up), harness.ports, &gps, false, outcome);
    EXPECT_EQ(6, harness.siteTime.lastHa.getHours());

    menu.handleKey(pressed(Button::Left), harness.ports, &gps, false, outcome);
    menu.handleKey(pressed(Button::Down), harness.ports, &gps, false, outcome);
    EXPECT_EQ(29, harness.siteTime.lastHa.getMinutes());
}

TEST(HaMenuTest, StoringFromManualEntryKeepsTheScreenOnTheSetState)
{
    HaMenu menu;
    Harness harness;
    FakeGps gps;
    HaMenuOutcome outcome;
    menu.enter(true);
    menu.handleKey(pressed(Button::Down), harness.ports, &gps, false, outcome);
    menu.handleKey(pressed(Button::Select), harness.ports, &gps, false, outcome);

    menu.handleKey(pressed(Button::Select), harness.ports, &gps, false, outcome);

    EXPECT_TRUE(outcome.haStored);
    EXPECT_EQ(app::HaMenu::State::ShowingSet, menu.state());
    EXPECT_EQ(1, harness.settings.storeHaCalls);
}

TEST(HaMenuTest, RightStopsListeningWhileTheReceiverIsStillRunning)
{
    HaMenu menu;
    Harness harness;
    FakeGps gps;
    HaMenuOutcome outcome;
    menu.enter(true);

    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Right), harness.ports, &gps, false, outcome));
    EXPECT_EQ(app::HaMenu::State::ShowingSync, menu.state());
}

TEST(HaMenuTest, WithoutAReceiverTheArrowsRepeatWhileHeld)
{
    HaMenu menu;
    Harness harness;
    HaMenuOutcome outcome;
    harness.read.haTime = core::DayTime(5, 30, 0);
    menu.enter(false);

    // A held key repeats the step; the edge is what the released / changed key produces.
    EXPECT_EQ(MenuKeyResult::Ignored, menu.handleKey(heldOnly(Button::Up), harness.ports, nullptr, false, outcome));
    EXPECT_EQ(6, harness.siteTime.lastHa.getHours());

    EXPECT_EQ(MenuKeyResult::Ignored, menu.handleKey(heldOnly(Button::Down), harness.ports, nullptr, false, outcome));
    EXPECT_EQ(4, harness.siteTime.lastHa.getHours());
    EXPECT_FALSE(outcome.haStored);
}
