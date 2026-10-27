#include <gtest/gtest.h>

#include "app/ui/screens/FocusMenu.hpp"
#include "../test_common/ports_fakes/FakeDisplay.hpp"
#include "../test_common/ports_fakes/FakeOptionalAxes.hpp"

using app::FocusMenu;
using app::MenuKeyInput;
using app::MenuKeyResult;
using fakes::FakeDisplay;
using fakes::FakeOptionalAxes;
using types::Button;

namespace
{

app::MenuKeyInput pressed(Button key)
{
    return MenuKeyInput {key, key};
}

app::MenuKeyInput held(Button key)
{
    return MenuKeyInput {Button::None, key};
}

}  // namespace

TEST(FocusMenuTest, TheHighlightRowIsTheOnlyChoice)
{
    FocusMenu menu;
    FakeOptionalAxes axes;
    FakeDisplay display;
    menu.enter();

    menu.render(axes, display);

    EXPECT_EQ(std::string(">Focus Adjust   "), std::string(display.row(1)));
}

TEST(FocusMenuTest, SelectWalksBetweenTheHighlightAndTheRateRow)
{
    FocusMenu menu;
    FakeOptionalAxes axes;
    FakeDisplay display;
    menu.enter();

    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Select), axes));
    menu.render(axes, display);
    EXPECT_EQ(std::string("Rate:  1 2 3>4<-"), std::string(display.row(1)));

    menu.handleKey(pressed(Button::Select), axes);
    menu.render(axes, display);
    EXPECT_EQ(std::string(">Focus Adjust   "), std::string(display.row(1)));
}

TEST(FocusMenuTest, RightOnTheHighlightRowAsksForTheNextScreen)
{
    FocusMenu menu;
    FakeOptionalAxes axes;
    menu.enter();

    EXPECT_EQ(MenuKeyResult::NextScreen, menu.handleKey(pressed(Button::Right), axes));
}

TEST(FocusMenuTest, HoldingUpRunsTheFocuserBackwards)
{
    FocusMenu menu;
    FakeOptionalAxes axes;
    menu.enter();
    menu.handleKey(pressed(Button::Select), axes);

    menu.handleKey(held(Button::Up), axes);

    EXPECT_EQ(1, axes.continuousCalls);
    EXPECT_EQ(types::FOCUS_BACKWARD, axes.lastDirection);
}

TEST(FocusMenuTest, HoldingDownRunsTheFocuserForwards)
{
    FocusMenu menu;
    FakeOptionalAxes axes;
    menu.enter();
    menu.handleKey(pressed(Button::Select), axes);

    menu.handleKey(held(Button::Down), axes);

    EXPECT_EQ(types::FOCUS_FORWARD, axes.lastDirection);
}

TEST(FocusMenuTest, ReleasingTheKeyStopsTheFocuser)
{
    FocusMenu menu;
    FakeOptionalAxes axes;
    menu.enter();
    menu.handleKey(pressed(Button::Select), axes);
    menu.handleKey(held(Button::Up), axes);

    menu.handleKey(held(Button::None), axes);

    EXPECT_EQ(1, axes.stopCalls);
    EXPECT_FALSE(axes.isRunningFocus());
}

TEST(FocusMenuTest, TheRateIsAdjustedAndClamped)
{
    FocusMenu menu;
    FakeOptionalAxes axes;
    menu.enter();
    menu.handleKey(pressed(Button::Select), axes);

    menu.handleKey(pressed(Button::Left), axes);
    EXPECT_EQ(3, axes.lastRate);  // one step below the default (fourth) rate

    for (int i = 0; i < 5; i++)
    {
        menu.handleKey(pressed(Button::Left), axes);
    }
    EXPECT_EQ(1, axes.lastRate);

    for (int i = 0; i < 5; i++)
    {
        menu.handleKey(pressed(Button::Right), axes);
    }
    EXPECT_EQ(4, axes.lastRate);
}

TEST(FocusMenuTest, TheRateRowMarksTheChosenRateAndTheDirection)
{
    FocusMenu menu;
    FakeOptionalAxes axes;
    menu.enter();
    menu.handleKey(pressed(Button::Select), axes);

    FakeDisplay display;
    menu.render(axes, display);
    EXPECT_EQ(std::string("Rate:  1 2 3>4<-"), std::string(display.row(1)));

    axes.running = true;
    axes.speed   = -1.0f;
    menu.render(axes, display);
    EXPECT_EQ(std::string("Rate:  1 2 3>4<~"), std::string(display.row(1)));
}
