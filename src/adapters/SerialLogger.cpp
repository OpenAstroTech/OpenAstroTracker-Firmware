#include "adapters/SerialLogger.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hal/WarningGuard.hpp"
#include "ports/LogFormat.hpp"

namespace adapters
{

SerialLogger::SerialLogger(hal::ISerialPort &port, const hal::ISystemClock &clock, uint16_t levelMask)
    : _port(port), _clock(clock), _levelMask(levelMask), _lastLogTime(0)
{
}

bool SerialLogger::isEnabled(uint16_t levelFlags) const
{
    return (levelFlags & _levelMask) != 0;
}

void SerialLogger::log(uint16_t levelFlags, ports::LogFormat format, ...)
{
    if (!isEnabled(levelFlags))
    {
        return;
    }

    const uint32_t now   = _clock.millis();
    const uint32_t delta = now - _lastLogTime;
    _lastLogTime         = now;

    char line[LINE_BUFFER_SIZE];
    const int prefix = snprintf(line, sizeof(line), "[%lu]{%lu}ms: ", static_cast<unsigned long>(now), static_cast<unsigned long>(delta));
    if (prefix < 0)
    {
        return;
    }

    size_t used = static_cast<size_t>(prefix);
    if (used >= sizeof(line))
    {
        used = sizeof(line) - 1;
    }

    va_list args;
    va_start(args, format);
#if defined(__AVR__)
    vsnprintf_P(line + used, sizeof(line) - used, static_cast<const char *>(format.text()), args);
#else
    vsnprintf(line + used, sizeof(line) - used, static_cast<const char *>(format.text()), args);
#endif
    va_end(args);

    const size_t length = strlen(line);
    _port.write(reinterpret_cast<const uint8_t *>(line), length);
    const uint8_t newline = '\n';
    _port.write(newline);
    _port.flush();
}

}  // namespace adapters
