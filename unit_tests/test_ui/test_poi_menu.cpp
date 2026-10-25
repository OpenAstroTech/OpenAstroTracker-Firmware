#include <gtest/gtest.h>

#include <string>

#include "app/ui/PointOfInterestTable.hpp"
#include "app/ui/screens/PoiMenu.hpp"
#include "../test_common/ports_fakes/FakeDisplay.hpp"
#include "../test_common/ports_fakes/FakeMountMotion.hpp"
#include "../test_common/ports_fakes/FakeStatusSources.hpp"

using app::MenuKeyInput;
using app::MenuKeyResult;
using app::PoiMenu;
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

/// The north-hemisphere convention, which is what the default site gives.
constexpr bool kNorth = true;

}  // namespace

TEST(PoiMenuTest, TheFirstObjectIsPolaris)
{
    PoiMenu menu;
    FakeMountReadModel read;
    FakeDisplay display;

    menu.render(read, display);

    EXPECT_EQ(">Polaris        ", std::string(display.row(1)));
}

TEST(PoiMenuTest, DownAndLeftWalkForwardAndUpWalksBack)
{
    PoiMenu menu;
    FakeMountReadModel read;
    FakeMountMotion motion;

    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Down), read, motion, kNorth));
    EXPECT_EQ(1, menu.selected());
    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Left), read, motion, kNorth));
    EXPECT_EQ(2, menu.selected());
    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Up), read, motion, kNorth));
    EXPECT_EQ(1, menu.selected());
}

TEST(PoiMenuTest, TheListWrapsOntoTheParkCommandAndBack)
{
    PoiMenu menu;
    FakeMountReadModel read;
    FakeMountMotion motion;

    menu.handleKey(pressed(Button::Up), read, motion, kNorth);
    EXPECT_EQ(app::kParkPointOfInterest, menu.selected());

    menu.handleKey(pressed(Button::Down), read, motion, kNorth);
    EXPECT_EQ(0, menu.selected());
}

TEST(PoiMenuTest, SelectingAnObjectWritesTheTargetAndStartsTheGoTo)
{
    PoiMenu menu;
    FakeMountReadModel read;
    FakeMountMotion motion;

    // Altair: RA 19h51m45s, DEC +8°55'15".
    menu.handleKey(pressed(Button::Down), read, motion, kNorth);
    ASSERT_EQ(1, menu.selected());

    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Select), read, motion, kNorth));
    EXPECT_EQ(1, motion.stopSlewingAnyCalls);  // the legacy screen stopped both axes first
    EXPECT_EQ(1, motion.slewToTargetCalls);

    EXPECT_EQ(19, read.targetRaValue.getHours());
    EXPECT_EQ(51, read.targetRaValue.getMinutes());
    EXPECT_EQ(45, read.targetRaValue.getSeconds());

    // +8°55'15" celestial is 90 - 8.9208 = 81.0792° of axis declination in the north.
    EXPECT_NEAR(
        ((90.0 * 3600.0) - ((8.0 * 3600.0) + (55.0 * 60.0) + 15.0)), static_cast<double>(read.targetDecValue.getTotalSeconds()), 1.0);
}

TEST(PoiMenuTest, ANegativeDeclinationKeepsItsSign)
{
    PoiMenu menu;
    FakeMountReadModel read;
    FakeMountMotion motion;

    // Lagoon Nebula: DEC -24°22'52".
    for (int i = 0; i < 8; i++)
    {
        menu.handleKey(pressed(Button::Down), read, motion, kNorth);
    }
    ASSERT_EQ(8, menu.selected());

    menu.handleKey(pressed(Button::Select), read, motion, kNorth);

    const double celestial = -(((24.0 * 3600.0) + (22.0 * 60.0) + 52.0));
    const double expected  = (90.0 * 3600.0) - celestial;
    EXPECT_NEAR(expected, static_cast<double>(read.targetDecValue.getTotalSeconds()), 1.0);
}

TEST(PoiMenuTest, TheSouthernHemisphereMirrorsTheAxisDeclination)
{
    PoiMenu menu;
    FakeMountReadModel read;
    FakeMountMotion motion;

    menu.handleKey(pressed(Button::Down), read, motion, !kNorth);
    menu.handleKey(pressed(Button::Select), read, motion, !kNorth);

    // Same magnitude, the other sign convention: 90 + 8.9208°.
    const double expected = (90.0 * 3600.0) + ((8.0 * 3600.0) + (55.0 * 60.0) + 15.0);
    EXPECT_NEAR(expected, static_cast<double>(read.targetDecValue.getTotalSeconds()), 1.0);
}

TEST(PoiMenuTest, TheThreeTrailingEntriesAreCommands)
{
    PoiMenu menu;
    FakeMountReadModel read;
    FakeMountMotion motion;

    // Home.
    for (int i = 0; i < app::kHomePointOfInterest; i++)
    {
        menu.handleKey(pressed(Button::Down), read, motion, kNorth);
    }
    EXPECT_EQ(app::kHomePointOfInterest, menu.selected());
    menu.handleKey(pressed(Button::Select), read, motion, kNorth);
    EXPECT_EQ(1, motion.slewToHomeCalls);
    EXPECT_EQ(0, motion.slewToTargetCalls);

    // Unpark resumes tracking.
    menu.handleKey(pressed(Button::Down), read, motion, kNorth);
    menu.handleKey(pressed(Button::Select), read, motion, kNorth);
    EXPECT_EQ(1, motion.startSlewingCalls);
    EXPECT_EQ(TRACKING, motion.lastSlewingDirection);

    // Park.
    menu.handleKey(pressed(Button::Down), read, motion, kNorth);
    menu.handleKey(pressed(Button::Select), read, motion, kNorth);
    EXPECT_EQ(1, motion.parkCalls);
    EXPECT_EQ(0, motion.slewToTargetCalls);
}

TEST(PoiMenuTest, TheRowIsNotDrawnWhileAnAxisMoves)
{
    PoiMenu menu;
    FakeMountReadModel read;
    FakeDisplay display;
    read.slewingIdle = false;

    menu.render(read, display);

    EXPECT_EQ(0, display.lineWrites);
}

TEST(PoiMenuTest, RightLeavesTheScreen)
{
    PoiMenu menu;
    FakeMountReadModel read;
    FakeMountMotion motion;

    EXPECT_EQ(MenuKeyResult::NextScreen, menu.handleKey(pressed(Button::Right), read, motion, kNorth));
    EXPECT_EQ(MenuKeyResult::Ignored, menu.handleKey(pressed(Button::None), read, motion, kNorth));
}
