#pragma once

#include <stdint.h>

namespace core
{
/// The serial and diagnostics cadence this build runs at.
///
/// `debugLevel` is the `DEBUG_*` bit mask; a zero mask is what selects the null logger, so the
/// value is meaningful even when no logging is compiled in.
///
/// Not here: the Wi-Fi settings (mode, host name, credentials, port). Their only reader is
/// `adapters/WifiControl`, and `adapters/` may not include `core/`, so they stay macros.
struct CommsConfig {
    uint32_t serialBaudrate      = 19200;
    uint16_t debugLevel          = 0;
    bool debugSeparateSerial     = false;
    uint32_t debugSerialBaudrate = 115200;
    uint32_t gpsBaudrate         = 9600;
};

}  // namespace core
