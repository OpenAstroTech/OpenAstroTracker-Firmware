#pragma once

/// The words the ports are written in: the direction bitset every motion and read call takes, the
/// coordinate-format selectors the readouts use, the axis identifiers and a plain date. Arduino-free
/// and dependent on nothing, which is what lets every layer include it.

// The direction argument every motion and read-model call takes. These were `#define`s in the
// mount's header; they are here because they are the vocabulary of the ports, not of one class,
// and the firmware has always passed them as an or-ed bit set (`EAST | WEST`). The values are
// pinned to `core::SlewDirection` and `core::CoordinateFormat` by `static_assert` in the executor.
#define NORTH          0x01
#define EAST           0x02
#define SOUTH          0x04
#define WEST           0x08
#define ALL_DIRECTIONS 0x0F
#define TRACKING       0x10
#define FOCUSING       0x20

// Coordinate format selectors, the second argument of every readout.
#define LCDMENU_STRING     0x01
#define MEADE_STRING       0x02
#define PRINT_STRING       0x03
#define LCD_STRING         0x04
#define COMPACT_STRING     0x05
#define FORMAT_STRING_MASK 0x07

// Target-vs-current selector.
#define TARGET_STRING  0x08
#define CURRENT_STRING 0x10

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
