**V1.9.26 - Updates**
- Delete unintentionally added workflow file

**V1.9.25 - Updates**
- clang-format codebase

**V1.9.24 - Updates**
- Fix hold currents of AZ/ALT/Focuser drivers.

**V1.9.23 - Updates**
- Add option to keep focuser motor enabled after moving.

**V1.9.22 - Updates**
- Add focuser pins for MKS Gen L v2.0.

**V1.9.21 - Updates**
- Fix southern hemisphere reversed DEC free slew directions

**V1.9.20 - Updates**
- Made github version check more robust

**V1.9.19 - Updates**
- Added support to reset the focus stepper position
- Fixed some focus validation tests

**V1.9.18 - Updates**
- Fix RA guiding multiplier not being applied correct

**V1.9.17 - Updates**
- Fix southern hemisphere returning RA offset by -12hr

**V1.9.16 - Updates**
- Add Meade extension command to move steppers by steps.

**V1.9.15 - Updates**
- Fixed incorrect speed for DEC when using 28BYJ-48 steppers

**V1.9.14 - Updates**
- Fix hardware reporting for "Unknown" boards

**V1.9.13 - Updates**
- Fix southern hemisphere returning incorrect DEC values (edge case)

**V1.9.12 - Updates**
- Fix southern hemisphere returning incorrect DEC values

**V1.9.11 - Updates**
- Add support for focuser on E1 motor for MKS board. 
- Add focuser command support to LX200 protocol.

**V1.9.10 - Updates**
- Fixed a bug that caused a compilation fail when enabling LCD_BUTTON_TEST
- Allowed connection test to be set in local config

**V1.9.09 - Updates**
- Sample Local Config - disabled as default

**V1.9.08 - Updates**
- Continuous integration improvements. Checking for version and changelog changes automatically

**V1.9.07 - Updates**
- ALT and AZ steppers can now be enabled and configured seperately.
- Reduced hold current for AZ and ALT to 10% if using TMCs and NEMAs.
- Fixed incorrect response in Meade LX200 guide pulse command.

**V1.9.05 - Updates**
- Lowered connection baudrate to 19200.
- Fixed incorrect response in Meade LX200 quit command.

**V1.9.03 - Updates**
- Removed connection test check and made it standard.
- Switched motors to normal mode (better perf), but allowed silent mode through XX_UART_STEALTH_MODE defines.
- Made DEC driver be in Guide microstep mode unless slewing.
- Added command to Meade protocol to calculate stepper positions for stellar coordinates.
- Fixed an overflow bug in the CAL LCD menu for RA/DEC steps (esoteric mounts).

**V1.9.00 - Updates**
- Fixes RMS current setting for TMC2209 UART to be more accurate and consistent
- Disables i_scale_analog (aka "USE_VREF") by default so that results are more consistent between users. rms_current will no longer depend on Vref, unless specifically configured by the user.
- Changes UART baud rate to 19200 rather than the default of 115200 - the Rx channel was too unreliable and repeatedly had CRC errors at the higher baud rate.
- Adds a test method for visual confirmation of Tx-only UART connectivity. At boot (before tracking starts), each axis will rotate slightly in each direction driven purely by UART commands. If the user sees the movement, they know UART Tx is functioning.
- Re-enabled support for different microstepping settings for slewing and tracking.
- Allowed microstep settings to be defined in the local config.
- Seperated guide stepper from DEC stepper so that guiding doesn't change DEC coordinates.
- Fixed a bug where tracking speed was not calculated correctly, nor adjusted on the fly when RA Steps/deg were changed.
- Saved some RAM by moving strings to flash memory.
- Fixed a bug where RA would jump a little at the beginning of a GoTo operation.
- Added Meade command to allow querying driver settings.

**V1.8.74 - Updates**
- Fixed bug in LX200 :XSD# command that prevented DEC steps from being stored.
- Fixed logging error in Wifi code.

**V1.8.73 - Updates**
- Fixed network status return value
- Added mode to network status
- Coding style applied.
- Updated Meade docs

**V1.8.72 - Updates**
- Factory reset now not only clears EEPROM, but also re-initializes everything.

**V1.8.71 - Updates**
- Added temperature retrieval (Digital Level addon)
- Made get LST (:XGL) and get HA (:XGH) Meade calls return realtime data.
- Fixed a bug in :Ga and :GL commands that returned a sign in front of the hour.
- Added comments for Get functions.

**V1.8.70 - Updates**
- Reduced required GPS coordinate freshness to 30s
- Changed LCD display of GPS sync to use more symbols and blink regularly
- Added logging for GPS
- Changed command marker comments in Meade code
- Capped daytime hours to 999

**V1.8.69 - Updates**
- Added Meade support for querying, setting and clearing the DEC motor limits.

**V1.8.68 - Updates**
- Fixed bug that returned incorrect data for site time (delimiter was missing).

**V1.8.67 - Updates**
- GPS simply provides data to mount now (no more time calcs)
- Added UTC menu item in CAL
- Unified coding style (specifically braces) in CAL and EEPROM files.
- Added UTC storage functions to EEPROM class.
- Fixed a bug that prevented extended flags from being used in EEPROM class.

**V1.8.66 - Updates**
- Fixed Longitude code to handle LX200GPS spec. New OATControl required.

**V1.8.65 - Updates**
- Guide pulse command should handle lowercase

**V1.8.64 - Updates**
- Cleaned up Meade comments
- Fixed a bug that caused issues when you turned off microstepping.
- Fixed a bug that prevented DEC from using floating point steps per degree.

**V1.8.63 - Updates**
- Fixed up tracking speed calculations. Steps per degree now support tenths.
- Fixed a bug that caused issues when you turned off microstepping.
- Improved stepper servicing performance on ESP32 boards.

**V1.8.62 - Updates**
*(Numbers are off because git)*
- Rewrote the coordinate handling to correctly handle negative coordinates between 0 and -1.
- Fixed some bugs in the coordinate handling.
- Prevented backlash code from doing nothing when backlash is zero.
- Added some more logging.
- Removed an errant Meade return value when no command was sent.

**V1.8.53 - Updates**
- Enforced DEC limits on slew movements. There is currently no indication that limiting has occurred.
- Added display of DEC limits to INFO screen.
- Changed some logging output.

**V1.8.52 - Updates**
- Moved LCD updates out of the mount loop.
- Fixed a bug that did not correctly handle displaying Roll and Pitch offset.
- Added ability to define DEC limits. Limits are not enforced yet, that code is still in development.
- Added new debug channel for digital level.

**V1.8.51 - Updates**
- Added EEPROM clear command to extended LX200 protocol.

**V1.8.50 - Updates**
- Fixed a bug related to setting EEPROM values.
- Moved some initialization code to a later stage during boot.

**V1.8.47 - Updates**
- Incorporated Roll levelling into guided startup (if Digital Level is present).

**V1.8.46 - Updates**
- Switched units for manual slew speed to degrees/sec.

**V1.8.45 - Updates**
- Fixed some ESP32 build errors
- Checked from some invalid ESP32 configurations
- Corrected some Meade command comments
- Sorted GO targets alphabetically (except Polaris)

**V1.8.44 - Updates**
- Fixed compiler define bug when Startup was disabled.

**V1.8.43 - Updates**
- Added ability to switch to the always-on coordinate display (top line RA, bottom line DEC) in CTRL menu. SELECT exits this mode.

**V1.8.42 - Updates**
- Fixed bug that turned of tracking when using NEMA17 with a non-UART driver.

**V1.8.41 - Updates**
- Removed UNO and ESP8266 support. Memory constraints are unsustainable.
- Removed Heating menu, since it hasn't been supported in months.
- Added user-submitted support for I2C LCD shield (thanks Christian Kardach)
- Added user-submitted support for Bluetooth on the ESP32 board (thanks mholeys)

**V1.8.40 - Updates**
- Fixed RA negative movement, which was not switching to Slew microstepping settings

**V1.8.39 - Updates**
- Fixed guiding to not jump back and forth on pulses.
- Fixed tracking during slew. We now turn off tracking for slew and compensate at the end of the slew.
- Fixed type for DEC motor speed.
- Added M33 and Pleiades to GO menu.
- Added lots of debug output around stepper motor control under new macro DEBUG_STEPPERS

**V1.8.38 - Updates**
- Fixed code to allow GPS use with Headless client.
- Tidied up some LCD strings

**V1.8.37 - Updates**
- Fixed bug that prevented the parking position from being read from persistent storage.
- Made sure Parking position was cleared  on EEPROM clear.

**V1.8.36 - Updates**
- Added GPS control to LX200 Meade protocol

**V1.8.35 - Updates**
- Added ability to define a parking position that is seperate from the Home position.

**V1.8.34 - Updates**
- Fixed a bunch of typos that massively screwed up DEC guiding

**V1.8.33 - Updates**
- Allowed axis inversion to be configured in local config
- Added Digital Level support to Meade serial protcol
- Fixed compilation problem with Digital Level

**V1.8.32 - Updates**
- Support for automatic local config selection based on selected board
- Included sample local config file.

**V1.8.31 - Updates**
- Now supports local configuration settings (thanks, AndreStefanov!)
- Major code re-structure, neccessary as the project grows.
- Fixed a bug that turned off Tracking when you tried to slew RA to the current location
- Moved some defines around
- Improved some comments
- Fixed a bug related to DEC guiding

**V1.8.27 - Updates**
- YABBFDTLOC (Yet Another Build Break FIx Due To Lack Of Coffee)
- Update Polaris to latest RA value.

**V1.8.26 - Updates**
- Build break fix for GPS-enabled configurations

**V1.8.25 - Updates**
- Added special char for Tracking display on LCD
- Renamed some conflicting defines (HALFSTEP et al to HALFSTEP_MODE)
- Moved all Debug strings to Flash memory
- Simplified EEPROM class (made it all static)
- Added prompt when pressing SELECT in either Roll or Pitch display in CAL menu, whether to save the reference value
- Fixed ESP32 compilation problem in freeMemory() implementation

**V1.8.24 - Updates**
- Allow EEPROM clearing via LCD buttons (press DOWN on boot)

**V1.8.23 - Updates**
- GPS location storage was broken. Fixed.

**V1.8.19 - Updates**
- Azimuth motor control during Polar Alignment was not implemented.
- After confirming Polar Alignment, the display shows slewing activity and no longer switches to the INFO menu.
- Pressing Right Button while slewing home after Polar Alignment now stops the slewing before changing menus.
- Removed some logging

**V1.8.16 - Updates**
- Throttled the INFO display update frequency (it was updating every cycle before).
- Added temperature display when using the Digital Level module
- Made the axis swap for the Digital Level on by default since most people will use the STL in the repo for the holder.

**V1.8.15 - Updates**
- Fixed linker error in ESP32 build.

**V1.8.14 - Updates**
- Turned the power to the Azimuth and Altitude motors on and off as needed.
- Enabled Altitude correction from LX200 commands
- Prevented tracking from starting after parking for NEMA motors.

**V1.8.13 - Updates**
- Made the LEFT button leave the Roll and Pitch menus without storing the offset (like SELECT does).

**V1.8.12 - Updates**
- Temporary fix for compile error from 1.8.11
- add different MS modes for DEC slewing/guiding
- add UART "noisefeedback" to config
- hardcoded guidemultipliers for 28BYJ-48 steppers
- stop tracking during a slew with NEMAs to prevent inaccurate slews

**V1.8.11 - Updates**
- Added support for electronic levelling via MPU-6050 module (CAL menu) (thanks to Marcel Isler for initial code)
- Disabled AzAlt stepper motors after leaving CAL menu
- Moved an overlapping EEPROM storage space (brightness, not used)

**V1.8.10 - Updates**
- Permanent display of tracking state in top-right corner of display.
- Shortened menu display
- Full GPS module (GT-U7) support for LST and location
- Separated HA menu into two files (one for GPS support, one for non-GPS)
- Added display of current location to INFO menu

**V1.8.06 - Updates**
- DEC axis was inadvertently inverted by default after merge. Fixed.
- Guide pulses were not 2x sidereal and 0 for default steppers. Fixed.

**V1.8.05 - Updates**
- Reverted Headless mode to off
- Increased screen updates back to 5Hz.

**V1.8.04 - Updates**
 - Merged the developautopa branch
 - Made AzAlt motors configurable.
 - Added extensions to the Meade protocol (:MAZn.n# and :MALn.n#) to move the aziumth and altitude motors.
 - Added Az and ALt motor status to :GX# command output
 - Added GPS and AzAlt features to hardware info query (:XGM#)
 - New CAL menu items to adjust Azimuth and Altitude in arcminutes. 
