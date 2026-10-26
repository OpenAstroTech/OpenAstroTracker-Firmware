#include <gtest/gtest.h>

#include "app/ui/screens/DecMenu.hpp"
#include "app/ui/screens/RaMenu.hpp"
#include "../test_common/ports_fakes/FakeDisplay.hpp"

using app::DecMenu;
using app::MenuKeyInput;
using app::MenuKeyResult;
using app::RaMenu;
using core::Button;
using fakes::FakeDisplay;

namespace
{
app::MenuKeyInput pressed(Button key)
{
    return MenuKeyInput {key, key};
}

app::MenuKeyInput heldOnly(Button key)
{
    return MenuKeyInput {Button::None, key};
}

/// Just enough of the read model to drive the coordinate screens.
class RecordingReadModel final : public ports::IMountReadModel
{
  public:
    core::DayTime currentRA() const override
    {
        return ra;
    }
    core::Declination currentDEC() const override
    {
        return dec;
    }
    core::DayTime &targetRA() override
    {
        return ra;
    }
    core::Declination &targetDEC() override
    {
        return dec;
    }
    core::DayTime calculateHa() override
    {
        return core::DayTime();
    }
    core::DayTime calculateLst() override
    {
        return core::DayTime();
    }
    void calculateStepperPositions(float, float, long &raPos, long &decPos) override
    {
        raPos  = 0;
        decPos = 0;
    }
    float checkRALimit() override
    {
        return 0.0f;
    }
    core::DayTime hourAngle() override
    {
        return core::DayTime();
    }
    long getCurrentStepperPosition(StepperAxis) override
    {
        return 0;
    }
    long getCurrentStepperPosition(int) override
    {
        return 0;
    }
    bool isAxisRunning(StepperAxis) override
    {
        return false;
    }
    bool isSlewingIdle() const override
    {
        return idle;
    }
    bool getStepperProgress(int &raPercentage, int &decPercentage) override
    {
        raPercentage  = 100;
        decPercentage = 100;
        return false;
    }
    float plannedRaSteps() const override
    {
        return 0.0f;
    }
    float plannedDecSteps() const override
    {
        return 0.0f;
    }
    bool isBootComplete() override
    {
        return true;
    }
    bool isGuiding() const override
    {
        return false;
    }
    bool isNorthernHemisphere() const override
    {
        return true;
    }
    const char *raText(uint8_t, uint8_t) override
    {
        return raTextValue;
    }
    const char *decText(uint8_t, uint8_t) override
    {
        return decTextValue;
    }
    const char *getStatusString() override
    {
        return "";
    }
    const char *getStepperInfo() override
    {
        return "";
    }
    const char *getMountHardwareInfo() override
    {
        return "";
    }
    const char *getAutoHomingStates() const override
    {
        return "";
    }
    long getNumCommandsReceived() override
    {
        return 0;
    }
    void commandReceived() override
    {
    }

    core::DayTime ra;
    core::Declination dec;
    bool idle                = true;
    const char *raTextValue  = "12:34:56#....";
    const char *decTextValue = "  45*06:07#..";
};

/// A clock that never advances is enough: `delay()` then returns on its first check.
class NeverAdvancingClock final : public ports::IClock
{
  public:
    explicit NeverAdvancingClock(uint32_t start = 1000) : nowMs(start)
    {
    }
    uint32_t now() override
    {
        return nowMs += 1000;  // always past the deadline, so the wait ends immediately
    }
    void delay(uint32_t) override
    {
    }
    uint32_t nowMs;
};

class YesScheduler final : public ports::IScheduler
{
  public:
    void yield() override
    {
    }
};

class ScreenMotion final : public ports::IMountMotion
{
  public:
    void startSlewingToTarget() override
    {
        startToTargetCalls++;
    }
    void stopSlewing(int direction) override
    {
        stopCalls++;
        lastStopDirection = direction;
    }
    bool isSlewingRAorDEC() const override
    {
        return slewing;
    }
    void stopSlewing(StepperAxis) override
    {
    }
    void startSlewingToHome() override
    {
    }
    void startSlewing(int) override
    {
    }
    void park() override
    {
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
    bool isSlewingTRK() const override
    {
        return false;
    }
    void setHome(bool) override
    {
    }
    void syncPosition(const core::DayTime &, const core::Declination &) override
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

    bool slewing           = false;
    int startToTargetCalls = 0;
    int stopCalls          = 0;
    int lastStopDirection  = 0;
};

struct Fixture {
    RecordingReadModel read;
    ScreenMotion motion;
    NeverAdvancingClock clock;
    YesScheduler scheduler;
    app::UiPorts ports;

    Fixture()
    {
        ports.motion    = &motion;
        ports.read      = &read;
        ports.clock     = &clock;
        ports.scheduler = &scheduler;
    }
};

}  // namespace

TEST(CoordinateMenuTest, UpAndDownStepTheSelectedFieldOfTheTarget)
{
    Fixture f;
    RaMenu menu;

    menu.handleKey(heldOnly(Button::Up), f.ports);
    EXPECT_EQ(1, f.read.targetRA().getHours());

    menu.handleKey(heldOnly(Button::Down), f.ports);
    menu.handleKey(heldOnly(Button::Down), f.ports);
    // The coordinate wraps like the real one: an hour below zero is 23.
    EXPECT_EQ(23, f.read.targetRA().getHours());
}

TEST(CoordinateMenuTest, LeftMovesTheFieldCursorAndWraps)
{
    Fixture f;
    RaMenu menu;
    FakeDisplay display;

    // Field 0 shows the full readout with the cursor on the marker column only at field 3.
    menu.render(f.ports, display);
    EXPECT_EQ(std::string("12:34:56#... Ta"), std::string(display.row(1), 15));

    for (int i = 0; i < 3; i++)
    {
        menu.handleKey(pressed(Button::Left), f.ports);
    }
    menu.render(f.ports, display);
    EXPECT_EQ(std::string("12:34:56#...>Ta"), std::string(display.row(1), 15));

    menu.handleKey(pressed(Button::Left), f.ports);
    menu.render(f.ports, display);
    EXPECT_EQ(std::string("12:34:56#... Ta"), std::string(display.row(1), 15));
}

TEST(CoordinateMenuTest, TheLastFieldTogglesTargetAndCurrent)
{
    Fixture f;
    RaMenu menu;
    FakeDisplay display;

    for (int i = 0; i < 3; i++)
    {
        menu.handleKey(pressed(Button::Left), f.ports);
    }
    menu.handleKey(heldOnly(Button::Up), f.ports);

    menu.render(f.ports, display);
    EXPECT_EQ(std::string("12:34:56#...>Cu"), std::string(display.row(1), 15));
}

TEST(CoordinateMenuTest, SelectStartsTheSlewWhenNothingIsMoving)
{
    Fixture f;
    RaMenu menu;

    EXPECT_EQ(MenuKeyResult::WaitForRelease, menu.handleKey(pressed(Button::Select), f.ports));
    EXPECT_EQ(1, f.motion.startToTargetCalls);
    EXPECT_EQ(0, f.motion.stopCalls);
}

TEST(CoordinateMenuTest, SelectStopsTheSlewWhenOneIsRunning)
{
    Fixture f;
    f.motion.slewing = true;
    RaMenu menu;

    menu.handleKey(pressed(Button::Select), f.ports);

    EXPECT_EQ(1, f.motion.stopCalls);
    EXPECT_EQ(ALL_DIRECTIONS, f.motion.lastStopDirection);
    EXPECT_EQ(0, f.motion.startToTargetCalls);
}

TEST(CoordinateMenuTest, RightAsksForTheNextScreen)
{
    Fixture f;
    RaMenu menu;

    EXPECT_EQ(MenuKeyResult::NextScreen, menu.handleKey(pressed(Button::Right), f.ports));
}

TEST(CoordinateMenuTest, TheDecRowIsOneColumnWider)
{
    Fixture f;
    DecMenu menu;
    FakeDisplay display;

    menu.render(f.ports, display);

    // 13 readout columns, the cursor, then the marker: 16 columns in total.
    EXPECT_EQ(std::string("  45*06:07#.. Ta"), std::string(display.row(1), 16));
}

TEST(CoordinateMenuTest, TheRowIsNotPaintedWhileTheMountIsSlewing)
{
    Fixture f;
    f.read.idle = false;
    RaMenu menu;
    FakeDisplay display;

    menu.render(f.ports, display);

    EXPECT_EQ(0, display.lineWrites);
}

TEST(CoordinateMenuTest, DecEditsDegrees)
{
    Fixture f;
    DecMenu menu;

    menu.handleKey(heldOnly(Button::Up), f.ports);

    EXPECT_EQ(1, f.read.targetDEC().getHours());
}
