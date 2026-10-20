#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

#include "adapters/ArduinoScheduler.hpp"

namespace adapters
{

void ArduinoScheduler::yield()
{
    ::yield();
}

}  // namespace adapters
