#pragma once

#include "types/MountEvent.hpp"

namespace ports
{

/// Anything that wants to observe mount events (a status display, a diagnostic log).
///
/// Deliberately not nested in `EventBus`: the bus's capacity is an implementation
/// detail, and a consumer should not have to name it to subscribe.
class IEventSink
{
  public:
    virtual ~IEventSink()
    {
    }

    /// Called once per queued event from the main-loop dispatch pass. Delivery is synchronous, so
    /// a handler should be quick; an event published here waits for the next tick.
    virtual void onMountEvent(const types::MountEvent &event) = 0;
};

}  // namespace ports
