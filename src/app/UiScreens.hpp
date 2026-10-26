#pragma once

#include "Configuration.hpp"

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

    #if SUPPORT_INFO_DISPLAY == 1
extern InfoMenu infoMenu;

/// The board facts the status screen shows, gathered from wherever this board keeps them.
InfoMenuEnvironment infoEnvironment();
    #endif

extern ControlMenu controlMenu;
extern HaMenu haMenu;
extern CalibrationMenu calibrationMenu;
extern StartupWizard startupWizard;

#endif  // DISPLAY_TYPE > 0

/// True while a serial host owns the display; a board without serial control never is.
bool serialHostOwnsDisplay();

}  // namespace app
