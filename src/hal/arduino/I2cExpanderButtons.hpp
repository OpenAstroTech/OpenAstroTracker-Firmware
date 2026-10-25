#pragma once

#include <stdint.h>

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

    #include "hal/IButtonMatrix.hpp"

namespace hal
{

class I2cCharLcd;

/// `IButtonMatrix` over the keypad scanner of an MCP23008/MCP23017 LCD backpack.
///
/// The expander that drives the display also scans the five keys, so this takes the same panel
/// object rather than a second handle to the same chip.
class I2cExpanderButtons final : public IButtonMatrix
{
  public:
    explicit I2cExpanderButtons(I2cCharLcd &panel);

    Button pressed() override;

    uint16_t rawValue() override;

  private:
    I2cCharLcd &_panel;
};

}  // namespace hal

#endif
