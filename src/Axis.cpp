#include "../Configuration.hpp"
#include <stdint.h>
#include "StepperConfiguration.h"
#include "Mount.hpp"

template <> Angle Mount::RA::position()
{
    auto trackedTime = (_totalTrackingTime + ((_recentTrackingStartTime) ? millis() - _recentTrackingStartTime : 0)) / 1000.0f;
    Angle raPos      = config::RA::stepper::position() / config::RA::TRANSMISSION;
    Angle trkPos     = (TRACKING_SPEED * trackedTime);
    // LOGV5(DEBUG_STEPPERS,
    //       F("[STEPLIB]: called RA position specialization. time tracked: %f.   Stpr: %f -   TRK: %f   -> RA: %f"),
    //       trackedTime,
    //       raPos.deg(),
    //       trkPos.deg(),
    //       (raPos - trkPos).deg()
    //       );
    return raPos - trkPos;
}

template <> Angle Mount::RA::trackingPosition()
{
    auto trackedTime = (_totalTrackingTime + ((_recentTrackingStartTime) ? millis() - _recentTrackingStartTime : 0)) / 1000.0f;
    // LOGV3(DEBUG_STEPPERS,
    //       F("[STEPLIB]: called RA trackingPosition specialization. time tracked: %f, TRK: %f"),
    //       trackedTime,
    //       (TRACKING_SPEED * trackedTime).deg());
    return (TRACKING_SPEED * trackedTime);
}

template <> void Mount::RA::setPosition(Angle value)
{
    Axis::setPosition(value);
    _totalTrackingTime       = 0;
    _recentTrackingStartTime = millis();
}
