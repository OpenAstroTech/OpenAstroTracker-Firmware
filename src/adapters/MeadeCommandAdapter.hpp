#pragma once

#include "core/MountEvent.hpp"
#include "core/meade/MeadeParser.hpp"
#include "core/meade/PendingReplies.hpp"
#include "ports/IClock.hpp"
#include "ports/IMountConfiguration.hpp"
#include "ports/IMountEvents.hpp"
#include "ports/IMountMotion.hpp"
#include "ports/IMountOptionalAxes.hpp"
#include "ports/IMountReadModel.hpp"
#include "ports/IMountSiteTime.hpp"
#include "ports/ITransport.hpp"

// Forward declarations
class LcdMenu;

/// Everything the command layer needs from the mount, and nothing else.
///
/// The adapter used to hold a `Mount *` and call 122 methods on it, which made the god object the
/// command layer's real interface: there was no way to drive a command from a test, and no place
/// that said which parts of the mount a host can actually reach. The ports below are that list.
/// `IMountConfiguration` extends `IMountMotion`, so the motion entry points come with it.
struct MeadeCommandPorts {
    ports::IMountConfiguration *configuration = nullptr;
    ports::IMountOptionalAxes *axes           = nullptr;
    ports::IMountReadModel *readModel         = nullptr;
    ports::IMountSiteTime *siteTime           = nullptr;
    ports::IMountEvents *events               = nullptr;
    /// The same clock the mount uses; the deferred-reply table measures deadlines on it.
    ports::IClock *clock = nullptr;
};

class MeadeCommandAdapter : private oat::core::meade::IMeadeHandlers, public ::core::IEventSink
{
  public:
    static MeadeCommandAdapter *createProcessor(const MeadeCommandPorts &ports, LcdMenu *lcdMenu);
    static MeadeCommandAdapter *instance();

    /// Runs one command and returns the bytes to send, or "" when there is nothing to send now.
    /// `origin` is the transport the command arrived on: a handler that defers its reply sends
    /// the answer there when the work finishes, so a reply started on Wi-Fi can never be written
    /// to the serial port that issued the next command. A caller that passes no transport (an
    /// internal command like the `:Qq#` the menu sends) gets no deferral.
    const char *processCommand(String inCmd, ports::ITransport *origin = nullptr);

    /// Sends every reply whose work finished or whose deadline passed. Called once per main-loop
    /// iteration; the event-driven half of this runs from `onMountEvent()`.
    void tick();

    /// Forgets the replies owed to a transport that has gone away. Their answers are not wrong,
    /// they are undeliverable.
    void transportGone(ports::ITransport *transport);

    /// The number of replies still owed. Diagnostics and tests.
    uint8_t pendingReplies() const;

    /// `core::IEventSink`: the deferred replies complete here.
    void onMountEvent(const ::core::MountEvent &event) override;

    ~MeadeCommandAdapter() override;

  private:
    MeadeCommandAdapter(const MeadeCommandPorts &ports, LcdMenu *lcdMenu);

    /// Hands the reply to the table instead of writing it inline. The handler's own return value
    /// is then ignored, and `processCommand()` reports "no bytes yet" rather than an empty answer.
    /// A full table leaves the reply pending and lets the handler answer inline: a host waiting on
    /// a reply that will never come is worse than an immediate, conventional answer.
    void deferReply(oat::core::meade::ReplyKind kind, ::core::MountEventType completesOn, uint32_t timeoutMs);

    /// Formats and writes one deferred reply.
    void sendPendingReply(const oat::core::meade::PendingReply &reply, bool succeeded);

    // Persist a freshly-built response across the handler return.
    // The returned pointer is valid until the next call to `store`.
    const char *store(oat::core::meade::MeadeResponse &response);

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

    // The mount, as four ports instead of one pointer (PLAN.md Phase 5 step 3). No `Mount *`
    // member: every call site below goes through one of these, which is what makes the command
    // layer's dependency on the mount visible in one place.
    ports::IMountConfiguration *_config    = nullptr;
    ports::IMountMotion *_motion           = nullptr;
    ports::IMountOptionalAxes *_axes       = nullptr;
    ports::IMountReadModel *_read          = nullptr;
    ports::IMountSiteTime *_siteTime       = nullptr;
    ports::IMountEvents *_events           = nullptr;
    ports::IClock *_clock                  = nullptr;
    // The transport the current command arrived on; null for commands the firmware issues itself.
    ports::ITransport *_origin       = nullptr;
    bool _deferredThisCommand        = false;
    oat::core::meade::PendingReplies<> _pending;
    LcdMenu *_lcdMenu;
    static MeadeCommandAdapter *_instance;
    oat::core::meade::MeadeResponse _response;

    // Storage backing the pointer-returning Get callbacks (firmware/product
    // names use string literals; mount status / site name use these buffers).
    String _mountStatusScratch;
    char _siteNameScratch[8];
};
