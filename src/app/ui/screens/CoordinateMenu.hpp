#pragma once

#include <stdint.h>

#include "app/UiPorts.hpp"
#include "app/ui/MenuScreen.hpp"
#include "ports/IDisplay.hpp"

namespace app
{

/// The RA and DEC screens: pick a field with LEFT, nudge the target with UP/DOWN, slew with SELECT.
///
/// They differ in three places only — which coordinate they edit, the unit names, and what SELECT
/// does when a slew is already running — so the behaviour lives here once and the two screens are
/// two configurations of it. The row composition is shared because the two screens have to keep
/// their exact wire shape: a twelve- or thirteen-column readout, the field cursor, and the
/// target/current marker.
class CoordinateMenu
{
  public:
    enum class Axis : uint8_t
    {
        RightAscension,
        Declination,
    };

    explicit CoordinateMenu(Axis axis);

    MenuKeyResult handleKey(const MenuKeyInput &input, UiPorts &ports);

    void render(UiPorts &ports, ports::IDisplay &display) const;

  private:
    Axis _axis;
    uint8_t _field;
    bool _showTarget;
};

}  // namespace app
