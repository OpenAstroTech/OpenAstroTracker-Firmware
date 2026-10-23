#include "../Configuration.hpp"
#include "../inc/Globals.hpp"

#include "UiPorts.hpp"

#include "../Mount.hpp"

namespace app
{

void UiPorts::delay(int ms)
{
    const unsigned long start = clock->now();
    while (clock->now() - start < static_cast<unsigned long>(ms))
    {
        scheduler->yield();
    }
}

void UiPorts::waitUntilStopped(int direction)
{
    while (((direction & (EAST | WEST)) && read->isAxisRunning(RA_STEPS))
           || ((direction & (NORTH | SOUTH)) && read->isAxisRunning(DEC_STEPS)) || ((direction & FOCUSING) && axes->isRunningFocus()))
    {
        if (loop != nullptr)
        {
            loop();
        }
        scheduler->yield();
    }
}

}  // namespace app
