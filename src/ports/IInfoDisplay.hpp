#pragma once

#include <stdint.h>

namespace ports
{

/// Everything the optional 128x64 status panel shows.
///
/// A plain POD: the producer (the coordinator) never hands an adapter live domain
/// objects, so the panel can be rendered from a consistent snapshot.
struct InfoDisplayState {
    long raSteps;
    long decSteps;
    bool tracking;
    bool slewing;
    bool parked;
    bool atHome;
    /// True while a command is being executed, so the panel can show its activity mark.
    bool commActive;
    /// Short status line, or nullptr when there is nothing to say.
    const char *statusText;
};

/// The optional status panel next to the menu display.
class IInfoDisplay
{
  public:
    virtual ~IInfoDisplay()
    {
    }

    virtual bool begin() = 0;

    virtual void render(const InfoDisplayState &state) = 0;
};

}  // namespace ports
