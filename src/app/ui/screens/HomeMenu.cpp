#include "app/ui/screens/HomeMenu.hpp"

#include "ports/ILogger.hpp"

namespace
{

/// The legacy row: two six-column choices on one line, with the selector in front of one of them.
constexpr const char *kHomeRow   = " Home  Park";
constexpr const char *kUnparkRow = " Home  Unpark";
constexpr uint8_t kChoiceWidth   = 6;

}  // namespace

namespace app
{

void HomeMenu::enter()
{
    _selection = 0;
}

MenuKeyResult HomeMenu::handleKey(const MenuKeyInput &input, ports::IMountMotion &motion)
{
    switch (input.edge)
    {
        case types::Button::Select:
            if (_selection == 0)
            {
                motion.startSlewingToHome();
            }
            else if (motion.isSlewingTRK())
            {
                motion.park();
            }
            else
            {
                motion.startSlewing(TRACKING);
            }
            return MenuKeyResult::WaitForRelease;

        case types::Button::Up:
        case types::Button::Down:
        case types::Button::Left:
            _selection = static_cast<uint8_t>(1 - _selection);
            return MenuKeyResult::WaitForRelease;

        case types::Button::Right:
            return MenuKeyResult::NextScreen;

        case types::Button::None:
        case types::Button::Invalid:
        default:
            return MenuKeyResult::Ignored;
    }
}

void HomeMenu::render(const ports::IMountMotion &motion, ports::IDisplay &display) const
{
    // Parking is only offered once the mount is tracking; before that the second choice is "unpark".
    char row[16];
    const char *source = motion.isSlewingTRK() ? kHomeRow : kUnparkRow;
    uint8_t i          = 0;
    for (; (i < sizeof(row) - 1) && (source[i] != '\0'); i++)
    {
        row[i] = source[i];
    }
    for (; i < sizeof(row) - 1; i++)
    {
        row[i] = ' ';
    }
    row[sizeof(row) - 1]           = '\0';
    row[_selection * kChoiceWidth] = '>';

    display.writeLine(1, row);
}

}  // namespace app
