#pragma once

#include <stdint.h>

#include "app/ui/MenuScreen.hpp"
#include "core/Button.hpp"
#include "ports/IDisplay.hpp"
#include "ports/IMountMotion.hpp"

namespace app
{

/// The GO menu: start the home slew, or park/unpark depending on what the mount is doing.
///
/// The two choices share one row, with the selector painted in front of the active one — the same
/// bytes the global submenu drew, now composed by a class that can be tested without a panel.
class HomeMenu
{
  public:
    /// Clears the choice, as entering the menu item always did.
    void enter();

    MenuKeyResult handleKey(core::Button key, ports::IMountMotion &motion);

    void render(const ports::IMountMotion &motion, ports::IDisplay &display) const;

  private:
    uint8_t _selection;
};

}  // namespace app
