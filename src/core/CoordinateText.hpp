#pragma once

#include <stdint.h>

namespace core
{

/// Which rendering of a coordinate the caller wants.
///
/// The numeric values are the `B0xxx` constants the menu screens already pass through
/// `ports::IMountReadModel`, so an adapter can forward a `type` byte unchanged; `Mount`
/// asserts at compile time that the two vocabularies still agree.
enum class CoordinateFormat : uint8_t
{
    LcdMenu = 1,  ///< menu line with room for a cursor
    Meade   = 2,  ///< LX200 wire form, terminated by '#'
    Print   = 3,  ///< human-readable, no terminator
    Lcd     = 4,  ///< LCD display, no spaces
    Compact = 5,  ///< bare digits, as `:D#` uses
};

/// The mask that separates the format from the target/current flag in a `type` byte.
constexpr uint8_t kCoordinateFormatMask = 0x07;

/// Set in a `type` byte when the caller wants the target coordinate rather than the current one.
constexpr uint8_t kTargetFlag = 0x08;

/// The format a `type` byte names, ignoring the target flag.
constexpr CoordinateFormat coordinateFormat(uint8_t type)
{
    return static_cast<CoordinateFormat>(type & kCoordinateFormatMask);
}

/// The `strftime`-ish pattern `core::formatRA()` expects for a right ascension in this format.
const char *raFormat(CoordinateFormat format);

/// The same for a declination. The two tables differ: declination carries a sign, and its wire
/// form uses `*` where right ascension uses `:`.
const char *decFormat(CoordinateFormat format);

/// Places the menu's `'>'` selection cursor on one field of an already-formatted menu line.
///
/// A menu line is four-character fields, so field `active` starts at `active * 4`. A declination
/// line is one character further along for every field but the first, because the sign occupies
/// the column the cursor would otherwise land on.
void insertLcdMenuCursor(char *buffer, uint8_t active, bool hasSignColumn);

}  // namespace core
