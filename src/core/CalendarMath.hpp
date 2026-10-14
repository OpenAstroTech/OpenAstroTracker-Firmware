#pragma once

namespace core
{

/// Pure date structure and calendar arithmetic (no Arduino deps).
struct CalendarDate {
    int year;
    int month;
    int day;
};

/// Advance a date by the given number of days.
/// Handles month-length tables and leap-year logic including century rules.
CalendarDate addDays(CalendarDate date, long days);

/// Convert a local calendar date to the UTC calendar date.
/// localSecondsOfDay is the local time of day in seconds since local midnight (0..86399).
/// utcOffsetHours is the local offset from UTC (local = UTC + offset).
CalendarDate localToUtcDate(CalendarDate localDate, long localSecondsOfDay, int utcOffsetHours);

}  // namespace core
