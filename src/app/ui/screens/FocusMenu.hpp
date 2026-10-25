#pragma once

#include <stdint.h>

#include "app/ui/MenuScreen.hpp"
#include "ports/IDisplay.hpp"
#include "ports/IMountOptionalAxes.hpp"

namespace app
{

/// The focuser screen: one highlighted row, then a rate row with a continuous move on UP/DOWN.
///
/// The two states are the legacy ones — "Focus Adjust" is a one-choice menu, and the rate row is
/// where the focuser actually moves — so a press on SELECT walks between them.
class FocusMenu
{
  public:
    /// Puts the screen back on its first row, as selecting the menu item always did.
    void enter();

    MenuKeyResult handleKey(const MenuKeyInput &input, ports::IMountOptionalAxes &axes);

    void render(const ports::IMountOptionalAxes &axes, ports::IDisplay &display) const;

  private:
    enum class State : uint8_t
    {
        Highlight,
        Adjust,
    };

    State _state;
    uint8_t _rateIndex;
};

}  // namespace app
