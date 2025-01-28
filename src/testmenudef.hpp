#include "testmenu.hpp"
TestMenuItem connectMenuItems[] = {
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to RA Driver", "Action:Connect|RA"),
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to DEC Driver", "Action:Connect|DEC"),
#endif
#if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to ALT Driver", "Action:Connect|ALT"),
#endif
#if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to AZ Driver", "Action:Connect|AZ"),
#endif
#if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to FOCUS Driver", "Action:Connect|FOC"),
#endif
};
TestMenu connectDriversMenu(1, "ConnectDrivers", "Main menu", connectMenuItems, sizeof(connectMenuItems) / sizeof(connectMenuItems[0]));

TestMenuItem primaryAxisMenuItems[] = {
    TestMenuItem("Move RA Axis 3h clockwise", "Action:MoveRAAxis|CW"),
    TestMenuItem("Move RA Axis 3h counter-clockwise", "Action:MoveRAAxis|CCW"),
    TestMenuItem("Move DEC Axis 15deg up", "Action:MoveDECAxis|UP"),
    TestMenuItem("Move DEC Axis 15deg down", "Action:MoveDECAxis|DOWN"),
    TestMenuItem("Stop/Start Tracking", "Action:ToggleTRK"),
};
TestMenu primaryAxisMenu(1, "PrimaryAxisMoves", "Move Primary Axes", primaryAxisMenuItems, sizeof(primaryAxisMenuItems) / sizeof(primaryAxisMenuItems[0]));

TestMenuItem secondaryAxisMenuItems[] = {
    TestMenuItem("Move ALT Axis Up", "Action:MoveALTAxis|UP"),
    TestMenuItem("Move ALT Axis Down", "Action:MoveALTAxis|DOWN"),
    TestMenuItem("Move AZ Axis left", "Action:MoveAZAxis|LEFT"),
    TestMenuItem("Move AZ Axis right", "Action:MoveAZAxis|RIGHT"),
};
TestMenu secondaryAxisMenu(1, "SecondaryAxisMoves", "Move Secondary Axes", secondaryAxisMenuItems, sizeof(secondaryAxisMenuItems) / sizeof(secondaryAxisMenuItems[0]));

TestMenuItem menuItems[] = {
    TestMenuItem("List Configured Hardware", "Action:ListHardware"),
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                              \
    || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                           \
    || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem("Connect to Drivers", "Submenu:ConnectDrivers", &connectDriversMenu),
#endif
    TestMenuItem("Move Primary Axes (RA/DEC)", "Submenu:PrimaryAxisMoves", &primaryAxisMenu),
    TestMenuItem("Move Secondary Axes (ALT/AZ)", "Submenu:SecondaryAxisMoves", &secondaryAxisMenu),
    TestMenuItem("Factory Reset (Erase EEPROM)", "Action:FactoryReset"),
};

TestMenu mainTestMenu(0, "OAT/OAM Testing menu", "", menuItems, sizeof(menuItems) / sizeof(menuItems[0]));
