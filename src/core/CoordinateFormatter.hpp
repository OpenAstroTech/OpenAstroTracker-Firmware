#pragma once

#include <stddef.h>

namespace core
{

/// Pure coordinate formatting to char buffers (no Arduino deps).

class DayTime;      // forward
class Declination;  // forward

/// Format RA (hours component) using a printf-style format string.
/// @param buf     output buffer
/// @param bufSize capacity of buf in bytes; the result is truncated to fit
/// @param ra      RA value (hours, minutes, seconds)
/// @param fmt     printf format with 3 %d placeholders for h, m, s
void formatRA(char *buf, size_t bufSize, const DayTime &ra, const char *fmt);

/// Format DEC using the formatString mini-language ({d}, {m}, {s}, {+}).
/// @param buf     output buffer, which must be large enough for the result
/// @param dec     DEC value
/// @param fmt     formatString format (e.g. "{d}*{m}'{s}#")
void formatDEC(char *buf, const Declination &dec, const char *fmt);

}  // namespace core
