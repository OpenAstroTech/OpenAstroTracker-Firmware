#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

#include "hal/arduino/ArduinoSystemClock.hpp"

namespace hal
{

uint32_t ArduinoSystemClock::millis() const
{
    return ::millis();
}

uint32_t ArduinoSystemClock::micros() const
{
    return ::micros();
}

void ArduinoSystemClock::delay(uint32_t milliseconds) const
{
    ::delay(milliseconds);
}

void ArduinoSystemClock::delayMicroseconds(uint16_t microseconds) const
{
    ::delayMicroseconds(microseconds);
}

}  // namespace hal
