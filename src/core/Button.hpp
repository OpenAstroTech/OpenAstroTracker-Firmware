#pragma once

#include <stdint.h>

namespace core
{

/// The keys the menu panel reports.
///
/// A value type rather than a HAL type: it is the vocabulary of `ports::IButtons`, so the screens
/// can compare keys without seeing a panel library.
enum class Button : uint8_t
{
    None,
    Right,
    Up,
    Down,
    Left,
    Select,
    Invalid,
};

}  // namespace core
