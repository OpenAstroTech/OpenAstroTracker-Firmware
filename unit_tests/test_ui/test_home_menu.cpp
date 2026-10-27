#include <gtest/gtest.h>

#include "app/ui/screens/HomeMenu.hpp"
#include "../test_common/ports_fakes/FakeDisplay.hpp"

using app::HomeMenu;
using app::MenuKeyResult;
using fakes::FakeDisplay;
using types::Button;

namespace
{

/// A press: the key settled on this value, and it is what is held.
app::MenuKeyInput pressed(Button key)
{
    return app::MenuKeyInput {key, key};
}

/// The motion port the home screen talks to, recording what it was asked to do.
class RecordingMotion final : public ports::IMountMotion
{
  public:
    void startSlewing(int direction) override
    {
        lastSlewDirection = direction;
        startSlewingCalls++;
    }
    void stopSlewing(int) override
    {
    }
    void stopSlewing(StepperAxis) override
    {
    }
    void startSlewingToTarget() override
    {
    }
    void startSlewingToHome() override
    {
        startSlewingToHomeCalls++;
    }
    void park() override
    {
        parkCalls++;
    }
    void guidePulse(uint8_t, int) override
    {
    }
    void moveStepperBy(StepperAxis, long) override
    {
    }
    void moveStepperTo(StepperAxis, long) override
    {
    }
    bool isSlewingRAorDEC() const override
    {
        return false;
    }
    bool isSlewingTRK() const override
    {
        return tracking;
    }
    void setHome(bool) override
    {
    }
    void syncPosition(const types::DayTime &, const types::Declination &) override
    {
    }
    void setTrackingStepperPos(long) override
    {
    }
    void startDriftAlignment(int) override
    {
    }
    bool findHomeByHallSensor(StepperAxis, int, int) override
    {
        return false;
    }

    bool tracking = false;

    int startSlewingToHomeCalls = 0;
    int startSlewingCalls       = 0;
    int parkCalls               = 0;
    int lastSlewDirection       = 0;
};

}  // namespace

TEST(HomeMenuTest, FirstChoiceStartsTheHomeSlew)
{
    HomeMenu menu;
    RecordingMotion motion;
    menu.enter();

    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Select), motion));
    EXPECT_EQ(1, motion.startSlewingToHomeCalls);
    EXPECT_EQ(0, motion.startSlewingCalls);
}

TEST(HomeMenuTest, SecondChoiceUnparksWhenTheMountIsIdle)
{
    HomeMenu menu;
    RecordingMotion motion;
    menu.enter();

    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Down), motion));
    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Select), motion));

    EXPECT_EQ(0, motion.parkCalls);
    EXPECT_EQ(1, motion.startSlewingCalls);
    EXPECT_EQ(TRACKING, motion.lastSlewDirection);
}

TEST(HomeMenuTest, SecondChoiceParksWhileTracking)
{
    HomeMenu menu;
    RecordingMotion motion;
    motion.tracking = true;
    menu.enter();

    menu.handleKey(pressed(Button::Up), motion);
    menu.handleKey(pressed(Button::Select), motion);

    EXPECT_EQ(1, motion.parkCalls);
    EXPECT_EQ(0, motion.startSlewingCalls);
}

TEST(HomeMenuTest, LeftAndDownBothToggleTheChoice)
{
    HomeMenu menu;
    RecordingMotion motion;
    menu.enter();

    menu.handleKey(pressed(Button::Left), motion);
    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Select), motion));
    EXPECT_EQ(0, motion.startSlewingToHomeCalls);  // the second choice was active

    menu.handleKey(pressed(Button::Down), motion);
    menu.handleKey(pressed(Button::Select), motion);
    EXPECT_EQ(1, motion.startSlewingToHomeCalls);  // back on the first
}

TEST(HomeMenuTest, RightAsksForTheNextScreen)
{
    HomeMenu menu;
    RecordingMotion motion;

    EXPECT_EQ(MenuKeyResult::NextScreen, menu.handleKey(pressed(Button::Right), motion));
}

TEST(HomeMenuTest, NoKeyIsIgnored)
{
    HomeMenu menu;
    RecordingMotion motion;

    EXPECT_EQ(MenuKeyResult::Ignored, menu.handleKey(pressed(Button::None), motion));
}

TEST(HomeMenuTest, TheIdleRowShowsTheUnparkChoiceWithTheSelectorFirst)
{
    HomeMenu menu;
    RecordingMotion motion;
    FakeDisplay display;

    menu.render(motion, display);

    EXPECT_EQ(std::string(">Home  Unpark   "), std::string(display.row(1), 16));
}

TEST(HomeMenuTest, TheTrackingRowShowsTheParkChoice)
{
    HomeMenu menu;
    RecordingMotion motion;
    motion.tracking = true;

    FakeDisplay display;
    menu.render(motion, display);

    EXPECT_EQ(std::string(">Home  Park     "), std::string(display.row(1), 16));
}

TEST(HomeMenuTest, TheSelectorMovesToTheSecondChoice)
{
    HomeMenu menu;
    RecordingMotion motion;
    menu.handleKey(pressed(Button::Down), motion);

    FakeDisplay display;
    menu.render(motion, display);

    EXPECT_EQ(std::string(" Home >Unpark   "), std::string(display.row(1), 16));
}
