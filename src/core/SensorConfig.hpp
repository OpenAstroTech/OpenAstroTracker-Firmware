#pragma once

namespace core
{
/// The polarity and back-off tuning of the switches and homing sensors.
///
/// The active state is stored as the raw `LOW`/`HIGH` value the board configuration names, which
/// is `0`/`1` on every Arduino core; keeping it an `int` is what lets this struct live in `core/`
/// without `<Arduino.h>`.
///
/// Not here: the homing-search distance. Its only reader is the Meade command adapter, which may
/// not include `core/`, so that macro stays where it is.
struct SensorConfig {
    int raEndSwitchActiveState        = 0;
    int decEndSwitchActiveState       = 0;
    int raHomingSensorActiveState     = 0;
    int decHomingSensorActiveState    = 0;
    float raEndSwitchBackslewDegrees  = 0.5f;
    float decEndSwitchBackslewDegrees = 0.5f;
};

}  // namespace core
