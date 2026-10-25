#pragma once

/// The ids the menu items carry. Only the id matters to the loop; the screen behind it is whatever
/// `MenuHost` maps that id to.
#define RA_Menu          1
#define DEC_Menu         2
#define HA_Menu          3
#define Heat_Menu        4
#define Calibration_Menu 5
#define Focuser_Menu     6
#define Control_Menu     7
#define Home_Menu        8
#define POI_Menu         9
#define Status_Menu      10

/// How many menu items at most? The menu's item table is sized with this.
#define MAXMENUITEMS 11
