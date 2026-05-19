
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
//        "OpenAstroTracker#" if the firmware was compiled for OAT
//        "OpenAstroMount#" if the firmware was compiled for OAM
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
//        "sDDD*MM#"
//      Parameters:
//        "s" is the sign of the longitude
//        "DDD" is the degrees
//        "MM" is the minutes
//      Remarks:
//        Note that this is the actual longitude, but east coordinates are negative (opposite of normal cartographic coordinates)
//
// :Gc#
//      Description:
//        Get current Clock format
//      Returns:
//        "24#"
//
// :GG#
//      Description:
//        Get offset to UTC time
//      Returns:
//        "sHH#"
//      Parameters:
//        "s" is the sign
//        "HH" is the number of hours
//      Remarks:
//        Note that this is NOT simply the timezone offset you are in (like -8 for Pacific Standard Time), it is the negative of it. So how many hours need to be added to your local time to get to UTC.
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
//         String reflecting the mounts' status. The string is a comma-delimited list of statuses.
//      Returns:
//        "Idle,--T--,11219,0,927,071906,+900000,,#"
//      Parameters:
//        [0] The mount status. One of 'Idle', 'Parked', 'Parking', 'Guiding', 'SlewToTarget', 'FreeSlew', 'ManualSlew', 'Tracking', 'Homing'
//        [1] The motion state (see Remarks below).
//        [2] The RA stepper position
//        [3] The DEC stepper position
//        [4] The Tracking stepper position
//        [5] The current RA coordinate
//        [6] The current DEC coordinate
//        [7] The FOC stepper position (if FOC enabled, else empty)
//      Remarks:
//        The motion state consists of 6 characters. If the character is a '-', the corresponding axis is not moving.
//        First character is RA slewing state ('R' is East, 'r' is West, '-' is stopped).
//        Second character is DEC slewing state ('d' is North, 'D' is South, '-' is stopped).
//        Third character is TRK slewing state ('T' is Tracking, '-' is stopped).
//        Fourth character is AZ slewing state ('Z' and 'z' is adjusting, '-' is stopped).
//        Fifth character is ALT slewing state ('A' and 'a' is adjusting, '-' is stopped).
//        Sixth character is FOC slewing state ('F' and 'f' is adjusting, '-' is stopped).
//        AZ, ALT, and FOC are only set if the corresponding axis is enabled. If not, the character is always '-'.
//        Since AZ/ALT rarely move, their positions are not returned here. To get the AZ and ALT stepper positions, use the ":XGAA#" command.
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
// :SgsDDD*MM#
//      Description:
//        Set Site Longitude
//      Information:
//        This sets the longitude of the location of the mount.
//      Returns:
//        "1" if successfully set
//        "0" otherwise
//      Parameters:
//        "s" (optional) is the sign of the longitude (see Remarks)
//        "DDD" is the number of degrees
//        "MM" is the minutes
//      Remarks:
//        When a sign is provided, longitudes are interpreted as given, with zero at Greenwich but negative coordinates going east (opposite of normal cartographic coordinates)
//        When a sign is not provided, longitudes are from 0 to 360 going WEST with 180 at Greenwich. So 369 is 179W and 1 is 179E. 190 would be 10W and 170 would be 10E.
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
//        "MM" is the month
//        "DD" is the day
//        "YY" is the year since 2000
//
//------------------------------------------------------------------
// SET Extensions
//
// :SHHH:MM#
//      Description:
//        Set HA (Hour Angle of Polaris)
//      Information:
//        This sets the scopes HA, which should be that of Polaris.
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
//        This tells the scope what exact coordinates it is currently pointing at. These coordinates become the new current RA/DEC coordinates of the mount.
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
//        This runs the RA or DEC steppers at an increased or decreased speed (in the case of RA) or a constant speed (in the case of DEC) for a short period of time. It is used for guiding.
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
//        This starts slewing the mount in the given direction. You must issue a stop command (such as the corresponding ":Qc#",
//        where 'c' is the same direction as passed to this command) or ":Q#" (stops all steppers) to stop it.
//      Parameters:
//        "c" is one of 'n', 'e', 'w', or 's'
//      Returns:
//        nothing
//
// :MXxnnnnn#
//      Description:
//        Move stepper
//      Information:
//        This starts moving one of the steppers by the given amount of steps and returns immediately. Steps can be positive or negative.
//      Parameters:
//        "x" is the stepper to move (r for RA, d for DEC, f for FOC, z for AZ, l for ALT)
//        "nnnn" is the number of steps
//      Returns:
//        "1" if successfully scheduled, else "0"
//
// :MHRxn#
//      Description:
//        Home RA stepper via Hall sensor
//      Information:
//        This attempts to find the hall sensor and to home the RA ring accordingly.
//      Parameters:
//        "x" is either 'R' or 'L' and determines the direction in which the search starts (L is CW, R is CCW).
//        "n" (Optional) is the maximum number of degrees to move while searching for the sensor location. Defaults to 30degs. Limited to the range 5degs - 75degs.
//      Remarks:
//        The ring is first moved 30 degrees (or the given amount) in the initial direction. If no hall sensor is encountered,
//        it will move twice the amount (60 degrees by default) in the opposite direction.
//        If a hall sensor is not encountered during that slew, the homing exits with a failure.
//        If the sensor is found, it will slew to the middle position of the Hall sensor trigger range and then to the offset
//        specified in the Home offset position (set with the ":XSHRnnnn#" command).
//        If the RA ring is positioned such that the Hall sensor is already triggered when the command is received, the mount will move
//        the RA ring off the trigger in the opposite direction specified for a max of 15 degrees before searching 30 degrees in the
//        specified direction.
//      Returns:
//        "1" if search is started
//        "0" if homing has not been enabled in the local configuration file
//
// :MHDxn#
//      Description:
//        Home DEC stepper via Hall sensor
//      Information:
//        This attempts to find the hall sensor and to home the DEC axis accordingly.
//      Parameters:
//        "x" is either 'U' or 'D' and determines the direction in which the search starts (U is up, D is down).
//        "n" (Optional) is the maximum number of degrees to move while searching for the sensor location. Defaults to 30degs. Limited to the range 5degs - 75degs.
//      Remarks:
//        The ring is first moved 30 degrees (or the given amount) in the initial direction. If no hall sensor is encountered,
//        it will move twice the amount (60 degrees by default) in the opposite direction.
//        If a hall sensor is not encountered during that slew, the homing exits with a failure.
//        If the sensor is found, it will slew to the middle position of the Hall sensor trigger range and then to the offset
//        specified in the Home offset position (set with the ":XSHDnnnn#" command).
//        If the DEC ring is positioned such that the Hall sensor is already triggered when the command is received, the mount will move
//        the DEC ring off the trigger in the opposite direction specified for a max of 15 degrees before searching 30 degrees in the
//        specified direction.
//      Returns:
//        "1" if search is started
//        "0" if homing has not been enabled in the local configuration file
//
// :MAAH#
//      Description:
//        Move Azimuth and Altitude to home
//      Information:
//        If the scope supports automated azimuth and altitude operations, move AZ and ALT axis to their zero positions.
//      Returns:
//        "1"
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
//        This slews the scope back to it's home position (RA ring centered, DEC at 90, basically
//        pointing at celestial pole), then advances to the parking position (defined by the Homing offsets)
//        and stops all movement (including tracking).
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
// HOME/PARK Extensions
//
// :hU#
//      Description:
//        Unpark Scope
//      Information:
//        This currently simply turns on tracking.
//      Returns:
//        "1"
//
// :hZ#
//      Description:
//        Set home position for AZ and ALT axes
//      Information:
//        If the mount supports AZ and ALT axes, this call sets their positions to 0 and stores this in persistent storage.
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
//        nothing
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
// EXTRA OAT FAMILY - These are used by the PC control application OATControl
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
//        Run drift alignment (only supported if SUPPORT_DRIFT_ALIGNMENT is enabled)
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
//        "1#" if successful
//        "0#" if there is no Digital Level
//
// :XGAA#
//      Description:
//        Get position of AZ and ALT axes
//      Information:
//        Get the current position in steps of the AZ and ALT axes if they are enabled.
//        If an axis is not enabled, this always returns zero as the axis's value.
//      Returns:
//        "azpos|altpos#" if either axis is enabled
//
// :XGAH#
//      Description:
//        Get auto homing state
//      Information:
//        Get the current state of RA and DEC Autohoming status. Only valid when at least
//        one Hall sensor based autohoming axis is enabled.
//      Returns:
//        "rastate|decstate#" if either axis is enabled
//        "|#" if no autohoming is enabled
//      Remarks:
//        While the mount status (:GX#) is 'Homing', the command returns one of these:
//          MOVE_OFF
//          MOVING_OFF
//          STOP_AT_TIME
//          WAIT_FOR_STOP
//          START_FIND_START
//          FINDING_START
//          FINDING_START_REVERSE
//          FINDING_END
//          RANGE_FOUND
//
//        If the mount status (:GX#) is not 'Homing' the command returns one of these:
//          SUCCEEDED
//          NEVER RUN
//          IN PROGRESS
//          CANT MOVE OFF SENSOR
//          CANT FIND SENSOR BEGIN
//          CANT FIND SENSOR END
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
// :XGZ#
//      Description:
//        Get AZ steps
//      Information:
//        Get the number of steps the AZ stepper motor needs to take to rotate AZ by one degree
//      Returns:
//        "float#" if AZ motor is present
//        "0#"     if AZ is not configured
//
// :XGA#
//      Description:
//        Get ALT steps
//      Information:
//        Get the number of steps the ALT stepper motor needs to take to rotate ALT by one degree
//      Returns:
//        "float#" if ALT motor is present
//        "0#"     if ALT is not configured
//
// :XGDLx#
//      Description:
//        Get DEC limits
//      Information:
//        Get either lower, upper or both limits for the DEC stepper motor in degrees.
//      Parameters:
//        'x' is optional or can be 'U' or 'L'. If it is 'U' only the upper bound is returned,
//            if it is 'L' only the lower bound is returned and if it is missing, both are returned.
//      Returns:
//        "float#" or "float|float#"
//
// :XGDP# (obsolete, disabled)
//      Description:
//        Get DEC parking position
//      Information:
//        Gets the number of steps from the home position to the parking position for DEC
//      Returns:
//        "0#"
//
// :XGS#
//      Description:
//        Get Tracking speed adjustment
//      Information:
//        Get the adjustment factor used to speed up (>1.0) or slow down (<1.0) the tracking speed of the mount.
//      Returns:
//        "float#"
//
// :XGST#
//      Description:
//        Get Remaining Safe Time
//      Information:
//        Get the number of hours before the RA ring reaches its end.
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
//        Get HA (Hour Angle of Polaris)
//      Information:
//        Get the current HA of Polaris that the mount thinks it is.
//      Returns:
//        "HHMMSS#"
//
// :XGHR#
//      Description:
//        Get RA Homing offset
//      Information:
//        Get the RA ring homing offset.
//        If a Hall sensor is present this is the number of steps from the center of the sensor range to
//        where the actual center position is located.
//        If no Hall sensor is present this is the number of steps from the power on position of the RA axis to
//        where the actual center position is located.
//      Returns:
//        "n#" - the number of steps
//
// :XGHD#
//      Description:
//        Get DEC Homing offset
//      Information:
//        Get the DEC ring homing offset.
//        If a Hall sensor is present this is the number of steps from the center of the sensor range to
//        where the actual center position is located.
//        If no Hall sensor is present this is the number of steps from the power on position of the DEC axis to
//        where the actual center position is located.
//      Returns:
//        "n#" - the number of steps
//
// :XGHS#
//      Description:
//        Get Hemisphere
//      Information:
//        Get the hemisphere that the OAT currently assumes it is operating in. This is set via setting Latitude (see ":St" command)
//      Returns:
//        "N#" - for northern hemisphere
//        "S#" - for southern hemisphere
//
// :XGM#
//      Description:
//        Get Mount configuration settings
//      Returns:
//        "<board>,<RA Stepper Info>,<DEC Stepper Info>,<GPS info>,<AzAlt info>,<Gyro info>,<Display info>,(more features...)#"
//      Parameters:
//        "<board>" is one of the supported boards (currently Mega, ESP32, MKS)
//        "<Stepper Info>" is a pipe-delimited string of Motor type (NEMA or 28BYJ), Pulley Teeth, Steps per revolution)
//        "<GPS info>" is either NO_GPS or GPS, depending on whether a GPS module is present
//        "<AzAlt info>" is either NO_AZ_ALT, AUTO_AZ_ALT, AUTO_AZ, or AUTO_ALT, depending on which AutoPA stepper motors are present
//        "<Gyro info>" is either NO_GYRO or GYRO depending on whether the Digital level is present
//        "<Display info>" is either NO_LCD or LCD_display_type depending on whether LCD is present and if so, which one
//        "<Focuser info>" is either NO_FOC or FOC depending on whether the focuser motor is enabled
//        "<RAHallSensor info>" is either NO_HSAH or HSAH depending on whether the Hall sensor based auto homing for RA is enabled
//        "<Endswitch info>" is either NO_ENDSW or ENDS_RA, ENDSW_DEC, or ENDSW_RA_DEC depending on which axis have end switches installed
//      Remarks:
//        As OAT/OAM firmware supports more features, these may be appended, separated by a comma. Any further features will
//        have a 'NO_xxxxx' if the feature is not supported.
//        To differentiate between OAT and OAM, use the Get Product Name (#GVP) command.
//      Example:
//        "ESP32,28BYJ|16|4096.00,28BYJ|16|4096.00,NO_GPS,NO_AZ_ALT,NO_GYRO,NO_LCD,NO_FOC,NO_ENDSW#"
//
// :XGMS#
//      Description:
//        Get Mount driver configuration
//      Returns:
//        "<RA driver>,<RA slewMS>,<RA trackMS>|<DEC driver>,<DEC slewMS>,<DEC guideMS>|#"
//      Parameters:
//        "<driver>" is one of the supported drivers: TU=TMC2209UART, TS=TMC2209STANDALONE, A=A4983
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
//        "HHMMSS#"
//
// :XSBn#
//      Description:
//        Set Backlash correction steps
//      Information:
//        Sets the number of steps the RA stepper motor needs to overshoot and backtrack when slewing east.
//      Returns:
//        nothing
//
// :XSHRnnn#
//      Description:
//        Set homing offset for RA ring from Hall sensor center
//      Information:
//        This offset is added to the position of the RA ring when it is centered on the hall sensor triggered range after running.
//        the RA homing command (:MHRx#)
//      Parameters:
//        "n" is the (positive or negative) number of steps that are needed from the center of the Hall sensor trigger range to the actual home position.
//      Returns:
//        nothing
//
// :XSHDnnn#
//      Description:
//        Set homing offset for DEC ring from Hall sensor center
//      Information:
//        This offset is added to the position of the DEC ring when it is centered on the hall sensor triggered range after running.
//        the DEC homing command (:MHDx#)
//      Parameters:
//        "n" is the (positive or negative) number of steps that are needed from the center of the Hall sensor trigger range to the actual home position.
//      Returns:
//        nothing
//
// :XSRn.n#
//      Description:
//        Set RA steps
//      Information:
//        Set the number of steps the RA stepper motor needs to take to rotate by one degree.
//      Parameters:
//        "n.n" is the number of steps (only one decimal point is supported, must be positive)
//      Returns:
//        nothing
//
// :XSDn.n#
//      Description:
//        Set DEC steps
//      Information:
//        Set the number of steps the DEC stepper motor needs to take to rotate by one degree.
//      Parameters:
//        "n.n" is the number of steps (only one decimal point is supported, must be positive)
//      Returns:
//        nothing
//
// :XSAn.n#
//      Description:
//        Set AZ steps
//      Information:
//        Set the number of steps the AZ stepper motor needs to take to rotate by one degree.
//      Parameters:
//        "n.n" is the number of steps (only one decimal point is supported, must be positive)
//      Returns:
//        nothing
//
// :XSLn.n#
//      Description:
//        Set ALT steps
//      Information:
//        Set the number of steps the ALT stepper motor needs to take to rotate by one degree.
//      Parameters:
//        "n.n" is the number of steps (only one decimal point is supported, must be positive)
//      Returns:
//        nothing
//
// :XSDLUnnnnn#
//      Description:
//        Set DEC upper limit
//      Information:
//        Set the upper limit for the DEC axis to the current position if no parameter is given,
//        otherwise to the given angle (in degrees from the home position).
//      Parameters:
//        "nnnnn" is the number of steps from home that the DEC ring can travel upwards. Passing 0 will reset it to the
//                limits defined in your configuration file. Omitting this parameter sets it to the current DEC position.
//      Returns:
//        nothing
//
// :XSDLu#
//      Description:
//        Clear DEC upper limit
//      Information:
//        Resets the upper limit for the DEC axis to the configuration-defined position.
//        If not configured, the limit is cleared.
//      Returns:
//        nothing
//
// :XSDLLnnnnn#
//      Description:
//        Set DEC lower limit
//      Information:
//        Set the lower limit for the DEC axis to the current position if no parameter is given,
//        otherwise to the given angle (in degrees from the home position).
//      Parameters:
//        "nnnnn" is the number of steps from home that the DEC ring can travel downwards. Passing 0 will reset it to the
//                limits defined in your configuration file. Omitting this parameter sets it to the current DEC position.
//      Returns:
//        nothing
//
// :XSDLl#
//      Description:
//        Clear DEC lower limit
//      Information:
//        Resets the lower limit for the DEC axis to the configuration-defined position.
//        If not configured, the limit is cleared.
//      Returns:
//        nothing
//
// :XSDPnnnn# (obsolete, disabled)
//      Description:
//        Set DEC parking position offset
//      Information:
//        This stores the number of steps needed to move from home to the parking position.
//      Returns:
//        nothing
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
// :XSTnnnn#
//      Description:
//        Set Tracking motor position (no movement)
//      Information:
//        This is purely a debugging aid. It is not recommended to call this unless you know what you are doing. It simply sets the internal tracking steps to the given value.
//      Parameters:
//        "nnn" is the stepper steps to set
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
//        Set focuser speed to <n> where <n> is an ASCII digit 1..4. 1 is slowest, 4 is fastest
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
//        Stops the stepper motor of the focuser.
//      Returns:
//        nothing
//
//------------------------------------------------------------------
/////////////////////////////////////////////////////////////////////////////////////////
