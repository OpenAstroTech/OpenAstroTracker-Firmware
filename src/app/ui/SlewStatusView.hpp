#pragma once

#include <stdint.h>

#include "ports/IDisplay.hpp"
#include "ports/IMountReadModel.hpp"

namespace app
{

/// The slew-progress view on the menu display.
///
/// This used to be `Mount::displayStepperPosition()`, i.e. the domain drawing on a display. It is a
/// view now: it reads the percentages and the two planned-move totals through the read model and
/// writes the rows it owns. Rows it does not paint are left alone, which is what lets a one-axis
/// move draw over the middle of the menu.
class SlewStatusView
{
  public:
    /// Paints the view, at most once per `DISPLAY_UPDATE_TIME`.
    void renderThrottled(ports::IMountReadModel &read, bool serialActive, unsigned long now, ports::IDisplay &display);

    /// Paints it unconditionally.
    void render(ports::IMountReadModel &read, bool serialActive, ports::IDisplay &display);

  private:
    unsigned long _lastUpdate = 0;
};

}  // namespace app
