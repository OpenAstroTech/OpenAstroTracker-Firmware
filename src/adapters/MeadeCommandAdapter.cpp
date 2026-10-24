#include "../inc/Globals.hpp"
#include "../Configuration.hpp"
#include "../Utility.hpp"
#include "../LcdMenu.hpp"
#include "MeadeCommandAdapter.hpp"
#include "WifiControl.hpp"
#include "../DayTime.hpp"
#include "../Declination.hpp"
#include "../Latitude.hpp"
#include "../Longitude.hpp"
#include "core/FocusController.hpp"
#include "core/meade/MeadeParser.hpp"

#include <stdarg.h>
#include <string.h>

#if USE_GPS == 1
bool gpsAqcuisitionComplete(int &indicator);  // defined in c72_menuHA_GPS.hpp
#endif

namespace meade = oat::core::meade;

MeadeCommandAdapter *MeadeCommandAdapter::_instance = nullptr;

const char *MeadeCommandAdapter::store(oat::core::meade::MeadeResponse &response)
{
    _response = response;
    return _response.c_str();
}

/////////////////////////////
// Create the processor
/////////////////////////////
MeadeCommandAdapter *MeadeCommandAdapter::createProcessor(const MeadeCommandPorts &ports, LcdMenu *lcdMenu)
{
    _instance = new MeadeCommandAdapter(ports, lcdMenu);
    return _instance;
}

/////////////////////////////
// Get the singleton
/////////////////////////////
MeadeCommandAdapter *MeadeCommandAdapter::instance()
{
    return _instance;
}

/////////////////////////////
// Constructor
/////////////////////////////
MeadeCommandAdapter::MeadeCommandAdapter(const MeadeCommandPorts &ports, LcdMenu *lcdMenu)
{
    _config   = ports.configuration;
    _motion   = ports.configuration;  // IMountConfiguration extends IMountMotion
    _axes     = ports.axes;
    _read     = ports.readModel;
    _siteTime = ports.siteTime;
    _events   = ports.events;
    _clock    = ports.clock;
    _gyro     = ports.gyro;

    // Deferred replies complete on mount events, so the command layer is a subscriber like the
    // status display.
    if (_events != nullptr)
    {
        _events->subscribe(this);
    }

    // In case of DISPLAY_TYPE_NONE mode, the lcdMenu is just an empty shell class to save having to null check everywhere
    _lcdMenu = lcdMenu;
}

MeadeCommandAdapter::~MeadeCommandAdapter()
{
    if (_events != nullptr)
    {
        _events->unsubscribe(this);
    }
}

void MeadeCommandAdapter::deferReply(oat::core::meade::ReplyKind kind, ::core::MountEventType completesOn, uint32_t timeoutMs)
{
    if (_origin == nullptr)
    {
        return;  // nobody to answer: an internally issued command answers inline
    }

    const uint32_t deadline = _clock->now() + timeoutMs;
    if (_pending.add(_origin, kind, completesOn, deadline) == 0)
    {
        LOG(DEBUG_MEADE, "[MEADE]: Deferred reply table full, answering inline");
        return;
    }

    _deferredThisCommand = true;
    LOG(DEBUG_MEADE, "[MEADE]: Reply deferred, %lu ms budget", static_cast<unsigned long>(timeoutMs));
}

void MeadeCommandAdapter::sendPendingReply(const oat::core::meade::PendingReply &reply, bool succeeded)
{
    if (reply.out == nullptr)
    {
        return;
    }

    switch (reply.kind)
    {
        case oat::core::meade::ReplyKind::SuccessFlag:
            reply.out->writeResponse(succeeded ? "1" : "0");
            break;

        case oat::core::meade::ReplyKind::Silent:
            break;  // the slot existed to hold the transport and the deadline, not to say anything
    }
}

void MeadeCommandAdapter::onMountEvent(const ::core::MountEvent &event)
{
    oat::core::meade::PendingReply reply;
    while (_pending.takeCompleted(event.type, reply))
    {
        // A homing failure is the one event that reports its own outcome; the rest completing at
        // all is the success.
        const bool succeeded = (event.type == ::core::MountEventType::HomingFinished) ? event.succeeded : true;
        LOG(DEBUG_MEADE, "[MEADE]: Deferred reply %u due (%s)", static_cast<unsigned>(reply.correlation), succeeded ? "ok" : "failed");
        sendPendingReply(reply, succeeded);
    }
}

void MeadeCommandAdapter::tick()
{
    if (_clock == nullptr)
    {
        return;
    }

    oat::core::meade::PendingReply reply;
    while (_pending.takeExpired(_clock->now(), reply))
    {
        LOG(DEBUG_MEADE, "[MEADE]: Deferred reply %u timed out", static_cast<unsigned>(reply.correlation));
        sendPendingReply(reply, false);
    }
}

void MeadeCommandAdapter::transportGone(ports::ITransport *transport)
{
    _pending.clearFor(transport);
}

uint8_t MeadeCommandAdapter::pendingReplies() const
{
    return _pending.size();
}

/////////////////////////////
// INIT
/////////////////////////////
void MeadeCommandAdapter::onEnterSerialControl()
{
    inSerialControl = true;
    _lcdMenu->setCursor(0, 0);
    _lcdMenu->printMenu(F("Remote control"));
    _lcdMenu->setCursor(0, 1);
    _lcdMenu->printMenu(F(">SELECT to quit"));
}

/////////////////////////////
// GET INFO
/////////////////////////////
// ---- IMeadeGetHandlers callbacks ---------------------------------------

const char *MeadeCommandAdapter::onFirmwareVersion()
{
    return VERSION;
}

const char *MeadeCommandAdapter::onProductName()
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
meade::RaCoordinate raFrom(const core::DayTime &t)
{
    return meade::RaCoordinate {
        static_cast<uint8_t>(t.getHours()),
        static_cast<uint8_t>(t.getMinutes()),
        static_cast<uint8_t>(t.getSeconds()),
    };
}

meade::DecCoordinate decFrom(const core::Declination &d)
{
    // The mount stores DEC as an axis coordinate (0 = pole above the mount);
    // Meade clients expect celestial declination. Apply the hemisphere
    // correction before splitting into components.
    //
    // The split folds the sign into `deg`, where anything between 0 and -1
    // degrees comes back as +0. Read the sign off the undivided total.
    const long celestialSeconds = core::Declination::axisToCelestialSeconds(d.getTotalSeconds(), inNorthernHemisphere);
    int deg, min, sec;
    core::DayTime::splitSeconds(celestialSeconds, deg, min, sec);
    return meade::DecCoordinate {
        static_cast<uint16_t>(deg < 0 ? -deg : deg),
        static_cast<uint8_t>(min),
        static_cast<uint8_t>(sec),
        celestialSeconds < 0,
    };
}

Declination decFromWire(meade::DecCoordinate const &d)
{
    // The parser keeps sign and magnitude apart, and they stay apart all the way
    // into the join. Flattening `negative` back into a signed degrees component
    // here would lose it again for "-00*30:00", which is the whole point of the
    // separate flag. celestialSecondsFrom is the declination counterpart of
    // siteSecondsFrom below.
    return Declination::fromCelestialSeconds(core::Declination::celestialSecondsFrom(d.degrees, d.minutes, d.seconds, d.negative));
}

// Signed arc-seconds for a magnitude/sign pair. The Latitude and Longitude
// constructors take signed degrees, which cannot express a site between 0 and
// -1 degree, so callers add this total to a zeroed coordinate instead.
long siteSecondsFrom(uint16_t degrees, uint8_t minutes, bool negative)
{
    const long seconds = ((static_cast<long>(degrees) * 60L) + minutes) * 60L;
    return negative ? -seconds : seconds;
}
}  // namespace

meade::RaCoordinate MeadeCommandAdapter::onCurrentRa()
{
    return raFrom(_read->currentRA());
}

meade::RaCoordinate MeadeCommandAdapter::onTargetRa()
{
    return raFrom(_read->targetRA());
}

meade::DecCoordinate MeadeCommandAdapter::onCurrentDec()
{
    return decFrom(_read->currentDEC());
}

meade::DecCoordinate MeadeCommandAdapter::onTargetDec()
{
    return decFrom(_read->targetDEC());
}

const char *MeadeCommandAdapter::onMountStatus()
{
    return _read->getStatusString();
}

bool MeadeCommandAdapter::onIsSlewing()
{
    return _motion->isSlewingRAorDEC();
}

bool MeadeCommandAdapter::onIsTracking()
{
    return _motion->isSlewingTRK();
}

bool MeadeCommandAdapter::onIsGuiding()
{
    return _read->isGuiding();
}

meade::MeadeLatitude MeadeCommandAdapter::onSiteLatitude()
{
    const core::Latitude lat = _siteTime->latitude();
    // getHours() folds the sign into the degrees component, so a site between
    // 0 and -1 degrees reports as +0. Read the sign off the total instead.
    const int degrees = lat.getHours();
    return meade::MeadeLatitude {
        static_cast<uint16_t>(degrees < 0 ? -degrees : degrees),
        static_cast<uint8_t>(lat.getMinutes()),
        lat.getTotalSeconds() < 0,
    };
}

meade::MeadeLongitude MeadeCommandAdapter::onSiteLongitude()
{
    const core::Longitude lon = _siteTime->longitude();
    const int degrees         = lon.getHours();
    return meade::MeadeLongitude {
        static_cast<uint16_t>(degrees < 0 ? -degrees : degrees),
        static_cast<uint8_t>(lon.getMinutes()),
        lon.getTotalSeconds() < 0,
    };
}

int MeadeCommandAdapter::onUtcOffset()
{
    return -_siteTime->getLocalUtcOffset();
}

meade::MeadeLocalTime MeadeCommandAdapter::onLocalTime()
{
    core::DayTime time = _siteTime->getLocalTime();
    return meade::MeadeLocalTime {
        static_cast<uint8_t>(time.getHours()),
        static_cast<uint8_t>(time.getMinutes()),
        static_cast<uint8_t>(time.getSeconds()),
    };
}

meade::MeadeLocalDate MeadeCommandAdapter::onLocalDate()
{
    LocalDate d = _siteTime->getLocalDate();
    return meade::MeadeLocalDate {
        static_cast<uint8_t>(d.month),
        static_cast<uint8_t>(d.day),
        static_cast<uint16_t>(d.year),
    };
}

meade::MeadeClockFormat MeadeCommandAdapter::onClockFormat()
{
    return meade::MeadeClockFormat::Hours24;
}

meade::MeadeTrackingRate MeadeCommandAdapter::onTrackingRate()
{
    return meade::MeadeTrackingRate::Sidereal;
}

const char *MeadeCommandAdapter::onSiteName(uint8_t index)
{
    snprintf(_siteNameScratch, sizeof(_siteNameScratch), "OAT%u", static_cast<unsigned>(index));
    return _siteNameScratch;
}

/////////////////////////////
// GPS CONTROL
/////////////////////////////
bool MeadeCommandAdapter::onStartGpsAcquisition(const char *timeoutPayload)
{
#if USE_GPS == 1
    // The requested timeout is honoured as a *budget for this call*, not as something to spin on.
    // It used to busy-wait for up to two minutes inside the handler, with the main loop and the
    // whole command layer stopped — no `:D#`, no `:Q#`, nothing. The protocol already defines the
    // answer for "not yet": this returns 0 and the host asks again, which is what
    // `MeadeProtocol.hpp` documents for `:gTnnn#` ("by using a low timeout, you can avoid long
    // pauses and let the user know that it's not ready yet").
    unsigned long budgetMs = 2UL * 60UL * 1000UL;
    if (timeoutPayload != nullptr && timeoutPayload[0] != '\0')
    {
        budgetMs = String(timeoutPayload).toInt();
    }

    // One probe per call, plus a short bounded wait so a fix that is one second away still
    // answers in the same exchange. The cap keeps the handler from ever owning the loop.
    constexpr unsigned long kMaxWaitPerCall = 250UL;
    const unsigned long deadline            = millis() + ((budgetMs < kMaxWaitPerCall) ? budgetMs : kMaxWaitPerCall);

    int indicator = 0;
    do
    {
        if (gpsAqcuisitionComplete(indicator))
        {
            LOG(DEBUG_MEADE, "[MEADE]: GPS startup, GPS acquired");
            return true;
        }
        delay(10);
    } while (millis() < deadline);
#else
    (void) timeoutPayload;
#endif
    LOG(DEBUG_MEADE, "[MEADE]: GPS startup, no GPS signal (yet)");
    return false;
}

/////////////////////////////
// SYNC CONTROL
/////////////////////////////
void MeadeCommandAdapter::onSyncToTarget()
{
    _motion->syncPosition(_read->targetRA(), _read->targetDEC());
}

/////////////////////////////
// SET INFO
/////////////////////////////
bool MeadeCommandAdapter::onSetTargetDec(meade::DecCoordinate dec)
{
    _read->targetDEC() = decFromWire(dec);
    LOG(DEBUG_MEADE, "[MEADE]: SetInfo: Received Target DEC: %s", Declination(_read->targetDEC()).ToString());
    return true;
}

bool MeadeCommandAdapter::onSetTargetRa(meade::RaCoordinate ra)
{
    _read->targetRA().set(static_cast<int>(ra.hours), static_cast<int>(ra.minutes), static_cast<int>(ra.seconds));
    LOG(DEBUG_MEADE, "[MEADE]: SetInfo: Received Target RA: %s", DayTime(_read->targetRA()).ToString());
    return true;
}

bool MeadeCommandAdapter::onSetLocalSiderealTime(meade::MeadeLocalTime lst)
{
    LOG(DEBUG_MEADE, "[MEADE]: SetInfo: Received LST: %u:%u:%u", lst.hours, lst.minutes, lst.seconds);
    _siteTime->setLST(core::DayTime(static_cast<int>(lst.hours), static_cast<int>(lst.minutes), static_cast<int>(lst.seconds)));
    return true;
}

bool MeadeCommandAdapter::onSetHomePoint()
{
    _motion->setHome(false);
    return true;
}

bool MeadeCommandAdapter::onSetHourAngle(uint8_t hours, uint8_t minutes)
{
    LOG(DEBUG_MEADE, "[MEADE]: SetInfo: Received HA: %u:%u:0", hours, minutes);
    _siteTime->setHA(core::DayTime(static_cast<int>(hours), static_cast<int>(minutes), 0));
    return true;
}

bool MeadeCommandAdapter::onSyncCoordinates(meade::DecCoordinate dec, meade::RaCoordinate ra)
{
    Declination decValue = decFromWire(dec);
    DayTime raValue(static_cast<int>(ra.hours), static_cast<int>(ra.minutes), static_cast<int>(ra.seconds));
    _motion->syncPosition(raValue, decValue);
    return true;
}

bool MeadeCommandAdapter::onSetSiteLatitude(meade::MeadeLatitude lat)
{
    Latitude value;
    value.addSeconds(siteSecondsFrom(lat.degrees, lat.minutes, lat.negative));
    _siteTime->setLatitude(value);
    return true;
}

bool MeadeCommandAdapter::onSetSiteLongitude(meade::MeadeLongitude lon)
{
    Longitude value;
    value.addSeconds(siteSecondsFrom(lon.degrees, lon.minutes, lon.negative));
    _siteTime->setLongitude(value);
    return true;
}

bool MeadeCommandAdapter::onSetUtcOffset(int hours)
{
    // Wire value is the local-time offset from UTC; the mount stores the
    // inverse so that "local + offset = UTC".
    _siteTime->setLocalUtcOffset(-hours);
    return true;
}

bool MeadeCommandAdapter::onSetLocalTime(meade::MeadeLocalTime t)
{
    _siteTime->setLocalStartTime(core::DayTime(static_cast<int>(t.hours), static_cast<int>(t.minutes), static_cast<int>(t.seconds)));
    return true;
}

bool MeadeCommandAdapter::onSetLocalDate(meade::MeadeLocalDate d)
{
    _siteTime->setLocalStartDate(static_cast<int>(d.year), static_cast<int>(d.month), static_cast<int>(d.day));
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
void MeadeCommandAdapter::onPark()
{
    _motion->park();
}

void MeadeCommandAdapter::onSlewToHome()
{
    _motion->startSlewingToHome();
}

void MeadeCommandAdapter::onUnpark()
{
    _motion->startSlewing(TRACKING);
}

void MeadeCommandAdapter::onSetAzAltHome()
{
    _axes->setAZALTHome();
}

void MeadeCommandAdapter::onSetSlewRate(uint8_t rate)
{
    _config->setSlewRate(static_cast<int>(rate));
}

bool MeadeCommandAdapter::onIsSlewingRaOrDec()
{
    return _motion->isSlewingRAorDEC();
}

/////////////////////////////
// EXTRA COMMANDS
/////////////////////////////
// ---- IMeadeExtraHandlers overrides ----------------------------------------

void MeadeCommandAdapter::onFactoryReset()
{
    _config->clearConfiguration();
}

void MeadeCommandAdapter::onDriftAlignment(int duration)
{
#if SUPPORT_DRIFT_ALIGNMENT == 1
    // The sequence used to run here, synchronously: stop, three 1.5 s delays and three blocking
    // passes, i.e. ~90 s in which the mount answered nothing because the main loop was inside this
    // handler. It is a phase machine in core/DriftAlignmentController now, ticked from `app::MountExecutor::loop()`.
    _lcdMenu->setCursor(0, 0);
    _lcdMenu->printMenu(F(">Drift Alignment"));
    _motion->startDriftAlignment(duration);
#else
    (void) duration;
#endif
}

float MeadeCommandAdapter::onGetRaStepsPerDegree()
{
    return _config->getStepsPerDegree(RA_STEPS);
}
float MeadeCommandAdapter::onGetDecStepsPerDegree()
{
    return _config->getStepsPerDegree(DEC_STEPS);
}
float MeadeCommandAdapter::onGetAltStepsPerDegree()
{
    return _config->getStepsPerDegree(ALTITUDE_STEPS);
}
float MeadeCommandAdapter::onGetAzStepsPerDegree()
{
    return _config->getStepsPerDegree(AZIMUTH_STEPS);
}

oat::core::meade::ExtraDecLimits MeadeCommandAdapter::onGetDecLimits()
{
    oat::core::meade::ExtraDecLimits lim;
    _config->getDecLimitPositions(lim.lo, lim.hi);
    return lim;
}

float MeadeCommandAdapter::onGetTrackingSpeedCalibration()
{
    return _config->getSpeedCalibration();
}
float MeadeCommandAdapter::onGetRemainingSafeTime()
{
    return _read->checkRALimit();
}
float MeadeCommandAdapter::onGetTrackingSpeed()
{
    return _config->getSpeed(TRACKING);
}
int MeadeCommandAdapter::onGetBacklashSteps()
{
    return _config->getBacklashCorrection();
}
const char *MeadeCommandAdapter::onGetAutoHomingStates()
{
    return _read->getAutoHomingStates();
}

oat::core::meade::ExtraAzAltPositions MeadeCommandAdapter::onGetAzAltPositions()
{
    oat::core::meade::ExtraAzAltPositions p;
    _axes->getAZALTPositions(p.az, p.alt);
    return p;
}

oat::core::meade::ExtraStepperCoords MeadeCommandAdapter::onGetTargetCoordinatePositions(float raCoord, float decCoord)
{
    oat::core::meade::ExtraStepperCoords pos;
    _read->calculateStepperPositions(raCoord, decCoord, pos.raPos, pos.decPos);
    return pos;
}

const char *MeadeCommandAdapter::onGetStepperInfo()
{
    return _read->getStepperInfo();
}
const char *MeadeCommandAdapter::onGetMountHardwareInfo()
{
    return _read->getMountHardwareInfo();
}
const char *MeadeCommandAdapter::onGetLogBuffer()
{
    _mountStatusScratch = getLogBuffer();
    return _mountStatusScratch.c_str();
}
long MeadeCommandAdapter::onGetRaHomingOffset()
{
    return _config->getHomingOffset(StepperAxis::RA_STEPS);
}
long MeadeCommandAdapter::onGetDecHomingOffset()
{
    return _config->getHomingOffset(StepperAxis::DEC_STEPS);
}
bool MeadeCommandAdapter::onGetHemisphere()
{
    return inNorthernHemisphere;
}

oat::core::meade::ExtraHms MeadeCommandAdapter::onGetHourAngle()
{
    core::DayTime ha = _read->calculateHa();
    oat::core::meade::ExtraHms t;
    t.hours   = ha.getHours();
    t.minutes = ha.getMinutes();
    t.seconds = ha.getSeconds();
    return t;
}

oat::core::meade::ExtraHms MeadeCommandAdapter::onGetLocalSiderealTime()
{
    core::DayTime lst = _read->calculateLst();
    oat::core::meade::ExtraHms t;
    t.hours   = lst.getHours();
    t.minutes = lst.getMinutes();
    t.seconds = lst.getSeconds();
    return t;
}

const char *MeadeCommandAdapter::onGetNetworkStatus()
{
#if (WIFI_ENABLED == 1)
    _mountStatusScratch = wifiControl.getStatus();
    return _mountStatusScratch.c_str();
#else
    return "0,";
#endif
}

void MeadeCommandAdapter::onSetRaStepsPerDegree(float v)
{
    _config->setStepsPerDegree(RA_STEPS, v);
}
void MeadeCommandAdapter::onSetDecStepsPerDegree(float v)
{
    _config->setStepsPerDegree(DEC_STEPS, v);
}
void MeadeCommandAdapter::onSetAzStepsPerDegree(float v)
{
    _config->setStepsPerDegree(AZIMUTH_STEPS, v);
}
void MeadeCommandAdapter::onSetAltStepsPerDegree(float v)
{
    _config->setStepsPerDegree(ALTITUDE_STEPS, v);
}

void MeadeCommandAdapter::onSetDecLimitLower(bool havePayload, float value)
{
    if (havePayload)
    {
        _config->setDecLimitPosition(false, value);
    }
    else
    {
        _config->setDecLimitPosition(false);
    }
}
void MeadeCommandAdapter::onSetDecLimitUpper(bool havePayload, float value)
{
    if (havePayload)
    {
        _config->setDecLimitPosition(true, value);
    }
    else
    {
        _config->setDecLimitPosition(true);
    }
}
void MeadeCommandAdapter::onClearDecLimitLower()
{
    _config->clearDecLimitPosition(false);
}
void MeadeCommandAdapter::onClearDecLimitUpper()
{
    _config->clearDecLimitPosition(true);
}
void MeadeCommandAdapter::onSetTrackingSpeedCalibration(float v)
{
    _config->setSpeedCalibration(v, true);
}
void MeadeCommandAdapter::onSetTrackingStepperPosition(long v)
{
    _motion->setTrackingStepperPos(v);
}
void MeadeCommandAdapter::onSetManualSlewMode(bool enable)
{
    _config->setManualSlewMode(enable);
}
void MeadeCommandAdapter::onSetRaManualSpeed(float v)
{
    _config->setSpeed(RA_STEPS, v);
}
void MeadeCommandAdapter::onSetDecManualSpeed(float v)
{
    _config->setSpeed(DEC_STEPS, v);
}
void MeadeCommandAdapter::onSetBacklashCorrection(int v)
{
    _config->setBacklashCorrection(v);
}
void MeadeCommandAdapter::onSetRaHomingOffset(long v)
{
    _config->setHomingOffset(StepperAxis::RA_STEPS, static_cast<int>(v));
}
void MeadeCommandAdapter::onSetDecHomingOffset(long v)
{
    _config->setHomingOffset(StepperAxis::DEC_STEPS, static_cast<int>(v));
}

bool MeadeCommandAdapter::onLevelIsAvailable()
{
#if USE_GYRO_LEVEL == 1
    return true;
#else
    return false;
#endif
}

oat::core::meade::ExtraPitchRoll MeadeCommandAdapter::onLevelGetReferenceAngles()
{
    oat::core::meade::ExtraPitchRoll pr;
#if USE_GYRO_LEVEL == 1
    pr.pitch = _config->getPitchCalibrationAngle();
    pr.roll  = _config->getRollCalibrationAngle();
#endif
    return pr;
}

oat::core::meade::ExtraPitchRoll MeadeCommandAdapter::onLevelGetCurrentAngles()
{
    oat::core::meade::ExtraPitchRoll pr;
#if USE_GYRO_LEVEL == 1
    const ports::GyroAngles angles = _gyro->angles();
    pr.pitch                       = angles.pitch;
    pr.roll                        = angles.roll;
#endif
    return pr;
}

float MeadeCommandAdapter::onLevelGetTemperature()
{
#if USE_GYRO_LEVEL == 1
    return _gyro->temperature();
#else
    return 0.0f;
#endif
}

void MeadeCommandAdapter::onLevelSetReferencePitch(float v)
{
#if USE_GYRO_LEVEL == 1
    _config->setPitchCalibrationAngle(v);
#else
    (void) v;
#endif
}
void MeadeCommandAdapter::onLevelSetReferenceRoll(float v)
{
#if USE_GYRO_LEVEL == 1
    _config->setRollCalibrationAngle(v);
#else
    (void) v;
#endif
}
void MeadeCommandAdapter::onLevelStartup()
{
#if USE_GYRO_LEVEL == 1
    _gyro->begin();
#endif
}
void MeadeCommandAdapter::onLevelShutdown()
{
#if USE_GYRO_LEVEL == 1
    _gyro->shutdown();
#endif
}

/////////////////////////////
// QUIT
/////////////////////////////
void MeadeCommandAdapter::onStopAll()
{
    // :Q# stops all motors but remains in Control mode.
    //
    // It used to block here until every axis had finished decelerating. That made an abort take
    // as long as the slowest ramp, with the main loop inside `waitUntilAllStopped()` and no
    // telemetry answered — a host that sends :Q# and then polls :D# was waiting for a reply the
    // mount could not give until it stopped. The axes decelerate on their own now and :D#
    // answers from MountState.
    _motion->stopSlewing(ALL_DIRECTIONS | TRACKING);
    _motion->stopSlewing(AZIMUTH_STEPS);
    _motion->stopSlewing(ALTITUDE_STEPS);
    _motion->stopSlewing(FOCUS_STEPS);
}

void MeadeCommandAdapter::onStopDirectionalAll()
{
    _motion->stopSlewing(ALL_DIRECTIONS);
}

void MeadeCommandAdapter::onStopEast()
{
    _motion->stopSlewing(EAST);
}

void MeadeCommandAdapter::onStopWest()
{
    _motion->stopSlewing(WEST);
}

void MeadeCommandAdapter::onStopNorth()
{
    _motion->stopSlewing(NORTH);
}

void MeadeCommandAdapter::onStopSouth()
{
    _motion->stopSlewing(SOUTH);
}

void MeadeCommandAdapter::onQuitControlMode()
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
void MeadeCommandAdapter::onFocusContinuousIn()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus focusContinuousMove IN");
    _axes->focusContinuousMove(core::FOCUS_BACKWARD);
#endif
}

void MeadeCommandAdapter::onFocusContinuousOut()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus focusContinuousMove OUT");
    _axes->focusContinuousMove(core::FOCUS_FORWARD);
#endif
}

void MeadeCommandAdapter::onFocusMoveBy(long steps)
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus move by %l steps", steps);
    _axes->focusMoveBy(steps);
#else
    (void) steps;
#endif
}

void MeadeCommandAdapter::onFocusSetSpeedByRate(int rate)
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus setSpeed %d", rate);
    _axes->focusSetSpeedByRate(rate);
#else
    (void) rate;
#endif
}

void MeadeCommandAdapter::onFocusStop()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus stop");
    _axes->focusStop();
#endif
}

long MeadeCommandAdapter::onFocusGetPosition()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus get stepperPosition");
    return _axes->focusGetStepperPosition();
#else
    return 0L;
#endif
}

bool MeadeCommandAdapter::onFocusIsAvailable()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    return true;
#else
    return false;
#endif
}

void MeadeCommandAdapter::onFocusSetPosition(long steps)
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus set stepperPosition %d", steps);
    _axes->focusSetStepperPosition(steps);
#else
    (void) steps;
#endif
}

bool MeadeCommandAdapter::onFocusGetState()
{
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Focus isRunningFocus");
    return _axes->isRunningFocus();
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

void MeadeCommandAdapter::onStartSlewToTarget()
{
    _motion->startSlewingToTarget();
}

void MeadeCommandAdapter::onTrackingOn()
{
    _motion->startSlewing(TRACKING);
}

void MeadeCommandAdapter::onTrackingOff()
{
    _motion->stopSlewing(TRACKING);
}

void MeadeCommandAdapter::onGuidePulse(oat::core::meade::MoveDirection dir, int durationMs)
{
    _motion->guidePulse(toMountDirection(dir), durationMs);
}

void MeadeCommandAdapter::onMoveAzAltHome()
{
    LOG(DEBUG_MEADE, "[MEADE]: Move AZ and ALT to home");
    _axes->moveAZALTToHome();
}

void MeadeCommandAdapter::onMoveAzimuth(float arcMinutes)
{
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Move AZ by %f arcmins", arcMinutes);
    _axes->moveBy(AZIMUTH_STEPS, arcMinutes);
#else
    (void) arcMinutes;
#endif
}

void MeadeCommandAdapter::onMoveAltitude(float arcMinutes)
{
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_MEADE, "[MEADE]: Move ALT by %f arcmins", arcMinutes);
    _axes->moveBy(ALTITUDE_STEPS, arcMinutes);
#else
    (void) arcMinutes;
#endif
}

void MeadeCommandAdapter::onSlewEast()
{
    _motion->startSlewing(EAST);
}

void MeadeCommandAdapter::onSlewWest()
{
    _motion->startSlewing(WEST);
}

void MeadeCommandAdapter::onSlewNorth()
{
    _motion->startSlewing(NORTH);
}

void MeadeCommandAdapter::onSlewSouth()
{
    _motion->startSlewing(SOUTH);
}

void MeadeCommandAdapter::onMoveStepper(oat::core::meade::MovementAxis axis, long steps)
{
    LOG(DEBUG_MEADE, "[MEADE]: MoveStepper: %l steps on axis %d", steps, static_cast<int>(axis));
    switch (axis)
    {
        case oat::core::meade::MovementAxis::Ra:
            _motion->moveStepperBy(RA_STEPS, steps);
            break;
        case oat::core::meade::MovementAxis::Dec:
            _motion->moveStepperBy(DEC_STEPS, steps);
            break;
        case oat::core::meade::MovementAxis::Azimuth:
            _motion->moveStepperBy(AZIMUTH_STEPS, steps);
            break;
        case oat::core::meade::MovementAxis::Altitude:
            _motion->moveStepperBy(ALTITUDE_STEPS, steps);
            break;
        case oat::core::meade::MovementAxis::Focus:
            _motion->moveStepperBy(FOCUS_STEPS, steps);
            break;
    }
}

bool MeadeCommandAdapter::onHomeRa(int direction, const char *distancePayload)
{
#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    int distance = RA_HOMING_SENSOR_SEARCH_DEGREES;
    if (distancePayload != nullptr && distancePayload[0] != '\0')
    {
        distance = clamp(static_cast<int>(strtol(distancePayload, nullptr, 10)), 5, 75);
        LOG(DEBUG_MEADE, "[MEADE]: RA AutoHome by %dh", distance);
    }
    return _motion->findHomeByHallSensor(StepperAxis::RA_STEPS, direction, distance);
#else
    (void) direction;
    (void) distancePayload;
    return false;
#endif
}

bool MeadeCommandAdapter::onHomeDec(int direction, const char *distancePayload)
{
#if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    int distance = DEC_HOMING_SENSOR_SEARCH_DEGREES;
    if (distancePayload != nullptr && distancePayload[0] != '\0')
    {
        distance = clamp(static_cast<int>(strtol(distancePayload, nullptr, 10)), 5, 75);
        LOG(DEBUG_MEADE, "[MEADE]: DEC AutoHome by %dh", distance);
    }
    return _motion->findHomeByHallSensor(StepperAxis::DEC_STEPS, direction, distance);
#else
    (void) direction;
    (void) distancePayload;
    return false;
#endif
}

const char *MeadeCommandAdapter::processCommand(String inCmd, ports::ITransport *origin)
{
    LOG(DEBUG_MEADE, "[MEADE]: Received command   '%s'", inCmd.c_str());
    _read->commandReceived();
    _origin              = origin;
    _deferredThisCommand = false;
    _response.clearDeferral();  // the response object is reused between commands
    meade::dispatchMeadeCommand(_response, inCmd.c_str(), *this);

    if (_deferredThisCommand || _response.deferred())
    {
        // The handler took ownership of the reply: it will write the bytes through the transport
        // it arrived on when the work finishes. Returning an empty string here is not the same as
        // "no reply" -- the transports treat both as nothing to send, but the deferral is what
        // tells the caller a reply is still owed.
        LOG(DEBUG_MEADE, "[MEADE]: Deferred reply owed for '%s'", inCmd.c_str());
        return "";
    }

    return _response.c_str();
}
