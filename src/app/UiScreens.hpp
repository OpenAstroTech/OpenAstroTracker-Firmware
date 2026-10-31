#pragma once

#include "Configuration.hpp"

#include "MenuSurface.hpp"
#include "app/ui/MenuIds.hpp"
#include "UiSession.hpp"

#include "app/ui/SlewStatusView.hpp"
#include "app/ui/screens/ControlMenu.hpp"
#include "app/ui/screens/CalibrationMenu.hpp"
#include "app/ui/screens/HaMenu.hpp"
#include "app/ui/screens/StartupWizard.hpp"
#include "app/ui/screens/InfoMenu.hpp"
#include "app/ui/screens/PoiMenu.hpp"

namespace app
{
/// The UI singletons. They own nothing but their own selection, and the menu loop is a single task,
/// so one instance each is the whole lifetime story. Declared in one place so the loop, the serial
/// path and the startup wizard all name the same object.
extern SlewStatusView slewStatusView;

/// The menu screens only exist where there is a menu.
#if DISPLAY_TYPE > 0

    #if SUPPORT_POINTS_OF_INTEREST == 1
extern PoiMenu poiMenu;
    #endif

/// The INFO screen is not optional on a board with a display: `Configuration_adv.hpp` defines
/// `SUPPORT_INFO_DISPLAY` to 1 whenever `DISPLAY_TYPE != DISPLAY_TYPE_NONE`.
extern InfoMenu infoMenu;

/// The board facts the status screen shows, gathered from wherever this board keeps them.
InfoMenuEnvironment infoEnvironment();

extern ControlMenu controlMenu;
extern HaMenu haMenu;
extern CalibrationMenu calibrationMenu;
extern StartupWizard startupWizard;

#endif  // DISPLAY_TYPE > 0

/// Registers the top-level menu items on the given surface.
///
/// The feature switches here are the same ones that decide which screens exist above, which is why
/// the ladder lives beside them rather than in `setup()`: one place answers "does this board have a
/// focuser menu".
void registerMenuItems(MenuSurface &surface);

/// True while a serial host owns the display; a board without serial control never is.
bool serialHostOwnsDisplay();

}  // namespace app
