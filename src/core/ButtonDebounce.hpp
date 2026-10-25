#pragma once

#include <stdint.h>

#include "core/Button.hpp"

namespace core
{

/// Commits a raw key once it has been stable for a moment.
///
/// The panel reports whatever it reads now; a membrane keypad bounces, so a key counts as pressed
/// only after it has not changed for `kCommitMs`. `changed()` reports edges of the *committed* key,
/// which is what the screens consume, so a bounce cannot open a screen twice.
class ButtonDebounce
{
  public:
    /// How long a raw reading must hold before it becomes the committed key.
    static constexpr uint32_t kCommitMs = 5;

    ButtonDebounce();

    /// Feeds one poll. `nowMs` is a free-running millisecond counter, so the comparison is an
    /// unsigned subtraction that survives the wrap.
    void update(Button raw, uint32_t nowMs);

    Button committed() const;

    /// True once per change of the committed key, with the new key in `out`.
    bool changed(Button *out);

  private:
    Button _raw;
    Button _committed;
    Button _reported;
    uint32_t _rawSince;
};

}  // namespace core
