#include "app/ui/screens/ControlMenu.hpp"

namespace
{

constexpr const char *kPromptRow = "Set home pos?";

/// " Yes  No  ": the Yes markers sit on the blanks around `Yes`, the No markers around `No`.
constexpr uint8_t kYesMarkerColumn      = 0;
constexpr uint8_t kYesCloseMarkerColumn = 4;
constexpr uint8_t kNoMarkerColumn       = 5;
constexpr uint8_t kNoCloseMarkerColumn  = 8;

}  // namespace

namespace app
{

void ControlMenu::enter()
{
    _state        = State::HighlightManual;
    _setZeroPoint = true;
    _commandedKey = types::Button::None;
}

void ControlMenu::startManual()
{
    _state        = State::Manual;
    _commandedKey = types::Button::None;
}

int ControlMenu::directionFor(types::Button key)
{
    switch (key)
    {
        case types::Button::Up:
            return NORTH;
        case types::Button::Down:
            return SOUTH;
        case types::Button::Left:
            return WEST;
        case types::Button::Right:
            return EAST;
        default:
            return 0;
    }
}

void ControlMenu::updateManualSlew(types::Button held, ports::IMountMotion &motion)
{
    if (_commandedKey == held)
    {
        return;
    }

    _commandedKey = held;
    motion.stopSlewing(ALL_DIRECTIONS);

    const int direction = directionFor(held);
    if (direction != 0)
    {
        motion.startSlewing(direction);
    }
}

ControlMenuOutcome ControlMenu::handleKey(const MenuKeyInput &input,
                                          ports::IMountMotion &motion,
                                          bool serialActive,
                                          bool startupAwaitingPole)
{
    ControlMenuOutcome outcome;

    switch (_state)
    {
        case State::HighlightManual:
            if (input.edge == types::Button::None)
            {
                return outcome;
            }
            outcome.result = MenuKeyResult::WaitForRelease;
            if (input.edge == types::Button::Select)
            {
                _state = State::Manual;
                motion.stopSlewing(ALL_DIRECTIONS);
            }
            else if ((input.edge == types::Button::Down) || (input.edge == types::Button::Up))
            {
                _state = State::HighlightSerial;
            }
            else if (input.edge == types::Button::Right)
            {
                outcome.result = MenuKeyResult::NextScreen;
            }
            return outcome;

        case State::HighlightSerial:
            if (input.edge == types::Button::None)
            {
                return outcome;
            }
            outcome.result = MenuKeyResult::WaitForRelease;
            if (input.edge == types::Button::Select)
            {
                outcome.serialChanged = true;
                outcome.serialActive  = !serialActive;
            }
            else if ((input.edge == types::Button::Down) || (input.edge == types::Button::Up))
            {
                _state = State::HighlightManual;
            }
            else if (input.edge == types::Button::Right)
            {
                if (serialActive)
                {
                    outcome.serialChanged = true;
                    outcome.serialActive  = false;
                }
                outcome.result = MenuKeyResult::NextScreen;
            }
            return outcome;

        case State::Manual:
            updateManualSlew(input.held, motion);

            if (input.edge == types::Button::Select)
            {
                outcome.result = MenuKeyResult::WaitForRelease;
                if (startupAwaitingPole)
                {
                    _state                = State::HighlightManual;
                    outcome.resumeStartup = true;
                }
                else
                {
                    _state                    = State::ConfirmHome;
                    outcome.suspendMenuUpdate = true;
                }
            }
            return outcome;

        case State::ConfirmHome:
            if (input.edge == types::Button::None)
            {
                return outcome;
            }
            outcome.result = MenuKeyResult::WaitForRelease;
            if (input.edge == types::Button::Select)
            {
                if (_setZeroPoint)
                {
                    // Leaving the control menu: the current position becomes home and the mount
                    // resumes tracking from there.
                    motion.setHome(true);
                    motion.startSlewing(TRACKING);
                }

                if (startupAwaitingPole)
                {
                    outcome.resumeStartup = true;
                }
                else
                {
                    outcome.result = MenuKeyResult::NextScreen;
                }

                _state                   = State::HighlightManual;
                outcome.resumeMenuUpdate = true;
                _setZeroPoint            = true;
            }
            else if (input.edge == types::Button::Left)
            {
                _setZeroPoint = !_setZeroPoint;
            }
            return outcome;

        default:
            return outcome;
    }
}

void ControlMenu::render(ports::IMountReadModel &read, bool serialActive, unsigned long now, ports::IDisplay &display)
{
    switch (_state)
    {
        case State::HighlightManual:
            display.writeLine(1, ">Manual slewing");
            return;

        case State::HighlightSerial:
            display.writeLine(1, ">Serial display");
            return;

        case State::ConfirmHome:
            {
                char choices[]                                                        = " Yes  No  ";
                choices[_setZeroPoint ? kYesMarkerColumn : kNoMarkerColumn]           = '>';
                choices[_setZeroPoint ? kYesCloseMarkerColumn : kNoCloseMarkerColumn] = '<';
                display.writeLine(0, kPromptRow);
                display.writeLine(1, choices);
            }
            return;

        case State::Manual:
        default:
            _slewStatus.renderThrottled(read, serialActive, now, display);
            return;
    }
}

}  // namespace app
