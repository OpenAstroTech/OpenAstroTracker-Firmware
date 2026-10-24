#include "StepPump.hpp"

#include "../EndSwitches.hpp"
#include "../HallSensorHoming.hpp"

#if defined(ESP32) || !defined(NEW_STEPPER_LIB)

namespace app
{

StepPump::StepPump(const core::MountState &state,
                   const core::FocusController &focus,
                   const Mount::Steppers &steppers,
                   const Mount::Sensors &sensors)
    : _state(state), _focusController(focus), _steppers(steppers), _sensors(sensors)
{
}

void StepPump::pump()
{
    // Only process guide pulses if we are tracking.
    if (_state.test(STATUS_GUIDE_PULSE) && _state.test(STATUS_TRACKING))
    {
        _steppers.tracking->runSpeed();
        if (_state.test(STATUS_GUIDE_PULSE_DEC))
        {
            _steppers.guide->runSpeed();
        }
        return;
    }

    if (_state.test(STATUS_TRACKING))
    {
        _steppers.tracking->runSpeed();
    }

    if (_state.test(STATUS_SLEWING))
    {
        if (_state.test(STATUS_SLEWING_MANUAL))
        {
            _steppers.dec->runSpeed();
            _steppers.ra->runSpeed();
        }
        else
        {
            _steppers.dec->run();
            _steppers.ra->run();
        }
    }
    else if (!_state.test(STATUS_TRACKING) && _steppers.tracking->isRunning())
    {
        // If we are not tracking, but the tracking stepper is running, we need to let it move.
        // This can happen when we need to compensate for a slew (during which the tracker is
        // stopped). After the slew, the tracker is advanced by the distance it would have
        // travelled during the slew if it had been tracking.
        // That compensation uses goto mode (using runToNewPosition()), so we need to use run(),
        // since runSpeed() only advances the stepper when it is in constant speed mode.
        _steppers.tracking->run();
    }

    if (_state.test(STATUS_FINDING_HOME))
    {
        if (Mount::kHasRaHoming && (_sensors.raHoming != nullptr))
        {
            _steppers.ra->run();
            _sensors.raHoming->processHomingProgress();
        }
        if (Mount::kHasDecHoming && (_sensors.decHoming != nullptr))
        {
            _steppers.dec->run();
            _sensors.decHoming->processHomingProgress();
        }
    }

    if (Mount::kHasAzStepper)
    {
        _steppers.az->run();
    }
    if (Mount::kHasAltStepper)
    {
        _steppers.alt->run();
    }

    // `FocusMode::Continuous` is kept from the original, which never selected it: a
    // continuous run targets a far position and therefore uses `run()`.
    if (Mount::kHasFocusStepper && _focusController.mode() == core::FocusMode::ToTarget)
    {
        _steppers.focus->run();
    }
    else if (Mount::kHasFocusStepper && _focusController.mode() == core::FocusMode::Continuous)
    {
        _steppers.focus->runSpeed();
    }

    if (Mount::kHasRaEndSwitch && (_sensors.raEndSwitch != nullptr))
    {
        _sensors.raEndSwitch->processEndSwitchState();
    }

    if (Mount::kHasDecEndSwitch && (_sensors.decEndSwitch != nullptr))
    {
        _sensors.decEndSwitch->processEndSwitchState();
    }
}

}  // namespace app

#endif
