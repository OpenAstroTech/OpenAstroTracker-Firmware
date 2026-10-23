#pragma once

#include <stdint.h>

namespace ports
{

/// Writes to the mount's status bitfield.
///
/// The bit vocabulary lives in `core::StatusBit` (see `core/MountState.hpp`); this port only
/// exposes the two operations the legacy helpers perform on it, so they need no mount object.
class IStatusFlags
{
  public:
    virtual ~IStatusFlags()
    {
    }

    virtual void set(uint16_t bits)   = 0;
    virtual void clear(uint16_t bits) = 0;
};

}  // namespace ports
