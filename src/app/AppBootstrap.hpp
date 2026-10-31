#pragma once

namespace app
{

/// Runs the whole boot sequence: debug hooks or the serial port, the status panel, the
/// peripherals, the driver pins, the menu, the command processor, the stepper service, the
/// persisted configuration and the final `bootComplete()`.
///
/// `setup()` used to be this sequence inline. It lives here so the Arduino entry point is only
/// "start the app, then run it": the boot order is `app/` policy, and each step's board
/// conditional sits beside the wiring it selects rather than in the sketch.
void bootstrap();

}  // namespace app
