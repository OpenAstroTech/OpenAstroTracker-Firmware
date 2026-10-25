#include "app/UiScreens.hpp"

#include "Utility.hpp"
#include "app/BoardWiring.hpp"

namespace app
{

SlewStatusView slewStatusView;
ControlMenu controlMenu;

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
