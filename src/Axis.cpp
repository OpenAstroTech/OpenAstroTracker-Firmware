#include "../Configuration.hpp"
#include <stdint.h>
#include "Axis.h"
#include "Mount.hpp"

template <>
Angle Mount::RA::position()
{
    LOGV1(DEBUG_STEPPERS, F("[STEPLIB]: called RA position specialization"));
    auto trackedTime = _totalTrackingTime + ((_recentTrackingStartTime) ? millis() - _recentTrackingStartTime : 0);
    return Axis::position() - (TRACKING_SPEED * trackedTime);
}

template <>
Angle Mount::RA::trackingPosition()
{
    LOGV1(DEBUG_STEPPERS, F("[STEPLIB]: called RA trackingPosition specialization"));
    auto trackedTime = _totalTrackingTime + ((_recentTrackingStartTime) ? millis() - _recentTrackingStartTime : 0);
    return Axis::TRACKING_SPEED * trackedTime;
}

template <>
void Mount::RA::setPosition(Angle value)
{
    Axis::setPosition(value);
    _totalTrackingTime       = 0;
    _recentTrackingStartTime = millis();
}

