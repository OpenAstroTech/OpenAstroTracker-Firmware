#ifndef _TYPES_HPP_
#define _TYPES_HPP_

enum StepperAxis
{
    RA_STEPS,
    DEC_STEPS,
    RA_AND_DEC_STEPS,
    AZIMUTH_STEPS,
    ALTITUDE_STEPS,
    FOCUS_STEPS
};

/// A calendar date as the mount keeps it: the local date the operator entered, or the one the
/// GPS reported. A plain value with no Arduino dependency, so a port can hand it out.
struct LocalDate {
    int year;
    int month;
    int day;
};

#endif