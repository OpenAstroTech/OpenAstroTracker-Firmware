#include "core/MeadeParser.hpp"

namespace oat
{
namespace core
{
namespace meade
{

namespace
{
MeadeCommandKind classifyMeadeCommandKind(char family)
{
    switch (family)
    {
        case 'S':
            return MeadeCommandKind::Set;
        case 'M':
            return MeadeCommandKind::Move;
        case 'G':
            return MeadeCommandKind::Get;
        case 'g':
            return MeadeCommandKind::Gps;
        case 'C':
            return MeadeCommandKind::Sync;
        case 'h':
            return MeadeCommandKind::Home;
        case 'I':
            return MeadeCommandKind::Init;
        case 'Q':
            return MeadeCommandKind::Quit;
        case 'R':
            return MeadeCommandKind::SlewRate;
        case 'D':
            return MeadeCommandKind::Distance;
        case 'X':
            return MeadeCommandKind::Extra;
        case 'F':
            return MeadeCommandKind::Focus;
        default:
            return MeadeCommandKind::Unknown;
    }
}

MeadeCommandDispatchTarget dispatchTargetForCommandKind(MeadeCommandKind kind)
{
    switch (kind)
    {
        case MeadeCommandKind::Set:
            return MeadeCommandDispatchTarget::SetInfo;
        case MeadeCommandKind::Move:
            return MeadeCommandDispatchTarget::Movement;
        case MeadeCommandKind::Get:
            return MeadeCommandDispatchTarget::GetInfo;
        case MeadeCommandKind::Gps:
            return MeadeCommandDispatchTarget::GpsCommands;
        case MeadeCommandKind::Sync:
            return MeadeCommandDispatchTarget::SyncControl;
        case MeadeCommandKind::Home:
            return MeadeCommandDispatchTarget::Home;
        case MeadeCommandKind::Init:
            return MeadeCommandDispatchTarget::Init;
        case MeadeCommandKind::Quit:
            return MeadeCommandDispatchTarget::Quit;
        case MeadeCommandKind::SlewRate:
            return MeadeCommandDispatchTarget::SetSlewRate;
        case MeadeCommandKind::Distance:
            return MeadeCommandDispatchTarget::Distance;
        case MeadeCommandKind::Extra:
            return MeadeCommandDispatchTarget::ExtraCommands;
        case MeadeCommandKind::Focus:
            return MeadeCommandDispatchTarget::FocusCommands;
        case MeadeCommandKind::Unknown:
        default:
            return MeadeCommandDispatchTarget::Unknown;
    }
}
}  // namespace

MeadeParseResult parseMeadeCommand(const char *input)
{
    MeadeParseResult result;
    if (input == nullptr || input[0] != ':')
    {
        return result;
    }

    std::string normalized;
    for (const char *cursor = input; *cursor != '\0'; ++cursor)
    {
        if (*cursor != ' ')
        {
            normalized.push_back(*cursor);
        }
    }

    if (normalized.length() < 2)
    {
        return result;
    }

    if (!normalized.empty() && normalized.back() == '#')
    {
        normalized.pop_back();
    }

    if (normalized.length() < 2)
    {
        return result;
    }

    result.kind = classifyMeadeCommandKind(normalized[1]);
    if (result.kind == MeadeCommandKind::Unknown)
    {
        return result;
    }

    result.valid          = true;
    result.dispatchTarget = dispatchTargetForCommandKind(result.kind);
    result.payload        = normalized.substr(2);
    return result;
}

MeadeGetParseResult parseMeadeGetCommand(const char *input)
{
    MeadeGetParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    switch (input[0])
    {
        case 'V':
            if (input[1] == 'N' && input[2] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::FirmwareVersion;
            }
            else if (input[1] == 'P' && input[2] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::ProductName;
            }
            return result;

        case 'r':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::TargetRa;
            }
            return result;

        case 'd':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::TargetDec;
            }
            return result;

        case 'R':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::CurrentRa;
            }
            return result;

        case 'D':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::CurrentDec;
            }
            return result;

        case 'X':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::MountStatus;
            }
            return result;

        case 'I':
            if (input[1] == 'S' && input[2] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::IsSlewing;
            }
            else if (input[1] == 'T' && input[2] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::IsTracking;
            }
            else if (input[1] == 'G' && input[2] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::IsGuiding;
            }
            return result;

        case 't':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::SiteLatitude;
            }
            return result;

        case 'g':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::SiteLongitude;
            }
            return result;

        case 'c':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::ClockFormat;
            }
            return result;

        case 'G':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::UtcOffset;
            }
            return result;

        case 'a':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::LocalTime12h;
            }
            return result;

        case 'L':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::LocalTime24h;
            }
            return result;

        case 'C':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::LocalDate;
            }
            return result;

        case 'M':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::SiteName1;
            }
            return result;

        case 'N':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::SiteName2;
            }
            return result;

        case 'O':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::SiteName3;
            }
            return result;

        case 'P':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::SiteName4;
            }
            return result;

        case 'T':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeGetCommandKind::TrackingRate;
            }
            return result;

        default:
            return result;
    }
}

MeadeGpsParseResult parseMeadeGpsCommand(const char *input)
{
    MeadeGpsParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    if (input[0] == 'T')
    {
        result.valid   = true;
        result.kind    = MeadeGpsCommandKind::StartAcquisition;
        result.payload = input + 1;
    }

    return result;
}

MeadeSetParseResult parseMeadeSetCommand(const char *input)
{
    MeadeSetParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    switch (input[0])
    {
        case 'd':
            result.valid   = true;
            result.kind    = MeadeSetCommandKind::TargetDec;
            result.payload = input + 1;
            return result;

        case 'r':
            result.valid   = true;
            result.kind    = MeadeSetCommandKind::TargetRa;
            result.payload = input + 1;
            return result;

        case 'H':
            result.valid = true;
            if (input[1] == 'L')
            {
                result.kind    = MeadeSetCommandKind::LocalSiderealTime;
                result.payload = input + 2;
            }
            else if (input[1] == 'P')
            {
                result.kind    = MeadeSetCommandKind::HomePoint;
                result.payload = input + 2;
            }
            else
            {
                result.kind    = MeadeSetCommandKind::HourAngle;
                result.payload = input + 1;
            }
            return result;

        case 'Y':
            result.valid   = true;
            result.kind    = MeadeSetCommandKind::SyncCoordinates;
            result.payload = input + 1;
            return result;

        case 't':
            result.valid   = true;
            result.kind    = MeadeSetCommandKind::SiteLatitude;
            result.payload = input + 1;
            return result;

        case 'g':
            result.valid   = true;
            result.kind    = MeadeSetCommandKind::SiteLongitude;
            result.payload = input + 1;
            return result;

        case 'G':
            result.valid   = true;
            result.kind    = MeadeSetCommandKind::UtcOffset;
            result.payload = input + 1;
            return result;

        case 'L':
            result.valid   = true;
            result.kind    = MeadeSetCommandKind::LocalTime;
            result.payload = input + 1;
            return result;

        case 'C':
            result.valid   = true;
            result.kind    = MeadeSetCommandKind::LocalDate;
            result.payload = input + 1;
            return result;

        default:
            return result;
    }
}

MeadeSyncParseResult parseMeadeSyncCommand(const char *input)
{
    MeadeSyncParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    if (input[0] == 'M' && input[1] == '\0')
    {
        result.valid = true;
        result.kind  = MeadeSyncCommandKind::SyncToTarget;
    }

    return result;
}

MeadeMovementParseResult parseMeadeMovementCommand(const char *input)
{
    MeadeMovementParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    switch (input[0])
    {
        case 'S':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeMovementCommandKind::SlewToTarget;
            }
            return result;

        case 'T':
            result.valid   = true;
            result.kind    = MeadeMovementCommandKind::TrackingToggle;
            result.payload = input + 1;
            return result;

        case 'G':
        case 'g':
            result.valid   = true;
            result.kind    = MeadeMovementCommandKind::GuidePulse;
            result.payload = input + 1;
            return result;

        case 'A':
            if (input[1] == 'A')
            {
                result.valid   = true;
                result.kind    = MeadeMovementCommandKind::MoveAzAltHome;
                result.payload = input + 2;
            }
            else if (input[1] == 'Z')
            {
                result.valid   = true;
                result.kind    = MeadeMovementCommandKind::MoveAzimuth;
                result.payload = input + 2;
            }
            else if (input[1] == 'L')
            {
                result.valid   = true;
                result.kind    = MeadeMovementCommandKind::MoveAltitude;
                result.payload = input + 2;
            }
            return result;

        case 'e':
            result.valid = true;
            result.kind  = MeadeMovementCommandKind::SlewEast;
            return result;

        case 'w':
            result.valid = true;
            result.kind  = MeadeMovementCommandKind::SlewWest;
            return result;

        case 'n':
            result.valid = true;
            result.kind  = MeadeMovementCommandKind::SlewNorth;
            return result;

        case 's':
            result.valid = true;
            result.kind  = MeadeMovementCommandKind::SlewSouth;
            return result;

        case 'X':
            result.valid   = true;
            result.kind    = MeadeMovementCommandKind::MoveStepper;
            result.payload = input + 1;
            return result;

        case 'H':
            if (input[1] == 'R')
            {
                result.valid   = true;
                result.kind    = MeadeMovementCommandKind::HomeRa;
                result.payload = input + 2;
            }
            else if (input[1] == 'D')
            {
                result.valid   = true;
                result.kind    = MeadeMovementCommandKind::HomeDec;
                result.payload = input + 2;
            }
            return result;

        default:
            return result;
    }
}

MeadeHomeParseResult parseMeadeHomeCommand(const char *input)
{
    MeadeHomeParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    switch (input[0])
    {
        case 'P':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeHomeCommandKind::Park;
            }
            return result;

        case 'F':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeHomeCommandKind::Home;
            }
            return result;

        case 'U':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeHomeCommandKind::Unpark;
            }
            return result;

        case 'Z':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeHomeCommandKind::SetAzAltHome;
            }
            return result;

        default:
            return result;
    }
}

MeadeQuitParseResult parseMeadeQuitCommand(const char *input)
{
    MeadeQuitParseResult result;
    if (input == nullptr)
    {
        return result;
    }

    if (input[0] == '\0')
    {
        result.valid = true;
        result.kind  = MeadeQuitCommandKind::StopAll;
        return result;
    }

    switch (input[0])
    {
        case 'a':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeQuitCommandKind::StopDirectionalAll;
            }
            return result;

        case 'e':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeQuitCommandKind::StopEast;
            }
            return result;

        case 'w':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeQuitCommandKind::StopWest;
            }
            return result;

        case 'n':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeQuitCommandKind::StopNorth;
            }
            return result;

        case 's':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeQuitCommandKind::StopSouth;
            }
            return result;

        case 'q':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeQuitCommandKind::QuitControlMode;
            }
            return result;

        default:
            return result;
    }
}

MeadeSlewRateParseResult parseMeadeSlewRateCommand(const char *input)
{
    MeadeSlewRateParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    switch (input[0])
    {
        case 'S':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeSlewRateCommandKind::Slew;
            }
            return result;

        case 'M':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeSlewRateCommandKind::Find;
            }
            return result;

        case 'C':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeSlewRateCommandKind::Center;
            }
            return result;

        case 'G':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeSlewRateCommandKind::Guide;
            }
            return result;

        default:
            return result;
    }
}

MeadeFocusParseResult parseMeadeFocusCommand(const char *input)
{
    MeadeFocusParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    switch (input[0])
    {
        case '+':
            result.valid = true;
            result.kind  = MeadeFocusCommandKind::ContinuousIn;
            return result;

        case '-':
            result.valid = true;
            result.kind  = MeadeFocusCommandKind::ContinuousOut;
            return result;

        case 'M':
            result.valid   = true;
            result.kind    = MeadeFocusCommandKind::MoveBy;
            result.payload = input + 1;
            return result;

        case '1':
        case '2':
        case '3':
        case '4':
            result.valid   = true;
            result.kind    = MeadeFocusCommandKind::SetSpeedByRate;
            result.payload = input;
            return result;

        case 'F':
            result.valid = true;
            result.kind  = MeadeFocusCommandKind::SetFastestRate;
            return result;

        case 'S':
            result.valid = true;
            result.kind  = MeadeFocusCommandKind::SetSlowestRate;
            return result;

        case 'p':
            result.valid = true;
            result.kind  = MeadeFocusCommandKind::GetPosition;
            return result;

        case 'P':
            result.valid   = true;
            result.kind    = MeadeFocusCommandKind::SetPosition;
            result.payload = input + 1;
            return result;

        case 'B':
            result.valid = true;
            result.kind  = MeadeFocusCommandKind::GetState;
            return result;

        case 'Q':
            result.valid = true;
            result.kind  = MeadeFocusCommandKind::Stop;
            return result;

        default:
            return result;
    }
}

MeadeExtraParseResult parseMeadeExtraCommand(const char *input)
{
    MeadeExtraParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    switch (input[0])
    {
        case 'D':
            result.valid   = true;
            result.kind    = MeadeExtraCommandKind::DriftAlignment;
            result.payload = input + 1;
            return result;
        case 'G':
            result.valid   = true;
            result.kind    = MeadeExtraCommandKind::Get;
            result.payload = input + 1;
            return result;
        case 'S':
            result.valid   = true;
            result.kind    = MeadeExtraCommandKind::Set;
            result.payload = input + 1;
            return result;
        case 'L':
            result.valid   = true;
            result.kind    = MeadeExtraCommandKind::Level;
            result.payload = input + 1;
            return result;
        case 'F':
            if (input[1] == 'R')
            {
                result.valid   = true;
                result.kind    = MeadeExtraCommandKind::FactoryReset;
                result.payload = input + 2;
            }
            return result;
        default:
            return result;
    }
}

namespace
{
MeadeExtraLeafParseResult parseMeadeExtraGetLeafCommand(const char *input)
{
    MeadeExtraLeafParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    switch (input[0])
    {
        case 'R':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetRaStepsPerDegree;
            }
            return result;

        case 'D':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetDecStepsPerDegree;
            }
            else if (input[1] == 'L')
            {
                result.valid = true;
                if (input[2] == '\0')
                {
                    result.kind = MeadeExtraLeafCommandKind::GetDecLimitBoth;
                }
                else if ((input[2] == 'L') && (input[3] == '\0'))
                {
                    result.kind = MeadeExtraLeafCommandKind::GetDecLimitLowerOnly;
                }
                else if ((input[2] == 'U') && (input[3] == '\0'))
                {
                    result.kind = MeadeExtraLeafCommandKind::GetDecLimitUpperOnly;
                }
                else
                {
                    result.kind    = MeadeExtraLeafCommandKind::GetDecLimitInvalidVariant;
                    result.payload = input + 2;
                }
            }
            else if ((input[1] == 'P') && (input[2] == '\0'))
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetDecParking;
            }
            return result;

        case 'S':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetTrackingSpeedCalibration;
            }
            else if ((input[1] == 'T') && (input[2] == '\0'))
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetRemainingSafeTime;
            }
            return result;

        case 'T':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetTrackingSpeed;
            }
            return result;

        case 'B':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetBacklashSteps;
            }
            return result;

        case 'A':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetAltStepsPerDegree;
            }
            else if ((input[1] == 'H') && (input[2] == '\0'))
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetAutoHomingStates;
            }
            else if ((input[1] == 'A') && (input[2] == '\0'))
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetAzAltPositions;
            }
            return result;

        case 'Z':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetAzStepsPerDegree;
            }
            return result;

        case 'C':
            result.valid   = true;
            result.kind    = MeadeExtraLeafCommandKind::GetTargetCoordinatePositions;
            result.payload = input + 1;
            return result;

        case 'M':
            result.valid = true;
            if ((input[1] == 'S') && (input[2] == '\0'))
            {
                result.kind = MeadeExtraLeafCommandKind::GetStepperInfo;
            }
            else
            {
                result.kind = MeadeExtraLeafCommandKind::GetMountHardwareInfo;
            }
            return result;

        case 'O':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetLogBuffer;
            }
            return result;

        case 'H':
            result.valid = true;
            if (input[1] == '\0')
            {
                result.kind = MeadeExtraLeafCommandKind::GetHourAngle;
            }
            else if ((input[1] == 'R') && (input[2] == '\0'))
            {
                result.kind = MeadeExtraLeafCommandKind::GetRaHomingOffset;
            }
            else if ((input[1] == 'D') && (input[2] == '\0'))
            {
                result.kind = MeadeExtraLeafCommandKind::GetDecHomingOffset;
            }
            else if ((input[1] == 'S') && (input[2] == '\0'))
            {
                result.kind = MeadeExtraLeafCommandKind::GetHemisphere;
            }
            else
            {
                result.kind    = MeadeExtraLeafCommandKind::GetHourAngleInvalidVariant;
                result.payload = input + 1;
            }
            return result;

        case 'L':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetLocalSiderealTime;
            }
            return result;

        case 'N':
            if (input[1] == '\0')
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::GetNetworkStatus;
            }
            return result;

        default:
            return result;
    }
}

MeadeExtraLeafParseResult parseMeadeExtraSetLeafCommand(const char *input)
{
    MeadeExtraLeafParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    switch (input[0])
    {
        case 'R':
            result.valid   = true;
            result.kind    = MeadeExtraLeafCommandKind::SetRaStepsPerDegree;
            result.payload = input + 1;
            return result;

        case 'A':
            result.valid   = true;
            result.kind    = MeadeExtraLeafCommandKind::SetAzStepsPerDegree;
            result.payload = input + 1;
            return result;

        case 'L':
            result.valid   = true;
            result.kind    = MeadeExtraLeafCommandKind::SetAltStepsPerDegree;
            result.payload = input + 1;
            return result;

        case 'D':
            if ((input[1] == 'L') && (input[2] == 'L'))
            {
                result.valid   = true;
                result.kind    = MeadeExtraLeafCommandKind::SetDecLimitLowerSet;
                result.payload = input + 3;
                return result;
            }
            if ((input[1] == 'L') && (input[2] == 'U'))
            {
                result.valid   = true;
                result.kind    = MeadeExtraLeafCommandKind::SetDecLimitUpperSet;
                result.payload = input + 3;
                return result;
            }
            if ((input[1] == 'L') && (input[2] == 'l') && (input[3] == '\0'))
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::SetDecLimitLowerClear;
                return result;
            }
            if ((input[1] == 'L') && (input[2] == 'u') && (input[3] == '\0'))
            {
                result.valid = true;
                result.kind  = MeadeExtraLeafCommandKind::SetDecLimitUpperClear;
                return result;
            }
            if (input[1] == 'P')
            {
                result.valid   = true;
                result.kind    = MeadeExtraLeafCommandKind::SetDecParking;
                result.payload = input + 2;
                return result;
            }
            if (input[1] != '\0')
            {
                result.valid   = true;
                result.kind    = MeadeExtraLeafCommandKind::SetDecStepsPerDegree;
                result.payload = input + 1;
            }
            return result;

        case 'S':
            result.valid   = true;
            result.kind    = MeadeExtraLeafCommandKind::SetTrackingSpeedCalibration;
            result.payload = input + 1;
            return result;

        case 'T':
            result.valid   = true;
            result.kind    = MeadeExtraLeafCommandKind::SetTrackingStepperPosition;
            result.payload = input + 1;
            return result;

        case 'M':
            result.valid   = true;
            result.kind    = MeadeExtraLeafCommandKind::SetManualSlewMode;
            result.payload = input + 1;
            return result;

        case 'X':
            result.valid   = true;
            result.kind    = MeadeExtraLeafCommandKind::SetRaManualSpeed;
            result.payload = input + 1;
            return result;

        case 'Y':
            result.valid   = true;
            result.kind    = MeadeExtraLeafCommandKind::SetDecManualSpeed;
            result.payload = input + 1;
            return result;

        case 'B':
            result.valid   = true;
            result.kind    = MeadeExtraLeafCommandKind::SetBacklashCorrection;
            result.payload = input + 1;
            return result;

        case 'H':
            if (input[1] == 'R')
            {
                result.valid   = true;
                result.kind    = MeadeExtraLeafCommandKind::SetRaHomingOffset;
                result.payload = input + 2;
                return result;
            }
            if (input[1] == 'D')
            {
                result.valid   = true;
                result.kind    = MeadeExtraLeafCommandKind::SetDecHomingOffset;
                result.payload = input + 2;
                return result;
            }
            return result;

        default:
            return result;
    }
}

MeadeExtraLeafParseResult parseMeadeExtraLevelLeafCommand(const char *input)
{
    MeadeExtraLeafParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }

    result.valid = true;

    switch (input[0])
    {
        case 'G':
            if (input[1] == 'R')
            {
                result.kind = MeadeExtraLeafCommandKind::LevelGetReferenceAngles;
                return result;
            }
            if (input[1] == 'C')
            {
                result.kind = MeadeExtraLeafCommandKind::LevelGetCurrentAngles;
                return result;
            }
            if (input[1] == 'T')
            {
                result.kind = MeadeExtraLeafCommandKind::LevelGetTemperature;
                return result;
            }
            result.kind    = MeadeExtraLeafCommandKind::LevelGetInvalidVariant;
            result.payload = input + 1;
            return result;

        case 'S':
            if (input[1] == 'P')
            {
                result.kind    = MeadeExtraLeafCommandKind::LevelSetReferencePitch;
                result.payload = input + 2;
                return result;
            }
            if (input[1] == 'R')
            {
                result.kind    = MeadeExtraLeafCommandKind::LevelSetReferenceRoll;
                result.payload = input + 2;
                return result;
            }
            result.kind    = MeadeExtraLeafCommandKind::LevelSetInvalidVariant;
            result.payload = input + 1;
            return result;

        case '1':
            result.kind = MeadeExtraLeafCommandKind::LevelStartup;
            return result;

        case '0':
            result.kind = MeadeExtraLeafCommandKind::LevelShutdown;
            return result;

        default:
            result.kind    = MeadeExtraLeafCommandKind::LevelUnknownVariant;
            result.payload = input;
            return result;
    }
}
}  // namespace

MeadeExtraLeafParseResult parseMeadeExtraLeafCommand(MeadeExtraCommandKind kind, const char *input)
{
    switch (kind)
    {
        case MeadeExtraCommandKind::Get:
            return parseMeadeExtraGetLeafCommand(input);

        case MeadeExtraCommandKind::Set:
            return parseMeadeExtraSetLeafCommand(input);

        case MeadeExtraCommandKind::Level:
            return parseMeadeExtraLevelLeafCommand(input);

        case MeadeExtraCommandKind::Unknown:
        case MeadeExtraCommandKind::DriftAlignment:
        case MeadeExtraCommandKind::FactoryReset:
            return MeadeExtraLeafParseResult();
    }
}

}  // namespace meade
}  // namespace core
}  // namespace oat