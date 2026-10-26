// The heap query. The bodies are the ones `Utility.cpp` carried; only the address changed.
#include "hal/arduino/FreeMemory.hpp"

#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

#if defined(ESP32)
int freeMemory()
{
    return ESP.getFreeHeap();
}
#else

    #ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char *sbrk(int incr);
    #else   // __ARM__
extern char *__brkval;
    #endif  // __arm__

int freeMemory()
{
    char top;
    #ifdef __arm__
    return &top - reinterpret_cast<char *>(sbrk(0));
    #elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
    return &top - __brkval;
    #else   // __arm__
    return __brkval ? &top - __brkval : &top - __malloc_heap_start;
    #endif  // __arm__
}
#endif
