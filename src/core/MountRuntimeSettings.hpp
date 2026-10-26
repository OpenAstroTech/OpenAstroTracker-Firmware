#pragma once

#include "core/PortVocabulary.hpp"

namespace core
{
/// The settings the mount keeps live in RAM: the ones a host or a menu changes and the EEPROM
/// remembers, as opposed to `MountConfig`'s build-time constants.
///
/// They are grouped here because they are one decision surface — `ports::IMountConfiguration`
/// exists to read and write exactly these — and because their owner should be the domain, not the
/// executor. The DEC limits stay in slewing microsteps, as the firmware has always stored them,
/// with 0 meaning "no limit".
class MountRuntimeSettings
{
  public:
    /// Restores the build-time defaults. What `Mount::initializeVariables()` used to do field by
    /// field.
    void reset(float defaultRaStepsPerDegree, float defaultDecStepsPerDegree)
    {
        _raStepsPerDegree        = defaultRaStepsPerDegree;
        _decStepsPerDegree       = defaultDecStepsPerDegree;
        _decLowerLimitSteps      = 0;
        _decUpperLimitSteps      = 0;
        _backlashCorrectionSteps = 0;
        _moveRate                = 4;
    }

    float stepsPerDegree(StepperAxis axis) const
    {
        return axis == DEC_STEPS ? _decStepsPerDegree : _raStepsPerDegree;
    }

    void setStepsPerDegree(StepperAxis axis, float value)
    {
        (axis == DEC_STEPS ? _decStepsPerDegree : _raStepsPerDegree) = value;
    }

    long decLowerLimitSteps() const
    {
        return _decLowerLimitSteps;
    }

    long decUpperLimitSteps() const
    {
        return _decUpperLimitSteps;
    }

    void setDecLowerLimitSteps(long steps)
    {
        _decLowerLimitSteps = steps;
    }

    void setDecUpperLimitSteps(long steps)
    {
        _decUpperLimitSteps = steps;
    }

    int backlashCorrectionSteps() const
    {
        return _backlashCorrectionSteps;
    }

    void setBacklashCorrectionSteps(int steps)
    {
        _backlashCorrectionSteps = steps;
    }

    /// Slew-rate index, 1 (slowest) to 4 (fastest).
    int moveRate() const
    {
        return _moveRate;
    }

    void setMoveRate(int rate)
    {
        _moveRate = rate;
    }

  private:
    /// Only RA and DEC have a steps-per-degree figure; the other axes ignore the argument.
    float _raStepsPerDegree      = 0.0f;
    float _decStepsPerDegree     = 0.0f;
    long _decLowerLimitSteps     = 0;
    long _decUpperLimitSteps     = 0;
    int _backlashCorrectionSteps = 0;
    int _moveRate                = 4;
};

}  // namespace core
