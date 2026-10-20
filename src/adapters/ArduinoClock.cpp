#include "adapters/ArduinoClock.hpp"

namespace adapters
{

ArduinoClock::ArduinoClock(const hal::ISystemClock &systemClock) : _systemClock(systemClock)
{
}

uint32_t ArduinoClock::now()
{
    return _systemClock.millis();
}

void ArduinoClock::delay(uint32_t milliseconds)
{
    _systemClock.delay(milliseconds);
}

}  // namespace adapters
