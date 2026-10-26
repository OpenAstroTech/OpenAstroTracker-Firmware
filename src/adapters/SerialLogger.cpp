#include "adapters/SerialLogger.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hal/WarningGuard.hpp"
#include "ports/ILogger.hpp"

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
    // The prefix format is a literal, so on AVR it has to be kept out of `.data` the
    // same way the message format is (see the `vsnprintf_P` below).
#if defined(__AVR__)
    const int prefix
        = snprintf_P(line, sizeof(line), PSTR("[%lu]{%lu}ms: "), static_cast<unsigned long>(now), static_cast<unsigned long>(delta));
#else
    const int prefix = snprintf(line, sizeof(line), "[%lu]{%lu}ms: ", static_cast<unsigned long>(now), static_cast<unsigned long>(delta));
#endif
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

    size_t length = strlen(line);
    if (length < sizeof(line))
    {
        line[length++] = '\n';
    }

    // One write per line: the arduino-pico USB CDC path can wedge when many small
    // writes per line race the TinyUSB FIFO, and the legacy logger printed a single
    // line for the same reason.
    retain(line, length);

    _port.write(reinterpret_cast<const uint8_t *>(line), length);
    _port.flush();
}

void SerialLogger::retain(const char *line, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        _ring[_ringWrite] = line[i];
        _ringWrite        = (_ringWrite + 1) % LOG_BUFFER_SIZE;
        if (_ringFilled < LOG_BUFFER_SIZE)
        {
            _ringFilled++;
        }
    }
}

size_t SerialLogger::readLog(char *out, size_t capacity)
{
    if ((out == nullptr) || (capacity < 2))
    {
        return 0;
    }

    const size_t available = _ringFilled;
    const size_t count     = (available < (capacity - 1)) ? available : (capacity - 1);
    size_t index           = (_ringWrite + LOG_BUFFER_SIZE - count) % LOG_BUFFER_SIZE;

    size_t written = 0;
    for (size_t n = 0; n < count; n++)
    {
        // `#` ends a Meade reply, so it cannot travel inside one.
        out[written++] = (_ring[index] == '#') ? '%' : _ring[index];
        index          = (index + 1) % LOG_BUFFER_SIZE;
    }
    out[written] = '\0';

    if (available > count)
    {
        // The tail was cut mid-line; start at the first whole line of what is left.
        const char *newline = strchr(out, '\n');
        if (newline != nullptr)
        {
            const size_t skip = static_cast<size_t>(newline - out) + 1;
            memmove(out, out + skip, written - skip + 1);
            written -= skip;
        }
    }

    return written;
}

}  // namespace adapters
