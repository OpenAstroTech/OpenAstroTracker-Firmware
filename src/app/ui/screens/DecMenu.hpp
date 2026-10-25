#pragma once

#include "app/ui/screens/CoordinateMenu.hpp"

namespace app
{

/// The DEC menu: the shared coordinate screen, editing declination.
class DecMenu final : public CoordinateMenu
{
  public:
    DecMenu() : CoordinateMenu(Axis::Declination)
    {
    }
};

}  // namespace app
