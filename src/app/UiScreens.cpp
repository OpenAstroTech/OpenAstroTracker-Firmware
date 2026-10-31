#include "app/UiScreens.hpp"

#include "hal/arduino/FreeMemory.hpp"
#include "ports/ILogger.hpp"
#include "app/BoardWiring.hpp"

namespace app
{
SlewStatusView slewStatusView;

// Everything below is a menu screen: a board without a display compiles nothing of it in, so the
// headless builds do not pay for the selection state they never show.
#if DISPLAY_TYPE > 0

ControlMenu controlMenu;
HaMenu haMenu;

/// Which entries the calibration menu offers: the four families a board may or may not have, plus
/// the two speeds the polar step drives the AZ/ALT axes at and the configured Polaris coordinate.
CalibrationMenu::Board calibrationBoard()
{
    CalibrationMenu::Board board;

    #if SUPPORT_DRIFT_ALIGNMENT == 1
    board.hasDriftAlignment = true;
    #endif
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    board.hasAzimuth   = true;
    board.azimuthSpeed = AZ_STEPPER_SPEED;
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    board.hasAltitude   = true;
    board.altitudeSpeed = ALT_STEPPER_SPEED;
    #endif
    board.hasLevelSensor = kHasGyro;

    board.polarisRaHours   = POLARIS_RA_HOUR;
    board.polarisRaMinutes = POLARIS_RA_MINUTE;
    board.polarisRaSeconds = POLARIS_RA_SECOND;
    return board;
}

CalibrationMenu calibrationMenu(calibrationBoard());

/// The wizard's route depends on two board facts, which is why it is built here rather than in the
/// screen.
StartupWizard startupWizard(USE_GYRO_LEVEL == 1, USE_GPS == 1);

    #if SUPPORT_POINTS_OF_INTEREST == 1
PoiMenu poiMenu;
    #endif

InfoMenu infoMenu;

InfoMenuEnvironment infoEnvironment()
{
    InfoMenuEnvironment environment;
    environment.version         = VERSION;
    environment.freeMemoryBytes = freeMemory();
    if (kHasGyro)
    {
        environment.gyroTemperatureC = gyro->temperature();
        environment.hasGyro          = true;
    }
    return environment;
}

#endif  // DISPLAY_TYPE > 0

void registerMenuItems(MenuSurface &surface)
{
#if DISPLAY_TYPE > 0
    surface.addItem("RA", RA_Menu);
    surface.addItem("DEC", DEC_Menu);

    #if SUPPORT_POINTS_OF_INTEREST == 1
    surface.addItem("GO", POI_Menu);
    #else
    surface.addItem("GO", Home_Menu);
    #endif

    surface.addItem("HA", HA_Menu);

    surface.addItem("CTRL", Control_Menu);
    surface.addItem("CAL", Calibration_Menu);

    #if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
    surface.addItem("FOC", Focuser_Menu);
    #endif

    surface.addItem("INFO", Status_Menu);
#else
    (void) surface;  // no panel, no menu
#endif
}

bool serialHostOwnsDisplay()
{
    return kHasSerialControl && uiSession.serialControl;
}

}  // namespace app
