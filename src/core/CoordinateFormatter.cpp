#include "CoordinateFormatter.hpp"
#include "types/DayTime.hpp"
#include "types/Declination.hpp"
#include <stdio.h>

namespace core
{

void formatRA(char *buf, size_t bufSize, const types::DayTime &ra, const char *fmt)
{
    int h, m, s;
    ra.getTime(h, m, s);
    snprintf(buf, bufSize, fmt, h, m, s);
}

void formatDEC(char *buf, const types::Declination &dec, const char *fmt)
{
    dec.formatString(buf, fmt);
}

}  // namespace core
