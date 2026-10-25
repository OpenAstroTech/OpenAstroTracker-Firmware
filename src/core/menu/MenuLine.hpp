#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/menu/MenuModel.hpp"

namespace core
{

/// Writes the menu row for the current selection into `out`.
///
/// Each item renders as `">text<"` when it is active and `" text "` otherwise. The window keeps the
/// active item's arrows as close to the middle of the row as the longest item allows, which is why
/// the selector appears not to move while the list scrolls behind it. The last column stays free so
/// the row never runs into the tracking indicator.
///
/// `out` should hold `columns + 1` bytes including the terminator; a smaller buffer truncates.
void composeMenuRow(char *out, size_t capacity, const MenuModel &menu, uint8_t columns);

}  // namespace core
