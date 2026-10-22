#pragma once

#include <stdint.h>

#include "Types.hpp"

namespace ports
{

/// What the command layer may ask the mount to move.
///
/// The command layer is the only caller of these (the menus call `Mount` directly today), so this
/// is the narrow slice of `Mount` that Phase 5 step 3 needs, not a facade of the whole class: the
/// adapter's *other* dependencies are separately named and will be moved one group at a time.
///
/// `direction` stays the `int` bitmask the mount has always used (`EAST`, `WEST`, `NORTH`,
/// `SOUTH`, `TRACKING`, `ALL_DIRECTIONS`). Those names live in `Mount.hpp` and are already the
/// vocabulary of the command layer; inventing a parallel enum here would mean two vocabularies in
/// flight at once. Typing it is a separate cleanup, tracked in PLAN.md.
///
/// Every request here is admitted or refused by the mount's own arbitration table
/// (`core::MountRequests`), which is why the methods return nothing: a refusal is not an error the
/// caller acts on — the legacy protocol acknowledges and the host polls for the resulting state.
class IMountMotion
{
  public:
    virtual ~IMountMotion()
    {
    }

    virtual void startSlewing(int direction)                  = 0;
    virtual void stopSlewing(int direction)                   = 0;
    virtual void stopSlewing(StepperAxis axis)                = 0;
    virtual void startSlewingToTarget()                       = 0;
    virtual void startSlewingToHome()                         = 0;
    virtual void park()                                       = 0;
    virtual void guidePulse(uint8_t direction, int duration)  = 0;
    virtual void moveStepperBy(StepperAxis which, long steps) = 0;

    virtual bool isSlewingRAorDEC() const = 0;
    virtual bool isSlewingTRK() const     = 0;
};

}  // namespace ports
