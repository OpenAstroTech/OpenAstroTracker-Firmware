#include "../Configuration.hpp"
#include <stdint.h>
#include "StepperConfiguration.h"
#include "Mount.hpp"

template <> Angle Mount::RA::position()
{
    float trackedTime  = (_totalTrackingTime + ((_recentTrackingStartTime) ? millis() - _recentTrackingStartTime : 0)) / 1000.0f;
    Angle trkPos      = (TRACKING_SPEED * trackedTime);
    float posGuideTime = _posGuidingTime / 1000.0f;  // seconds spent guiding in positive direction
    Angle posGuidePos = (POS_GUIDING_SPEED * posGuideTime);
    float negGuideTime = _negGuidingTime / 1000.0f;  // seconds spent guiding in positive direction
    Angle negGuidePos = (NEG_GUIDING_SPEED * negGuideTime);

    Angle raPos = config::RA::stepper::position() / config::RA::TRANSMISSION;
    LOGV4(DEBUG_STEPPERS,
          F("[STEPLIB]: Called RA GetPosition specialization. Time: TRK: %f, PG:%f,  NG: %f "),
          trackedTime,
          posGuideTime,
          negGuideTime);
    LOGV6(DEBUG_STEPPERS,
          F("[STEPLIB]: RA GetPosition. RA: %f,  TRK:%f,  NG: %f,  PG:%f -> RA: %f"),
          raPos.deg(),
          trkPos.deg(),
          negGuidePos.deg(),
          posGuidePos.deg(),
          (raPos - trkPos - posGuidePos - negGuidePos).deg());
    return raPos - trkPos - posGuidePos - negGuidePos;
}

template <> unsigned long Mount::RA::getTotalTrackingTime()
{
    return _totalTrackingTime;
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
    LOGV2(DEBUG_MOUNT, F("[STEPLIB]: special RA setPosition(%f)"), value.deg());
    config::RA::stepper::position(value * config::RA::TRANSMISSION);
    _totalTrackingTime       = 0;
    _recentTrackingStartTime = millis();
    _negGuidingTime          = 0UL;
    _posGuidingTime          = 0UL;
}

template <> Angle Mount::DEC::position()
{
    float posGuideTime = _posGuidingTime / 1000.0f;  // seconds spent guiding in positive direction
    Angle posGuidePos = (POS_GUIDING_SPEED * posGuideTime);
    float negGuideTime = _negGuidingTime / 1000.0f;  // seconds spent guiding in positive direction
    Angle negGuidePos = (NEG_GUIDING_SPEED * negGuideTime);

    Angle decPos = config::DEC::stepper::position() / config::DEC::TRANSMISSION;
    LOGV3(DEBUG_STEPPERS,
          F("[STEPLIB]: Called DEC GetPosition specialization. Time: PG:%f,  NG: %f "),
          posGuideTime,
          negGuideTime);
    LOGV5(DEBUG_STEPPERS,
          F("[STEPLIB]: DEC GetPosition. DEC: %f,  NG: %f,  PG:%f -> DEC: %f"),
          decPos.deg(),
          negGuidePos.deg(),
          posGuidePos.deg(),
          (decPos - posGuidePos - negGuidePos).deg());
    return decPos - posGuidePos - negGuidePos;
}

template <> void Mount::DEC::setPosition(Angle value)
{
    LOGV2(DEBUG_MOUNT, F("[STEPLIB]: special DEC setPosition(%f)"), value.deg());
    config::DEC::stepper::position(value * config::DEC::TRANSMISSION);
    _negGuidingTime = 0UL;
    _posGuidingTime = 0UL;
}
