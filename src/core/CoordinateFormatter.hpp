#pragma once

namespace core
{

/// Pure coordinate formatting to char buffers (no Arduino deps).
/// Formats RA as HH:MM:SS and DEC using the formatString mini-language.

class RightAscension;  // forward
class Declination;     // forward

/// Format RA (hours component) using a printf-style format string.
/// @param buf     output buffer (must be large enough)
/// @param ra      RA value (hours, minutes, seconds)
/// @param fmt     printf format with 3 %d placeholders for h, m, s
void formatRA(char *buf, const RightAscension &ra, const char *fmt);

/// Format DEC using the formatString mini-language ({d}, {m}, {s}, {+}).
/// @param buf     output buffer
/// @param dec     DEC value
/// @param fmt     formatString format (e.g. "{d}*{m}'{s}#")
void formatDEC(char *buf, const Declination &dec, const char *fmt);

}  // namespace core
