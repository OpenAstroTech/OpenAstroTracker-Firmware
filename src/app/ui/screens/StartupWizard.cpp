#include "app/ui/screens/StartupWizard.hpp"

#include "types/MathUtil.hpp"

namespace
{

/// The three answers on the first screen, with the selector drawn in place of a blank.
constexpr const char *kAnswerRow = " Yes  No  Cancl ";  // 16 columns, markers on the blanks

struct AnswerMarkers {
    app::StartupWizard::HomeAnswer answer;
    uint8_t open;
    uint8_t close;
};

constexpr AnswerMarkers kAnswerMarkers[] = {
    {app::StartupWizard::HomeAnswer::Yes, 0, 4},
    {app::StartupWizard::HomeAnswer::No, 5, 8},
    {app::StartupWizard::HomeAnswer::Cancel, 9, 15},
};

}  // namespace

namespace app
{

StartupWizard::StartupWizard(bool hasLevelSensor, bool hasGps) : _hasLevelSensor(hasLevelSensor), _hasGps(hasGps)
{
}

void StartupWizard::enter()
{
    _step       = Step::InHomePosition;
    _homeAnswer = HomeAnswer::No;
}

StartupOutcome StartupWizard::complete()
{
    _step = Step::Completed;
    StartupOutcome outcome;
    outcome.action = StartupAction::Completed;
    return outcome;
}

StartupOutcome StartupWizard::handleKey(const MenuKeyInput &input, UiPorts &ports)
{
    StartupOutcome outcome;

    // Four steps act on the pass after the state changed rather than on a key: they switch to
    // another screen, or finish, and the legacy code did the same in its state dispatch.
    switch (_step)
    {
        case Step::SetRoll:
            _step           = Step::WaitForRollCompletion;
            outcome.action  = StartupAction::HandOverToCalibration;
            outcome.caption = "Level front";
            return outcome;

        case Step::RollConfirmed:
            _step = Step::SetHaTime;
            return outcome;

        case Step::SetHaTime:
            _step           = Step::WaitForHaCompletion;
            outcome.action  = StartupAction::HandOverToHaMenu;
            outcome.caption = _hasGps ? "Finding GPS..." : "Set current HA";
            return outcome;

        case Step::HaConfirmed:
            applyHourAngleConfirmation(ports);
            return complete();

        case Step::PoleConfirmed:
            // Back to the question, with "Yes" pre-selected because the operator just did it.
            _homeAnswer = HomeAnswer::Yes;
            _step       = Step::InHomePosition;
            return outcome;

        default:
            break;
    }

    if (_step != Step::InHomePosition)
    {
        // Another screen owns the keys while the wizard waits for it.
        return outcome;
    }

    if (input.edge == types::Button::None)
    {
        return outcome;
    }

    outcome.result = MenuKeyResult::WaitForRelease;
    if (input.edge == types::Button::Left)
    {
        _homeAnswer = static_cast<HomeAnswer>((static_cast<int>(_homeAnswer) + 1) % 3);
        return outcome;
    }
    if (input.edge != types::Button::Select)
    {
        return outcome;
    }

    if (_homeAnswer == HomeAnswer::Yes)
    {
        _step = _hasLevelSensor ? Step::SetRoll : Step::SetHaTime;
        return outcome;
    }

    if (_homeAnswer == HomeAnswer::No)
    {
        // The operator has to park the mount by hand first, which the control menu does.
        _step           = Step::WaitForPoleCompletion;
        outcome.action  = StartupAction::HandOverToControlMenu;
        outcome.caption = "Home with ^~<>";
        return outcome;
    }

    return complete();
}

void StartupWizard::rollConfirmed()
{
    if (_step == Step::WaitForRollCompletion)
    {
        _step = Step::RollConfirmed;
    }
}

void StartupWizard::haConfirmed()
{
    if (_step == Step::WaitForHaCompletion)
    {
        _step = Step::HaConfirmed;
    }
}

void StartupWizard::applyHourAngleConfirmation(UiPorts &ports)
{
    // The mount is where it is: that becomes home, and the hour angle the operator just set is
    // written back so the two agree.
    ports.motion->setHome(true);
    const types::DayTime hourAngle = ports.read->hourAngle();
    ports.siteTime->setHA(hourAngle);
    ports.read->targetRA() = ports.read->currentRA();
}

void StartupWizard::poleConfirmed()
{
    if (_step == Step::WaitForPoleCompletion)
    {
        _step = Step::PoleConfirmed;
    }
}

void StartupWizard::render(ports::IDisplay &display) const
{
    if (_step != Step::InHomePosition)
    {
        return;
    }

    char row[17];
    uint8_t i = 0;
    for (; (i < 16) && (kAnswerRow[i] != '\0'); i++)
    {
        row[i] = kAnswerRow[i];
    }
    for (; i < 16; i++)
    {
        row[i] = ' ';
    }
    row[16] = '\0';

    for (const AnswerMarkers &markers : kAnswerMarkers)
    {
        if (markers.answer == _homeAnswer)
        {
            row[markers.open]  = '>';
            row[markers.close] = '<';
            break;
        }
    }

    display.writeLine(0, "Home position?");
    display.writeLine(1, row);
}

}  // namespace app
