#include "testmenu.hpp"
TestMenuItem connectMenuItems[] = {
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to RA Driver", "Action:Connect-RA"),
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to DEC Driver", "Action:Connect-DEC"),
#endif
#if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to ALT Driver", "Action:Connect-ALT"),
#endif
#if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to AZ Driver", "Action:Connect-AZ"),
#endif
#if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to FOCUS Driver", "Action:Connect-FOC"),
#endif
};

TestMenu connectDriversMenu(1, "ConnectDrivers", "Main menu", connectMenuItems, sizeof(connectMenuItems) / sizeof(connectMenuItems[0]));

TestMenuItem menuItems[] = {
    TestMenuItem("List Configured Hardware", "Action:ListHardware"),
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                              \
    || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                           \
    || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to Drivers", "Submenu:ConnectDrivers", &connectDriversMenu),
#endif
    TestMenuItem("Move RA Axis", "Action:MoveRAAxis"),
    TestMenuItem("Move DEC Axis", "Action:MoveDECAxis"),
};

TestMenu mainTestMenu(0, "OAT/OAM Testing menu", "", menuItems, sizeof(menuItems) / sizeof(menuItems[0]));
