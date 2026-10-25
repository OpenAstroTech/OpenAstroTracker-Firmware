#include "MountReadout.hpp"

#include "../DayTime.hpp"
#include "../HallSensorHoming.hpp"
#include "../Utility.hpp"
#include "../core/CoordinateFormatter.hpp"
#include "../core/CoordinateRead.hpp"
#include "../core/CoordinateText.hpp"
#include "../core/ActivityState.hpp"
#include "../core/meade/MountStatusReport.hpp"
#include "../core/SlewProgressReport.hpp"
#include "../ports/IClock.hpp"
#include "../Declination.hpp"

// slewStatus()
#define SLEWING_DEC      B00000010
#define SLEWING_RA       B00000001
#define SLEWING_BOTH     B00000011
#define SLEWING_TRACKING B00001000
#define NOT_SLEWING      B00000000
#define SLEW_MASK_ANY    B1111

namespace app
{

MountReadout::MountReadout(AxisBank &axes,
                           core::MountCoordinator &coordinator,
                           core::MountRuntimeSettings &settings,
                           const core::MountConfig &config,
                           core::SiteTime &siteTime,
                           core::DayTime &targetRa,
                           core::Declination &targetDec,
                           float &zeroPosDec,
                           ports::IClock &clock,
                           long &commandsReceived,
                           RaLimitQuery raLimit)
    : _axes(axes), _coordinator(coordinator), _settings(settings), _config(config), _siteTime(siteTime), _targetRa(targetRa),
      _targetDec(targetDec), _zeroPosDec(zeroPosDec), _clock(clock), _commandsReceived(commandsReceived), _raLimit(raLimit)
{
}

long MountReadout::getNumCommandsReceived()
{
    return _commandsReceived;
}

const char *MountReadout::panelStatusText()
{
    _infoStatusScratch = getStatusStateString();
    return _infoStatusScratch.c_str();
}

const char *MountReadout::getStepperInfo()
{
    String ret = "";

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    ret += "TU";
#elif RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE
    ret += "TS";
#elif RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC
    ret += "A";
#else
    ret += "?";
#endif

    ret += ",";
    ret += String(_config.raSlewMicrosteps);
    ret += ",";
    ret += String(_config.raTrackingMicrosteps());

    ret += "|";

#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    ret += "TU";
#elif DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE
    ret += "TS";
#elif DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC
    ret += "A";
#else
    ret += "?";
#endif

    ret += ",";
    ret += String(_config.decSlewMicrosteps);
    ret += ",";
    ret += String(_config.decGuidingMicrosteps());

    ret += "|";

    _readoutScratch = ret;
    return _readoutScratch.c_str();
}

const char *MountReadout::getMountHardwareInfo()
{
    String ret = F("Unknown,");
#if defined(ESP32)
    ret = F("ESP32,");
#elif defined(__AVR_ATmega2560__)
    ret = F("Mega,");
#elif defined(ARDUINO_ARCH_RP2040)
    ret = F("RP2040,");
#endif

    ret += F("NEMA|");

    ret += String(RA_PULLEY_TEETH) + "|";
    ret += String(RA_STEPPER_SPR) + ",";

    ret += F("NEMA|");

    ret += String(DEC_PULLEY_TEETH) + "|";
    ret += String(DEC_STEPPER_SPR) + ",";

#if USE_GPS == 1
    ret += F("GPS,");
#else
    ret += F("NO_GPS,");
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) && (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    ret += F("AUTO_AZ_ALT,");
#elif (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    ret += F("AUTO_AZ,");
#elif (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    ret += F("AUTO_ALT,");
#else
    ret += F("NO_AZ_ALT,");
#endif

#if USE_GYRO_LEVEL == 1
    ret += F("GYRO,");
#else
    ret += F("NO_GYRO,");
#endif

#if DISPLAY_TYPE == DISPLAY_TYPE_NONE
    ret += F("NO_LCD,");
#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD
    ret += F("LCD_KEYPAD,");
#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008
    ret += F("LCD_I2C_MCP23008,");
#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
    ret += F("LCD_I2C_MCP23017,");
#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    ret += F("LCD_JOY_I2C_SSD1306,");
#endif

#if INFO_DISPLAY_TYPE == DISPLAY_TYPE_NONE
    ret += F("NO_INFO_DISP,");
#elif INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64
    ret += F("INFO_I2C_SSD1306_128x64,");
    // To add new display types, format this string in the same way: INFO_<interface>_<chip>_<resolution>
#else
    ret += F("INFO_UNKNOWN,");
#endif

#if FOCUS_STEPPER_TYPE == STEPPER_TYPE_NONE
    ret += F("NO_FOC,");
#else
    ret += F("FOC,");
#endif

#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    ret += F("HSAH,");
#else
    ret += F("NO_HSAH,");
#endif
#if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    ret += F("HSAV,");
#else
    ret += F("NO_HSAV,");
#endif

#if (USE_RA_END_SWITCH == 1) || (USE_DEC_END_SWITCH == 1)
    ret += F("ENDSW");
    #if (USE_RA_END_SWITCH == 1)
    ret += F("_RA");
    #endif
    #if (USE_DEC_END_SWITCH == 1)
    ret += F("_DEC");
    #endif
    ret += F(",");
#else
    ret += F("NO_ENDSW,");
#endif

    _readoutScratch = ret;
    return _readoutScratch.c_str();
}

core::DayTime MountReadout::currentRA() const
{
    // The arithmetic lives in core::currentRa; this gathers the six numbers it needs.
    core::PositionFacts facts;
    facts.raSteps           = _axes.ra()->currentPosition();
    facts.decSteps          = _axes.dec()->currentPosition();
    facts.raStepsPerDegree  = _settings.stepsPerDegree(RA_STEPS);
    facts.decStepsPerDegree = _settings.stepsPerDegree(DEC_STEPS);
    facts.zeroPosRaHours    = DayTime(_siteTime.raZeroPoint()).getTotalHours();
    facts.zeroPosDecDegrees = _zeroPosDec;
    return core::currentRa(facts);
}

core::Declination MountReadout::currentDEC() const
{
    // u-steps / u-steps/deg = deg; the arithmetic lives in core::currentDec.
    core::PositionFacts facts;
    facts.raSteps           = _axes.ra()->currentPosition();
    facts.decSteps          = _axes.dec()->currentPosition();
    facts.raStepsPerDegree  = _settings.stepsPerDegree(RA_STEPS);
    facts.decStepsPerDegree = _settings.stepsPerDegree(DEC_STEPS);
    facts.zeroPosRaHours    = DayTime(_siteTime.raZeroPoint()).getTotalHours();
    facts.zeroPosDecDegrees = _zeroPosDec;
    return core::currentDec(facts);
}

bool MountReadout::isAxisRunning(StepperAxis axis)
{
    switch (axis)
    {
        case RA_STEPS:
            return _axes.ra()->isRunning();
        case DEC_STEPS:
            return _axes.dec()->isRunning();
            if (kHasAltStepper)
            {
                case ALTITUDE_STEPS:
                    return _axes.alt()->isRunning();
            }
            if (kHasAzStepper)
            {
                case AZIMUTH_STEPS:
                    return _axes.az()->isRunning();
            }
            if (kHasFocusStepper)
            {
                case FOCUS_STEPS:
                    return _axes.focus()->isRunning();
            }
        default:
            break;
    }
    return false;
}

String MountReadout::getStatusStateString()
{
    // The precedence lives in core::classifyActivity; the words stay here as flash literals,
    // because on AVR a plain literal is copied into SRAM.
    switch (core::classifyActivity(_coordinator.state(),
                                   isFindingHome(),
                                   isGuiding(),
                                   _axes.ra()->isRunning() || _axes.dec()->isRunning(),
                                   _coordinator.state().test(STATUS_TRACKING)))
    {
        case core::ActivityState::Parked:
            return F("Parked");
        case core::ActivityState::Parking:
            return F("Parking");
        case core::ActivityState::Homing:
            return F("Homing");
        case core::ActivityState::Guiding:
            return F("Guiding");
        case core::ActivityState::SlewToTarget:
            return F("SlewToTarget");
        case core::ActivityState::FreeSlew:
            return F("FreeSlew");
        case core::ActivityState::ManualSlew:
            return F("ManualSlew");
        case core::ActivityState::Tracking:
            return F("Tracking");
        case core::ActivityState::Unknown:
            return String();
        case core::ActivityState::Idle:
        default:
            return F("Idle");
    }
}

const char *MountReadout::getStatusString()
{
    // The field order lives in core::meade; this gathers the facts and keeps the buffer.
    const bool trackingAxis = _coordinator.state().test(STATUS_TRACKING);
    const bool raRunning    = _axes.ra()->isRunning();
    const bool decRunning   = _axes.dec()->isRunning();

    oat::core::meade::MountStatusFacts facts {};
    _infoStatusScratch = getStatusStateString();
    facts.activity     = _infoStatusScratch.c_str();

    // The RA/DEC glyphs only appear while a slew is in flight, as before.
    const bool slewing          = _coordinator.state().test(STATUS_SLEWING);
    facts.raRunning             = slewing && raRunning;
    facts.decRunning            = slewing && decRunning;
    facts.trackingAxisRunning   = slewing && trackingAxis;
    facts.raTargetBelowCurrent  = _axes.ra()->targetPosition() < _axes.ra()->currentPosition();
    facts.decTargetBelowCurrent = _axes.dec()->targetPosition() < _axes.dec()->currentPosition();
    facts.trackingOnly          = isSlewingTRK() && !slewing;

    if (kHasAzStepper)
    {
        facts.azRunning            = _axes.az()->isRunning();
        facts.azTargetBelowCurrent = _axes.az()->targetPosition() < _axes.az()->currentPosition();
    }
    if (kHasAltStepper)
    {
        facts.altRunning            = _axes.alt()->isRunning();
        facts.altTargetBelowCurrent = _axes.alt()->targetPosition() < _axes.alt()->currentPosition();
    }
    if (kHasFocusStepper)
    {
        facts.focusRunning            = _axes.focus()->isRunning();
        facts.focusTargetBelowCurrent = _axes.focus()->targetPosition() < _axes.focus()->currentPosition();
        facts.hasFocus                = true;
        facts.focusSteps              = _axes.focus()->currentPosition();
    }

    facts.raSteps  = _axes.ra()->currentPosition();
    facts.decSteps = _axes.dec()->currentPosition();
    facts.trkSteps = _axes.tracking()->currentPosition();
    facts.raText   = raText(COMPACT_STRING | CURRENT_STRING);
    facts.decText  = decText(COMPACT_STRING | CURRENT_STRING);

    char buffer[160] = {0};
    if (!oat::core::meade::formatMountStatus(buffer, sizeof(buffer), facts))
    {
        // The readout the host already has is better than a truncated line.
        return _readoutScratch.c_str();
    }

    _readoutScratch = buffer;
    return _readoutScratch.c_str();
}

byte MountReadout::slewStatus() const
{
    if (_coordinator.state().isParked())
    {
        return NOT_SLEWING;
    }
    if (isGuiding())
    {
        return NOT_SLEWING;
    }
    byte slewState = _axes.ra()->isRunning() ? SLEWING_RA : NOT_SLEWING;
    slewState |= _axes.dec()->isRunning() ? SLEWING_DEC : NOT_SLEWING;

    slewState |= _coordinator.state().test(STATUS_TRACKING) ? SLEWING_TRACKING : NOT_SLEWING;
    return slewState;
}

bool MountReadout::isGuiding() const
{
    return _coordinator.state().test(STATUS_GUIDE_PULSE);
}

bool MountReadout::isSlewingTRK() const
{
    return (slewStatus() & SLEWING_TRACKING) != 0;
}

bool MountReadout::isFindingHome() const
{
    return _coordinator.state().test(STATUS_FINDING_HOME);
}

const char *MountReadout::getAutoHomingStates() const
{
    String state = homingStateText(kHasRaHoming && (_axes.raHoming() != nullptr), _axes.raHoming());
    state += "|";
    state += homingStateText(kHasDecHoming && (_axes.decHoming() != nullptr), _axes.decHoming());
    _readoutScratch = state;
    return _readoutScratch.c_str();
}

String MountReadout::homingStateText(bool present, const HallSensorHoming *sensor) const
{
    if (!present)
    {
        return String();
    }
    if (!sensor->isIdleOrComplete())
    {
        return sensor->getHomingState(sensor->getHomingState());
    }
    return sensor->getLastResult();
}

const char *MountReadout::raText(uint8_t type, uint8_t active)
{
    RAString(type, active);
    return scratchBuffer;
}

const char *MountReadout::decText(uint8_t type, uint8_t active)
{
    DECString(type, active);
    return scratchBuffer;
}

ports::InfoDisplayActivity MountReadout::activity()
{
    return ports::InfoDisplayActivity {isAxisRunning(RA_STEPS), isAxisRunning(DEC_STEPS)};
}

bool MountReadout::getStepperProgress(int &raPercentage, int &decPercentage)
{
    // The rules live in core::SlewProgressReport; this gathers the four numbers it needs.
    const core::SlewProgress progress = core::SlewProgressReport::evaluate(
        _coordinator.slew().totalRaSteps(), _coordinator.slew().totalDecSteps(), _axes.ra()->distanceToGo(), _axes.dec()->distanceToGo());
    raPercentage  = progress.raPercent;
    decPercentage = progress.decPercent;
    return progress.inProgress;
}

String MountReadout::DECString(byte type, byte active)
{
    Declination dec;
    if ((type & TARGET_STRING) == TARGET_STRING)
    {
        dec = _targetDec;
    }
    else
    {
        dec = Declination(currentDEC());
    }

    const core::CoordinateFormat format = core::coordinateFormat(type);
    core::formatDEC(scratchBuffer, dec, core::decFormat(format));

    if (format == core::CoordinateFormat::LcdMenu)
    {
        core::insertLcdMenuCursor(scratchBuffer, active, /*hasSignColumn=*/true);
    }

    return String(scratchBuffer);
}

String MountReadout::RAString(byte type, byte active)
{
    DayTime ra;
    if ((type & TARGET_STRING) == TARGET_STRING)
    {
        ra = _targetRa;
    }
    else
    {
        ra = currentRA();
    }

    const core::CoordinateFormat format = core::coordinateFormat(type);
    core::formatRA(scratchBuffer, sizeof(scratchBuffer), ra, core::raFormat(format));
    if (format == core::CoordinateFormat::LcdMenu)
    {
        core::insertLcdMenuCursor(scratchBuffer, active, /*hasSignColumn=*/false);
    }
    return String(scratchBuffer);
}

float MountReadout::getStepsPerDegree(StepperAxis which)
{
    if (which == RA_STEPS)
    {
        return _settings.stepsPerDegree(RA_STEPS);  // u-steps/degree
    }
    if (which == DEC_STEPS)
    {
        return _settings.stepsPerDegree(DEC_STEPS);  // u-steps/degree
    }
    if (which == AZIMUTH_STEPS)
    {
        return kHasAzStepper ? _axes.azStepsPerDegree() : 1;  // u-steps/degree
    }
    if (which == ALTITUDE_STEPS)
    {
        return kHasAltStepper ? _axes.altStepsPerDegree() : 1;  // u-steps/degree
    }

    return 0;
}

bool MountReadout::isRunningAZ() const
{
    return kHasAzStepper && _axes.az()->isRunning();
}

bool MountReadout::isRunningALT() const
{
    return kHasAltStepper && _axes.alt()->isRunning();
}

long MountReadout::getCurrentStepperPosition(int direction)
{
    if (direction & TRACKING)
    {
        return _axes.tracking()->currentPosition();
    }
    if (direction & (NORTH | SOUTH))
    {
        return _axes.dec()->currentPosition();
    }
    if (direction & (EAST | WEST))
    {
        return _axes.ra()->currentPosition();
    }
    return 0;
}

long MountReadout::getCurrentStepperPosition(StepperAxis axis)
{
    if (axis == StepperAxis::DEC_STEPS)
    {
        return _axes.dec()->currentPosition();
    }
    if (axis == StepperAxis::RA_STEPS)
    {
        return _axes.ra()->currentPosition();
    }
    if (axis == StepperAxis::FOCUS_STEPS)
    {
        if (kHasFocusStepper)
        {
            return _axes.focus()->currentPosition();
        }
    }
    if (axis == StepperAxis::ALTITUDE_STEPS)
    {
        if (kHasAltStepper)
        {
            return _axes.alt()->currentPosition();
        }
    }
    if (axis == StepperAxis::AZIMUTH_STEPS)
    {
        if (kHasAzStepper)
        {
            return _axes.az()->currentPosition();
        }
    }
    return 0;
}

void MountReadout::fillInfoDisplayState(ports::InfoDisplayState &state)
{
    state.raSteps           = getCurrentStepperPosition(StepperAxis::RA_STEPS);
    state.decSteps          = getCurrentStepperPosition(StepperAxis::DEC_STEPS);
    state.trkSteps          = getCurrentStepperPosition(TRACKING);
    state.raStepsPerDegree  = getStepsPerDegree(StepperAxis::RA_STEPS);
    state.decStepsPerDegree = getStepsPerDegree(StepperAxis::DEC_STEPS);

    state.raRunning  = isAxisRunning(RA_STEPS);
    state.decRunning = isAxisRunning(DEC_STEPS);
    state.altRunning = isRunningALT();
    state.azRunning  = isRunningAZ();
    state.trkRunning = isSlewingTRK();
    state.guiding    = isGuiding();

    state.tracking = _coordinator.state().test(STATUS_TRACKING);
    state.slewing  = _coordinator.state().test(STATUS_SLEWING);
    state.parked   = _coordinator.state().isParked();

    state.commandsReceived = _commandsReceived;

    state.progressActive = getStepperProgress(state.raProgressPercent, state.decProgressPercent);

    state.latitudeHours  = _siteTime.latitude().getTotalHours();
    state.longitudeHours = _siteTime.longitude().getTotalHours();
    state.hoursToLimit   = _raLimit.valid() ? _raLimit.hoursToLimit(_raLimit.context) : 0.0f;
    state.lst            = calculateLst();

    // The panel's coordinate text is the LCD shape the renderer drew before, produced here so the
    // renderer needs no formatter and no mount.
    const String raText  = RAString(LCD_STRING | CURRENT_STRING);
    const String decText = DECString(LCD_STRING | CURRENT_STRING);
    snprintf(state.raText, sizeof(state.raText), "%s", raText.c_str());
    snprintf(state.decText, sizeof(state.decText), "%s", decText.c_str());

    // The status line outlives this call because it is copied into the scratch above.
    state.statusText = panelStatusText();
}

core::DayTime MountReadout::calculateLst()
{
    return _siteTime.lstAt(_clock.now());
}

}  // namespace app
