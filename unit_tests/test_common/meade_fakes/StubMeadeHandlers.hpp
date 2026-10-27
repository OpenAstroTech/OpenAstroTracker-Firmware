#pragma once

// A complete, no-op `IMeadeHandlers` for tests that drive the whole command pipeline.
//
// Every family interface has to be implemented before a test can dispatch anything, which is
// 115 methods of boilerplate that no individual test wants to read. This stub answers every
// callback with a neutral value and records the last one that fired, so a pipeline test can
// assert *that the command reached the right handler* as well as what went back on the wire.
// Parameters are unnamed because the stub ignores them all.
//
// It is deliberately not `final` and its callbacks are deliberately not `final`: a test that
// cares about one callback derives from this and overrides it.

#include "adapters/meade/MeadeParser.hpp"

namespace fakes
{
namespace meade = adapters::meade;
using namespace meade;  // the handler vocabulary (RaCoordinate, MoveDirection, ...)

class StubMeadeHandlers : public adapters::meade::IMeadeHandlers
{
  public:
    /// Name of the last callback that fired, or null before the first command.
    const char *lastCall = nullptr;

    /// Set by a handler that handed its reply to the pending table instead of answering inline.
    /// The adapter keeps the same flag: a handler cannot reach the response object, so the
    /// deferral is state the caller consults, not a return value.
    bool deferred = false;

    /// Values the string-returning callbacks answer with. Recognisable on purpose: a test that
    /// sees one of these knows the value came from the stub and not from a leftover buffer.
    const char *firmware = "V1.2.3";
    const char *product  = "OpenAstroTracker";
    const char *text     = "Stub";

    const char *onFirmwareVersion() override
    {
        lastCall = "onFirmwareVersion";
        return firmware;
    }

    const char *onProductName() override
    {
        lastCall = "onProductName";
        return product;
    }

    RaCoordinate onCurrentRa() override
    {
        lastCall = "onCurrentRa";
        return {};
    }

    RaCoordinate onTargetRa() override
    {
        lastCall = "onTargetRa";
        return {};
    }

    DecCoordinate onCurrentDec() override
    {
        lastCall = "onCurrentDec";
        return {};
    }

    DecCoordinate onTargetDec() override
    {
        lastCall = "onTargetDec";
        return {};
    }

    const char *onMountStatus() override
    {
        lastCall = "onMountStatus";
        return text;
    }

    bool onIsSlewing() override
    {
        lastCall = "onIsSlewing";
        return false;
    }

    bool onIsTracking() override
    {
        lastCall = "onIsTracking";
        return false;
    }

    bool onIsGuiding() override
    {
        lastCall = "onIsGuiding";
        return false;
    }

    MeadeLatitude onSiteLatitude() override
    {
        lastCall = "onSiteLatitude";
        return {};
    }

    MeadeLongitude onSiteLongitude() override
    {
        lastCall = "onSiteLongitude";
        return {};
    }

    int onUtcOffset() override
    {
        lastCall = "onUtcOffset";
        return 0;
    }

    MeadeLocalTime onLocalTime() override
    {
        lastCall = "onLocalTime";
        return {};
    }

    MeadeLocalDate onLocalDate() override
    {
        lastCall = "onLocalDate";
        return {};
    }

    MeadeClockFormat onClockFormat() override
    {
        lastCall = "onClockFormat";
        return MeadeClockFormat::Hours24;
    }

    MeadeTrackingRate onTrackingRate() override
    {
        lastCall = "onTrackingRate";
        return MeadeTrackingRate::Sidereal;
    }

    const char *onSiteName(uint8_t) override
    {
        lastCall = "onSiteName";
        return "";
    }

    bool onSetTargetDec(DecCoordinate) override
    {
        lastCall = "onSetTargetDec";
        return false;
    }

    bool onSetTargetRa(RaCoordinate) override
    {
        lastCall = "onSetTargetRa";
        return false;
    }

    bool onSetLocalSiderealTime(MeadeLocalTime) override
    {
        lastCall = "onSetLocalSiderealTime";
        return false;
    }

    bool onSetHomePoint() override
    {
        lastCall = "onSetHomePoint";
        return false;
    }

    bool onSetHourAngle(uint8_t, uint8_t) override
    {
        lastCall = "onSetHourAngle";
        return false;
    }

    bool onSyncCoordinates(DecCoordinate, RaCoordinate) override
    {
        lastCall = "onSyncCoordinates";
        return false;
    }

    bool onSetSiteLatitude(MeadeLatitude) override
    {
        lastCall = "onSetSiteLatitude";
        return false;
    }

    bool onSetSiteLongitude(MeadeLongitude) override
    {
        lastCall = "onSetSiteLongitude";
        return false;
    }

    bool onSetUtcOffset(int) override
    {
        lastCall = "onSetUtcOffset";
        return false;
    }

    bool onSetLocalTime(MeadeLocalTime) override
    {
        lastCall = "onSetLocalTime";
        return false;
    }

    bool onSetLocalDate(MeadeLocalDate) override
    {
        lastCall = "onSetLocalDate";
        return false;
    }

    void onStopAll() override
    {
        lastCall = "onStopAll";
    }

    void onStopDirectionalAll() override
    {
        lastCall = "onStopDirectionalAll";
    }

    void onStopEast() override
    {
        lastCall = "onStopEast";
    }

    void onStopWest() override
    {
        lastCall = "onStopWest";
    }

    void onStopNorth() override
    {
        lastCall = "onStopNorth";
    }

    void onStopSouth() override
    {
        lastCall = "onStopSouth";
    }

    void onQuitControlMode() override
    {
        lastCall = "onQuitControlMode";
    }

    bool onIsSlewingRaOrDec() override
    {
        lastCall = "onIsSlewingRaOrDec";
        return false;
    }

    void onEnterSerialControl() override
    {
        lastCall = "onEnterSerialControl";
    }

    void onSyncToTarget() override
    {
        lastCall = "onSyncToTarget";
    }

    void onPark() override
    {
        lastCall = "onPark";
    }

    void onSlewToHome() override
    {
        lastCall = "onSlewToHome";
    }

    void onUnpark() override
    {
        lastCall = "onUnpark";
    }

    void onSetAzAltHome() override
    {
        lastCall = "onSetAzAltHome";
    }

    void onSetSlewRate(uint8_t) override
    {
        lastCall = "onSetSlewRate";
    }

    bool onStartGpsAcquisition(const char *) override
    {
        lastCall = "onStartGpsAcquisition";
        return false;
    }

    void onFocusContinuousIn() override
    {
        lastCall = "onFocusContinuousIn";
    }

    void onFocusContinuousOut() override
    {
        lastCall = "onFocusContinuousOut";
    }

    void onFocusMoveBy(long) override
    {
        lastCall = "onFocusMoveBy";
    }

    void onFocusSetSpeedByRate(int) override
    {
        lastCall = "onFocusSetSpeedByRate";
    }

    void onFocusStop() override
    {
        lastCall = "onFocusStop";
    }

    long onFocusGetPosition() override
    {
        lastCall = "onFocusGetPosition";
        return 0;
    }

    bool onFocusIsAvailable() override
    {
        lastCall = "onFocusIsAvailable";
        return false;
    }

    void onFocusSetPosition(long) override
    {
        lastCall = "onFocusSetPosition";
    }

    bool onFocusGetState() override
    {
        lastCall = "onFocusGetState";
        return false;
    }

    void onStartSlewToTarget() override
    {
        lastCall = "onStartSlewToTarget";
    }

    void onTrackingOn() override
    {
        lastCall = "onTrackingOn";
    }

    void onTrackingOff() override
    {
        lastCall = "onTrackingOff";
    }

    void onGuidePulse(MoveDirection, int) override
    {
        lastCall = "onGuidePulse";
    }

    void onMoveAzAltHome() override
    {
        lastCall = "onMoveAzAltHome";
    }

    void onMoveAzimuth(float) override
    {
        lastCall = "onMoveAzimuth";
    }

    void onMoveAltitude(float) override
    {
        lastCall = "onMoveAltitude";
    }

    void onSlewEast() override
    {
        lastCall = "onSlewEast";
    }

    void onSlewWest() override
    {
        lastCall = "onSlewWest";
    }

    void onSlewNorth() override
    {
        lastCall = "onSlewNorth";
    }

    void onSlewSouth() override
    {
        lastCall = "onSlewSouth";
    }

    void onMoveStepper(MovementAxis, long) override
    {
        lastCall = "onMoveStepper";
    }

    bool onHomeRa(int, const char *) override
    {
        lastCall = "onHomeRa";
        return false;
    }

    bool onHomeDec(int, const char *) override
    {
        lastCall = "onHomeDec";
        return false;
    }

    void onFactoryReset() override
    {
        lastCall = "onFactoryReset";
    }

    void onDriftAlignment(int) override
    {
        lastCall = "onDriftAlignment";
    }

    float onGetRaStepsPerDegree() override
    {
        lastCall = "onGetRaStepsPerDegree";
        return 0.0f;
    }

    float onGetDecStepsPerDegree() override
    {
        lastCall = "onGetDecStepsPerDegree";
        return 0.0f;
    }

    float onGetAltStepsPerDegree() override
    {
        lastCall = "onGetAltStepsPerDegree";
        return 0.0f;
    }

    float onGetAzStepsPerDegree() override
    {
        lastCall = "onGetAzStepsPerDegree";
        return 0.0f;
    }

    ExtraDecLimits onGetDecLimits() override
    {
        lastCall = "onGetDecLimits";
        return {};
    }

    float onGetTrackingSpeedCalibration() override
    {
        lastCall = "onGetTrackingSpeedCalibration";
        return 0.0f;
    }

    float onGetRemainingSafeTime() override
    {
        lastCall = "onGetRemainingSafeTime";
        return 0.0f;
    }

    float onGetTrackingSpeed() override
    {
        lastCall = "onGetTrackingSpeed";
        return 0.0f;
    }

    int onGetBacklashSteps() override
    {
        lastCall = "onGetBacklashSteps";
        return 0;
    }

    const char *onGetAutoHomingStates() override
    {
        lastCall = "onGetAutoHomingStates";
        return text;
    }

    ExtraAzAltPositions onGetAzAltPositions() override
    {
        lastCall = "onGetAzAltPositions";
        return {};
    }

    ExtraStepperCoords onGetTargetCoordinatePositions(float, float) override
    {
        lastCall = "onGetTargetCoordinatePositions";
        return {};
    }

    const char *onGetStepperInfo() override
    {
        lastCall = "onGetStepperInfo";
        return text;
    }

    const char *onGetMountHardwareInfo() override
    {
        lastCall = "onGetMountHardwareInfo";
        return text;
    }

    const char *onGetLogBuffer() override
    {
        lastCall = "onGetLogBuffer";
        return text;
    }

    long onGetRaHomingOffset() override
    {
        lastCall = "onGetRaHomingOffset";
        return 0;
    }

    long onGetDecHomingOffset() override
    {
        lastCall = "onGetDecHomingOffset";
        return 0;
    }

    bool onGetHemisphere() override
    {
        lastCall = "onGetHemisphere";
        return false;
    }

    ExtraHms onGetHourAngle() override
    {
        lastCall = "onGetHourAngle";
        return {};
    }

    ExtraHms onGetLocalSiderealTime() override
    {
        lastCall = "onGetLocalSiderealTime";
        return {};
    }

    const char *onGetNetworkStatus() override
    {
        lastCall = "onGetNetworkStatus";
        return text;
    }

    void onSetRaStepsPerDegree(float) override
    {
        lastCall = "onSetRaStepsPerDegree";
    }

    void onSetDecStepsPerDegree(float) override
    {
        lastCall = "onSetDecStepsPerDegree";
    }

    void onSetAzStepsPerDegree(float) override
    {
        lastCall = "onSetAzStepsPerDegree";
    }

    void onSetAltStepsPerDegree(float) override
    {
        lastCall = "onSetAltStepsPerDegree";
    }

    void onSetDecLimitLower(bool, float) override
    {
        lastCall = "onSetDecLimitLower";
    }

    void onSetDecLimitUpper(bool, float) override
    {
        lastCall = "onSetDecLimitUpper";
    }

    void onClearDecLimitLower() override
    {
        lastCall = "onClearDecLimitLower";
    }

    void onClearDecLimitUpper() override
    {
        lastCall = "onClearDecLimitUpper";
    }

    void onSetTrackingSpeedCalibration(float) override
    {
        lastCall = "onSetTrackingSpeedCalibration";
    }

    void onSetTrackingStepperPosition(long) override
    {
        lastCall = "onSetTrackingStepperPosition";
    }

    void onSetManualSlewMode(bool) override
    {
        lastCall = "onSetManualSlewMode";
    }

    void onSetRaManualSpeed(float) override
    {
        lastCall = "onSetRaManualSpeed";
    }

    void onSetDecManualSpeed(float) override
    {
        lastCall = "onSetDecManualSpeed";
    }

    void onSetBacklashCorrection(int) override
    {
        lastCall = "onSetBacklashCorrection";
    }

    void onSetRaHomingOffset(long) override
    {
        lastCall = "onSetRaHomingOffset";
    }

    void onSetDecHomingOffset(long) override
    {
        lastCall = "onSetDecHomingOffset";
    }

    bool onLevelIsAvailable() override
    {
        lastCall = "onLevelIsAvailable";
        return false;
    }

    ExtraPitchRoll onLevelGetReferenceAngles() override
    {
        lastCall = "onLevelGetReferenceAngles";
        return {};
    }

    ExtraPitchRoll onLevelGetCurrentAngles() override
    {
        lastCall = "onLevelGetCurrentAngles";
        return {};
    }

    float onLevelGetTemperature() override
    {
        lastCall = "onLevelGetTemperature";
        return 0.0f;
    }

    void onLevelSetReferencePitch(float) override
    {
        lastCall = "onLevelSetReferencePitch";
    }

    void onLevelSetReferenceRoll(float) override
    {
        lastCall = "onLevelSetReferenceRoll";
    }

    void onLevelStartup() override
    {
        lastCall = "onLevelStartup";
    }

    void onLevelShutdown() override
    {
        lastCall = "onLevelShutdown";
    }
};

}  // namespace fakes
