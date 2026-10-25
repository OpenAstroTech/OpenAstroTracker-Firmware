#include "StepPump.hpp"

#include "EndSwitchMonitor.hpp"
#include "HomingRunner.hpp"

#if defined(ESP32) || !defined(NEW_STEPPER_LIB)

namespace app
{

StepPump::StepPump(const core::MountState &state, const core::FocusController &focus, const AxisBank &axes)
    : _state(state), _focusController(focus), _axes(axes)
{
}

void StepPump::pump()
{
    // Only process guide pulses if we are tracking.
    if (_state.test(STATUS_GUIDE_PULSE) && _state.test(STATUS_TRACKING))
    {
        _axes.tracking()->runSpeed();
        if (_state.test(STATUS_GUIDE_PULSE_DEC))
        {
            _axes.guide()->runSpeed();
        }
        return;
    }

    if (_state.test(STATUS_TRACKING))
    {
        _axes.tracking()->runSpeed();
    }

    if (_state.test(STATUS_SLEWING))
    {
        if (_state.test(STATUS_SLEWING_MANUAL))
        {
            _axes.dec()->runSpeed();
            _axes.ra()->runSpeed();
        }
        else
        {
            _axes.dec()->run();
            _axes.ra()->run();
        }
    }
    else if (!_state.test(STATUS_TRACKING) && _axes.tracking()->isRunning())
    {
        // If we are not tracking, but the tracking stepper is running, we need to let it move.
        // This can happen when we need to compensate for a slew (during which the tracker is
        // stopped). After the slew, the tracker is advanced by the distance it would have
        // travelled during the slew if it had been tracking.
        // That compensation uses goto mode (using runToNewPosition()), so we need to use run(),
        // since runSpeed() only advances the stepper when it is in constant speed mode.
        _axes.tracking()->run();
    }

    if (_state.test(STATUS_FINDING_HOME))
    {
        if (kHasRaHoming && (_axes.raHoming() != nullptr))
        {
            _axes.ra()->run();
            _axes.raHoming()->processHomingProgress();
        }
        if (kHasDecHoming && (_axes.decHoming() != nullptr))
        {
            _axes.dec()->run();
            _axes.decHoming()->processHomingProgress();
        }
    }

    if (kHasAzStepper)
    {
        _axes.az()->run();
    }
    if (kHasAltStepper)
    {
        _axes.alt()->run();
    }

    // `FocusMode::Continuous` is kept from the original, which never selected it: a
    // continuous run targets a far position and therefore uses `run()`.
    if (kHasFocusStepper && _focusController.mode() == core::FocusMode::ToTarget)
    {
        _axes.focus()->run();
    }
    else if (kHasFocusStepper && _focusController.mode() == core::FocusMode::Continuous)
    {
        _axes.focus()->runSpeed();
    }

    if (kHasRaEndSwitch && (_axes.raEndSwitch() != nullptr))
    {
        _axes.raEndSwitch()->processEndSwitchState();
    }

    if (kHasDecEndSwitch && (_axes.decEndSwitch() != nullptr))
    {
        _axes.decEndSwitch()->processEndSwitchState();
    }
}

}  // namespace app

#endif
