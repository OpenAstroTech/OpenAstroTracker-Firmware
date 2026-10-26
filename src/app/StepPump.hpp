#pragma once

#include "../../Configuration.hpp"

#if defined(ESP32) || !defined(NEW_STEPPER_LIB)

    #include "AxisBank.hpp"
    #include "../core/FocusController.hpp"
    #include "../core/MountState.hpp"

namespace app
{
/// The step-generation pump: advances every axis that is mid-move.
///
/// It decides nothing. It reads the status bits and the focus mode and calls `run()`, or
/// `runSpeed()` where the axis is holding a constant rate, on the axes; that is the only part of
/// the firmware that has to happen at step rate.
///
/// It runs from an interrupt on the boards whose driver has no step generator of its own (the
/// RP2040 timer) and from a FreeRTOS task on core 0 on the ESP32. A board whose driver generates
/// its own steps has no pump at all, which is why this class is compiled only for the platforms
/// that need it: the references below would otherwise cost SRAM on every board that never calls
/// `pump()`.
///
/// The bank is held by reference rather than copied, because the root builds the axes in `setup()`
/// while this object is a static — a copy would capture the empty struct the wiring starts from.
class StepPump
{
  public:
    StepPump(const core::MountState &state, const core::FocusController &focus, const AxisBank &axes);

    /// One service interval. Called from interrupt context on RP2040.
    void pump();

  private:
    const core::MountState &_state;
    const core::FocusController &_focusController;
    const AxisBank &_axes;
};

}  // namespace app

#endif
