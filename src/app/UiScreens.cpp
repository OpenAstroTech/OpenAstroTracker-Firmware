#include "app/UiScreens.hpp"

#include "Utility.hpp"
#include "app/BoardWiring.hpp"

namespace app
{

SlewStatusView slewStatusView;
ControlMenu controlMenu;
HaMenu haMenu;

/// The wizard's route depends on two board facts, which is why it is built here rather than in the
/// screen.
StartupWizard startupWizard(USE_GYRO_LEVEL == 1, USE_GPS == 1);

#if SUPPORT_POINTS_OF_INTEREST == 1
PoiMenu poiMenu;
#endif

#if SUPPORT_INFO_DISPLAY == 1
InfoMenu infoMenu;

InfoMenuEnvironment infoEnvironment()
{
    InfoMenuEnvironment environment;
    environment.version         = VERSION;
    environment.freeMemoryBytes = freeMemory();
    #if USE_GYRO_LEVEL == 1
    environment.gyroTemperatureC = gyro->temperature();
    environment.hasGyro          = true;
    #endif
    return environment;
}
#endif

bool serialHostOwnsDisplay()
{
#if SUPPORT_SERIAL_CONTROL == 1
    return inSerialControl;
#else
    return false;
#endif
}

}  // namespace app
