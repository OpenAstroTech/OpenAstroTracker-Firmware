#include <gtest/gtest.h>

#include <string>

#include "app/ui/screens/StartupWizard.hpp"
#include "../test_common/ports_fakes/FakeDisplay.hpp"
#include "../test_common/ports_fakes/FakeMountMotion.hpp"
#include "../test_common/ports_fakes/FakeStatusSources.hpp"

using app::MenuKeyInput;
using app::MenuKeyResult;
using app::StartupAction;
using app::StartupOutcome;
using app::StartupWizard;
using types::Button;
using fakes::FakeDisplay;
using fakes::FakeMountMotion;
using fakes::FakeMountReadModel;
using fakes::FakeSiteTime;

namespace
{

MenuKeyInput pressed(Button key)
{
    return MenuKeyInput {key, key};
}

constexpr bool kNoSensor = false;
constexpr bool kSensor   = true;
constexpr bool kNoGps    = false;
constexpr bool kGps      = true;

/// The ports the wizard reads; only three of them matter.
struct Harness {
    FakeMountReadModel read;
    FakeMountMotion motion;
    FakeSiteTime siteTime;
    app::UiPorts ports;

    Harness()
    {
        ports.motion    = &motion;
        ports.read      = &read;
        ports.siteTime  = &siteTime;
        ports.clock     = &clock;
        ports.scheduler = &scheduler;
    }

    struct Clock final : public ports::IClock {
        uint32_t now() override
        {
            return 0;
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

TEST(StartupWizardTest, TheFirstScreenAsksWhetherTheMountIsHome)
{
    StartupWizard wizard(kNoSensor, kNoGps);
    FakeDisplay display;
    wizard.enter();

    wizard.render(display);

    EXPECT_EQ("Home position?  ", std::string(display.row(0)));
    EXPECT_EQ(" Yes >No< Cancl ", std::string(display.row(1)));
    EXPECT_EQ(StartupWizard::HomeAnswer::No, wizard.homeAnswer());
}

TEST(StartupWizardTest, LeftCyclesTheThreeAnswers)
{
    StartupWizard wizard(kNoSensor, kNoGps);
    Harness harness;
    wizard.enter();

    wizard.handleKey(pressed(Button::Left), harness.ports);
    EXPECT_EQ(StartupWizard::HomeAnswer::Cancel, wizard.homeAnswer());

    wizard.handleKey(pressed(Button::Left), harness.ports);
    EXPECT_EQ(StartupWizard::HomeAnswer::Yes, wizard.homeAnswer());

    wizard.handleKey(pressed(Button::Left), harness.ports);
    EXPECT_EQ(StartupWizard::HomeAnswer::No, wizard.homeAnswer());
}

TEST(StartupWizardTest, AnsweringCancelCompletesTheStartup)
{
    StartupWizard wizard(kNoSensor, kNoGps);
    Harness harness;
    wizard.enter();
    wizard.handleKey(pressed(Button::Left), harness.ports);  // Cancel

    const StartupOutcome outcome = wizard.handleKey(pressed(Button::Select), harness.ports);

    EXPECT_EQ(StartupAction::Completed, outcome.action);
    EXPECT_EQ(StartupWizard::Step::Completed, wizard.step());
}

TEST(StartupWizardTest, AnsweringNoHandsOverToTheControlMenuWithTheRightPrompt)
{
    StartupWizard wizard(kNoSensor, kNoGps);
    Harness harness;
    wizard.enter();  // "No" is the default

    const StartupOutcome outcome = wizard.handleKey(pressed(Button::Select), harness.ports);

    EXPECT_EQ(StartupAction::HandOverToControlMenu, outcome.action);
    EXPECT_STREQ("Home with ^~<>", outcome.caption);
    EXPECT_EQ(StartupWizard::Step::WaitForPoleCompletion, wizard.step());
}

TEST(StartupWizardTest, AnsweringYesSkipsTheRollStepWithoutALevelSensor)
{
    StartupWizard wizard(kNoSensor, kNoGps);
    Harness harness;
    wizard.enter();
    wizard.handleKey(pressed(Button::Left), harness.ports);  // Cancel
    wizard.handleKey(pressed(Button::Left), harness.ports);  // Yes

    const StartupOutcome outcome = wizard.handleKey(pressed(Button::Select), harness.ports);

    EXPECT_EQ(StartupAction::None, outcome.action);
    EXPECT_EQ(StartupWizard::Step::SetHaTime, wizard.step());
}

TEST(StartupWizardTest, AnsweringYesGoesThroughTheLevelStepWithASensor)
{
    StartupWizard wizard(kSensor, kNoGps);
    Harness harness;
    wizard.enter();
    wizard.handleKey(pressed(Button::Left), harness.ports);  // Cancel
    wizard.handleKey(pressed(Button::Left), harness.ports);  // Yes
    wizard.handleKey(pressed(Button::Select), harness.ports);
    EXPECT_EQ(StartupWizard::Step::SetRoll, wizard.step());

    const StartupOutcome outcome = wizard.handleKey(MenuKeyInput {Button::None, Button::None}, harness.ports);
    EXPECT_EQ(StartupAction::HandOverToCalibration, outcome.action);
    EXPECT_STREQ("Level front", outcome.caption);
    EXPECT_EQ(StartupWizard::Step::WaitForRollCompletion, wizard.step());
}

TEST(StartupWizardTest, TheHourAngleStepPromptsForGpsWhenTheBoardHasOne)
{
    StartupWizard withGps(kNoSensor, kGps);
    Harness harness;
    withGps.enter();
    withGps.handleKey(pressed(Button::Left), harness.ports);
    withGps.handleKey(pressed(Button::Left), harness.ports);
    withGps.handleKey(pressed(Button::Select), harness.ports);  // Yes -> SetHaTime

    const StartupOutcome outcome = withGps.handleKey(MenuKeyInput {Button::None, Button::None}, harness.ports);
    EXPECT_EQ(StartupAction::HandOverToHaMenu, outcome.action);
    EXPECT_STREQ("Finding GPS...", outcome.caption);
}

TEST(StartupWizardTest, TheHourAngleConfirmationSetsHomeAndCopiesTheTarget)
{
    StartupWizard wizard(kNoSensor, kNoGps);
    Harness harness;
    wizard.enter();
    wizard.handleKey(pressed(Button::Left), harness.ports);
    wizard.handleKey(pressed(Button::Left), harness.ports);
    wizard.handleKey(pressed(Button::Select), harness.ports);                    // Yes -> SetHaTime
    wizard.handleKey(MenuKeyInput {Button::None, Button::None}, harness.ports);  // -> WaitForHaCompletion

    harness.read.haTime         = types::DayTime(7, 15, 0);
    harness.read.currentRaValue = types::DayTime(3, 4, 5);

    // The HA screen reports the confirmation; the wizard applies it on its next pass.
    wizard.haConfirmed();
    EXPECT_EQ(StartupWizard::Step::HaConfirmed, wizard.step());

    const StartupOutcome outcome = wizard.handleKey(MenuKeyInput {Button::None, Button::None}, harness.ports);

    EXPECT_EQ(StartupAction::Completed, outcome.action);
    EXPECT_EQ(1, harness.motion.setHomeCalls);
    EXPECT_TRUE(harness.motion.lastSetHomeClearsZeroPos);
    EXPECT_EQ(7, harness.siteTime.lastHa.getHours());
    EXPECT_EQ(3, harness.read.targetRaValue.getHours());
    EXPECT_EQ(4, harness.read.targetRaValue.getMinutes());
}

TEST(StartupWizardTest, ThePoleConfirmationReturnsToTheQuestionWithYesSelected)
{
    StartupWizard wizard(kNoSensor, kNoGps);
    Harness harness;
    wizard.enter();
    wizard.handleKey(pressed(Button::Select), harness.ports);  // No -> wait for the pole
    EXPECT_EQ(StartupWizard::Step::WaitForPoleCompletion, wizard.step());

    wizard.poleConfirmed();
    EXPECT_EQ(StartupWizard::Step::PoleConfirmed, wizard.step());

    wizard.handleKey(MenuKeyInput {Button::None, Button::None}, harness.ports);
    EXPECT_EQ(StartupWizard::Step::InHomePosition, wizard.step());
    EXPECT_EQ(StartupWizard::HomeAnswer::Yes, wizard.homeAnswer());
}

TEST(StartupWizardTest, AKeypressIsSwallowedWhileAnotherScreenOwnsTheKeys)
{
    StartupWizard wizard(kNoSensor, kNoGps);
    Harness harness;
    wizard.enter();
    wizard.handleKey(pressed(Button::Select), harness.ports);  // No

    const StartupOutcome outcome = wizard.handleKey(pressed(Button::Select), harness.ports);

    EXPECT_EQ(StartupAction::None, outcome.action);
    EXPECT_EQ(MenuKeyResult::Ignored, outcome.result);
}
