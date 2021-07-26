#include "inc/Globals.hpp"
#include "../Configuration.hpp"
#include "Utility.hpp"
#include "LcdMenu.hpp"
#include "Mount.hpp"
#include "MeadeCommandProcessor.hpp"
#include "WifiControl.hpp"
#include "Gyro.hpp"

#if USE_GPS == 1
bool gpsAqcuisitionComplete(int &indicator);  // defined in c72_menuHA_GPS.hpp
#endif
/////////////////////////////////////////////////////////////////////////////////////////
//
// Serial support
//
// The Serial protocol implemented here is the Meade LX200 Classic protocol with some extensions.
// The Meade protocol commands start with a colon and end with a hash.
// The first letter determines the family of functions (G for Get, S for Set, M for Movement, etc.)
//
// The set of Meade features implemented are:
//
//------------------------------------------------------------------
// INITIALIZE FAMILY
//
// :I#
//      Description:
//        Initialize Scope
//      Information:
//        This puts the Arduino in Serial Control Mode and displays RA on line 1 and
//        DEC on line 2 of the display. Serial Control Mode can be ended manually by
//        pressing the SELECT key, or programmatically with the :Qq# command.
//      Returns:
//        nothing
//
//------------------------------------------------------------------
// SYNC CONTROL FAMILY
//
// :CM#
//      Description:
//        Synchronize Declination and Right Ascension.
//      Information:
//        This tells the scope what it is currently pointing at. The scope synchronizes
//        to the current target coordinates
//      Remarks:
//        Set with ":Sd#" and ":Sr#"
//      Returns:
//        "NONE#"
//
//------------------------------------------------------------------
// DISTANCE FAMILY
//
// :D#
//      Description:
//        Query Mount Status
//      Information:
//        This queries the mount for its slewing status
//      Returns:
//        "|#" if slewing
//        " #" if not
//
//------------------------------------------------------------------
// GPS FAMILY
//
// :gT#
//      Description:
//        Set Mount Time
//      Information:
//        Attempts to set the mount time and location from the GPS for 2 minutes. This is essentially a
//        blocking call, no other activities take place (except tracking, but only if interrupt-driven).
//      Remarks:
//        Use ":Gt#" and ":Gg#" to retrieve Lat and Long,
//      Returns:
//        "1" if the data was set
//        "0" if not (timedout)
//
// :gTnnn#
//      Description:
//        Set Mount Time w/ timeout
//      Information:
//        Attempts to set the mount time and location from the GPS with a custom timeout. This is also blocking
//        but by using a low timeout, you can avoid long pauses and let the user know that it's not ready yet.
//      Returns:
//        "1" if the data was set
//        "0" if not (timedout)
//      Parameters:
//        "nnn" is an integer defining the number of milliseconds to wait for the GPS to get a bearing
//
//------------------------------------------------------------------
// GET FAMILY
//
// :GVP#
//      Description:
//        Get the Product Name
//      Returns:
//        "OpenAstroTracker#"
//
// :GVN#
//      Description:
//        Get the Firmware Version Number
//      Returns:
//        "V1.major.minor#" from version.h
//
// :Gd#
//      Description:
//        Get Target Declination
//      Returns:
//        "sDD*MM'SS#"
//      Parameters:
//        "s" is + or -
//        "DD" is degrees
//        "MM" is minutes
//        "SS" is seconds
//
// :GD#
//      Description:
//        Get Current Declination
//      Returns:
//        "sDD*MM'SS#"
//      Parameters:
//        "s" is + or -
//        "DD" is degrees
//        "MM" is minutes
//        "SS" is seconds.
//
// :Gr#
//      Description:
//        Get Target Right Ascension
//      Returns:
//        HH:MM:SS#
//      Parameters:
//        "HH" is hour
//        "MM" is minutes
//        "SS" is seconds
//
// :GR#
//      Description:
//        Get Current Right Ascension
//      Returns:
//        "HH:MM:SS#"
//      Parameters:
//        "HH" is hour
//        "MM" is minutes
//        "SS" is seconds
//
// :Gt#
//      Description:
//        Get Site Latitude
//      Returns:
//        "sDD*MM#"
//      Parameters:
//        "s" is + or -
//        "DD" is the latitude in degrees
//        "MM" the minutes
//
// :Gg#
//      Description:
//        Get Site Longitude
//      Returns:
//        "DDD*MM#"
//      Parameters:
//        "DDD" is the longitude in degrees
//        "MM" the minutes
//      Remarks:
//        Longitudes are from 0 to 360 going WEST. so 179W is 359 and 179E is 1.
//
// :Gc#
//      Description:
//        Get current Clock format
//      Returns:
//        "24#"
//
// :GG#
//      Description:
//        Get UTC offset time
//      Returns:
//        "sHH#"
//      Parameters:
//        "s" is the sign
//        "HH" are the number of hours that need to be added to local time to convert to UTC time
//
// :Ga#
//      Description:
//        Get local time in 12h format
//      Returns:
//        "HH:MM:SS#"
//      Parameters:
//        "HH" are hours (modulo 12)
//        "MM" are minutes
//        "SS" are seconds of the local time
//
// :GL#
//      Description:
//        Get local time in 24h format
//      Returns:
//        "HH:MM:SS#"
//      Parameters:
//        "HH" are hours
//        "MM" are minutes
//        "SS" are seconds of the local time
//
// :GC#
//      Description:
//        Get current date
//      Returns:
//        "MM/DD/YY#"
//      Parameters:
//        "MM" is the month (1-12)
//        "day" is the day (1-31)
//        "year" is the lower two digits of the year
//
// :GM#
//      Description:
//        Get Site Name 1
//      Returns:
//        "OAT1#"
//
// :GN#
//      Description:
//        Get Site Name 2
//      Returns:
//        "OAT2#"
//
// :GO#
//      Description:
//        Get Site Name 3
//      Returns:
//        OAT2#
//
// :GP#
//      Description:
//        Get Site Name 4
//      Returns:
//        OAT4#
//
// :GT#
//      Description:
//        Get tracking rate
//      Returns:
//        60.0#
//
//------------------------------------------------------------------
// GET EXTENSIONS
//
// :GIS#
//      Description:
//        Get DEC or RA Slewing
//      Returns:
//        "1#" if either RA or DEC is slewing
//        "0#" if not
//
// :GIT#
//      Description:
//        Get Tracking
//      Returns:
//        "1#" if tracking is on
//        "0#" if not
//
// :GIG#
//      Description:
//        Get Guiding
//      Returns:
//        "1#" if currently guiding
//        "0#" if not
//
// :GX#
//      Description:
//        Get Mount Status
//      Information:
//         String reflecting the mounts' status. The string is a comma-delimited list of statuses
//      Returns:
//        "Idle,--T--,11219,0,927,071906,+900000,#"
//      Parameters:
//        [0] The mount status. One of 'Idle', 'Parked', 'Parking', 'Guiding', 'SlewToTarget', 'FreeSlew', 'ManualSlew', 'Tracking', 'Homing'
//        [1] The motion state.
//        [2] The RA stepper position
//        [3] The DEC stepper position
//        [4] The Tracking stepper position
//        [5] The current RA position
//        [6] The current DEC position
//      Remarks:
//        The motion state
//        First character is RA slewing state ('R' is East, 'r' is West, '-' is stopped).
//        Second character is DEC slewing state ('d' is North, 'D' is South, '-' is stopped).
//        Third character is TRK slewing state ('T' is Tracking, '-' is stopped).
//        Fourth character is AZ slewing state ('Z' and 'z' is adjusting, '-' is stopped).
//        Fifth character is ALT slewing state ('A' and 'a' is adjusting, '-' is stopped).
//        Az and Alt are optional. The string may only be 3 characters long
//
//------------------------------------------------------------------
// SET FAMILY
//
// :SdsDD*MM:SS#
//      Description:
//        Set Target Declination
//      Information:
//        This sets the target DEC. Use a Movement command to slew there.
//      Parameters:
//        "s" is + or -
//        "DD" is degrees
//        "MM" is minutes
//        "SS" is seconds
//      Returns:
//        "1" if successfully set
//        "0" otherwise
//
// :SrHH:MM:SS#
//      Description:
//        Set Right Ascension
//      Information:
//        This sets the target RA. Use a Movement command to slew there.
//      Returns:
//        "1" if successfully set
//        "0" otherwise
//      Parameters:
//        "HH" is hours
//        "MM" is minutes
//        "SS" is seconds
//
// :StsDD*MM#
//      Description:
//        Set Site Latitude
//      Information:
//        This sets the latitude of the location of the mount.
//      Returns:
//        "1" if successfully set
//        "0" otherwise
//      Parameters:
//        "s" is the sign ('+' or '-')
//        "DD" is the degree (90 or less)
//        "MM" is minutes
//
// :SgDDD*MM#
//      Description:
//        Set Site Longitude
//      Information:
//        This sets the longitude of the location of the mount.
//      Returns:
//        "1" if successfully set
//        "0" otherwise
//      Parameters:
//        "DDD" the nmber of degrees (0 to 360)
//        "MM" is minutes
//      Remarks:
//        Longitudes are from 0 to 360 going WEST. so 179W is 359 and 179E is 1.
//
// :SGsHH#
//      Description:
//        Set Site UTC Offset
//      Information:
//        This sets the offset of the timezone in which the mount is in hours from UTC.
//      Returns:
//        "1"
//      Parameters:
//        "s" is the sign
//        "HH" is the number of hours
//
// :SLHH:MM:SS#
//      Description:
//        Set Site Local Time
//      Information:
//        This sets the local time of the timezone in which the mount is located.
//      Returns:
//        "1"
//      Parameters:
//        "HH" is hours
//        "MM" is minutes
//        "SS" is seconds
//
// :SCMM/DD/YY#
//      Description:
//        Set Site Date
//      Information:
//        This sets the date
//      Returns:
//        "1Updating Planetary Data#                              #"
//      Parameters:
//        "HHMM" is the month
//        "DD" is the day
//        "YY" is the year since 2000
//
//------------------------------------------------------------------
// SET Extensions
//
// :SHHH:MM#
//      Description:
//        Set Hour Time (HA)
//      Information:
//        This sets the scopes HA.
//      Returns:
//        "1" if successfully set
//        "0" otherwise
//      Parameters:
//        "HH" is hours
//        "MM" is minutes
//
// :SHP#
//      Description:
//        Set Home Point
//      Information:
//        This sets the current orientation of the scope as its home point.
//      Returns:
//        "1"
//
// :SHLHH:MM#
//      Description:
//        Set LST Time
//      Information:
//        This sets the scopes LST (and HA).
//      Returns:
//        "1" if successfully set
//        "0" otherwise
//      Parameters:
//        "HH" is hours
//        "MM" is minutes
//
// :SYsDD*MM:SS.HH:MM:SS#
//      Description:
//        Synchronize Declination and Right Ascension.
//      Information:
//        This tells the scope what it is currently pointing at.
//      Returns:
//        "1" if successfully set
//        "0" otherwise
//      Parameters:
//        "s" is + or -
//        "DD" is degrees
//        "HH" is hours
//        "MM" is minutes
//        "SS" is seconds
//
//------------------------------------------------------------------
// RATE CONTROL FAMILY
//
// :Rs#
//      Description:
//        Set Slew rate
//      Parameters:
//        "s" is one of 'S', 'M', 'C', or 'G' in order of decreasing speed
//      Returns:
//        nothing
//
//------------------------------------------------------------------
// MOVEMENT FAMILY
//
// :MS#
//      Description:
//        Start Slew to Target (Asynchronously)
//      Information:
//        This starts slewing the scope to the target RA and DEC coordinates and returns immediately.
//      Returns:
//        "0"
//
//------------------------------------------------------------------
// MOVEMENT EXTENSIONS
//
// :MGdnnnn#
//      Description:
//        Run a Guide pulse
//      Information:
//        This runs the motors at increased speed for a short period of time.
//      Parameters:
//        "d" is one of 'N', 'E', 'W', or 'S'
//        "nnnn" is the duration in ms
//      Returns:
//        "1"
//
// :MTs#
//      Description:
//        Set Tracking mode
//      Information:
//        This turns the scopes tracking mode on or off.
//      Parameters:
//        "s" is "1" to turn on Tracking and "0" to turn it off
//      Returns:
//        "1"
//
// :Mc#
//      Description:
//        Start slewing
//      Information:
//        This starts slewing the mount in the given direction.
//      Parameters:
//        "c" is one of 'n', 'e', 'w', or 's'
//      Returns:
//        nothing
//
// :MXxnnnnn#
//      Description:
//        Move stepper
//      Information:
//        This starts moving one of the steppers by the given amount of steps and returns immediately.
//      Parameters:
//        "x" is the stepper to move (r for RA, d for DEC, f for FOC, z for AZ, t for ALT)
//        "nnnn" is the number of steps
//      Returns:
//        "1" if successfully scheduled
//
// :MAZn.nn#
//      Description:
//        Move Azimuth
//      Information:
//        If the scope supports automated azimuth operation, move azimuth by n.nn arcminutes
//      Parameters:
//        "n.nn" is a signed floating point number representing the number of arcminutes to move the mount left or right
//      Returns:
//        nothing
//
// :MALn.nn#
//      Description:
//        Move Altitude
//      Information:
//        If the scope supports automated altitude operation, move altitude by n.nn arcminutes
//      Parameters:
//        "n.nn" is a signed floating point number representing the number of arcminutes to raise or lower the mount.
//      Returns:
//        nothing
//
//------------------------------------------------------------------
// HOME FAMILY
//
// :hP#
//      Description:
//        Park Scope and stop motors
//      Information:
//        This slews the scope back to it's home position (RA ring centered, DEC
//        at 90, basically pointing at celestial pole) and stops all movement (including tracking).
//      Returns:
//        nothing
//
// :hF#
//      Description:
//        Move Scope to Home position
//      Information:
//        This slews the scope back to its home position (RA ring centered, DEC
//        at 90, basically pointing at celestial pole). Mount will keep tracking.
//      Returns:
//        nothing
//
//------------------------------------------------------------------
// PARK Extensions
//
// :hU#
//      Description:
//        Unpark Scope
//      Information:
//        This currently simply turns on tracking.
//      Returns:
//        "1"
//
//------------------------------------------------------------------
// QUIT MOVEMENT FAMILY
//
// :Q#
//      Description:
//        Stop all motors
//      Information:
//        This stops all motors, including tracking. Note that deceleration curves are still followed.
//      Returns:
//        "1" when all motors have stopped
//
// :Qd#
//      Description:
//        Stop Slewing
//      Information:
//        Stops slew in specified direction where d is n, s, e, w, a (the first four are the cardinal directions, a stands for All).
//      Returns:
//        nothing
//
//------------------------------------------------------------------
// QUIT MOVEMENT Extensions
//
// :Qq#
//      Description:
//        Disconnect, Quit Control mode
//      Information:
//        This quits Serial Control mode and starts tracking.
//      Returns:
//        nothing
//
//------------------------------------------------------------------
// EXTRA OAT FAMILY - These are meant for the PC control app
//
// :XFR#
//      Description:
//        Perform a Factory Reset
//      Information:
//        Clears all the EEPROM settings
//      Returns:
//        "1#"
//
// :XDnnn#
//      Description:
//        Run drift alignment
//      Information:
//        This runs a drift alignment procedure where the mounts slews east, pauses, slews west and pauses.
//        Where nnn is the number of seconds the entire alignment should take. The call is blocking and will
//        only return once the drift alignment is complete.
//      Returns:
//        nothing
//
// :XL0#
//      Description:
//        Turn off the Digital level
//      Returns:
//        "1#" if successful
//        "0#" if there is no Digital Level
//
// :XL1#
//      Description:
//        Turn on the Digital level
//      Returns:
//        "1#" if successful
//        "0#" if there is no Digital Level
//
// :XLGR#
//      Description:
//        Digital Level - Get Reference
//      Information:
//        Gets the reference pitch and roll values of the mount (Digital Level addon). These
//        values are the values of the pitch and roll when the mount is level.
//      Returns:
//        "<pitch>,<roll>#"
//        "0#" if there is no Digital Level
//
// :XLGC#
//      Description:
//        Digital Level - Get Values
//      Information:
//        Gets the current pitch and roll values of the mount (Digital Level addon).
//      Returns:
//        "<pitch>,<roll>#"
//        "0#" if there is no Digital Level
//
// :XLGT#
//      Description:
//        Digital Level - Get Temperature
//      Information:
//        Get the current temperature in Celsius of the mount (Digital Level addon).
//      Returns:
//        "<temp>#"
//        "0#" if there is no Digital Level
//
// :XLSR#
//      Description:
//        Digital Level - Set Reference Roll
//      Information:
//        Sets the reference roll value of the mount (Digital Level addon). This is the value
//        at which the mount is level.
//      Returns:
//        "1#" if successful
//        "0#" if there is no Digital Level
//
// :XLSP#
//      Description:
//        Digital Level - Set Reference Pitch
//      Information:
//        Sets the reference pitch value of the mount (Digital Level addon). This is the value
//        at which the mount is level.
//      Returns:
//        "1#" if succsessful
//        "0#" if there is no Digital Level
//
// :XGB#
//      Description:
//        Get Backlash correction steps
//      Information:
//        Get the number of steps the RA stepper motor needs to overshoot and backtrack when slewing east.
//      Returns:
//        "integer#"
//
// :XGCn.nn*m.mm#
//      Description:
//        Get stepper motor positions for target
//      Information:
//        Get the positions of stepper motors when pointed at the given coordinates.
//      Parameters:
//        "n.nn" is the RA coordinate (0.0 - 23.999)
//        "m.mm" is the DEC coordinate (-90.00 - +90.00)
//        "ralong" is the stepper position of the RA stepper
//        "declong" is the stepper position of the DEC stepper
//      Returns:
//        "ralong,declong#"
//
// :XGR#
//      Description:
//        Get RA steps
//      Information:
//        Get the number of steps the RA stepper motor needs to take to rotate RA by one degree
//      Returns:
//        "float#"
//
// :XGD#
//      Description:
//        Get DEC steps
//      Information:
//        Get the number of steps the DEC stepper motor needs to take to rotate DEC by one degree
//      Returns:
//        "float#"
//
// :XGDL#
//      Description:
//        Get DEC limits
//      Information:
//        Get the lower and upper limits for the DEC stepper motor in steps
//      Returns:
//        "integer|integer#"
//
// :XGS#
//      Description:
//        Get Tracking speed adjustment
//      Information:
//        Get the adjustment factor used to speed up (>1.0) or slow down (<1.0) the tracking speed of the mount.
//      Returns:
//        "float#"
//
// :XGT#
//      Description:
//        Get Tracking speed
//      Information:
//        Get the absolute tracking speed of the mount.
//      Returns:
//        "float#"
//
// :XGH#
//      Description:
//        Get HA
//      Information:
//        Get the current HA of the mount.
//      Returns:
//        "HHMMSS#"
//
// :XGM#
//      Description:
//        Get Mount configuration settings
//      Returns:
//        "<board>,<RA Stepper Info>,<DEC Stepper Info>,<GPS info>,<AzAlt info>,<Gyro info>#"
//      Parameters:
//        "<board>" is one of the supported boards (currently Mega, ESP32, MKS)
//        "<Stepper Info>" is a pipe-delimited string of Motor type (NEMA or 28BYJ), Pulley Teeth, Steps per revolution)
//        "<GPS info>" is either NO_GPS or GPS, depending on whether a GPS module is present
//        "<AzAlt info>" is either NO_AZ_ALT, AUTO_AZ_ALT, AUTO_AZ, or AUTO_ALT, depending on which AutoPA stepper motors are present
//        "<Gyro info>" is either NO_GYRO or GYRO depending on whether the Digial level is present
//      Example:
//        "ESP32,28BYJ|16|4096.00,28BYJ|16|4096.00,NO_GPS,NO_AZ_ALT,NO_GYRO#"
//
// :XGMS#
//      Description:
//        Get Mount driver configuration
//      Returns:
//        "<RA driver>,<RA slewMS>,<RA trackMS>|<DEC driver>,<DEC slewMS>,<DEC guideMS>|#"
//      Parameters:
//        "<driver>" is one of the supported drivers: U = ULN2003, TU=TMC2209UART, TS=TMC2209STANDALONE, A=A4983
//        "<slewMS>" is the microstepping divider (1, 2, 4, 8, 15, 21, 64, 128, 256) used when slewing
//        "<trackMS>" is the microstepping divider (1, 2, 4, 8, 15, 21, 64, 128, 256) used when tracking RA
//        "<guideMS>" is the microstepping divider (1, 2, 4, 8, 15, 21, 64, 128, 256) used when guiding DEC
//      Example:
//        "TU,8,64|TU,16,64|#"
//
// :XGN#
//      Description:
//        Get network settings
//      Information:
//        Gets the current status of the Wifi connection. Reply only available when running on ESP boards.
//      Returns:
//        "1,<mode>,<status>,<hostname>,<ip>:<port>,<SSID>,<OATHostname>#" if Wifi is enabled
//        "0,#" if Wifi is not enabled
//
// :XGL#
//      Description:
//        Get LST
//      Information:
//        Get the current LST of the mount.
//      Returns:
//        "HHMMSS"
//
// :XSBn#
//      Description:
//        Set Backlash correction steps
//      Information:
//        Sets the number of steps the RA stepper motor needs to overshoot and backtrack when slewing east.
//      Returns:
//        nothing
//
// :XSRn.n#
//      Description:
//        Set RA steps
//      Information:
//        Set the number of steps the RA stepper motor needs to take to rotate by one degree.
//      Parameters:
//        "n.n" is the number of steps (only one decimal point is supported)
//      Returns:
//        nothing
//
// :XSDn.n#
//      Description:
//        Set DEC steps
//      Information:
//        Set the number of steps the DEC stepper motor needs to take to rotate by one degree.
//      Parameters:
//        "n.n" is the number of steps (only one decimal point is supported)
//      Returns:
//        nothing
//
// :XSDLU#
//      Description:
//        Set DEC upper limit
//      Information:
//        Set the upper limit for the DEC stepper motor to the current position
//      Returns:
//        nothing
//
// :XSDLu#
//      Description:
//        Clear DEC upper limit
//      Information:
//        Clears the upper limit for the DEC stepper motor
//      Returns:
//        nothing
//
// :XSDLL#
//      Description:
//        Set DEC lower limit
//      Information:
//        Set the lowerlimit for the DEC stepper motor to the current position
//      Returns:
//        nothing
//
// :XSDLl#
//      Description:
//        Clear DEC lower limit
//      Information:
//        Clear the lower limit for the DEC stepper motor
//      Returns: nothing
//
// :XSSn.nnn#
//      Description:
//        Set Tracking speed adjustment
//      Information:
//        Set the adjustment factor used to speed up "(>1.0)" or slow down "(<1.0)" the tracking speed of the mount
//      Parameters:
//        "n.nnn" is the factor to multiply the theoretical speed by
//      Returns:
//        nothing
//
// :XSMn#
//      Description:
//        Set Manual Slewing Mode
//      Information:
//        Toggle the manual slewing mode state where the RA and DEC motors run at a constant speed
//      Parameters:
//        "n" is '1' to turn it on, otherwise turn it off
//      Returns:
//        nothing
//
// :XSXn.nnn#
//      Description:
//        Set RA Speed
//      Information:
//        Set RA manual slewing speed in degrees/sec immediately. Max is around 2.5 degs/s
//      Returns:
//        nothing
//      Remarks:
//        Must be in manual slewing mode.
//
// :XSYn.nnn#
//      Description:
//        Set DEC Speed
//      Information:
//        Set DEC manual slewing speed in degrees/sec immediately. Max is around 2.5 degs/s
//      Returns:
//        nothing
//      Remarks:
//        Must be in manual slewing mode.
//
//------------------------------------------------------------------
// FOCUS FAMILY
//
// :F+#
//      Description:
//        Start Focuser moving inward (toward objective)
//      Information:
//        Continues pull in until stopped
//      Returns:
//        nothing
//
// :F-#
//      Description:
//        Pull out
//      Information:
//        Continues pull out until stopped
//      Returns:
//        nothing
//
// :Fn#
//      Description:
//        Set speed factor
//      Information:
//        Set focuser speed to <n> where <n> is an ASCII digit 1..4. 1 is slowest, 4 i fastest
//      Returns:
//        nothing
//
// :FS#
//      Description:
//        Set slowest speed factor
//      Information:
//        Set focuser to the slowest speed it can use
//      Returns:
//        nothing
//
// :FF#
//      Description:
//        Set fastest speed factor
//      Information:
//        Set focuser speed to the fastest speed it can use
//      Returns:
//        nothing
//
// :Fp#
//      Description:
//        Get position
//      Information:
//        Get the current position of the focus stepper motor
//      Returns:
//        "nnn#" "nnn" is the current position of the stepper
//
//
// :FPnnn#
//      Description:
//        Set position
//      Information:
//        Sets the current position of the focus stepper motor
//      Returns:
//        "1"
//      Parameters:
//        "nnn" is the new position of the stepper. The stepper is not moved.
//
// :FB#
//      Description:
//        Get focuser state
//      Information:
//        Gets the state of the focuser stepper.
//      Returns:
//        "0" if the focuser is idle
//        "1" if the focuser is moving
//
// :FQ#
//      Description:
//        Stop focuser
//      Information:
//        Stops the stepper motor of teh focuser.
//      Returns:
//        nothing
//
//------------------------------------------------------------------
/////////////////////////////////////////////////////////////////////////////////////////

MeadeCommandProcessor *MeadeCommandProcessor::_instance = nullptr;

/////////////////////////////
// Create the processor
/////////////////////////////
MeadeCommandProcessor *MeadeCommandProcessor::createProcessor(Mount *mount, LcdMenu *lcdMenu)
{
    _instance = new MeadeCommandProcessor(mount, lcdMenu);
    return _instance;
}

/////////////////////////////
// Get the singleton
/////////////////////////////
MeadeCommandProcessor *MeadeCommandProcessor::instance()
{
    return _instance;
}

/////////////////////////////
// Constructor
/////////////////////////////
MeadeCommandProcessor::MeadeCommandProcessor(Mount *mount, LcdMenu *lcdMenu)
{
    _mount = mount;

    // In case of DISPLAY_TYPE_NONE mode, the lcdMenu is just an empty shell class to save having to null check everywhere
    _lcdMenu = lcdMenu;
}

/////////////////////////////
// INIT
/////////////////////////////
String MeadeCommandProcessor::handleMeadeInit(String inCmd)
{
    inSerialControl = true;
    _lcdMenu->setCursor(0, 0);
    _lcdMenu->printMenu("Remote control");
    _lcdMenu->setCursor(0, 1);
    _lcdMenu->printMenu(">SELECT to quit");
    return "";
}

/////////////////////////////
// GET INFO
/////////////////////////////
String MeadeCommandProcessor::handleMeadeGetInfo(String inCmd)
{
    char cmdOne = inCmd[0];
    char cmdTwo = (inCmd.length() > 1) ? inCmd[1] : '\0';
    char achBuffer[20];

    switch (cmdOne)
    {
        case 'V':
            if (cmdTwo == 'N')  // :GVN
            {
                return String(VERSION) + "#";
            }
            else if (cmdTwo == 'P')  // :GVP
            {
                return "OpenAstroTracker#";
            }
            break;

        case 'r':                                                   // :Gr
            return _mount->RAString(MEADE_STRING | TARGET_STRING);  // returns trailing #

        case 'd':                                                    // :Gd
            return _mount->DECString(MEADE_STRING | TARGET_STRING);  // returns trailing #

        case 'R':                                                    // :GR
            return _mount->RAString(MEADE_STRING | CURRENT_STRING);  // returns trailing #

        case 'D':                                                     // :GD
            return _mount->DECString(MEADE_STRING | CURRENT_STRING);  // returns trailing #

        case 'X':  // :GX
            return _mount->getStatusString() + "#";

        case 'I':
            {
                String retVal = "";
                if (cmdTwo == 'S')  // :GIS
                {
                    retVal = _mount->isSlewingRAorDEC() ? "1" : "0";
                }
                else if (cmdTwo == 'T')  // :GIT
                {
                    retVal = _mount->isSlewingTRK() ? "1" : "0";
                }
                else if (cmdTwo == 'G')  // :GIG
                {
                    retVal = _mount->isGuiding() ? "1" : "0";
                }
                return retVal + "#";
            }
        case 't':  // :Gt
            {
                _mount->latitude().formatString(achBuffer, "{d}*{m}#");
                return String(achBuffer);
            }
        case 'g':  // :Gg
            {
                _mount->longitude().formatString(achBuffer, "{d}*{m}#");
                return String(achBuffer);
            }
        case 'c':  // :Gc
            {
                return "24#";
            }
        case 'G':  // :GG
            {
                int offset = _mount->getLocalUtcOffset();
                sprintf(achBuffer, "%+03d#", offset);
                return String(achBuffer);
            }
        case 'a':  // :Ga
            {
                DayTime time = _mount->getLocalTime();
                if (time.getHours() > 12)
                {
                    time.addHours(-12);
                }
                time.formatString(achBuffer, "{d}:{m}:{s}#");
                return String(achBuffer + 1);
            }
        case 'L':  // :GL
            {
                DayTime time = _mount->getLocalTime();
                time.formatString(achBuffer, "{d}:{m}:{s}#");
                return String(achBuffer + 1);
            }
        case 'C':  // :GC
            {
                LocalDate date = _mount->getLocalDate();
                sprintf(achBuffer, "%02d/%02d/%02d#", date.month, date.day, date.year % 100);
                return String(achBuffer);
            }
        case 'M':  // :GM
            {
                return "OAT1#";
            }
        case 'N':  // :GN
            {
                return "OAT2#";
            }
        case 'O':  // :GO
            {
                return "OAT3#";
            }
        case 'P':  // :GP
            {
                return "OAT4#";
            }
        case 'T':  // :GT
            {
                return "60.0#";  //default MEADE Tracking Frequency
            }
    }

    return "";
}

/////////////////////////////
// GPS CONTROL
/////////////////////////////
String MeadeCommandProcessor::handleMeadeGPSCommands(String inCmd)
{
#if USE_GPS == 1
    if (inCmd[0] == 'T')
    {
        unsigned long timeoutLen = 2UL * 60UL * 1000UL;
        if (inCmd.length() > 1)
        {
            timeoutLen = inCmd.substring(1).toInt();
        }
        // Wait at most 2 minutes
        unsigned long timeoutTime = millis() + timeoutLen;
        int indicator             = 0;
        while (millis() < timeoutTime)
        {
            if (gpsAqcuisitionComplete(indicator))
            {
                LOGV1(DEBUG_MEADE, F("MEADE: GPS startup, GPS acquired"));
                return "1";
            }
        }
    }
#endif
    LOGV1(DEBUG_MEADE, F("MEADE: GPS startup, no GPS signal"));
    return "0";
}

/////////////////////////////
// SYNC CONTROL
/////////////////////////////
String MeadeCommandProcessor::handleMeadeSyncControl(String inCmd)
{
    if (inCmd[0] == 'M')
    {
        _mount->syncPosition(_mount->targetRA(), _mount->targetDEC());
        return "NONE#";
    }

    return "FAIL#";
}

/////////////////////////////
// SET INFO
/////////////////////////////
String MeadeCommandProcessor::handleMeadeSetInfo(String inCmd)
{
    if ((inCmd[0] == 'd') && (inCmd.length() == 10))
    {
        // Set DEC
        //   0123456789
        // :Sd+84*03:02
        if (((inCmd[4] == '*') || (inCmd[4] == ':')) && (inCmd[7] == ':'))
        {
            Declination dec     = Declination::ParseFromMeade(inCmd.substring(1));
            _mount->targetDEC() = dec;
            LOGV2(DEBUG_MEADE, F("MEADE: SetInfo: Received Target DEC: %s"), _mount->targetDEC().ToString());
            return "1";
        }
        else
        {
            // Did not understand the coordinate
            return "0";
        }
    }
    else if (inCmd[0] == 'r' && (inCmd.length() == 9))
    {
        // :Sr11:04:57#
        // Set RA
        //   012345678
        // :Sr04:03:02
        if ((inCmd[3] == ':') && (inCmd[6] == ':'))
        {
            _mount->targetRA().set(inCmd.substring(1, 3).toInt(), inCmd.substring(4, 6).toInt(), inCmd.substring(7, 9).toInt());
            LOGV2(DEBUG_MEADE, F("MEADE: SetInfo: Received Target RA: %s"), _mount->targetRA().ToString());
            return "1";
        }
        else
        {
            // Did not understand the coordinate
            return "0";
        }
    }
    else if (inCmd[0] == 'H')
    {
        if (inCmd[1] == 'L')
        {
            // Set LST
            int hLST   = inCmd.substring(2, 4).toInt();
            int minLST = inCmd.substring(4, 6).toInt();
            int secLST = 0;
            if (inCmd.length() > 7)
            {
                secLST = inCmd.substring(6, 8).toInt();
            }

            DayTime lst(hLST, minLST, secLST);
            LOGV4(DEBUG_MEADE, F("MEADE: SetInfo: Received LST: %d:%d:%d"), hLST, minLST, secLST);
            _mount->setLST(lst);
        }
        else if (inCmd[1] == 'P')
        {
            // Set home point
            _mount->setHome(false);
            _mount->startSlewing(TRACKING);
        }
        else
        {
            // Set HA
            int hHA   = inCmd.substring(1, 3).toInt();
            int minHA = inCmd.substring(4, 6).toInt();
            LOGV4(DEBUG_MEADE, F("MEADE: SetInfo: Received HA: %d:%d:%d"), hHA, minHA, 0);
            _mount->setHA(DayTime(hHA, minHA, 0));
        }

        return "1";
    }
    else if ((inCmd[0] == 'Y') && inCmd.length() == 19)
    {
        // Sync RA, DEC - current position is the given coordinate
        //   0123456789012345678
        // :SY+84*03:02.18:34:12
        if (((inCmd[4] == '*') || (inCmd[4] == ':')) && (inCmd[7] == ':') && (inCmd[10] == '.') && (inCmd[13] == ':') && (inCmd[16] == ':'))
        {
            Declination dec = Declination::ParseFromMeade(inCmd.substring(1, 9));
            DayTime ra      = DayTime::ParseFromMeade(inCmd.substring(11));

            _mount->syncPosition(ra, dec);
            return "1";
        }
        return "0";
    }
    else if ((inCmd[0] == 't'))  // latitude: :St+30*29#
    {
        Latitude lat = Latitude::ParseFromMeade(inCmd.substring(1));
        _mount->setLatitude(lat);
        return "1";
    }
    else if (inCmd[0] == 'g')  // longitude :Sg097*34#
    {
        Longitude lon = Longitude::ParseFromMeade(inCmd.substring(1));

        _mount->setLongitude(lon);
        return "1";
    }
    else if (inCmd[0] == 'G')  // utc offset :SG+05#
    {
        int offset = inCmd.substring(1, 4).toInt();
        _mount->setLocalUtcOffset(offset);
        return "1";
    }
    else if (inCmd[0] == 'L')  // Local time :SL19:33:03#
    {
        _mount->setLocalStartTime(DayTime::ParseFromMeade(inCmd.substring(1)));
        return "1";
    }
    else if (inCmd[0] == 'C')
    {  // Set Date (MM/DD/YY) :SC04/30/20#
        int month = inCmd.substring(1, 3).toInt();
        int day   = inCmd.substring(4, 6).toInt();
        int year  = 2000 + inCmd.substring(7, 9).toInt();
        _mount->setLocalStartDate(year, month, day);

        /*
    From https://www.astro.louisville.edu/software/xmtel/archive/xmtel-indi-6.0/xmtel-6.0l/support/lx200/CommandSet.html :
    SC: Calendar: If the date is valid 2 <string>s are returned, each string is 31 bytes long. 
    The first is: "Updating planetary data#" followed by a second string of 30 spaces terminated by '#'
    */
        return "1Updating Planetary Data#                              #";  //
    }
    else
    {
        return "0";
    }
}

/////////////////////////////
// MOVEMENT
/////////////////////////////
String MeadeCommandProcessor::handleMeadeMovement(String inCmd)
{
    if (inCmd[0] == 'S')  // :MS#
    {
        _mount->startSlewingToTarget();
        return "0";
    }
    else if (inCmd[0] == 'T')  // :MT1
    {
        if (inCmd.length() > 1)
        {
            if (inCmd[1] == '1')
            {
                _mount->startSlewing(TRACKING);
                return "1";
            }
            else if (inCmd[1] == '0')
            {
                _mount->stopSlewing(TRACKING);
                return "1";
            }
        }
        else
        {
            return "0";
        }
    }
    else if ((inCmd[0] == 'G') || (inCmd[0] == 'g'))  // MG
    {
        // The spec calls for lowercase, but ASCOM Drivers prior to 0.3.1.0 sends uppercase, so we allow both for now.
        // Guide pulse
        //   012345678901
        // :MGd0403
        if (inCmd.length() == 6)
        {
            byte direction = EAST;
            inCmd.toLowerCase();
            if (inCmd[1] == 'n')
                direction = NORTH;
            else if (inCmd[1] == 's')
                direction = SOUTH;
            else if (inCmd[1] == 'e')
                direction = EAST;
            else if (inCmd[1] == 'w')
                direction = WEST;
            int duration = (inCmd[2] - '0') * 1000 + (inCmd[3] - '0') * 100 + (inCmd[4] - '0') * 10 + (inCmd[5] - '0');
            _mount->guidePulse(direction, duration);
            return "";
        }
    }
    else if (inCmd[0] == 'A')
    {
// Move Azimuth or Altitude by given arcminutes
// :MAZ+32.1# or :MAL-32.1#
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        if (inCmd[1] == 'Z')  // :MAZ
        {
            float arcMinute = inCmd.substring(2).toFloat();
            _mount->moveBy(AZIMUTH_STEPS, arcMinute);
        }
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        if (inCmd[1] == 'L')  // :MAL
        {
            float arcMinute = inCmd.substring(2).toFloat();
            _mount->moveBy(ALTITUDE_STEPS, arcMinute);
        }
#endif
        return "";
    }
    else if (inCmd[0] == 'e')
    {
        _mount->startSlewing(EAST);
        return "";
    }
    else if (inCmd[0] == 'w')
    {
        _mount->startSlewing(WEST);
        return "";
    }
    else if (inCmd[0] == 'n')
    {
        _mount->startSlewing(NORTH);
        return "";
    }
    else if (inCmd[0] == 's')
    {
        _mount->startSlewing(SOUTH);
        return "";
    }
    else if (inCmd[0] == 'X')  // :MX
    {
        long steps = inCmd.substring(2).toInt();
        if (inCmd[1] == 'r')
            _mount->moveStepperBy(RA_STEPS, steps);
        else if (inCmd[1] == 'd')
            _mount->moveStepperBy(DEC_STEPS, steps);
        else if (inCmd[1] == 'z')
            _mount->moveStepperBy(AZIMUTH_STEPS, steps);
        else if (inCmd[1] == 'l')
            _mount->moveStepperBy(ALTITUDE_STEPS, steps);
        else if (inCmd[1] == 'f')
            _mount->moveStepperBy(FOCUS_STEPS, steps);
        else
            return "0";
        return "1";
    }

    return "0";
}

/////////////////////////////
// HOME
/////////////////////////////
String MeadeCommandProcessor::handleMeadeHome(String inCmd)
{
    if (inCmd[0] == 'P')
    {  // Park
        _mount->park();
    }
    else if (inCmd[0] == 'F')
    {  // Home
        _mount->goHome();
    }
    else if (inCmd[0] == 'U')
    {  // Unpark
        _mount->startSlewing(TRACKING);
        return "1";
    }
    return "";
}

String MeadeCommandProcessor::handleMeadeDistance(String inCmd)
{
    if (_mount->isSlewingRAorDEC())
    {
        return "|#";
    }
    return " #";
}

/////////////////////////////
// EXTRA COMMANDS
/////////////////////////////
String MeadeCommandProcessor::handleMeadeExtraCommands(String inCmd)
{
    //   0123
    // :XDmmm
    if (inCmd[0] == 'D')  // :XD
    {                     // Drift Alignemnt
        int duration = inCmd.substring(1, 4).toInt() - 3;
        _lcdMenu->setCursor(0, 0);
        _lcdMenu->printMenu(">Drift Alignment");
        _lcdMenu->setCursor(0, 1);
        _lcdMenu->printMenu("Pause 1.5s....");
        _mount->stopSlewing(ALL_DIRECTIONS | TRACKING);
        _mount->waitUntilStopped(ALL_DIRECTIONS);
        _mount->delay(1500);
        _lcdMenu->setCursor(0, 1);
        _lcdMenu->printMenu("Eastward pass...");
        _mount->runDriftAlignmentPhase(EAST, duration);
        _lcdMenu->setCursor(0, 1);
        _lcdMenu->printMenu("Pause 1.5s....");
        _mount->delay(1500);
        _lcdMenu->printMenu("Westward pass...");
        _mount->runDriftAlignmentPhase(WEST, duration);
        _lcdMenu->setCursor(0, 1);
        _lcdMenu->printMenu("Pause 1.5s....");
        _mount->delay(1500);
        _lcdMenu->printMenu("Reset _mount->..");
        _mount->runDriftAlignmentPhase(0, duration);
        _lcdMenu->setCursor(0, 1);
        _mount->startSlewing(TRACKING);
    }
    else if (inCmd[0] == 'G')
    {                         // Get RA/DEC steps/deg, speedfactor
        if (inCmd[1] == 'R')  // :XGR#
        {
            return String(_mount->getStepsPerDegree(RA_STEPS), 1) + "#";
        }
        else if (inCmd[1] == 'D')
        {
            if (inCmd.length() > 2)
            {
                if (inCmd[2] == 'L')  // :XGDL#
                {
                    long loLimit, hiLimit;
                    _mount->getDecLimitPositions(loLimit, hiLimit);
                    char scratchBuffer[20];
                    sprintf(scratchBuffer, "%ld|%ld#", loLimit, hiLimit);
                    return String(scratchBuffer);
                }
            }
            else  // :XGD#
            {
                return String(_mount->getStepsPerDegree(DEC_STEPS), 1) + "#";
            }
        }
        else if (inCmd[1] == 'S')  // :XGS#
        {
            return String(_mount->getSpeedCalibration(), 5) + "#";
        }
        else if (inCmd[1] == 'T')  // :XGT#
        {
            return String(_mount->getSpeed(TRACKING), 7) + "#";
        }
        else if (inCmd[1] == 'B')  // :XGB#
        {
            return String(_mount->getBacklashCorrection()) + "#";
        }
        else if (inCmd[1] == 'C')  // :XGCn.nn*m.mm#
        {
            String coords = inCmd.substring(2);
            int star      = coords.indexOf('*');
            if (star > 0)
            {
                long raPos, decPos;
                float raCoord  = coords.substring(0, star).toFloat();
                float decCoord = coords.substring(star + 1).toFloat();
                _mount->calculateStepperPositions(raCoord, decCoord, raPos, decPos);
                char scratchBuffer[20];
                sprintf(scratchBuffer, "%ld|%ld#", raPos, decPos);
                return String(scratchBuffer);
            }
        }
        else if (inCmd[1] == 'M')
        {
            if ((inCmd.length() > 2) && (inCmd[2] == 'S'))  // :XGMS#
            {
                return _mount->getStepperInfo() + "#";
            }
            return _mount->getMountHardwareInfo() + "#";  // :XGM#
        }
        else if (inCmd[1] == 'O')  // :XGO#
        {
            return getLogBuffer();
        }
        else if (inCmd[1] == 'H')  // :XGH#
        {
            char scratchBuffer[10];
            DayTime ha = _mount->calculateHa();
            sprintf(scratchBuffer, "%02d%02d%02d#", ha.getHours(), ha.getMinutes(), ha.getSeconds());
            return String(scratchBuffer);
        }
        else if (inCmd[1] == 'L')  // :XGL#
        {
            char scratchBuffer[10];
            DayTime lst = _mount->calculateLst();
            sprintf(scratchBuffer, "%02d%02d%02d#", lst.getHours(), lst.getMinutes(), lst.getSeconds());
            return String(scratchBuffer);
        }
        else if (inCmd[1] == 'N')  // :XGN#
        {
#if (WIFI_ENABLED == 1)
            return wifiControl.getStatus() + "#";
#endif

            return "0,#";
        }
    }
    else if (inCmd[0] == 'S')
    {                         // Set RA/DEC steps/deg, speedfactor
        if (inCmd[1] == 'R')  // :XSR#
        {
            _mount->setStepsPerDegree(RA_STEPS, inCmd.substring(2).toFloat());
        }
        else if (inCmd[1] == 'D')  // :XSD
        {
            if ((inCmd.length() > 3) && (inCmd[2] == 'L'))  // :XSDL
            {
                if (inCmd[3] == 'L')  // :XSDLL
                {
                    _mount->setDecLimitPosition(false);
                }
                else if (inCmd[3] == 'U')  // :XSDLU
                {
                    _mount->setDecLimitPosition(true);
                }
                else if (inCmd[3] == 'l')  // :XSDLl
                {
                    _mount->clearDecLimitPosition(false);
                }
                else if (inCmd[3] == 'u')  // :XSDLU
                {
                    _mount->clearDecLimitPosition(true);
                }
            }
            else
            {
                _mount->setStepsPerDegree(DEC_STEPS, inCmd.substring(2).toFloat());
            }
        }
        else if (inCmd[1] == 'S')  // :XSS
        {
            _mount->setSpeedCalibration(inCmd.substring(2).toFloat(), true);
        }
        else if (inCmd[1] == 'M')  // :XSM
        {
            _mount->setManualSlewMode(inCmd[2] == '1');
        }
        else if (inCmd[1] == 'X')  // :XSX
        {
            _mount->setSpeed(RA_STEPS, inCmd.substring(2).toFloat());
        }
        else if (inCmd[1] == 'Y')  // :XSY
        {
            _mount->setSpeed(DEC_STEPS, inCmd.substring(2).toFloat());
        }
        else if (inCmd[1] == 'B')  // :XSB
        {
            _mount->setBacklashCorrection(inCmd.substring(2).toInt());
        }
    }
    else if (inCmd[0] == 'L')
    {  // Digital Level
#if USE_GYRO_LEVEL == 1
        if (inCmd[1] == 'G')
        {                         // get values
            if (inCmd[2] == 'R')  // :XLGR
            {                     // get Calibration/Reference values
                return String(_mount->getPitchCalibrationAngle(), 4) + "," + String(_mount->getRollCalibrationAngle(), 4) + "#";
            }
            else if (inCmd[2] == 'C')  // :XLGC
            {                          // Get current values
                auto angles = Gyro::getCurrentAngles();
                return String(angles.pitchAngle, 4) + "," + String(angles.rollAngle, 4) + "#";
            }
            else if (inCmd[2] == 'T')  // :XLGT
            {                          // Get current temp
                float temp = Gyro::getCurrentTemperature();
                return String(temp, 1) + "#";
            }
        }
        else if (inCmd[1] == 'S')
        {                         // set values
            if (inCmd[2] == 'P')  // :XLSP
            {                     // get Calibration/Reference values
                _mount->setPitchCalibrationAngle(inCmd.substring(3).toFloat());
                return String("1#");
            }
            else if (inCmd[2] == 'R')  // :XLSR
            {
                _mount->setRollCalibrationAngle(inCmd.substring(3).toFloat());
                return String("1#");
            }
        }
        else if (inCmd[1] == '1')  // :XL1
        {                          // Turn on Gyro
            Gyro::startup();
            return String("1#");
        }
        else if (inCmd[1] == '0')  // :XL0
        {                          // Turn off Gyro
            Gyro::shutdown();
            return String("1#");
        }
        else
        {
            return "Unknown Level command: X" + inCmd;
        }
#endif
        return String("0#");
    }
    else if ((inCmd[0] == 'F') && (inCmd[1] == 'R'))
    {
        _mount->clearConfiguration();  // :XFR
        return String("1#");
    }

    return "";
}

/////////////////////////////
// QUIT
/////////////////////////////
String MeadeCommandProcessor::handleMeadeQuit(String inCmd)
{
    // :Q# stops a motors - remains in Control mode
    // :Qq# command does not stop motors, but quits Control mode
    if (inCmd.length() == 0)
    {
        _mount->stopSlewing(ALL_DIRECTIONS | TRACKING);
        _mount->waitUntilStopped(ALL_DIRECTIONS);
        return "";
    }

    switch (inCmd[0])
    {
        case 'a':
            _mount->stopSlewing(ALL_DIRECTIONS);
            break;
        case 'e':
            _mount->stopSlewing(EAST);
            break;
        case 'w':
            _mount->stopSlewing(WEST);
            break;
        case 'n':
            _mount->stopSlewing(NORTH);
            break;
        case 's':
            _mount->stopSlewing(SOUTH);
            break;
        case 'q':
            inSerialControl = false;
            _lcdMenu->setCursor(0, 0);
            _lcdMenu->updateDisplay();
            break;
    }

    return "";
}

/////////////////////////////
// Set Slew Rates
/////////////////////////////
String MeadeCommandProcessor::handleMeadeSetSlewRate(String inCmd)
{
    switch (inCmd[0])
    {
        case 'S':
            _mount->setSlewRate(4);
            break;  // Slew   - Fastest
        case 'M':
            _mount->setSlewRate(3);
            break;  // Find   - 2nd Fastest
        case 'C':
            _mount->setSlewRate(2);
            break;  // Center - 2nd Slowest
        case 'G':
            _mount->setSlewRate(1);
            break;  // Guide  - Slowest
        default:
            break;
    }
    return "";
}

/////////////////////////////
// FOCUS COMMANDS
/////////////////////////////
String MeadeCommandProcessor::handleMeadeFocusCommands(String inCmd)
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (inCmd[0] == '+')  // :F+
    {
        LOGV1(DEBUG_MEADE, F("Meade: Focus focusContinuousMove IN"));
        _mount->focusContinuousMove(FOCUS_BACKWARD);
    }
    else if (inCmd[0] == '-')  // :F-
    {
        LOGV1(DEBUG_MEADE, F("Meade: Focus focusContinuousMove OUT"));
        _mount->focusContinuousMove(FOCUS_FORWARD);
    }
    else if (inCmd[0] == 'M')  // :FMnnnn
    {
        long steps = inCmd.substring(1).toInt();
        LOGV2(DEBUG_MEADE, F("Meade: Focus move by %l steps"), steps);
        _mount->focusMoveBy(steps);
    }
    else if ((inCmd[0] >= '1') && (inCmd[0] <= '4'))  // :F1 - Slowest, F4 fastest
    {
        int speed = inCmd[0] - '0';
        LOGV2(DEBUG_MEADE, F("Meade: Focus setSpeed %d"), speed);
        _mount->focusSetSpeedByRate(speed);
    }
    else if (inCmd[0] == 'F')  // :FF
    {
        LOGV1(DEBUG_MEADE, F("Meade: Focus setSpeed fastest"));
        _mount->focusSetSpeedByRate(4);
    }
    else if (inCmd[0] == 'S')  // :FS
    {
        LOGV1(DEBUG_MEADE, F("Meade: Focus setSpeed slowest"));
        _mount->focusSetSpeedByRate(1);
    }
    else if (inCmd[0] == 'p')  // :Fp
    {
        LOGV1(DEBUG_MEADE, F("Meade: Focus get stepperPosition"));
        long focusPos = _mount->focusGetStepperPosition();
        return String(focusPos) + "#";
    }
    else if (inCmd[0] == 'P')  // :FPnnn
    {
        long steps = inCmd.substring(1).toInt();
        LOGV2(DEBUG_MEADE, F("Meade: Focus set stepperPosition %d"), steps);
        _mount->focusSetStepperPosition(steps);
        return "1";
    }
    else if (inCmd[0] == 'B')  // :FB
    {
        LOGV1(DEBUG_MEADE, F("Meade: Focus isRunningFocus"));
        return _mount->isRunningFocus() ? "1" : "0";
    }
    else if (inCmd[0] == 'Q')  // :FQ
    {
        LOGV1(DEBUG_MEADE, F("Meade: Focus stop"));
        _mount->focusStop();
    }
#else
    if (inCmd[0] == 'p')  // :Fp
    {
        return "0#";
    }
    else if (inCmd[0] == 'B')  // :FB
    {
        return "0";
    }

#endif
    return "";
}

String MeadeCommandProcessor::processCommand(String inCmd)
{
    if (inCmd[0] == ':')
    {
        LOGV2(DEBUG_MEADE, F("MEADE: Received command '%s'"), inCmd.c_str());

        // Apparently some LX200 implementations put spaces in their commands..... remove them with impunity.
        int spacePos;
        while ((spacePos = inCmd.indexOf(' ')) != -1)
        {
            inCmd.remove(spacePos, 1);
        }

        LOGV2(DEBUG_MEADE, F("MEADE: Processing command '%s'"), inCmd.c_str());
        char command = inCmd[1];
        inCmd        = inCmd.substring(2);
        switch (command)
        {
            case 'S':
                return handleMeadeSetInfo(inCmd);
            case 'M':
                return handleMeadeMovement(inCmd);
            case 'G':
                return handleMeadeGetInfo(inCmd);
            case 'g':
                return handleMeadeGPSCommands(inCmd);
            case 'C':
                return handleMeadeSyncControl(inCmd);
            case 'h':
                return handleMeadeHome(inCmd);
            case 'I':
                return handleMeadeInit(inCmd);
            case 'Q':
                return handleMeadeQuit(inCmd);
            case 'R':
                return handleMeadeSetSlewRate(inCmd);
            case 'D':
                return handleMeadeDistance(inCmd);
            case 'X':
                return handleMeadeExtraCommands(inCmd);
            case 'F':
                return handleMeadeFocusCommands(inCmd);
            default:
                LOGV2(DEBUG_MEADE, F("MEADE: Received unknown command '%s'"), inCmd.c_str());
                break;
        }
    }
    return "";
}
