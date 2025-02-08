#if TEST_VERIFY_MODE == 1
    #include "testmenu.hpp"

TestMenuItem connectMenuItems[] = {
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem(MENU_CONNECT_RA),
    #endif
    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem(MENU_CONNECT_DEC),
    #endif
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem(MENU_CONNECT_ALT),
    #endif
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem(MENU_CONNECT_AZ),
    #endif
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem(MENU_CONNECT_FOC),
    #endif
};
TestMenu connectDriversMenu(1, "ConnectDrivers", "Main menu", connectMenuItems, sizeof(connectMenuItems) / sizeof(connectMenuItems[0]));

TestMenuItem primaryAxisMenuItems[] = {
    TestMenuItem(MENU_PRIMARY_RA_CW),
    TestMenuItem(MENU_PRIMARY_RA_CCW),
    TestMenuItem(MENU_PRIMARY_DEC_UP),
    TestMenuItem(MENU_PRIMARY_DEC_DOWN),
    TestMenuItem(MENU_PRIMARY_SET_HOME),
    TestMenuItem(MENU_PRIMARY_GO_HOME),
    TestMenuItem(MENU_TOGGLE_TRK),
};
TestMenu primaryAxisMenu(
    1, "PrimaryAxisMoves", "Move Primary Axes", primaryAxisMenuItems, sizeof(primaryAxisMenuItems) / sizeof(primaryAxisMenuItems[0]));

TestMenuItem secondaryAxisMenuItems[] = {
    TestMenuItem(MENU_SECONDARY_RATE_1),
    TestMenuItem(MENU_SECONDARY_RATE_2),
    TestMenuItem(MENU_SECONDARY_RATE_3),
    TestMenuItem(MENU_SECONDARY_RATE_4),
    TestMenuItem(MENU_SECONDARY_RATE_5),
    #if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
    TestMenuItem(MENU_SECONDARY_ALT_UP),
    TestMenuItem(MENU_SECONDARY_ALT_DOWN),
    #endif
    #if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
    TestMenuItem(MENU_SECONDARY_AZ_LEFT),
    TestMenuItem(MENU_SECONDARY_AZ_RIGHT),
    #endif
};
TestMenu secondaryAxisMenu(1,
                           "SecondaryAxisMoves",
                           "Move Secondary Axes",
                           secondaryAxisMenuItems,
                           sizeof(secondaryAxisMenuItems) / sizeof(secondaryAxisMenuItems[0]));

TestMenuItem menuItems[] = {
    TestMenuItem(MENU_MAIN_LIST_HARDWARE),
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                          \
        || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                       \
        || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TestMenuItem(MENU_MAIN_CONNECT_DRIVERS, &connectDriversMenu),
    #endif
    TestMenuItem(MENU_MAIN_PRIMARY_AXIS_MOVES, &primaryAxisMenu),
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) || (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    TestMenuItem(MENU_MAIN_SECONDARY_AXIS_MOVES, &secondaryAxisMenu),
    #endif
    TestMenuItem(MENU_PASSTHROUGH_COMMAND),
    TestMenuItem(MENU_FACTORY_RESET),
};

TestMenu mainTestMenu(0, "OAT/OAM Testing menu", "", menuItems, sizeof(menuItems) / sizeof(menuItems[0]));
#endif