#include "app/ui/screens/CoordinateMenu.hpp"

namespace
{

/// The readout fills the first twelve or thirteen columns, then the field cursor, then two letters.
constexpr uint8_t kRaTextColumns  = 12;
constexpr uint8_t kDecTextColumns = 13;

constexpr uint8_t kFieldCount = 4;  // hours/degrees, minutes, seconds, target-vs-current

}  // namespace

namespace app
{

CoordinateMenu::CoordinateMenu(Axis axis) : _axis(axis), _field(0), _showTarget(true)
{
}

MenuKeyResult CoordinateMenu::handleKey(const MenuKeyInput &input, UiPorts &ports)
{
    ports::IMountReadModel &read = *ports.read;

    if ((input.held == core::Button::Up) || (input.held == core::Button::Down))
    {
        const int step = (input.held == core::Button::Up) ? 1 : -1;

        switch (_field)
        {
            case 0:
                if (_axis == Axis::RightAscension)
                {
                    read.targetRA().addHours(step);
                }
                else
                {
                    read.targetDEC().addDegrees(step);
                }
                _showTarget = true;
                break;

            case 1:
                if (_axis == Axis::RightAscension)
                {
                    read.targetRA().addMinutes(step);
                }
                else
                {
                    read.targetDEC().addMinutes(step);
                }
                _showTarget = true;
                break;

            case 2:
                if (_axis == Axis::RightAscension)
                {
                    read.targetRA().addSeconds(step);
                }
                else
                {
                    read.targetDEC().addSeconds(step);
                }
                _showTarget = true;
                break;

            default:
                _showTarget = !_showTarget;
                break;
        }

        // Auto-repeat is deliberate but slow: 200 ms between steps.
        ports.delay(200);

        // Only the target-vs-current toggle waits for the key to come up; the numeric steps repeat.
        const bool toggled = (_field == (kFieldCount - 1));
        return (toggled && (input.edge == input.held)) ? MenuKeyResult::WaitForRelease : MenuKeyResult::Ignored;
    }

    switch (input.edge)
    {
        case core::Button::Left:
            _field = static_cast<uint8_t>((_field + 1) % kFieldCount);
            return MenuKeyResult::WaitForRelease;

        case core::Button::Select:
            if (ports.motion->isSlewingRAorDEC())
            {
                ports.motion->stopSlewing(ALL_DIRECTIONS);
                if (_axis == Axis::RightAscension)
                {
                    // RA waits for the axes to settle, DEC does not: the two legacy paths differed.
                    ports.waitUntilStopped(ALL_DIRECTIONS);
                }
            }
            else
            {
                ports.motion->startSlewingToTarget();
            }
            return MenuKeyResult::WaitForRelease;

        case core::Button::Right:
            return MenuKeyResult::NextScreen;

        default:
            return MenuKeyResult::Ignored;
    }
}

void CoordinateMenu::render(UiPorts &ports, ports::IDisplay &display) const
{
    if (!ports.read->isSlewingIdle())
    {
        return;
    }

    const uint8_t marker = (_showTarget ? TARGET_STRING : CURRENT_STRING);
    const char *text     = (_axis == Axis::RightAscension) ? ports.read->raText(LCDMENU_STRING | marker, _field)
                                                           : ports.read->decText(LCDMENU_STRING | marker, _field);

    // readout columns, the field cursor, then the two-letter marker: 12+1+2 for RA, 13+1+2 for DEC.
    const uint8_t readoutColumns = (_axis == Axis::RightAscension) ? kRaTextColumns : kDecTextColumns;
    const uint8_t rowWidth       = static_cast<uint8_t>(readoutColumns + 3);

    char row[17];
    uint8_t i = 0;
    for (; (i < readoutColumns) && (text[i] != '\0'); i++)
    {
        row[i] = text[i];
    }
    for (; i < readoutColumns; i++)
    {
        row[i] = ' ';
    }
    row[readoutColumns]     = (_field == (kFieldCount - 1)) ? '>' : ' ';
    row[readoutColumns + 1] = _showTarget ? 'T' : 'C';
    row[readoutColumns + 2] = _showTarget ? 'a' : 'u';
    row[rowWidth]           = '\0';

    display.writeLine(1, row);
}

}  // namespace app
