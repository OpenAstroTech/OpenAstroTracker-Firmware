#pragma once

#include "app/ui/screens/CoordinateMenu.hpp"

namespace app
{

/// The RA menu: the shared coordinate screen, editing right ascension.
class RaMenu final : public CoordinateMenu
{
  public:
    RaMenu() : CoordinateMenu(Axis::RightAscension)
    {
    }
};

}  // namespace app
