#include "app/ui/screens/FocusMenu.hpp"

namespace
{

constexpr uint8_t kRateSteps = 4;  // rates 1..4, selectable with LEFT/RIGHT
constexpr uint8_t kMaxRate   = kRateSteps - 1;

/// The rate row: eight leading columns, then the four numbers, then the speed marker column.
constexpr const char *kRateRowTemplate = "Rate:  1 2 3 4 *";

constexpr uint8_t kMarkerBase   = 6;
constexpr uint8_t kMarkerStride = 2;
constexpr uint8_t kSpeedColumn  = 15;

}  // namespace

namespace app
{

void FocusMenu::enter()
{
    _state     = State::Highlight;
    _rateIndex = 3;
}

MenuKeyResult FocusMenu::handleKey(const MenuKeyInput &input, ports::IMountOptionalAxes &axes)
{
    if (_state == State::Adjust)
    {
        // Holding UP/DOWN drives the focuser continuously; releasing it stops.
        if (input.held == types::Button::Up)
        {
            if (!axes.isRunningFocus())
            {
                axes.focusContinuousMove(types::FOCUS_BACKWARD);
            }
        }
        else if (input.held == types::Button::Down)
        {
            if (!axes.isRunningFocus())
            {
                axes.focusContinuousMove(types::FOCUS_FORWARD);
            }
        }
        else if (input.held == types::Button::None)
        {
            if (axes.isRunningFocus())
            {
                axes.focusStop();
            }
        }
    }

    switch (input.edge)
    {
        case types::Button::Select:
            _state = (_state == State::Adjust) ? State::Highlight : State::Adjust;
            return MenuKeyResult::WaitForRelease;

        case types::Button::Right:
            if (_state == State::Adjust)
            {
                if (_rateIndex < kMaxRate)
                {
                    _rateIndex++;
                }
                axes.focusSetSpeedByRate(_rateIndex + 1);
                return MenuKeyResult::WaitForRelease;
            }
            return MenuKeyResult::NextScreen;

        case types::Button::Left:
            if (_state == State::Adjust)
            {
                if (_rateIndex > 0)
                {
                    _rateIndex--;
                }
                axes.focusSetSpeedByRate(_rateIndex + 1);
                return MenuKeyResult::WaitForRelease;
            }
            return MenuKeyResult::Ignored;

        default:
            return MenuKeyResult::Ignored;
    }
}

void FocusMenu::render(const ports::IMountOptionalAxes &axes, ports::IDisplay &display) const
{
    char row[17];
    uint8_t i = 0;

    if (_state == State::Highlight)
    {
        const char *source = ">Focus Adjust";
        for (; source[i] != '\0' && i < sizeof(row) - 1; i++)
        {
            row[i] = source[i];
        }
    }
    else
    {
        for (; kRateRowTemplate[i] != '\0' && i < sizeof(row) - 1; i++)
        {
            row[i] = kRateRowTemplate[i];
        }
        // The chosen rate gets the selector, and the column past the last rate shows the direction.
        row[kMarkerBase + _rateIndex * kMarkerStride]       = '>';
        row[kMarkerBase + (_rateIndex + 1) * kMarkerStride] = '<';
        row[kSpeedColumn]                                   = axes.isRunningFocus() ? ((axes.getFocusSpeed() < 0.0f) ? '~' : '^') : '-';
    }

    row[i] = '\0';
    display.writeLine(1, row);
}

}  // namespace app
