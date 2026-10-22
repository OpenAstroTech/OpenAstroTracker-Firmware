#pragma once

#include "IMountConfiguration.hpp"
#include "core/FocusController.hpp"

namespace ports
{

/// The axes a board may not have: the focuser and the AZ/ALT pair.
///
/// Phase 4 made all three runtime-present — a board without them holds a null axis, and every
/// entry point here is a no-op rather than absent code. That is exactly why they belong in one
/// port: the command layer calls them without asking whether this board has the hardware, which is
/// the point of the work that made them runtime data.
///
/// Extends the interface chain rather than standing beside it, so `Mount` still carries one vtable
/// pointer (see `IMountConfiguration`).
class IMountOptionalAxes : public IMountConfiguration
{
  public:
    virtual ~IMountOptionalAxes()
    {
    }

    // --- focuser ---
    virtual void focusMoveBy(long steps)                               = 0;
    virtual void focusSetStepperPosition(long steps)                   = 0;
    virtual long focusGetStepperPosition()                             = 0;
    virtual void focusSetSpeedByRate(int rate)                         = 0;
    virtual void focusContinuousMove(core::FocuserDirection direction) = 0;
    virtual void focusStop()                                           = 0;
    virtual bool isRunningFocus() const                                = 0;

    // --- azimuth / altitude ---
    virtual void moveBy(int direction, float arcMinutes)                    = 0;
    virtual void moveAZALTToHome()                                          = 0;
    virtual void getAZALTPositions(long &azimuthSteps, long &altitudeSteps) = 0;
};

}  // namespace ports
