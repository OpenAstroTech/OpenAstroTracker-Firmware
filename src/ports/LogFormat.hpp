#pragma once

namespace ports
{

/// Opaque handle to a log format string.
///
/// The AVR build keeps 273 log format literals (13.9 KB) in flash. Handing the
/// implementation a plain `const char*` would move them into the 8 KB of SRAM, so
/// the port passes this handle instead: the literal stays wherever the toolchain put
/// it and the adapter knows how to read it back (`F()`/`pgm_read_byte` on AVR, a
/// plain dereference elsewhere).
///
/// Construct one with `OAT_LOG_FORMAT(...)` rather than directly, so each platform
/// picks the right representation.
class LogFormat
{
  public:
    explicit LogFormat(const void *text) : _text(text)
    {
    }

    /// The platform representation of the format string. Only a flash-aware adapter
    /// may dereference it.
    const void *text() const
    {
        return _text;
    }

  private:
    const void *_text;
};

}  // namespace ports

#if defined(__AVR__)
    #include <avr/pgmspace.h>

/// Wraps a literal so it stays in program memory on AVR. This is the only
/// platform-specific code under `ports/`, and it exists precisely to keep SRAM free.
    #define OAT_LOG_FORMAT(literal) ports::LogFormat(PSTR(literal))
#else
    #define OAT_LOG_FORMAT(literal) ports::LogFormat(literal)
#endif

/// Wraps a literal so it stays in program memory on AVR.
#if defined(__AVR__)
    #include <stdio.h>  // `snprintf_P`, which avr-libc keeps beside `snprintf`
    #define OAT_SNPRINTF(buffer, size, format, ...) snprintf_P((buffer), (size), PSTR(format), ##__VA_ARGS__)
#else
    #include <stdio.h>
    #define OAT_SNPRINTF(buffer, size, format, ...) snprintf((buffer), (size), (format), ##__VA_ARGS__)
#endif

// The level comes from the build's `DEBUG_LEVEL`, so a build that logs nothing emits no call and
// does not even evaluate the arguments.
#if DEBUG_LEVEL != 0
    /// One diagnostic line through a `ports::ILogger`.
    ///
    /// Same shape as the legacy `LOG()`: the format literal stays in flash on AVR
    /// (`OAT_LOG_FORMAT` is `PSTR()` there) and the whole call compiles out when `DEBUG_LEVEL` is
    /// `DEBUG_NONE`.
    #define OAT_LOG(logger, level, format, ...) (logger)->log((level), OAT_LOG_FORMAT(format), ##__VA_ARGS__)
#else
    #define OAT_LOG(logger, level, format, ...)
#endif
