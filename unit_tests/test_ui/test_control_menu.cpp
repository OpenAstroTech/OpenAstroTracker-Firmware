#include <gtest/gtest.h>

#include <string>

#include "app/ui/screens/ControlMenu.hpp"
#include "../test_common/ports_fakes/FakeDisplay.hpp"
#include "../test_common/ports_fakes/FakeMountMotion.hpp"
#include "../test_common/ports_fakes/FakeStatusSources.hpp"

using app::ControlMenu;
using app::ControlMenuOutcome;
using app::MenuKeyInput;
using app::MenuKeyResult;
using core::Button;
using fakes::FakeDisplay;
using fakes::FakeMountMotion;
using fakes::FakeMountReadModel;

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

constexpr bool kNotAwaitingPole = false;

/// Walks the dashboard to the point where manual slewing is live.
void enterManual(ControlMenu &menu, FakeMountMotion &motion)
{
    menu.handleKey(pressed(Button::Select), motion, false, kNotAwaitingPole);
}

}  // namespace

TEST(ControlMenuTest, TheFirstRowOffersManualSlewing)
{
    ControlMenu menu;
    FakeMountReadModel read;
    FakeDisplay display;
    menu.enter();

    menu.render(read, false, 10000, display);

    EXPECT_EQ(">Manual slewing ", std::string(display.row(1)));
}

TEST(ControlMenuTest, UpAndDownSwapTheTwoHighlights)
{
    ControlMenu menu;
    FakeMountMotion motion;
    FakeMountReadModel read;
    FakeDisplay display;
    menu.enter();

    menu.handleKey(pressed(Button::Down), motion, false, kNotAwaitingPole);
    menu.render(read, false, 10000, display);
    EXPECT_EQ(">Serial display ", std::string(display.row(1)));

    menu.handleKey(pressed(Button::Up), motion, false, kNotAwaitingPole);
    menu.render(read, false, 10000, display);
    EXPECT_EQ(">Manual slewing ", std::string(display.row(1)));
}

TEST(ControlMenuTest, SelectTogglesSerialModeAndRightTurnsItOff)
{
    ControlMenu menu;
    FakeMountMotion motion;
    menu.enter();
    menu.handleKey(pressed(Button::Down), motion, false, kNotAwaitingPole);

    ControlMenuOutcome outcome = menu.handleKey(pressed(Button::Select), motion, false, kNotAwaitingPole);
    EXPECT_TRUE(outcome.serialChanged);
    EXPECT_TRUE(outcome.serialActive);

    outcome = menu.handleKey(pressed(Button::Right), motion, true, kNotAwaitingPole);
    EXPECT_EQ(MenuKeyResult::NextScreen, outcome.result);
    EXPECT_TRUE(outcome.serialChanged);
    EXPECT_FALSE(outcome.serialActive);
}

TEST(ControlMenuTest, TheHeldKeyCommandsTheAxesOncePerChange)
{
    ControlMenu menu;
    FakeMountMotion motion;
    menu.enter();
    enterManual(menu, motion);

    // The first pass with a new held key commands it immediately, as the legacy screen did.
    menu.handleKey(heldOnly(Button::Up), motion, false, kNotAwaitingPole);
    EXPECT_EQ(1, motion.startSlewingCalls);
    EXPECT_EQ(NORTH, motion.lastSlewingDirection);
    EXPECT_GE(motion.stopSlewingAnyCalls, 1);

    // Holding the same key does not re-command it.
    menu.handleKey(heldOnly(Button::Up), motion, false, kNotAwaitingPole);
    menu.handleKey(heldOnly(Button::Up), motion, false, kNotAwaitingPole);
    EXPECT_EQ(1, motion.startSlewingCalls);

    // A release stops the axes, and commands no direction.
    const int stopsBefore = motion.stopSlewingAnyCalls;
    menu.handleKey(heldOnly(Button::None), motion, false, kNotAwaitingPole);
    EXPECT_EQ(1, motion.startSlewingCalls);
    EXPECT_EQ(stopsBefore + 1, motion.stopSlewingAnyCalls);
}

TEST(ControlMenuTest, EachArrowNamesADirection)
{
    const struct
    {
        Button key;
        int direction;
    } cases[] = {
        {Button::Up, NORTH},
        {Button::Down, SOUTH},
        {Button::Left, WEST},
        {Button::Right, EAST},
    };

    for (const auto &testCase : cases)
    {
        ControlMenu menu;
        FakeMountMotion motion;
        menu.enter();
        enterManual(menu, motion);

        menu.handleKey(heldOnly(testCase.key), motion, false, kNotAwaitingPole);

        EXPECT_EQ(1, motion.startSlewingCalls);
        EXPECT_EQ(testCase.direction, motion.lastSlewingDirection);
    }
}

TEST(ControlMenuTest, SelectInManualAsksWhetherToSetHome)
{
    ControlMenu menu;
    FakeMountMotion motion;
    FakeMountReadModel read;
    FakeDisplay display;
    menu.enter();
    enterManual(menu, motion);

    const ControlMenuOutcome outcome = menu.handleKey(pressed(Button::Select), motion, false, kNotAwaitingPole);
    EXPECT_EQ(MenuKeyResult::WaitForRelease, outcome.result);
    EXPECT_TRUE(outcome.suspendMenuUpdate);

    menu.render(read, false, 10000, display);
    EXPECT_EQ("Set home pos?   ", std::string(display.row(0)));
    EXPECT_EQ(">Yes< No        ", std::string(display.row(1)));
}

TEST(ControlMenuTest, LeftMovesTheHomePromptToNo)
{
    ControlMenu menu;
    FakeMountMotion motion;
    FakeMountReadModel read;
    FakeDisplay display;
    menu.enter();
    enterManual(menu, motion);
    menu.handleKey(pressed(Button::Select), motion, false, kNotAwaitingPole);
    menu.handleKey(pressed(Button::Left), motion, false, kNotAwaitingPole);

    menu.render(read, false, 10000, display);
    EXPECT_EQ(" Yes >No<       ", std::string(display.row(1)));
}

TEST(ControlMenuTest, ConfirmingYesSetsHomeAndLeavesTheScreen)
{
    ControlMenu menu;
    FakeMountMotion motion;
    menu.enter();
    enterManual(menu, motion);
    menu.handleKey(pressed(Button::Select), motion, false, kNotAwaitingPole);

    const ControlMenuOutcome outcome = menu.handleKey(pressed(Button::Select), motion, false, kNotAwaitingPole);
    EXPECT_EQ(MenuKeyResult::NextScreen, outcome.result);
    EXPECT_TRUE(outcome.resumeMenuUpdate);
    EXPECT_FALSE(outcome.resumeStartup);
    EXPECT_EQ(1, motion.setHomeCalls);
    EXPECT_TRUE(motion.lastSetHomeClearsZeroPos);
    EXPECT_EQ(1, motion.startSlewingCalls);
    EXPECT_EQ(TRACKING, motion.lastSlewingDirection);
}

TEST(ControlMenuTest, ConfirmingNoLeavesWithoutTouchingHome)
{
    ControlMenu menu;
    FakeMountMotion motion;
    menu.enter();
    enterManual(menu, motion);
    menu.handleKey(pressed(Button::Select), motion, false, kNotAwaitingPole);
    menu.handleKey(pressed(Button::Left), motion, false, kNotAwaitingPole);

    const ControlMenuOutcome outcome = menu.handleKey(pressed(Button::Select), motion, false, kNotAwaitingPole);
    EXPECT_EQ(MenuKeyResult::NextScreen, outcome.result);
    EXPECT_EQ(0, motion.setHomeCalls);
    EXPECT_EQ(0, motion.startSlewingCalls);
}

TEST(ControlMenuTest, DuringTheStartupPoleAlignmentSelectResumesTheWizard)
{
    ControlMenu menu;
    FakeMountMotion motion;
    menu.enter();
    enterManual(menu, motion);

    const ControlMenuOutcome outcome = menu.handleKey(pressed(Button::Select), motion, false, true);
    EXPECT_TRUE(outcome.resumeStartup);
    EXPECT_FALSE(outcome.suspendMenuUpdate);
    EXPECT_EQ(MenuKeyResult::WaitForRelease, outcome.result);
}

TEST(ControlMenuTest, TheManualStateShowsTheSlewView)
{
    ControlMenu menu;
    FakeMountMotion motion;
    FakeMountReadModel read;
    FakeDisplay display;
    read.plannedRaStepsValue = 100.0F;
    menu.enter();
    menu.startManual();

    menu.render(read, false, 10000, display);

    EXPECT_EQ("R 01:02:03 100% ", std::string(display.row(1)));
}
