#include "inc/Globals.hpp"
#include "../Configuration.hpp"
#include "Utility.hpp"
#include "LcdMenu.hpp"
#include "Mount.hpp"
#include "MeadeCommandProcessor.hpp"
#include "WifiControl.hpp"
#include "Gyro.hpp"
#include "core/meade/MeadeParser.hpp"

#include <stdarg.h>
#include <string.h>

#if USE_GPS == 1
bool gpsAqcuisitionComplete(int &indicator);  // defined in c72_menuHA_GPS.hpp
#endif

namespace meade = oat::core::meade;

MeadeCommandProcessor *MeadeCommandProcessor::_instance = nullptr;

const char *MeadeCommandProcessor::store(oat::core::meade::MeadeResponse &response)
{
    _response = response;
    return _response.c_str();
}

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
void MeadeCommandProcessor::onEnterSerialControl()
{
    inSerialControl = true;
    _lcdMenu->setCursor(0, 0);
    _lcdMenu->printMenu("Remote control");
    _lcdMenu->setCursor(0, 1);
    _lcdMenu->printMenu(">SELECT to quit");
}

/////////////////////////////
// GET INFO
/////////////////////////////
// ---- IMeadeGetHandlers callbacks ---------------------------------------

const char *MeadeCommandProcessor::onFirmwareVersion()
{
    return VERSION;
}

const char *MeadeCommandProcessor::onProductName()
{
#ifdef OAM
    return "OpenAstroMount";
#elif defined(OAE)
    return "OpenAstroExplorer";
#else
    return "OpenAstroTracker";
#endif
}

namespace
{
meade::RaCoordinate raFrom(const DayTime &t)
{
    return meade::RaCoordinate {
        static_cast<uint8_t>(t.getHours()),
        static_cast<uint8_t>(t.getMinutes()),
        static_cast<uint8_t>(t.getSeconds()),
    };
}

Declination decToInternal(const meade::DecCoordinate &d)
{
    long seconds = labs(static_cast<long>(d.degrees)) * 3600L + d.minutes * 60L + d.seconds;
    return Declination::FromSeconds(d.degrees < 0 ? -seconds : seconds);
}

meade::DecCoordinate decFrom(const Declination &d)
{
    const long celestialSeconds = inNorthernHemisphere ? 90L * 3600L - labs(d.getTotalSeconds()) : -90L * 3600L + labs(d.getTotalSeconds());
    const long absoluteSeconds  = labs(celestialSeconds);
    int16_t degrees             = static_cast<int16_t>(absoluteSeconds / 3600L);
    if (celestialSeconds < 0)
    {
        degrees = -degrees;
    }

    return meade::DecCoordinate {
        degrees,
        static_cast<uint8_t>((absoluteSeconds / 60L) % 60L),
        static_cast<uint8_t>(absoluteSeconds % 60L),
    };
}
}  // namespace

meade::RaCoordinate MeadeCommandProcessor::onCurrentRa()
{
    return raFrom(_mount->currentRA());
}

meade::RaCoordinate MeadeCommandProcessor::onTargetRa()
{
    return raFrom(_mount->targetRA());
}

meade::DecCoordinate MeadeCommandProcessor::onCurrentDec()
{
    return decFrom(_mount->currentDEC());
}

meade::DecCoordinate MeadeCommandProcessor::onTargetDec()
{
    return decFrom(_mount->targetDEC());
}

const char *MeadeCommandProcessor::onMountStatus()
{
    _mountStatusScratch = _mount->getStatusString();
    return _mountStatusScratch.c_str();
}

bool MeadeCommandProcessor::onIsSlewing()
{
    return _mount->isSlewingRAorDEC();
}

bool MeadeCommandProcessor::onIsTracking()
{
    return _mount->isSlewingTRK();
}

bool MeadeCommandProcessor::onIsGuiding()
{
    return _mount->isGuiding();
}

meade::MeadeLatitude MeadeCommandProcessor::onSiteLatitude()
{
    const Latitude lat = _mount->latitude();
    return meade::MeadeLatitude {
        static_cast<int16_t>(lat.getHours()),
        static_cast<uint8_t>(lat.getMinutes()),
    };
}

meade::MeadeLongitude MeadeCommandProcessor::onSiteLongitude()
{
    const Longitude lon = _mount->longitude();
    long secs           = labs(static_cast<long>(lon.getTotalSeconds()));
    int16_t deg         = static_cast<int16_t>(secs / 3600L);
    uint8_t min         = static_cast<uint8_t>((secs / 60L) % 60L);
    if (lon.getTotalHours() > 0)
    {
        deg = -deg;
    }
    return meade::MeadeLongitude {
        deg,
        min,
    };
}

int MeadeCommandProcessor::onUtcOffset()
{
    return -_mount->getLocalUtcOffset();
}

meade::MeadeLocalTime MeadeCommandProcessor::onLocalTime()
{
    DayTime time = _mount->getLocalTime();
    return meade::MeadeLocalTime {
        static_cast<uint8_t>(time.getHours()),
        static_cast<uint8_t>(time.getMinutes()),
        static_cast<uint8_t>(time.getSeconds()),
    };
}

meade::MeadeLocalDate MeadeCommandProcessor::onLocalDate()
{
    ::LocalDate d = _mount->getLocalDate();
    return meade::MeadeLocalDate {
        static_cast<uint8_t>(d.month),
        static_cast<uint8_t>(d.day),
        static_cast<uint16_t>(d.year),
    };
}

meade::MeadeClockFormat MeadeCommandProcessor::onClockFormat()
{
    return meade::MeadeClockFormat::Hours24;
}

meade::MeadeTrackingRate MeadeCommandProcessor::onTrackingRate()
{
    return meade::MeadeTrackingRate::Sidereal;
}

const char *MeadeCommandProcessor::onSiteName(uint8_t index)
{
    snprintf(_siteNameScratch, sizeof(_siteNameScratch), "OAT%u", static_cast<unsigned>(index));
    return _siteNameScratch;
}

/////////////////////////////
// GPS CONTROL
/////////////////////////////
bool MeadeCommandProcessor::onStartGpsAcquisition(const char *timeoutPayload)
{
#if USE_GPS == 1
    unsigned long timeoutLen = 2UL * 60UL * 1000UL;
    if (timeoutPayload != nullptr && timeoutPayload[0] != '\0')
    {
        timeoutLen = String(timeoutPayload).toInt();
    }
    unsigned long timeoutTime = millis() + timeoutLen;
    int indicator             = 0;
    while (millis() < timeoutTime)
    {
        if (gpsAqcuisitionComplete(indicator))
        {
            LOG(DEBUG_MEADE, "[MEADE]: GPS startup, GPS acquired");
            return true;
        }
    }
#else
    (void) timeoutPayload;
#endif
    LOG(DEBUG_MEADE, "[MEADE]: GPS startup, no GPS signal");
    return false;
}

/////////////////////////////
// SYNC CONTROL
/////////////////////////////
void MeadeCommandProcessor::onSyncToTarget()
{
    _mount->syncPosition(_mount->targetRA(), _mount->targetDEC());
}

/////////////////////////////
// SET INFO
/////////////////////////////
bool MeadeCommandProcessor::onSetTargetDec(meade::DecCoordinate dec)
{
    _mount->targetDEC() = decToInternal(dec);
    LOG(DEBUG_MEADE, "[MEADE]: SetInfo: Received Target DEC: %s", _mount->targetDEC().ToString());
    return true;
}

bool MeadeCommandProcessor::onSetTargetRa(meade::RaCoordinate ra)
{
    _mount->targetRA().set(static_cast<int>(ra.hours), static_cast<int>(ra.minutes), static_cast<int>(ra.seconds));
    LOG(DEBUG_MEADE, "[MEADE]: SetInfo: Received Target RA: %s", _mount->targetRA().ToString());
    return true;
}

bool MeadeCommandProcessor::onSetLocalSiderealTime(meade::MeadeLocalTime lst)
{
    LOG(DEBUG_MEADE, "[MEADE]: SetInfo: Received LST: %u:%u:%u", lst.hours, lst.minutes, lst.seconds);
    _mount->setLST(DayTime(static_cast<int>(lst.hours), static_cast<int>(lst.minutes), static_cast<int>(lst.seconds)));
    return true;
}

bool MeadeCommandProcessor::onSetHomePoint()
{
    _mount->setHome(false);
    return true;
}

bool MeadeCommandProcessor::onSetHourAngle(uint8_t hours, uint8_t minutes)
{
    LOG(DEBUG_MEADE, "[MEADE]: SetInfo: Received HA: %u:%u:0", hours, minutes);
    _mount->setHA(DayTime(static_cast<int>(hours), static_cast<int>(minutes), 0));
    return true;
}

bool MeadeCommandProcessor::onSyncCoordinates(meade::DecCoordinate dec, meade::RaCoordinate ra)
{
    Declination decValue = decToInternal(dec);
    DayTime raValue(static_cast<int>(ra.hours), static_cast<int>(ra.minutes), static_cast<int>(ra.seconds));
    _mount->syncPosition(raValue, decValue);
    return true;
}

bool MeadeCommandProcessor::onSetSiteLatitude(meade::MeadeLatitude lat)
{
    _mount->setLatitude(Latitude(static_cast<int>(lat.degrees), static_cast<int>(lat.minutes), 0));
    return true;
}

bool MeadeCommandProcessor::onSetSiteLongitude(meade::MeadeLongitude lon)
{
    _mount->setLongitude(Longitude(-static_cast<int>(lon.degrees), static_cast<int>(lon.minutes), 0));
    return true;
}

bool MeadeCommandProcessor::onSetUtcOffset(int hours)
{
    // Wire value is the local-time offset from UTC; the mount stores the
    // inverse so that "local + offset = UTC".
    _mount->setLocalUtcOffset(-hours);
    return true;
}

bool MeadeCommandProcessor::onSetLocalTime(meade::MeadeLocalTime t)
{
    _mount->setLocalStartTime(DayTime(static_cast<int>(t.hours), static_cast<int>(t.minutes), static_cast<int>(t.seconds)));
    return true;
}

bool MeadeCommandProcessor::onSetLocalDate(meade::MeadeLocalDate d)
{
    _mount->setLocalStartDate(static_cast<int>(d.year), static_cast<int>(d.month), static_cast<int>(d.day));
    /*
    From https://www.astro.louisville.edu/software/xmtel/archive/xmtel-indi-6.0/xmtel-6.0l/support/lx200/CommandSet.html :
    SC: Calendar: If the date is valid 2 <string>s are returned, each string is 31 bytes long.
    The first is: "Updating planetary data#" followed by a second string of 30 spaces terminated by '#'
    */
    return true;
}

/////////////////////////////
// MOVEMENT
/////////////////////////////
/////////////////////////////
// HOME
/////////////////////////////
void MeadeCommandProcessor::onPark()
{
    _mount->park();
}

void MeadeCommandProcessor::onSlewToHome()
{
    _mount->startSlewingToHome();
}

void MeadeCommandProcessor::onUnpark()
{
    _mount->startSlewing(TRACKING);
}

void MeadeCommandProcessor::onSetAzAltHome()
{
    _mount->setAZALTHome();
}

void MeadeCommandProcessor::onSetSlewRate(uint8_t rate)
{
    _mount->setSlewRate(static_cast<int>(rate));
}

bool MeadeCommandProcessor::onIsSlewingRaOrDec()
{
    return _mount->isSlewingRAorDEC();
}

/////////////////////////////
// EXTRA COMMANDS
/////////////////////////////
// ---- IMeadeExtraHandlers overrides ----------------------------------------

void MeadeCommandProcessor::onFactoryReset()
{
    _mount->clearConfiguration();
}

void MeadeCommandProcessor::onDriftAlignment(int duration)
{
#if SUPPORT_DRIFT_ALIGNMENT == 1
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
#else
    (void) duration;
#endif
}

float MeadeCommandProcessor::onGetRaStepsPerDegree()
{
    return _mount->getStepsPerDegree(RA_STEPS);
}
float MeadeCommandProcessor::onGetDecStepsPerDegree()
{
    return _mount->getStepsPerDegree(DEC_STEPS);
}
float MeadeCommandProcessor::onGetAltStepsPerDegree()
{
    return _mount->getStepsPerDegree(ALTITUDE_STEPS);
}
float MeadeCommandProcessor::onGetAzStepsPerDegree()
{
    return _mount->getStepsPerDegree(AZIMUTH_STEPS);
}

oat::core::meade::ExtraDecLimits MeadeCommandProcessor::onGetDecLimits()
{
    oat::core::meade::ExtraDecLimits lim;
    _mount->getDecLimitPositions(lim.lo, lim.hi);
    return lim;
}

float MeadeCommandProcessor::onGetTrackingSpeedCalibration()
{
    return _mount->getSpeedCalibration();
}
float MeadeCommandProcessor::onGetRemainingSafeTime()
{
    return _mount->checkRALimit();
}
float MeadeCommandProcessor::onGetTrackingSpeed()
{
    return _mount->getSpeed(TRACKING);
}
int MeadeCommandProcessor::onGetBacklashSteps()
{
    return _mount->getBacklashCorrection();
}
const char *MeadeCommandProcessor::onGetAutoHomingStates()
{
    _mountStatusScratch = _mount->getAutoHomingStates();
    return _mountStatusScratch.c_str();
}

oat::core::meade::ExtraAzAltPositions MeadeCommandProcessor::onGetAzAltPositions()
{
    oat::core::meade::ExtraAzAltPositions p;
    _mount->getAZALTPositions(p.az, p.alt);
    return p;
}

oat::core::meade::ExtraStepperCoords MeadeCommandProcessor::onGetTargetCoordinatePositions(float raCoord, float decCoord)
{
    oat::core::meade::ExtraStepperCoords pos;
    _mount->calculateStepperPositions(raCoord, decCoord, pos.raPos, pos.decPos);
    return pos;
}

const char *MeadeCommandProcessor::onGetStepperInfo()
{
    _mountStatusScratch = _mount->getStepperInfo();
    return _mountStatusScratch.c_str();
}
const char *MeadeCommandProcessor::onGetMountHardwareInfo()
{
    _mountStatusScratch = _mount->getMountHardwareInfo();
    return _mountStatusScratch.c_str();
}
const char *MeadeCommandProcessor::onGetLogBuffer()
{
    _mountStatusScratch = getLogBuffer();
    return _mountStatusScratch.c_str();
}
long MeadeCommandProcessor::onGetRaHomingOffset()
{
    return _mount->getHomingOffset(StepperAxis::RA_STEPS);
}
long MeadeCommandProcessor::onGetDecHomingOffset()
{
    return _mount->getHomingOffset(StepperAxis::DEC_STEPS);
}
bool MeadeCommandProcessor::onGetHemisphere()
{
    return inNorthernHemisphere;
}

oat::core::meade::ExtraHms MeadeCommandProcessor::onGetHourAngle()
{
    DayTime ha = _mount->calculateHa();
    oat::core::meade::ExtraHms t;
    t.hours   = ha.getHours();
    t.minutes = ha.getMinutes();
    t.seconds = ha.getSeconds();
    return t;
}

oat::core::meade::ExtraHms MeadeCommandProcessor::onGetLocalSiderealTime()
{
    DayTime lst = _mount->calculateLst();
    oat::core::meade::ExtraHms t;
    t.hours   = lst.getHours();
    t.minutes = lst.getMinutes();
    t.seconds = lst.getSeconds();
    return t;
}

const char *MeadeCommandProcessor::onGetNetworkStatus()
{
#if (WIFI_ENABLED == 1)
    _mountStatusScratch = wifiControl.getStatus();
    return _mountStatusScratch.c_str();
#else
    return "0,";
#endif
}

void MeadeCommandProcessor::onSetRaStepsPerDegree(float v)
{
    _mount->setStepsPerDegree(RA_STEPS, v);
}
void MeadeCommandProcessor::onSetDecStepsPerDegree(float v)
{
    _mount->setStepsPerDegree(DEC_STEPS, v);
}
void MeadeCommandProcessor::onSetAzStepsPerDegree(float v)
{
    _mount->setStepsPerDegree(AZIMUTH_STEPS, v);
}
void MeadeCommandProcessor::onSetAltStepsPerDegree(float v)
{
    _mount->setStepsPerDegree(ALTITUDE_STEPS, v);
}

void MeadeCommandProcessor::onSetDecLimitLower(bool havePayload, float value)
{
    if (havePayload)
    {
        _mount->setDecLimitPosition(false, value);
    }
    else
    {
        _mount->setDecLimitPosition(false);
    }
}
void MeadeCommandProcessor::onSetDecLimitUpper(bool havePayload, float value)
{
    if (havePayload)
    {
        _mount->setDecLimitPosition(true, value);
    }
    else
    {
        _mount->setDecLimitPosition(true);
    }
}
void MeadeCommandProcessor::onClearDecLimitLower()
{
    _mount->clearDecLimitPosition(false);
}
void MeadeCommandProcessor::onClearDecLimitUpper()
{
    _mount->clearDecLimitPosition(true);
}
void MeadeCommandProcessor::onSetTrackingSpeedCalibration(float v)
{
    _mount->setSpeedCalibration(v, true);
}
void MeadeCommandProcessor::onSetTrackingStepperPosition(long v)
{
    _mount->setTrackingStepperPos(v);
}
void MeadeCommandProcessor::onSetManualSlewMode(bool enable)
{
    _mount->setManualSlewMode(enable);
}
void MeadeCommandProcessor::onSetRaManualSpeed(float v)
{
    _mount->setSpeed(RA_STEPS, v);
}
void MeadeCommandProcessor::onSetDecManualSpeed(float v)
{
    _mount->setSpeed(DEC_STEPS, v);
}
void MeadeCommandProcessor::onSetBacklashCorrection(int v)
{
    _mount->setBacklashCorrection(v);
}
void MeadeCommandProcessor::onSetRaHomingOffset(long v)
{
    _mount->setHomingOffset(StepperAxis::RA_STEPS, static_cast<int>(v));
}
void MeadeCommandProcessor::onSetDecHomingOffset(long v)
{
    _mount->setHomingOffset(StepperAxis::DEC_STEPS, static_cast<int>(v));
}

bool MeadeCommandProcessor::onLevelIsAvailable()
{
#if USE_GYRO_LEVEL == 1
    return true;
#else
    return false;
#endif
}

oat::core::meade::ExtraPitchRoll MeadeCommandProcessor::onLevelGetReferenceAngles()
{
    oat::core::meade::ExtraPitchRoll pr;
#if USE_GYRO_LEVEL == 1
    pr.pitch = _mount->getPitchCalibrationAngle();
    pr.roll  = _mount->getRollCalibrationAngle();
#endif
    return pr;
}

oat::core::meade::ExtraPitchRoll MeadeCommandProcessor::onLevelGetCurrentAngles()
{
    oat::core::meade::ExtraPitchRoll pr;
#if USE_GYRO_LEVEL == 1
    auto angles = Gyro::getCurrentAngles();
    pr.pitch    = angles.pitchAngle;
    pr.roll     = angles.rollAngle;
#endif
    return pr;
}

float MeadeCommandProcessor::onLevelGetTemperature()
{
#if USE_GYRO_LEVEL == 1
    return Gyro::getCurrentTemperature();
#else
    return 0.0f;
#endif
}

void MeadeCommandProcessor::onLevelSetReferencePitch(float v)
{
#if USE_GYRO_LEVEL == 1
    _mount->setPitchCalibrationAngle(v);
#else
    (void) v;
#endif
}
void MeadeCommandProcessor::onLevelSetReferenceRoll(float v)
{
#if USE_GYRO_LEVEL == 1
    _mount->setRollCalibrationAngle(v);
#else
    (void) v;
#endif
}
void MeadeCommandProcessor::onLevelStartup()
{
#if USE_GYRO_LEVEL == 1
    Gyro::startup();
#endif
}
void MeadeCommandProcessor::onLevelShutdown()
{
#if USE_GYRO_LEVEL == 1
    Gyro::shutdown();
#endif
}

/////////////////////////////
// QUIT
/////////////////////////////
void MeadeCommandProcessor::onStopAll()
{
    // :Q# stops all motors but remains in Control mode.
    _mount->stopSlewing(ALL_DIRECTIONS | TRACKING);
    _mount->stopSlewing(AZIMUTH_STEPS);
    _mount->stopSlewing(ALTITUDE_STEPS);
    _mount->stopSlewing(FOCUS_STEPS);
    _mount->waitUntilAllStopped();
}

void MeadeCommandProcessor::onStopDirectionalAll()
{
    _mount->stopSlewing(ALL_DIRECTIONS);
}

void MeadeCommandProcessor::onStopEast()
{
    _mount->stopSlewing(EAST);
}

void MeadeCommandProcessor::onStopWest()
{
    _mount->stopSlewing(WEST);
}

void MeadeCommandProcessor::onStopNorth()
{
    _mount->stopSlewing(NORTH);
}

void MeadeCommandProcessor::onStopSouth()
{
    _mount->stopSlewing(SOUTH);
}

void MeadeCommandProcessor::onQuitControlMode()
{
    // :Qq# does not stop motors, just leaves Control mode.
    inSerialControl = false;
    _lcdMenu->setCursor(0, 0);
    _lcdMenu->updateDisplay();
}

/////////////////////////////
// Set Slew Rates
/////////////////////////////
/////////////////////////////
// FOCUS COMMANDS
/////////////////////////////
void MeadeCommandProcessor::onFocusContinuousIn()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus focusContinuousMove IN");
    _mount->focusContinuousMove(FOCUS_BACKWARD);
#endif
}

void MeadeCommandProcessor::onFocusContinuousOut()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus focusContinuousMove OUT");
    _mount->focusContinuousMove(FOCUS_FORWARD);
#endif
}

void MeadeCommandProcessor::onFocusMoveBy(long steps)
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus move by %l steps", steps);
    _mount->focusMoveBy(steps);
#else
    (void) steps;
#endif
}

void MeadeCommandProcessor::onFocusSetSpeedByRate(int rate)
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus setSpeed %d", rate);
    _mount->focusSetSpeedByRate(rate);
#else
    (void) rate;
#endif
}

void MeadeCommandProcessor::onFocusStop()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus stop");
    _mount->focusStop();
#endif
}

long MeadeCommandProcessor::onFocusGetPosition()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus get stepperPosition");
    return _mount->focusGetStepperPosition();
#else
    return 0L;
#endif
}

bool MeadeCommandProcessor::onFocusIsAvailable()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    return true;
#else
    return false;
#endif
}

void MeadeCommandProcessor::onFocusSetPosition(long steps)
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus set stepperPosition %d", steps);
    _mount->focusSetStepperPosition(steps);
#else
    (void) steps;
#endif
}

bool MeadeCommandProcessor::onFocusGetState()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus isRunningFocus");
    return _mount->isRunningFocus();
#else
    return false;
#endif
}

// ---------------------------------------------------------------------------
// IMeadeMovementHandlers overrides — bridge the parser-side callbacks to
// `_mount`. Hardware-feature guards (AZ/ALT stepper presence, Hall-sensor
// auto-home) live here so the dispatcher stays hardware-agnostic.
// ---------------------------------------------------------------------------

namespace
{
byte toMountDirection(oat::core::meade::MoveDirection dir)
{
    switch (dir)
    {
        case oat::core::meade::MoveDirection::North:
            return NORTH;
        case oat::core::meade::MoveDirection::South:
            return SOUTH;
        case oat::core::meade::MoveDirection::West:
            return WEST;
        case oat::core::meade::MoveDirection::East:
        default:
            return EAST;
    }
}
}  // namespace

void MeadeCommandProcessor::onStartSlewToTarget()
{
    _mount->startSlewingToTarget();
}

void MeadeCommandProcessor::onTrackingOn()
{
    _mount->startSlewing(TRACKING);
}

void MeadeCommandProcessor::onTrackingOff()
{
    _mount->stopSlewing(TRACKING);
}

void MeadeCommandProcessor::onGuidePulse(oat::core::meade::MoveDirection dir, int durationMs)
{
    _mount->guidePulse(toMountDirection(dir), durationMs);
}

void MeadeCommandProcessor::onMoveAzAltHome()
{
    LOG(DEBUG_MEADE, "[MEADE]: Move AZ and ALT to home");
    _mount->moveAZALTToHome();
}

void MeadeCommandProcessor::onMoveAzimuth(float arcMinutes)
{
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Move AZ by %f arcmins", arcMinutes);
    _mount->moveBy(AZIMUTH_STEPS, arcMinutes);
#else
    (void) arcMinutes;
#endif
}

void MeadeCommandProcessor::onMoveAltitude(float arcMinutes)
{
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Move ALT by %f arcmins", arcMinutes);
    _mount->moveBy(ALTITUDE_STEPS, arcMinutes);
#else
    (void) arcMinutes;
#endif
}

void MeadeCommandProcessor::onSlewEast()
{
    _mount->startSlewing(EAST);
}

void MeadeCommandProcessor::onSlewWest()
{
    _mount->startSlewing(WEST);
}

void MeadeCommandProcessor::onSlewNorth()
{
    _mount->startSlewing(NORTH);
}

void MeadeCommandProcessor::onSlewSouth()
{
    _mount->startSlewing(SOUTH);
}

void MeadeCommandProcessor::onMoveStepper(oat::core::meade::MovementAxis axis, long steps)
{
    LOG(DEBUG_MEADE, "[MEADE]: MoveStepper: %l steps on axis %d", steps, static_cast<int>(axis));
    switch (axis)
    {
        case oat::core::meade::MovementAxis::Ra:
            _mount->moveStepperBy(RA_STEPS, steps);
            break;
        case oat::core::meade::MovementAxis::Dec:
            _mount->moveStepperBy(DEC_STEPS, steps);
            break;
        case oat::core::meade::MovementAxis::Azimuth:
            _mount->moveStepperBy(AZIMUTH_STEPS, steps);
            break;
        case oat::core::meade::MovementAxis::Altitude:
            _mount->moveStepperBy(ALTITUDE_STEPS, steps);
            break;
        case oat::core::meade::MovementAxis::Focus:
            _mount->moveStepperBy(FOCUS_STEPS, steps);
            break;
    }
}

bool MeadeCommandProcessor::onHomeRa(int direction, const char *distancePayload)
{
#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    int distance = RA_HOMING_SENSOR_SEARCH_DEGREES;
    if (distancePayload != nullptr && distancePayload[0] != '\0')
    {
        distance = clamp(static_cast<int>(strtol(distancePayload, nullptr, 10)), 5, 75);
        LOG(DEBUG_MEADE, "[MEADE]: RA AutoHome by %dh", distance);
    }
    return _mount->findHomeByHallSensor(StepperAxis::RA_STEPS, direction, distance);
#else
    (void) direction;
    (void) distancePayload;
    return false;
#endif
}

bool MeadeCommandProcessor::onHomeDec(int direction, const char *distancePayload)
{
#if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    int distance = DEC_HOMING_SENSOR_SEARCH_DEGREES;
    if (distancePayload != nullptr && distancePayload[0] != '\0')
    {
        distance = clamp(static_cast<int>(strtol(distancePayload, nullptr, 10)), 5, 75);
        LOG(DEBUG_MEADE, "[MEADE]: DEC AutoHome by %dh", distance);
    }
    return _mount->findHomeByHallSensor(StepperAxis::DEC_STEPS, direction, distance);
#else
    (void) direction;
    (void) distancePayload;
    return false;
#endif
}

const char *MeadeCommandProcessor::processCommand(String inCmd)
{
    LOG(DEBUG_MEADE, "[MEADE]: Received command   '%s'", inCmd.c_str());
    _mount->commandReceived();
    meade::dispatchMeadeCommand(_response, inCmd.c_str(), *this);
    return _response.c_str();
}
