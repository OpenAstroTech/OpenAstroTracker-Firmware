#pragma once

#include <stdint.h>

#include "hal/IButtonMatrix.hpp"

namespace hal
{

/// The keys a headless board has: none. The UI keeps working and never sees a press.
class NullButtonMatrix final : public IButtonMatrix
{
  public:
    Button pressed() override
    {
        return Button::None;
    }

    uint16_t rawValue() override
    {
        return 0;
    }
};

}  // namespace hal
