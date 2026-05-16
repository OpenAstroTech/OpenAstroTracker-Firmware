#pragma once

#include <string>

namespace oat
{
namespace core
{

enum class MeadeCommandKind
{
    Unknown,
    Set,
    Move,
    Get,
    Gps,
    Sync,
    Home,
    Init,
    Quit,
    SlewRate,
    Distance,
    Extra,
    Focus,
};

enum class MeadeCommandDispatchTarget
{
    Unknown,
    SetInfo,
    Movement,
    GetInfo,
    GpsCommands,
    SyncControl,
    Home,
    Init,
    Quit,
    SetSlewRate,
    Distance,
    ExtraCommands,
    FocusCommands,
};

struct MeadeParseResult {
    bool valid                                = false;
    MeadeCommandKind kind                     = MeadeCommandKind::Unknown;
    MeadeCommandDispatchTarget dispatchTarget = MeadeCommandDispatchTarget::Unknown;
    std::string payload;
};

enum class MeadeExtraCommandKind
{
    Unknown,
    DriftAlignment,
    Get,
    Set,
    Level,
    FactoryReset,
};

struct MeadeExtraParseResult {
    bool valid                 = false;
    MeadeExtraCommandKind kind = MeadeExtraCommandKind::Unknown;
    std::string payload;
};

enum class MeadeExtraLeafCommandKind
{
    Unknown,
    GetRaStepsPerDegree,
    GetDecStepsPerDegree,
    GetDecLimitBoth,
    GetDecLimitLowerOnly,
    GetDecLimitUpperOnly,
    GetDecLimitInvalidVariant,
    GetDecParking,
    GetTrackingSpeedCalibration,
    GetRemainingSafeTime,
    GetTrackingSpeed,
    GetBacklashSteps,
    GetAltStepsPerDegree,
    GetAzStepsPerDegree,
    GetAutoHomingStates,
    GetAzAltPositions,
    GetTargetCoordinatePositions,
    GetMountHardwareInfo,
    GetStepperInfo,
    GetLogBuffer,
    GetHourAngle,
    GetHourAngleInvalidVariant,
    GetRaHomingOffset,
    GetDecHomingOffset,
    GetHemisphere,
    GetLocalSiderealTime,
    GetNetworkStatus,
    SetRaStepsPerDegree,
    SetAzStepsPerDegree,
    SetAltStepsPerDegree,
    SetDecStepsPerDegree,
    SetDecLimitLowerSet,
    SetDecLimitUpperSet,
    SetDecLimitLowerClear,
    SetDecLimitUpperClear,
    SetDecParking,
    SetTrackingSpeedCalibration,
    SetTrackingStepperPosition,
    SetManualSlewMode,
    SetRaManualSpeed,
    SetDecManualSpeed,
    SetBacklashCorrection,
    SetRaHomingOffset,
    SetDecHomingOffset,
    LevelGetReferenceAngles,
    LevelGetCurrentAngles,
    LevelGetTemperature,
    LevelGetInvalidVariant,
    LevelSetReferencePitch,
    LevelSetReferenceRoll,
    LevelSetInvalidVariant,
    LevelStartup,
    LevelShutdown,
    LevelUnknownVariant,
};

struct MeadeExtraLeafParseResult {
    bool valid                     = false;
    MeadeExtraLeafCommandKind kind = MeadeExtraLeafCommandKind::Unknown;
    std::string payload;
};

enum class MeadeGetCommandKind
{
    Unknown,
    FirmwareVersion,
    ProductName,
    TargetRa,
    TargetDec,
    CurrentRa,
    CurrentDec,
    MountStatus,
    IsSlewing,
    IsTracking,
    IsGuiding,
    SiteLatitude,
    SiteLongitude,
    ClockFormat,
    UtcOffset,
    LocalTime12h,
    LocalTime24h,
    LocalDate,
    SiteName1,
    SiteName2,
    SiteName3,
    SiteName4,
    TrackingRate,
};

struct MeadeGetParseResult {
    bool valid               = false;
    MeadeGetCommandKind kind = MeadeGetCommandKind::Unknown;
    std::string payload;
};

enum class MeadeGpsCommandKind
{
    Unknown,
    StartAcquisition,
};

struct MeadeGpsParseResult {
    bool valid               = false;
    MeadeGpsCommandKind kind = MeadeGpsCommandKind::Unknown;
    std::string payload;
};

enum class MeadeSetCommandKind
{
    Unknown,
    TargetDec,
    TargetRa,
    LocalSiderealTime,
    HomePoint,
    HourAngle,
    SyncCoordinates,
    SiteLatitude,
    SiteLongitude,
    UtcOffset,
    LocalTime,
    LocalDate,
};

struct MeadeSetParseResult {
    bool valid               = false;
    MeadeSetCommandKind kind = MeadeSetCommandKind::Unknown;
    std::string payload;
};

enum class MeadeSyncCommandKind
{
    Unknown,
    SyncToTarget,
};

struct MeadeSyncParseResult {
    bool valid                = false;
    MeadeSyncCommandKind kind = MeadeSyncCommandKind::Unknown;
    std::string payload;
};

enum class MeadeMovementCommandKind
{
    Unknown,
    SlewToTarget,
    TrackingToggle,
    GuidePulse,
    MoveAzAltHome,
    MoveAzimuth,
    MoveAltitude,
    SlewEast,
    SlewWest,
    SlewNorth,
    SlewSouth,
    MoveStepper,
    HomeRa,
    HomeDec,
};

struct MeadeMovementParseResult {
    bool valid                    = false;
    MeadeMovementCommandKind kind = MeadeMovementCommandKind::Unknown;
    std::string payload;
};

enum class MeadeHomeCommandKind
{
    Unknown,
    Park,
    Home,
    Unpark,
    SetAzAltHome,
};

struct MeadeHomeParseResult {
    bool valid                = false;
    MeadeHomeCommandKind kind = MeadeHomeCommandKind::Unknown;
    std::string payload;
};

enum class MeadeQuitCommandKind
{
    Unknown,
    StopAll,
    StopDirectionalAll,
    StopEast,
    StopWest,
    StopNorth,
    StopSouth,
    QuitControlMode,
};

struct MeadeQuitParseResult {
    bool valid                = false;
    MeadeQuitCommandKind kind = MeadeQuitCommandKind::Unknown;
    std::string payload;
};

enum class MeadeSlewRateCommandKind
{
    Unknown,
    Slew,
    Find,
    Center,
    Guide,
};

struct MeadeSlewRateParseResult {
    bool valid                    = false;
    MeadeSlewRateCommandKind kind = MeadeSlewRateCommandKind::Unknown;
    std::string payload;
};

enum class MeadeFocusCommandKind
{
    Unknown,
    ContinuousIn,
    ContinuousOut,
    MoveBy,
    SetSpeedByRate,
    SetFastestRate,
    SetSlowestRate,
    GetPosition,
    SetPosition,
    GetState,
    Stop,
};

struct MeadeFocusParseResult {
    bool valid                 = false;
    MeadeFocusCommandKind kind = MeadeFocusCommandKind::Unknown;
    std::string payload;
};

MeadeParseResult parseMeadeCommand(const char *input);
MeadeGetParseResult parseMeadeGetCommand(const char *input);
MeadeGpsParseResult parseMeadeGpsCommand(const char *input);
MeadeSetParseResult parseMeadeSetCommand(const char *input);
MeadeSyncParseResult parseMeadeSyncCommand(const char *input);
MeadeMovementParseResult parseMeadeMovementCommand(const char *input);
MeadeHomeParseResult parseMeadeHomeCommand(const char *input);
MeadeQuitParseResult parseMeadeQuitCommand(const char *input);
MeadeSlewRateParseResult parseMeadeSlewRateCommand(const char *input);
MeadeFocusParseResult parseMeadeFocusCommand(const char *input);
MeadeExtraParseResult parseMeadeExtraCommand(const char *input);
MeadeExtraLeafParseResult parseMeadeExtraLeafCommand(MeadeExtraCommandKind kind, const char *input);

}  // namespace core
}  // namespace oat