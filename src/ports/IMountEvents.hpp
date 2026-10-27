#pragma once

#include "IEventSink.hpp"

namespace ports
{
/// The mount's event stream, as the command layer sees it.
///
/// Deferred Meade replies complete on mount events (`SlewArrived`, `Parked`, `FocusStopped`,
/// `HomingFinished`), so the adapter has to be able to subscribe and unsubscribe. That is two
/// calls, but it is the difference between holding a `Mount *` for the rest of the program's life
/// and holding the four ports the command layer actually uses.
class IMountEvents
{
  public:
    virtual ~IMountEvents()
    {
    }

    virtual void subscribe(IEventSink *sink)   = 0;
    virtual void unsubscribe(IEventSink *sink) = 0;
};

}  // namespace ports
