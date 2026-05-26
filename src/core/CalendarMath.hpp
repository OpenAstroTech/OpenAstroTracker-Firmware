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

}  // namespace core
