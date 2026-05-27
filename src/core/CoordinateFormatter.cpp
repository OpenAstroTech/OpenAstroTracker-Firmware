#include "CoordinateFormatter.hpp"
#include "types/RightAscension.hpp"
#include "types/Declination.hpp"
#include <stdio.h>

namespace core
{

void formatRA(char *buf, const RightAscension &ra, const char *fmt)
{
    int h, m, s;
    ra.getTime(h, m, s);
    // RA is always non-negative in display (0–24h range)
    int absHours = abs(h);
    snprintf(buf, 32, fmt, absHours, m, s);
}

void formatDEC(char *buf, const Declination &dec, const char *fmt)
{
    dec.formatString(buf, fmt);
}

}  // namespace core
