#include <gtest/gtest.h>

#include <string>

#include "app/ui/screens/CalibrationMenu.hpp"
#include "../test_common/ports_fakes/FakeDisplay.hpp"
#include "../test_common/ports_fakes/FakeMountMotion.hpp"
#include "../test_common/ports_fakes/FakeOptionalAxes.hpp"
#include "../test_common/ports_fakes/FakeStatusSources.hpp"

using app::CalibrationMenu;
using app::CalibrationOutcome;
using app::MenuKeyInput;
using app::MenuKeyResult;
using types::Button;
using fakes::FakeDisplay;
using fakes::FakeMountConfiguration;
using fakes::FakeMountMotion;
using fakes::FakeMountReadModel;
using fakes::FakeOptionalAxes;
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

app::CalibrationMenu::Board plainBoard()
{
    return app::CalibrationMenu::Board {};
}

/// Everything the screen reads, plus a clock whose reads advance so `UiPorts::delay()` terminates.
/// A level sensor the test drives.
class FakeGyro final : public ports::IGyro
{
  public:
    bool begin() override
    {
        beginCalls++;
        return true;
    }
    void shutdown() override
    {
    }
    bool isPresent() override
    {
        return true;
    }
    ports::GyroAngles angles() override
    {
        return current;
    }
    float temperature() override
    {
        return 21.0F;
    }

    ports::GyroAngles current {0.0F, 0.0F};
    int beginCalls = 0;
};

struct Harness {
    FakeMountReadModel read;
    FakeMountConfiguration config;
    FakeSiteTime siteTime;
    FakeOptionalAxes axes;
    FakeMountMotion motion;
    FakeDisplay display;
    app::UiPorts ports;

    Harness()
    {
        ports.motion    = &motion;
        ports.config    = &config;
        ports.read      = &read;
        ports.siteTime  = &siteTime;
        ports.axes      = &axes;
        ports.clock     = &clock;
        ports.scheduler = &scheduler;
    }

    struct Clock final : public ports::IClock {
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

    struct Scheduler final : public ports::IScheduler {
        void yield() override
        {
        }
    } scheduler;
};

}  // namespace

TEST(CalibrationMenuTest, TheRingStartsWithPolarAndSkipsWhatTheBoardLacks)
{
    CalibrationMenu menu(plainBoard());
    Harness harness;

    menu.render(harness.ports, nullptr, harness.display);
    EXPECT_EQ(">Polar alignment", std::string(harness.display.row(1)));

    // No drift, no AZ/ALT, no level sensor: the next entry is the speed calibration.
    menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);
    menu.render(harness.ports, nullptr, harness.display);
    EXPECT_EQ(">Speed calibratn", std::string(harness.display.row(1)));
}

TEST(CalibrationMenuTest, TheRingWrapsAtBothEnds)
{
    CalibrationMenu menu(plainBoard());
    Harness harness;

    menu.handleKey(pressed(Button::Up), harness.ports, nullptr, harness.display, false);
    menu.render(harness.ports, nullptr, harness.display);
    EXPECT_EQ(">LCD Brightness ", std::string(harness.display.row(1)));

    menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);
    menu.render(harness.ports, nullptr, harness.display);
    EXPECT_EQ(">Polar alignment", std::string(harness.display.row(1)));
}

TEST(CalibrationMenuTest, TheRingIncludesTheExtraEntriesWhenTheBoardHasThem)
{
    CalibrationMenu::Board board;
    board.hasDriftAlignment = true;
    board.hasAzimuth        = true;
    board.hasAltitude       = true;
    board.hasLevelSensor    = true;

    CalibrationMenu menu(board);
    Harness harness;

    // Polar, Drift, Speed, ... Azimuth, Altitude, Roll, Pitch, Backlight.
    menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);
    menu.render(harness.ports, nullptr, harness.display);
    EXPECT_EQ(">Drift alignment", std::string(harness.display.row(1)));

    // From Drift, the rest of the ring is Speed, RA, DEC, Backlash, lower, upper, UTC, Azimuth,
    // Altitude, Roll, Pitch — eleven more steps.
    for (int i = 0; i < 11; i++)
    {
        menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);
    }
    menu.render(harness.ports, nullptr, harness.display);
    EXPECT_EQ(">Pitch Offset   ", std::string(harness.display.row(1)));
}

TEST(CalibrationMenuTest, SelectOpensTheSpeedEditorWhichPreviewsAndStores)
{
    CalibrationMenu menu(plainBoard());
    Harness harness;
    harness.config.speedCalibration = 1.0025F;
    menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);  // speed
    EXPECT_EQ(CalibrationMenu::State::HighlightSpeed, menu.state());

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    EXPECT_EQ(CalibrationMenu::State::SpeedCalibration, menu.state());

    // One step is a ten-thousandth of the factor; the preview is not persisted.
    // The editor works in ten-thousandths of the factor and rounds the seed up, so 1.0025 loads as
    // 25.5 and one step lands on 1.00265 — the legacy arithmetic, float rounding included.
    menu.handleKey(heldOnly(Button::Up), harness.ports, nullptr, harness.display, false);
    EXPECT_NEAR(1.00265, static_cast<double>(harness.config.speedCalibration), 1e-6);
    EXPECT_EQ(1, harness.config.liveSpeedCalls);

    const CalibrationOutcome outcome = menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    EXPECT_STREQ("Speed Stored.", outcome.messages[0]);
    EXPECT_EQ(1, harness.config.persistedSpeedCalls);
    EXPECT_EQ(CalibrationMenu::State::HighlightSpeed, menu.state());
}

TEST(CalibrationMenuTest, RightLeavesTheScreenAndShutsTheSensorDown)
{
    CalibrationMenu::Board board;
    board.hasLevelSensor = true;
    CalibrationMenu menu(board);
    Harness harness;

    const CalibrationOutcome outcome = menu.handleKey(pressed(Button::Right), harness.ports, nullptr, harness.display, false);

    EXPECT_EQ(MenuKeyResult::NextScreen, outcome.result);
    EXPECT_TRUE(outcome.shutdownGyro);
    EXPECT_EQ(CalibrationMenu::State::HighlightPolar, menu.state());
}

TEST(CalibrationMenuTest, TheStepsPerDegreeEntriesEditAtTenthResolution)
{
    CalibrationMenu menu(plainBoard());
    Harness harness;
    harness.config.raStepsPerDegree = 52.0F;
    menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);  // speed
    menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);  // RA steps
    EXPECT_EQ(CalibrationMenu::State::HighlightRaSteps, menu.state());

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    menu.handleKey(heldOnly(Button::Up), harness.ports, nullptr, harness.display, false);
    menu.render(harness.ports, nullptr, harness.display);
    EXPECT_EQ("RA Steps: 52.1  ", std::string(harness.display.row(1)));

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    EXPECT_NEAR(52.1, static_cast<double>(harness.config.raStepsPerDegree), 1e-4);
}

TEST(CalibrationMenuTest, TheDecLimitPromptOffersSetClearAndCancel)
{
    CalibrationMenu menu(plainBoard());
    Harness harness;
    // Polar -> Speed -> RA -> DEC -> Backlash -> lower limit.
    for (int i = 0; i < 5; i++)
    {
        menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);
    }
    EXPECT_EQ(CalibrationMenu::State::HighlightDecLowerLimit, menu.state());

    const CalibrationOutcome outcome = menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    EXPECT_TRUE(outcome.suspendMenuUpdate);
    menu.render(harness.ports, nullptr, harness.display);
    EXPECT_EQ("DEC Lower Limit ", std::string(harness.display.row(0)));
    EXPECT_EQ(">Set  Clr  Cancl", std::string(harness.display.row(1)));

    // Cancel explains what to do instead of setting a limit.
    menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);
    menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);
    const CalibrationOutcome cancelled = menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    EXPECT_STREQ("Use CTRL to move", cancelled.messages[0]);
    EXPECT_STREQ("OAT to limit,", cancelled.messages[1]);
    EXPECT_TRUE(cancelled.resumeMenuUpdate);
    EXPECT_EQ(CalibrationMenu::State::HighlightDecLowerLimit, menu.state());
}

TEST(CalibrationMenuTest, SettingTheUpperLimitTellsTheConfigurationItIsTheUpperOne)
{
    CalibrationMenu menu(plainBoard());
    Harness harness;
    for (int i = 0; i < 6; i++)
    {
        menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);
    }
    EXPECT_EQ(CalibrationMenu::State::HighlightDecUpperLimit, menu.state());

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);

    EXPECT_TRUE(harness.config.lastDecLimitWasUpper);
    EXPECT_EQ(1, harness.config.setDecLimitCalls);
}

TEST(CalibrationMenuTest, TheUtcOffsetIsEditedThenStored)
{
    CalibrationMenu menu(plainBoard());
    Harness harness;
    harness.siteTime.utcOffset = 2;
    for (int i = 0; i < 7; i++)
    {
        menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);
    }
    EXPECT_EQ(CalibrationMenu::State::HighlightUtcOffset, menu.state());

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    menu.handleKey(heldOnly(Button::Up), harness.ports, nullptr, harness.display, false);
    menu.render(harness.ports, nullptr, harness.display);
    EXPECT_EQ("UTC : 3         ", std::string(harness.display.row(1)));

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    EXPECT_EQ(3, harness.siteTime.utcOffset);
}

TEST(CalibrationMenuTest, TheBacklightEntryPreviewsAndPersists)
{
    CalibrationMenu menu(plainBoard());
    Harness harness;
    harness.display.backlightLevel = 100;
    menu.handleKey(pressed(Button::Up), harness.ports, nullptr, harness.display, false);  // wrap to backlight
    EXPECT_EQ(CalibrationMenu::State::HighlightBacklight, menu.state());

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    menu.handleKey(heldOnly(Button::Down), harness.ports, nullptr, harness.display, false);
    EXPECT_EQ(99, harness.display.backlightLevel);
    EXPECT_EQ(1, harness.display.previewBacklightCalls);

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    EXPECT_EQ(1, harness.display.persistBacklightCalls);
}

TEST(CalibrationMenuTest, TheAzimuthEntryNudgesTheAxisByArcMinutes)
{
    CalibrationMenu::Board board;
    board.hasAzimuth = true;
    CalibrationMenu menu(board);
    Harness harness;

    // Polar -> Speed -> RA -> DEC -> Backlash -> lower -> upper -> UTC -> Azimuth.
    for (int i = 0; i < 8; i++)
    {
        menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);
    }
    EXPECT_EQ(CalibrationMenu::State::HighlightAzimuth, menu.state());

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    menu.handleKey(heldOnly(Button::Up), harness.ports, nullptr, harness.display, false);
    menu.render(harness.ports, nullptr, harness.display);
    EXPECT_EQ("Az: 1 arcmins   ", std::string(harness.display.row(1)));

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    EXPECT_EQ(1, harness.axes.moveByCalls);
    EXPECT_EQ(AZIMUTH_STEPS, harness.axes.lastMoveDirection);
    EXPECT_NEAR(1.0, static_cast<double>(harness.axes.lastArcMinutes), 1e-6);
}

TEST(CalibrationMenuTest, ThePolarStepTargetsPolarisAndParksAfterwards)
{
    CalibrationMenu menu(plainBoard());
    Harness harness;
    harness.siteTime.siteLatitude = types::Latitude(52.5F);

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    EXPECT_EQ(CalibrationMenu::State::PolarWaitCenter, menu.state());
    EXPECT_EQ(1, harness.motion.slewToTargetCalls);
    EXPECT_EQ(3, harness.read.targetRaValue.getHours());
    // 88 - 90 degrees plus 42'11", with the sign taken from the degrees alone — the DEC axis ends up
    // along the pole-to-Polaris line. `Declination(-2, 42, 11)` is sign-magnitude, so it is
    // -(2 deg 42' 11"), exactly what the legacy expression produced.
    EXPECT_EQ(-((2 * 3600) + (42 * 60) + 11), harness.read.targetDecValue.getTotalSeconds());

    harness.motion.slewing           = false;
    const CalibrationOutcome outcome = menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    EXPECT_STREQ("Aligned, homing", outcome.messages[0]);
    EXPECT_EQ(1, harness.motion.syncCalls);
    EXPECT_EQ(1, harness.motion.slewToHomeCalls);
    EXPECT_EQ(CalibrationMenu::State::PolarWaitHome, menu.state());

    // Once the axes stop the screen returns to the menu on its own.
    harness.motion.slewing = false;
    const CalibrationOutcome parked
        = menu.handleKey(MenuKeyInput {Button::None, Button::None}, harness.ports, nullptr, harness.display, false);
    EXPECT_TRUE(parked.resumeMenuUpdate);
    EXPECT_EQ(CalibrationMenu::State::HighlightPolar, menu.state());
}

TEST(CalibrationMenuTest, TheRollStepGoesStraightToTheReadoutDuringStartup)
{
    CalibrationMenu::Board board;
    board.hasLevelSensor = true;
    CalibrationMenu menu(board);
    Harness harness;

    CalibrationOutcome outcome = menu.handleKey(MenuKeyInput {Button::None, Button::None}, harness.ports, nullptr, harness.display, true);
    EXPECT_EQ(CalibrationMenu::State::RollOffsetCalibration, menu.state());

    outcome = menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, true);
    EXPECT_TRUE(outcome.resumeStartup);
    EXPECT_TRUE(outcome.shutdownGyro);
    EXPECT_EQ(MenuKeyResult::NextScreen, outcome.result);
}

TEST(CalibrationMenuTest, TheRollConfirmAppliesTheSensorAngleOnlyWhenAnsweredYes)
{
    CalibrationMenu::Board board;
    board.hasLevelSensor = true;
    CalibrationMenu menu(board);
    Harness harness;
    FakeGyro gyro;
    gyro.current                        = {0.75F, 0.5F};  // pitch, roll: half a degree of roll against the 1.5 stored
    harness.config.rollCalibrationAngle = 1.5F;

    // Walk to the roll entry: Polar, Speed, RA, DEC, Backlash, lower, upper, UTC, Roll.
    for (int i = 0; i < 8; i++)
    {
        menu.handleKey(pressed(Button::Down), harness.ports, &gyro, harness.display, false);
    }
    EXPECT_EQ(CalibrationMenu::State::HighlightRoll, menu.state());

    menu.handleKey(pressed(Button::Select), harness.ports, &gyro, harness.display, false);
    EXPECT_EQ(CalibrationMenu::State::RollOffsetCalibration, menu.state());
    menu.render(harness.ports, &gyro, harness.display);
    // The readout is the sensor minus the stored offset: 0.5 - 1.5 = -1.0, marked one cell left.
    EXPECT_EQ("R: ->---1.0-----", std::string(harness.display.row(1)));

    const CalibrationOutcome prompt = menu.handleKey(pressed(Button::Select), harness.ports, &gyro, harness.display, false);
    EXPECT_TRUE(prompt.suspendMenuUpdate);
    menu.render(harness.ports, &gyro, harness.display);
    EXPECT_EQ(">Yes< No        ", std::string(harness.display.row(1)));

    const CalibrationOutcome stored = menu.handleKey(pressed(Button::Select), harness.ports, &gyro, harness.display, false);
    EXPECT_TRUE(stored.resumeMenuUpdate);
    EXPECT_EQ(1, harness.config.setRollCalls);
    EXPECT_EQ(CalibrationMenu::State::HighlightRoll, menu.state());
}

TEST(CalibrationMenuTest, TheDriftDurationsAreTheFourLegacyChoices)
{
    CalibrationMenu::Board board;
    board.hasDriftAlignment = true;
    CalibrationMenu menu(board);
    Harness harness;

    menu.handleKey(pressed(Button::Down), harness.ports, nullptr, harness.display, false);    // drift
    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);  // durations
    EXPECT_EQ(CalibrationMenu::State::DriftWait, menu.state());
    menu.render(harness.ports, nullptr, harness.display);
    EXPECT_EQ(" 1m >2m  3m  5m ", std::string(harness.display.row(1)));

    menu.handleKey(pressed(Button::Select), harness.ports, nullptr, harness.display, false);
    EXPECT_EQ(CalibrationMenu::State::DriftRunning, menu.state());

    menu.handleKey(MenuKeyInput {Button::None, Button::None}, harness.ports, nullptr, harness.display, false);
    EXPECT_EQ(1, harness.motion.driftAlignmentCalls);
    EXPECT_EQ(57, harness.motion.lastDriftDuration);
    EXPECT_EQ(CalibrationMenu::State::HighlightDrift, menu.state());
}
