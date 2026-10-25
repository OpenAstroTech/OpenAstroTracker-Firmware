#pragma once

#include <stdint.h>

#include "app/ui/MenuScreen.hpp"
#include "ports/IDisplay.hpp"
#include "ports/IMountMotion.hpp"
#include "ports/IMountReadModel.hpp"

namespace app
{

/// The point-of-interest screen: one row per object, and three commands at the end of the list.
///
/// The list is the table in `PointOfInterestTable.hpp`; this class owns only which row is selected
/// and what SELECT does to it. Selecting an object writes the target and starts the go-to, selecting
/// Home/Park/Unpark drives the mount's own command instead.
class PoiMenu
{
  public:
    MenuKeyResult handleKey(const MenuKeyInput &input, ports::IMountReadModel &read, ports::IMountMotion &motion, bool northernHemisphere);

    /// Draws the selected row, or nothing at all while an axis is still moving.
    void render(ports::IMountReadModel &read, ports::IDisplay &display) const;

    /// The selected row, which is the index into `kPointsOfInterest[]`.
    uint16_t selected() const
    {
        return _selected;
    }

  private:
    uint16_t _selected = 0;
};

}  // namespace app
