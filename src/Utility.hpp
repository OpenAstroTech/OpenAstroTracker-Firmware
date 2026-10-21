#ifndef UTILITY_HPP_
#define UTILITY_HPP_

#include "inc/Globals.hpp"
#include "ports/LogFormat.hpp"

#ifndef DEBUG_LEVEL
    #error Configuration.hpp must be included before Utility.hpp for correct debug configuration
#endif

String getLogBuffer();
int freeMemory();

/// Formatted output whose format string stays in flash on AVR.
///
/// `sprintf`/`snprintf` take a plain `const char *`, and on AVR a plain literal is linked
/// into `.data` — i.e. it occupies SRAM. `OAT_SNPRINTF` is the same call with the format
/// in program memory: `snprintf_P` + `PSTR()` on AVR, a plain `snprintf` elsewhere. It
/// also takes the buffer size, which the `sprintf` sites never passed (so it truncates
/// instead of overflowing).
///
/// This is the buffer-writing sibling of `OAT_LOG`; `lcdMenu.printMenu(F("..."))` covers
/// the sites that pass a literal to `String`.
#if defined(__AVR__)
    #define OAT_SNPRINTF(buffer, size, format, ...) snprintf_P((buffer), (size), PSTR(format), ##__VA_ARGS__)
#else
    #define OAT_SNPRINTF(buffer, size, format, ...) snprintf((buffer), (size), (format), ##__VA_ARGS__)
#endif

#if DEBUG_LEVEL != 0
    #define LOG(level, format, ...) logv((level), (F(format)), ##__VA_ARGS__)

    /// One diagnostic line through a `ports::ILogger`.
    ///
    /// Same shape as `LOG()`: the format literal stays in flash on AVR
    /// (`OAT_LOG_FORMAT` is `PSTR()` there) and the whole call compiles out when
    /// `DEBUG_LEVEL` is `DEBUG_NONE`, so arguments are not even evaluated. The
    /// logger then filters on the level bits at run time.
    #define OAT_LOG(logger, level, format, ...) (logger)->log((level), OAT_LOG_FORMAT(format), ##__VA_ARGS__)

// Realtime timer class using microseconds to time stuff
class RealTime
{
    static unsigned long _pausedTime;
    static unsigned long _suspendStart;
    static int _suspended;

  public:
    static void suspend()
    {
        if (_suspended == 0)
        {
            _suspendStart = micros();
        }
        _suspended++;
    }

    static void resume()
    {
        _suspended--;
        if (_suspended == 0)
        {
            unsigned long now = micros();
            _pausedTime += now - _suspendStart;
        }
    }

    static unsigned long currentTime()
    {
        if (_suspended != 0)
        {
            unsigned long now            = micros();
            unsigned long pausedUntilNow = now - _suspendStart;
            return now - pausedUntilNow;
        }
        else
        {
            return micros() - _pausedTime;
        }
    }
};

// Performance measurement class
class PerfMeasure
{
    unsigned long _start;
    unsigned long _end;
    unsigned long _duration;
    int _indent;
    String _name;
    bool _running;
    bool _printed;

  public:
    PerfMeasure(int indent, String name)
    {
        _name    = name;
        _running = true;
        _printed = false;
        _indent  = indent;
        _start   = RealTime::currentTime();
    }

    ~PerfMeasure()
    {
        if (_running)
            stop();
        print();
    }

    void stop()
    {
        _end      = RealTime::currentTime();
        _duration = _end - _start;
        _running  = false;
    }

    float durationMs()
    {
        return 0.001 * _duration;
    }

    void print()
    {
        if (_running)
            stop();
        RealTime::suspend();
        if (!_printed)
        {
            char buf[128];
            memset(buf, ' ', 127);
            buf[127] = 0;

            String disp = String(durationMs(), 3);
            char *p     = buf + (_indent * 1);
            memcpy(p, _name.c_str(), _name.length());
            p = buf + 36 - disp.length();
            memcpy(p, disp.c_str(), disp.length());
            p    = buf + 36;
            *p++ = 'm';
            *p++ = 's';
            *p++ = 0;
            Serial.println(String(buf));
            _printed = true;
        }
        RealTime::resume();
    }
};

String formatArg(const char *input, va_list args);
String format(const char *input, ...);
// void log(const char* input);
// void log(String input);
void logv(int levelFlags, String input, ...);

#else  // DEBUG_LEVEL>0
    #define LOG(level, format, ...)
    #define OAT_LOG(logger, level, format, ...)
#endif  // DEBUG_LEVEL>0

// For some reason arduino just defines all float functions to be as double
#if defined(fabsf)
    #undef fabsf
#endif
#if defined(roundf)
    #undef roundf
#endif
#if defined(atanf)
    #undef atanf
#endif
float fabsf(float);
float roundf(float);
float atanf(float);

// Adjust the given number by the given adjustment, wrap around the limits.
// Limits are inclusive, so they represent the lowest and highest valid number.
int adjustWrap(int current, int adjustBy, int minVal, int maxVal);

// Adjust the given number by the given adjustment, clamping to the limits.
// Limits are inclusive, so they represent the lowest and highest valid number.
int adjustClamp(int current, int adjustBy, int minVal, int maxVal);

// Clamp the given number to the limits.
// Limits are inclusive, so they represent the lowest and highest valid number.
long clamp(long current, long minVal, long maxVal);

// Clamp the given number to the limits.
// Limits are inclusive, so they represent the lowest and highest valid number.
int clamp(int current, int minVal, int maxVal);

// Clamp the given number to the limits.
// Limits are inclusive, so they represent the lowest and highest valid number.
float clamp(float current, float minVal, float maxVal);

// Return -1 if the given number is less than zero, 1 if not.
int sign(long num);

// Return -1 if the given number is less than zero, 1 if not.
int fsign(float num);

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define CASERETURN(c, r)                                                                                                                   \
    case c:                                                                                                                                \
        return r

// Return an array of String* with a nullptr sentinel
String *splitStringBy(String str, char splitChar);

#endif
