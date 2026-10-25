#pragma once

#include <stdint.h>

#include "core/Button.hpp"

namespace app
{

/// What a screen wants the menu loop to do after it handled a key.
enum class MenuKeyResult : uint8_t
{
    /// The screen did not use this key.
    Ignored,
    /// The screen used the key: the loop should not deliver another until it is released.
    WaitForRelease,
    /// The operator asked to move to the next menu item.
    NextScreen,
};

}  // namespace app
