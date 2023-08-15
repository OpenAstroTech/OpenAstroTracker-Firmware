**V1.12.18 - Updates**
- Added RA Autohoming via Hall sensor to the LCD menu
- Added DEC Autohoming via Hall sensor to the LCD menu
- Disabled tracking when setting up the home position via the LCD menu

**V1.12.17 - Updates**
- Fixed a bug that prevented clients from writing the DEC offset.

**V1.12.16 - Updates**
- Fixed a bug that prevented clients from reading the DEC offset.

**V1.12.15 - Updates**
- Corrected ALT calculation for OAM
- Allowed overriding AZ_CIRCUMFERENCE in local config
- Allowed overriding the maximum search distance for autohoming in local config, default remains 30degs

**V1.12.14 - Updates**
- Southern hemisphere fix
- Added and changed some logging

**V1.12.13 - Updates**
- Added ability to override RA Wheel circumference in local configuration
- Added a physical limit define which defines how far RA can turn before physical issues arise. 
- Manual slewing now targets the physical limit of the axis, instead of just going x thousand steps.
- Added support to define the back slew distance that is required to un-signal a signaled end switch. Some
  mechanical switches have a hysteresis that requires a lot (600%!) of back movement to un-signal.
- Manual slewing termination was incorrect and not executing pending operations.
- Set Tracking compensation speed to slewing speed instead of other calculation that was way too slow and 
  caused long post-slew delays.
- Firmware returns"OpenAstroMount" for :GVP# Meade command if compiled for OAM.
- Enhanced logging output to show time since last log output.
- Added output of all stepper settings to log output.

**V1.12.12 - Updates**
- Change MKS Gen L v1.0, v2.0, v2.1 default separate debug serial port from `Serial3` to `Serial2`.

- **V1.12.11 - Updates**
- Allowed the active state of the hall sensors for auto homing RA and DEC to be configured.

**V1.12.10 - Updates**
- Fixed a problem where slewing to targets more than 90degrees from the pole (in either hemisphere) would result in a target mirrored around the 'equator'.

**V1.12.9 - Updates**
- Fixed integer size mismatch for RA and DEC speeds. This caused integer overflows when RA was configured for 256 step microstepping.
- Added some logging at boot to show stepper variables.

**V1.12.8 - Updates**
- Fixed compile error caused by GPS being enabled in RAMPS environment.

**V1.12.7 - Updates**
- Fixed a bug where southern hemisphere setting would not persist between session.

**V1.12.6 - Updates**
- Fixed all known issues related to running in the southern hemisphere.
- Added support for DIR inversion to interrupt stepper library.
- Hemisphere no longer needs to be defined in config, firmware determines it automatically from the given Latitude, switching at the equator.
- Fixed the logic in Sync call to account for both hemispheres.
- Fixed some logic that stopped tracking when setting home position.
- Fixed a bug that caused the firmware to fail to recognize the end of very short slews.
- Added Meade Extension command to query remaining tracking time
- Added Meade Extension command to query the hemisphere that is set.

**V1.12.5 - Updates**
- Bound interrupt stepper library to version 0.0.1.

**V1.12.4 - Updates**
- Fixed a bug that incorrectly stopped the RA motor after issuing a DEC move.

**V1.12.3 - Updates**
- Fixed a bug that incorrectly calculated minimum stepper frequency. This caused Tracking to never run.

**V1.12.2 - Updates**
- Fixed a bug that caused Focuser stepper to misbehave after booting.

**V1.12.1 - Updates**
- Fixed a bug that caused Autohoming to cause a build break.
- Fixed a bug that would prevent Hall sensor based Autohoming from completing.

**V1.12.0 - Updates**
- Rewrite of the Stepper driver logic on ATMega2560 based boards.
  - Now using https://github.com/andre-stefanov/avr-interrupt-stepper instead of AccelStepper.
  - Now using dynamic interrupts instead of main loop for stepping control.
  - The maximum stepping rate (in total) increased drastically (theoretically up to 80.000 steps/s but it is recommended to stay under 40.000 to keep UART stable).
  - Improved stepping frequency stability and thus tracking speed accuracy.
  - ESP32 still uses AccelStepper (likely to change in future releases).
- Added CONFIG_VERSION validation to allow breaking changes in local configurations.

**V1.11.15 - Updates**
- Add DEC Autohoming via Hall sensor (same as RA Autohoming)

**V1.11.14 - Updates**
- Prevented firmware from hanging at boot if LCD is defined but not connected.
- Fixed a (rare) bug that would throw off GoTo commands when a target coordinate had been set.

**V1.11.13 - Updates**
- Added end switch support for RA and DEC.
- Guide pulses are only executed when tracking.

**V1.11.12 - Updates**
- Change DEC limit code to return degrees instead of stepper positions. 
- Support separate limits for DEC Limits being queried.

**V1.11.11 - Updates**
- Fix DEC limit code to use parameters.

**V1.11.10 - Updates**
- Fix dew heater pin assignments.

**V1.11.9 - Updates**
- Change storage of RA and DEC steps/degree to be independent of microstepping settings.

**V1.11.8 - Updates**
- Remove dead code
- Clean up README

**V1.11.7 - Updates**
- Add .mailmap file

**V1.11.6 - Updates**
- Fix platformio GitHub workflow to coincide with 6.0.2 update
- Disable Wdouble-promotion globally due to esp32 build failure and GCC bug 55578

**V1.11.5 - Updates**
- Corrected Longitude parsing to account for sign
- Corrected Longitude output to provide sign
- Corrected inverted UTC offset
- Corrected handshake response to 0x06 to be P for Polar mode

**V1.11.4 - Updates**
- Allow disabling Points Of Interest in LCD menu

**V1.11.3 - Updates**
- Change logging macro LOGVx to LOG

**V1.11.2 - Updates**
- Cache build files by default

**V1.11.1 - Updates**
- Fix ESP32 build failure

**V1.11.0 - Updates**
- Tracking now stops automaticaly when the end of the RA ring is reached (using a configurable limit)
- Syncing no longer changes where the firmware expects "home" to be for both DEC and RA
- Fixed RA limits to allow proper meridian flips when slewing
- Fixed parking/return-to-home functionality
- Fixed potential issue of DEC/RA flipping incorrectly when in the southern hemisphere
- Added configuration option for DEC limits
- Fixed a bug in RA Autohoming
- Added option for external debugging on separate serial port to allow debugging while using LX200 control
- Cleaned up logging some more

**V1.10.12 - Updates**
- Remove enabling tracking when home is set.

**V1.10.11 - Updates**
- Revise default microstep settings when using UART
- Revise default DEC guide pulse settings to match RA

**V1.10.10 - Updates**
- Fix Arduino IDE build

**V1.10.9 - Updates**
- Change UART TX test to be configurable movement distance

**V1.10.8 - Updates**
- Add initial unit test structure
- Unit tests for MappedDict.hpp

**V1.10.7 - Updates**
- Fix Arduino IDE issues

**V1.10.6 - Updates**
- Use consistent enable pin logic for all drivers.
- Increase maximum current for TMC2209 to 2A in accordance with BigTreeTech's published maximum continuous drive current.

**V1.10.5 - Updates**
- Add ability to disable tracking at boot by default

**V1.10.4 - Updates**
- Cleaned up logging somewhat
- Ensured all log messages have a category start word.
- Ensured all log messages are stored in Flash memory.

**V1.10.3 - Updates**
- Add optional parameter to Meade Extension commands :XSDLL# and :XSDLU# to allow direct setting of DEC limits

**V1.10.2 - Updates**
- Revise default MKS LCD pins to match ribbon cable LCD assembly.

**V1.10.1 - Updates**
- fix a bug with "Set Home" on OAM

**V1.10.0 - Updates**
- Delete support of ULN2003 drivers

**V1.9.38 - Updates**
- Add support for RAMPS 1.4 Arduino Mega shields

**V1.9.37 - Updates**
- Add HE1 to dew heater output for MKS boards

**V1.9.36 - Updates**
- Removed auto-homing code that was based on stall guard
- Converted Hall sensor based auto homing to be asynchronous (via state machine) instead of blocking
- Allowed user to specify the distance (number of hours) to search for Hall sensor in Meade command
- Allowed overriding the guide pulse multiplier in local config

**V1.9.35 - Updates**
- Enable configuration of hold current setting for AZ and ALT steppers when always energized

**V1.9.34 - Updates**
- Added two Meade commands: :XGDP# and :XSDPnnn# to retrieve and set the DEC parking offset.
- Fixed a bug that incorrectly returned a Homing status when the Hall sensor was enabled.
- Removing support for bluetooth
- Removing support for running steppers in main loop

**V1.9.33 - Updates**
- Fixed a bug that did not reset RA coordinate after setting home position.

**V1.9.32 - Updates**
- Add inverted axis support for alt/az

**V1.9.31 - Updates**
- Focuser can be set to be always on with non-100% holding current.
- Focuser got its own debug channel.
- Added some more logging.

**V1.9.30 - Updates**
- Updated alt/az code to correct SPR errors and add support for AutoPA v2.

**V1.9.29 - Updates**
- Support for a Hall sensor based auto homing routine for the RA ring.

**V1.9.28 - Updates**
- Add configurations for OAM

**V1.9.27 - Updates**
- Fix github actions formatting check

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
