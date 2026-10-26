#include "MountExecutor.hpp"

#include "ports/ILogger.hpp"
#include "EndSwitchMonitor.hpp"
#include "HomingRunner.hpp"

namespace app
{

#if (USE_HALL_SENSOR_RA_AUTOHOME == 1) || (USE_HALL_SENSOR_DEC_AUTOHOME == 1)
bool MountExecutor::findHomeByHallSensor(StepperAxis axis, int initialDirection, int searchDistance)
{
    #if USE_HALL_SENSOR_RA_AUTOHOME == 1
    if (axis == StepperAxis::RA_STEPS)
    {
        if (_axes.raHoming() == nullptr)
        {
            return false;
        }
        const bool started = _axes.raHoming()->findHomeByHallSensor(initialDirection, searchDistance);
        if (started)
        {
            publishEvent(core::MountEventType::HomingStarted);
        }
        return started;
    }
    #endif

    #if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    if (axis == StepperAxis::DEC_STEPS)
    {
        if (_axes.decHoming() == nullptr)
        {
            return false;
        }
        const bool started = _axes.decHoming()->findHomeByHallSensor(initialDirection, searchDistance);
        if (started)
        {
            publishEvent(core::MountEventType::HomingStarted);
        }
        return started;
    }
    #endif
    return false;
}

/////////////////////////////////
/// A board with no hall sensor compiles both branches out, so this becomes the no-op the
/// mount loop calls every tick.
#endif

void MountExecutor::processHomingProgress()
{
    if (kHasRaHoming && (_axes.raHoming() != nullptr) && (!_axes.raHoming()->isIdleOrComplete()))
    {
        _axes.raHoming()->processHomingProgress();
        if (_axes.raHoming()->isIdleOrComplete())
        {
            publishEvent(core::MountEventType::HomingFinished, _axes.raHoming()->homingResult() == core::HomingResult::Succeeded);
        }
    }

    if (kHasDecHoming && (_axes.decHoming() != nullptr) && (!_axes.decHoming()->isIdleOrComplete()))
    {
        _axes.decHoming()->processHomingProgress();
        if (_axes.decHoming()->isIdleOrComplete())
        {
            publishEvent(core::MountEventType::HomingFinished, _axes.decHoming()->homingResult() == core::HomingResult::Succeeded);
        }
    }
}

/// End switches are polled twice: once to debounce the raw pin, once to act on the
/// debounced state. On a board without them both calls compile out.
void MountExecutor::pollEndSwitches()
{
    if (kHasRaEndSwitch && (_axes.raEndSwitch() != nullptr))
    {
        _axes.raEndSwitch()->processEndSwitchState();
        _axes.raEndSwitch()->checkSwitchState();
    }

    if (kHasDecEndSwitch && (_axes.decEndSwitch() != nullptr))
    {
        _axes.decEndSwitch()->processEndSwitchState();
        _axes.decEndSwitch()->checkSwitchState();
    }
}

}  // namespace app
