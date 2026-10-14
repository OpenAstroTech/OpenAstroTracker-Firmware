#include "CalendarMath.hpp"

namespace core
{

static int daysInMonth(int year, int month)
{
    switch (month)
    {
        case 2:
            if (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0))
                return 29;
            return 28;
        case 4:
        case 6:
        case 9:
        case 11:
            return 30;
        default:
            return 31;
    }
}

CalendarDate addDays(CalendarDate date, long days)
{
    if (days >= 0)
    {
        date.day += static_cast<int>(days);
        while (true)
        {
            int maxDays = daysInMonth(date.year, date.month);
            if (date.day > maxDays)
            {
                date.day -= maxDays;
                date.month++;
                if (date.month > 12)
                {
                    date.month = 1;
                    date.year++;
                }
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        // Negative days: go backwards
        date.day += static_cast<int>(days);
        while (date.day < 1)
        {
            date.month--;
            if (date.month < 1)
            {
                date.month = 12;
                date.year--;
            }
            date.day += daysInMonth(date.year, date.month);
        }
    }

    return date;
}

CalendarDate localToUtcDate(CalendarDate localDate, long localSecondsOfDay, int utcOffsetHours)
{
    long utcSeconds = localSecondsOfDay - utcOffsetHours * 3600L;
    long dayShift   = 0;
    while (utcSeconds < 0)
    {
        utcSeconds += 86400L;
        dayShift--;
    }
    while (utcSeconds >= 86400L)
    {
        utcSeconds -= 86400L;
        dayShift++;
    }

    return addDays(localDate, dayShift);
}

}  // namespace core
