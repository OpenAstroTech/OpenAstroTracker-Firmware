#pragma once

#include <stdint.h>

#include "core/Button.hpp"

namespace app
{

/// What the loop saw on this pass.
///
/// `edge` is the key that just settled on a new value (or `None`), `held` is what is down right
/// now. Screens that auto-repeat on a held key need the second one; screens that act on presses
/// need the first.
struct MenuKeyInput {
    core::Button edge;
    core::Button held;
};

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
