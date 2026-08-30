#include "DayTime.hpp"

namespace core
{

DayTime::DayTime()
{
    totalSeconds = 0;
}

DayTime::DayTime(const DayTime &other)
{
    totalSeconds = other.totalSeconds;
}

DayTime::DayTime(int h, int m, int s)
{
    long sgn     = sign(h);
    h            = abs(h);
    totalSeconds = sgn * ((60L * h + m) * 60L + s);
}

DayTime::DayTime(float timeInHours)
{
    long sgn     = fsign(timeInHours);
    timeInHours  = fabsf(timeInHours);
    totalSeconds = sgn * static_cast<long>(roundf(timeInHours * 60.0f * 60.0f));
}

int DayTime::getHours() const
{
    int h, m, s;
    getTime(h, m, s);
    return h;
}

int DayTime::getMinutes() const
{
    int h, m, s;
    getTime(h, m, s);
    return m;
}

int DayTime::getSeconds() const
{
    int h, m, s;
    getTime(h, m, s);
    return s;
}

float DayTime::getTotalHours() const
{
    return 1.0f * totalSeconds / 3600.0f;
}

float DayTime::getTotalMinutes() const
{
    return 1.0f * totalSeconds / 60.0f;
}

long DayTime::getTotalSeconds() const
{
    return totalSeconds;
}

void DayTime::getTime(int &h, int &m, int &s) const
{
    long seconds = labs(totalSeconds);

    h       = (int) (seconds / 3600L);
    seconds = seconds - (h * 3600L);
    m       = (int) (seconds / 60L);
    s       = (int) (seconds - (m * 60L));

    h *= sign(totalSeconds);
}

void DayTime::splitSeconds(long secs, int &h, int &m, int &s)
{
    long remainder = labs(secs);
    h              = static_cast<int>(remainder / 3600L);
    remainder      = remainder - (h * 3600L);
    m              = static_cast<int>(remainder / 60L);
    s              = static_cast<int>(remainder - (m * 60L));
    h *= sign(secs);
}

void DayTime::set(int h, int m, int s)
{
    DayTime dt(h, m, s);
    totalSeconds = dt.totalSeconds;
    checkHours();
}

void DayTime::set(const DayTime &other)
{
    totalSeconds = other.totalSeconds;
    checkHours();
}

// Add hours, wrapping days (which are not tracked)
void DayTime::addHours(float deltaHours)
{
    totalSeconds += long(deltaHours * 3600L);
    checkHours();
}

void DayTime::checkHours()
{
    while (totalSeconds >= secondsPerDay)
    {
        totalSeconds -= secondsPerDay;
    }

    while (totalSeconds < 0)
    {
        totalSeconds += secondsPerDay;
    }
}

// Add minutes, wrapping hours if needed
void DayTime::addMinutes(int deltaMins)
{
    totalSeconds += deltaMins * 60;
    checkHours();
}

// Add seconds, wrapping minutes and hours if needed
void DayTime::addSeconds(long deltaSecs)
{
    totalSeconds += deltaSecs;
    checkHours();
}

// Add time components, wrapping seconds, minutes and hours if needed
void DayTime::addTime(int deltaHours, int deltaMinutes, int deltaSeconds)
{
    totalSeconds += ((60L * deltaHours + deltaMinutes) * 60L + deltaSeconds);
    checkHours();
}

// Add another time, wrapping seconds, minutes and hours if needed
void DayTime::addTime(const DayTime &other)
{
    totalSeconds += other.totalSeconds;
    checkHours();
}

// Subtract another time, wrapping seconds, minutes and hours if needed
void DayTime::subtractTime(const DayTime &other)
{
    totalSeconds -= other.totalSeconds;
    checkHours();
}

void DayTime::printTwoDigits(char *achDegs, int num) const
{
    achDegs[0] = '0' + (num / 10);
    achDegs[1] = '0' + (num % 10);
    achDegs[2] = 0;
}

const char *DayTime::formatStringImpl(char *targetBuffer, const char *format, char sgn, long degs, long mins, long secs) const
{
    char achDegs[5];
    char achMins[3];
    char achSecs[3];
    const char *f = format;
    char *p       = targetBuffer;

    int i = 0;
    if (sgn != '\0')
    {
        achDegs[0] = sgn;
        i++;
    }

    long absdegs = labs(degs);
    if (absdegs >= 100)
    {
        achDegs[i++] = '0' + (absdegs / 100);
        if (absdegs / 100 > 9)
            achDegs[i - 1] = '9';
        absdegs = absdegs % 100;
    }

    printTwoDigits(achDegs + i, absdegs);
    printTwoDigits(achMins, mins);
    printTwoDigits(achSecs, secs);

    char macro   = '\0';
    bool inMacro = false;
    while (*f)
    {
        switch (*f)
        {
            case '{':
                {
                    inMacro = true;
                }
                break;
            case '}':
                {
                    if (inMacro)
                    {
                        switch (macro)
                        {
                            case '+':
                                {
                                    *p++ = (degs < 0 ? '-' : '+');
                                }
                                break;
                            case 'd':
                                {
                                    strcpy(p, achDegs);
                                    p += strlen(achDegs);
                                }
                                break;
                            case 'm':
                                {
                                    strcpy(p, achMins);
                                    p += 2;
                                }
                                break;
                            case 's':
                                {
                                    strcpy(p, achSecs);
                                    p += 2;
                                }
                                break;
                        }
                        inMacro = false;
                    }
                }
                break;
            default:
                {
                    if (inMacro)
                    {
                        macro = *f;
                    }
                    else
                    {
                        *p++ = *f;
                    }
                }
        }
        f++;
    }

    *p = '\0';
    return targetBuffer;
}

const char *DayTime::formatString(char *targetBuffer, const char *format, long *pSecs) const
{
    long secs = pSecs == nullptr ? totalSeconds : *pSecs;
    char sgn  = secs < 0 ? '-' : '+';
    secs      = labs(secs);
    long degs = secs / 3600;
    secs      = secs - degs * 3600;
    long mins = secs / 60;
    secs      = secs - mins * 60;

    return formatStringImpl(targetBuffer, format, sgn, degs, mins, secs);
}

}  // namespace core
