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

    virtual void onMountEvent(const types::MountEvent &event) = 0;
};

}  // namespace ports
