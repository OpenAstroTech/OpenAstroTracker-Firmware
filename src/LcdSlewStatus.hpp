#pragma once

class LcdMenu;
namespace app
{
struct UiPorts;
}

/// The slew-progress view on the menu LCD.
///
/// This used to be `Mount::displayStepperPosition()`, i.e. the domain drawing on a display, and it
/// was the last place where the mount reached into the UI. The paint lives here now and the loop
/// calls it, next to `serviceInfoDisplay()`, so a board's display is something the composition
/// root owns rather than something the mount knows about.
///
/// The state it shows is read, not owned: the percentages come from
/// `Mount::getStepperProgress()`, and the two planned-move totals decide which axes get a bar.
void displayLcdSlewStatus(app::UiPorts &ports, LcdMenu &menu);

/// Same, but at most once per `DISPLAY_UPDATE_TIME`. This is what the main loop calls.
void displayLcdSlewStatusThrottled(app::UiPorts &ports, LcdMenu &menu);
