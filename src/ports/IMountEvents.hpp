#pragma once

#include "IEventSink.hpp"

namespace ports
{
/// The mount's event stream, as the command layer sees it.
///
/// Deferred Meade replies complete on mount events (`SlewArrived`, `Parked`, `FocusStopped`,
/// `HomingFinished`), so the command layer has to subscribe and unsubscribe. That is two calls, and
/// it keeps the command layer from holding on to a `Mount *` for the rest of the program's life.
class IMountEvents
{
  public:
    virtual ~IMountEvents()
    {
    }

    /// Registers a sink. Registering the same sink twice is a no-op rather than a duplicate
    /// delivery, and a full bus drops the newcomer.
    virtual void subscribe(IEventSink *sink) = 0;

    /// Removes one sink, leaving the others in place. Unknown sinks are ignored.
    virtual void unsubscribe(IEventSink *sink) = 0;
};

}  // namespace ports
