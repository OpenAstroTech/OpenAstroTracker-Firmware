#pragma once

#include "core/MeadeParser.hpp"
#include "core/MeadeResponse.hpp"

// Forward declarations
class Mount;
class LcdMenu;

class MeadeCommandProcessor : private oat::core::meade::IMeadeGetHandlers,
                              private oat::core::meade::IMeadeSetHandlers,
                              private oat::core::meade::IMeadeQuitHandlers,
                              private oat::core::meade::IMeadeDistanceHandlers,
                              private oat::core::meade::IMeadeInitHandlers,
                              private oat::core::meade::IMeadeSyncControlHandlers,
                              private oat::core::meade::IMeadeHomeHandlers,
                              private oat::core::meade::IMeadeSlewRateHandlers,
                              private oat::core::meade::IMeadeGpsHandlers,
                              private oat::core::meade::IMeadeFocusHandlers,
                              private oat::core::meade::IMeadeMovementHandlers,
                              private oat::core::meade::IMeadeExtraHandlers
{
  public:
    static MeadeCommandProcessor *createProcessor(Mount *mount, LcdMenu *lcdMenu);
    static MeadeCommandProcessor *instance();
    const char *processCommand(String inCmd);

  private:
    MeadeCommandProcessor(Mount *mount, LcdMenu *lcdMenu);

    // Persist a freshly-built response across the handler return.
    // The returned pointer is valid until the next call to `store`.
    const char *store(oat::core::meade::MeadeResponse response);
    const char *handleMeadeSetInfo(const String &inCmd);
    const char *handleMeadeMovement(const String &inCmd);
    const char *handleMeadeGetInfo(const String &inCmd);
    const char *handleMeadeGPSCommands(const String &inCmd);
    const char *handleMeadeSyncControl(const String &inCmd);
    const char *handleMeadeHome(const String &inCmd);
    const char *handleMeadeInit(const String &inCmd);
    const char *handleMeadeQuit(const String &inCmd);
    const char *handleMeadeDistance(const String &inCmd);
    const char *handleMeadeSetSlewRate(const String &inCmd);
    const char *handleMeadeExtraCommands(const String &inCmd);
    const char *handleMeadeFocusCommands(const String &inCmd);

    // IMeadeGetHandlers overrides. Each method returns a typed domain value;
    // the parser layer handles all Meade wire formatting.
    const char *onFirmwareVersion() override;
    const char *onProductName() override;
    oat::core::meade::RaCoordinate onCurrentRa() override;
    oat::core::meade::RaCoordinate onTargetRa() override;
    oat::core::meade::DecCoordinate onCurrentDec() override;
    oat::core::meade::DecCoordinate onTargetDec() override;
    const char *onMountStatus() override;
    bool onIsSlewing() override;
    bool onIsTracking() override;
    bool onIsGuiding() override;
    oat::core::meade::MeadeLatitude onSiteLatitude() override;
    oat::core::meade::MeadeLongitude onSiteLongitude() override;
    int onUtcOffset() override;
    oat::core::meade::MeadeLocalTime onLocalTime() override;
    oat::core::meade::MeadeLocalDate onLocalDate() override;
    oat::core::meade::MeadeClockFormat onClockFormat() override;
    oat::core::meade::MeadeTrackingRate onTrackingRate() override;
    const char *onSiteName(uint8_t index) override;

    // IMeadeSetHandlers overrides. Each method receives a parser-validated
    // typed value and returns whether the mount accepted it.
    bool onSetTargetDec(oat::core::meade::DecCoordinate dec) override;
    bool onSetTargetRa(oat::core::meade::RaCoordinate ra) override;
    bool onSetLocalSiderealTime(oat::core::meade::MeadeLocalTime lst) override;
    bool onSetHomePoint() override;
    bool onSetHourAngle(uint8_t hours, uint8_t minutes) override;
    bool onSyncCoordinates(oat::core::meade::DecCoordinate dec, oat::core::meade::RaCoordinate ra) override;
    bool onSetSiteLatitude(oat::core::meade::MeadeLatitude lat) override;
    bool onSetSiteLongitude(oat::core::meade::MeadeLongitude lon) override;
    bool onSetUtcOffset(int hours) override;
    bool onSetLocalTime(oat::core::meade::MeadeLocalTime t) override;
    bool onSetLocalDate(oat::core::meade::MeadeLocalDate d) override;

    // IMeadeQuitHandlers overrides. All callbacks are side-effect only; the
    // dispatcher emits an empty wire response regardless.
    void onStopAll() override;
    void onStopDirectionalAll() override;
    void onStopEast() override;
    void onStopWest() override;
    void onStopNorth() override;
    void onStopSouth() override;
    void onQuitControlMode() override;

    // IMeadeDistanceHandlers overrides.
    bool onIsSlewingRaOrDec() override;

    // IMeadeInitHandlers overrides.
    void onEnterSerialControl() override;

    // IMeadeSyncControlHandlers overrides.
    void onSyncToTarget() override;

    // IMeadeHomeHandlers overrides.
    void onPark() override;
    void onSlewToHome() override;
    void onUnpark() override;
    void onSetAzAltHome() override;

    // IMeadeSlewRateHandlers overrides.
    void onSetSlewRate(uint8_t rate) override;

    // IMeadeGpsHandlers overrides.
    bool onStartGpsAcquisition(const char *timeoutPayload) override;

    // IMeadeFocusHandlers overrides.
    void onFocusContinuousIn() override;
    void onFocusContinuousOut() override;
    void onFocusMoveBy(long steps) override;
    void onFocusSetSpeedByRate(int rate) override;
    void onFocusStop() override;
    long onFocusGetPosition() override;
    bool onFocusIsAvailable() override;
    void onFocusSetPosition(long steps) override;
    bool onFocusGetState() override;

    // IMeadeMovementHandlers overrides.
    void onStartSlewToTarget() override;
    void onTrackingOn() override;
    void onTrackingOff() override;
    void onGuidePulse(oat::core::meade::MoveDirection dir, int durationMs) override;
    void onMoveAzAltHome() override;
    void onMoveAzimuth(float arcMinutes) override;
    void onMoveAltitude(float arcMinutes) override;
    void onSlewEast() override;
    void onSlewWest() override;
    void onSlewNorth() override;
    void onSlewSouth() override;
    void onMoveStepper(oat::core::meade::MovementAxis axis, long steps) override;
    bool onHomeRa(int direction, const char *distancePayload) override;
    bool onHomeDec(int direction, const char *distancePayload) override;

    // IMeadeExtraHandlers overrides.
    void onFactoryReset() override;
    void onDriftAlignment(int duration) override;
    float onGetRaStepsPerDegree() override;
    float onGetDecStepsPerDegree() override;
    float onGetAltStepsPerDegree() override;
    float onGetAzStepsPerDegree() override;
    oat::core::meade::ExtraDecLimits onGetDecLimits() override;
    float onGetTrackingSpeedCalibration() override;
    float onGetRemainingSafeTime() override;
    float onGetTrackingSpeed() override;
    int onGetBacklashSteps() override;
    const char *onGetAutoHomingStates() override;
    oat::core::meade::ExtraAzAltPositions onGetAzAltPositions() override;
    oat::core::meade::ExtraStepperCoords onGetTargetCoordinatePositions(float raCoord, float decCoord) override;
    const char *onGetStepperInfo() override;
    const char *onGetMountHardwareInfo() override;
    const char *onGetLogBuffer() override;
    long onGetRaHomingOffset() override;
    long onGetDecHomingOffset() override;
    bool onGetHemisphere() override;
    oat::core::meade::ExtraHms onGetHourAngle() override;
    oat::core::meade::ExtraHms onGetLocalSiderealTime() override;
    const char *onGetNetworkStatus() override;
    void onSetRaStepsPerDegree(float v) override;
    void onSetDecStepsPerDegree(float v) override;
    void onSetAzStepsPerDegree(float v) override;
    void onSetAltStepsPerDegree(float v) override;
    void onSetDecLimitLower(bool havePayload, float value) override;
    void onSetDecLimitUpper(bool havePayload, float value) override;
    void onClearDecLimitLower() override;
    void onClearDecLimitUpper() override;
    void onSetTrackingSpeedCalibration(float v) override;
    void onSetTrackingStepperPosition(long v) override;
    void onSetManualSlewMode(bool enable) override;
    void onSetRaManualSpeed(float v) override;
    void onSetDecManualSpeed(float v) override;
    void onSetBacklashCorrection(int v) override;
    void onSetRaHomingOffset(long v) override;
    void onSetDecHomingOffset(long v) override;
    bool onLevelIsAvailable() override;
    oat::core::meade::ExtraPitchRoll onLevelGetReferenceAngles() override;
    oat::core::meade::ExtraPitchRoll onLevelGetCurrentAngles() override;
    float onLevelGetTemperature() override;
    void onLevelSetReferencePitch(float v) override;
    void onLevelSetReferenceRoll(float v) override;
    void onLevelStartup() override;
    void onLevelShutdown() override;

    Mount *_mount;
    LcdMenu *_lcdMenu;
    static MeadeCommandProcessor *_instance;
    oat::core::meade::MeadeResponse _response;

    // Storage backing the pointer-returning Get callbacks (firmware/product
    // names use string literals; mount status / site name use these buffers).
    String _mountStatusScratch;
    char _siteNameScratch[8];
};
