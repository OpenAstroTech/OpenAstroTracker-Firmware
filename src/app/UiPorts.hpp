#pragma once

#include "ports/IClock.hpp"
#include "ports/IMountConfiguration.hpp"
#include "ports/IMountMotion.hpp"
#include "ports/IMountOptionalAxes.hpp"
#include "ports/IMountReadModel.hpp"
#include "ports/IMountSiteTime.hpp"
#include "ports/IScheduler.hpp"

namespace app
{

/// Everything a menu screen may ask of the mount, as ports.
///
/// The menu screens used to take `Mount &` and call ~60 of its methods. They only ever needed the
/// three surfaces the command layer already uses — motion, read model, settings — plus the clock
/// and the scheduler for their waits, so they take those instead. Nothing here is a new
/// capability: each entry is an existing port.
///
/// This is the seam that lets the screens outlive `Mount`: when the coordinator owns the
/// controllers, it implements the same ports and `UiPorts` is built from it instead.
struct UiPorts {
    ports::IMountMotion *motion        = nullptr;
    ports::IMountConfiguration *config = nullptr;
    ports::IMountReadModel *read       = nullptr;
    ports::IMountSiteTime *siteTime    = nullptr;
    ports::IMountOptionalAxes *axes    = nullptr;
    ports::IClock *clock               = nullptr;
    ports::IScheduler *scheduler       = nullptr;

    /// Same contract as `Mount::delay()`: keeps the step generator running for `ms` without
    /// touching `millis()` or `yield()` directly.
    void delay(int ms);

    /// Blocks until the axes named by `direction` (the `EAST|WEST|…|FOCUSING` bit set) stop.
    /// Ticks the mount between checks, as `Mount::waitUntilStopped()` did.
    void waitUntilStopped(int direction);

    /// The main-loop tick, so a wait keeps the mount's bookkeeping current. Set by the
    /// composition root; a wait with no tick still terminates, just without updating state.
    void (*loop)() = nullptr;
};

}  // namespace app
