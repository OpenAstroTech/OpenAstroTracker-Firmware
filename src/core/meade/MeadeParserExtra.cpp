/**
 * @file MeadeParserExtra.cpp
 * @brief Extra-family (`:X...`) dispatcher for the Meade LX200 parser.
 *
 * Two-level dispatch: :X<family><leaf-payload> where family is one of
 * FR (factory reset), D (drift alignment), G (Get-leaves), S (Set-leaves),
 * or L (Level-leaves).
 */

#include "MeadeParser.hpp"
#include "MeadeParserHelpers.hpp"

#include <stdlib.h>
#include <string.h>

namespace oat
{
namespace core
{
namespace meade
{

namespace
{

MeadeResponse handleExtraGetLeaf(MeadeResponse &r, const char *leafInput, IMeadeExtraHandlers &h)
{
    if (leafInput == nullptr || leafInput[0] == '\0')
    {
        return r;
    }

    if (isExact(leafInput, "R"))
    {
        fillNumericFloatResponse(r, h.onGetRaStepsPerDegree(), 1);
        return r;
    }
    if (isExact(leafInput, "D"))
    {
        fillNumericFloatResponse(r, h.onGetDecStepsPerDegree(), 1);
        return r;
    }
    if (isExact(leafInput, "DL"))
    {
        ExtraDecLimits lim = h.onGetDecLimits();
        fillDecLimitsPairResponse(r, lim.lo, lim.hi);
        return r;
    }
    if (isExact(leafInput, "DLL"))
    {
        fillNumericFloatResponse(r, h.onGetDecLimits().lo, 1);
        return r;
    }
    if (isExact(leafInput, "DLU"))
    {
        fillNumericFloatResponse(r, h.onGetDecLimits().hi, 1);
        return r;
    }
    if (startsWith(leafInput, "DL"))
    {
        fillBooleanResponse(r, false);
        return r;
    }
    if (isExact(leafInput, "DP"))
    {
        fillBooleanResponse(r, false);
        return r;
    }
    if (isExact(leafInput, "S"))
    {
        fillNumericFloatResponse(r, h.onGetTrackingSpeedCalibration(), 5);
        return r;
    }
    if (isExact(leafInput, "ST"))
    {
        fillNumericFloatResponse(r, h.onGetRemainingSafeTime(), 7);
        return r;
    }
    if (isExact(leafInput, "T"))
    {
        fillNumericFloatResponse(r, h.onGetTrackingSpeed(), 7);
        return r;
    }
    if (isExact(leafInput, "B"))
    {
        fillIntResponse(r, h.onGetBacklashSteps());
        return r;
    }
    if (isExact(leafInput, "A"))
    {
        fillNumericFloatResponse(r, h.onGetAltStepsPerDegree(), 1);
        return r;
    }
    if (isExact(leafInput, "AH"))
    {
        fillFramedTextResponse(r, h.onGetAutoHomingStates());
        return r;
    }
    if (isExact(leafInput, "AA"))
    {
        ExtraAzAltPositions p = h.onGetAzAltPositions();
        fillLongPairPipeResponse(r, p.az, p.alt);
        return r;
    }
    if (isExact(leafInput, "Z"))
    {
        fillNumericFloatResponse(r, h.onGetAzStepsPerDegree(), 1);
        return r;
    }
    if (startsWith(leafInput, "C"))
    {
        // Payload format: "<ra>*<dec>" — float pair separated by '*'.
        const char *payload = leafInput + 1;
        const char *star    = strchr(payload, '*');
        if (star == nullptr || star == payload)
        {
            return r;
        }
        const float raCoord    = static_cast<float>(strtod(payload, nullptr));
        const float decCoord   = static_cast<float>(strtod(star + 1, nullptr));
        ExtraStepperCoords pos = h.onGetTargetCoordinatePositions(raCoord, decCoord);
        fillLongPairPipeResponse(r, pos.raPos, pos.decPos);
        return r;
    }
    if (isExact(leafInput, "MS"))
    {
        fillFramedTextResponse(r, h.onGetStepperInfo());
        return r;
    }
    if (startsWith(leafInput, "M"))
    {
        fillFramedTextResponse(r, h.onGetMountHardwareInfo());
        return r;
    }
    if (isExact(leafInput, "O"))
    {
        fillLiteralResponse(r, h.onGetLogBuffer());
        return r;
    }
    if (isExact(leafInput, "HR"))
    {
        fillLongResponse(r, h.onGetRaHomingOffset());
        return r;
    }
    if (isExact(leafInput, "HD"))
    {
        fillLongResponse(r, h.onGetDecHomingOffset());
        return r;
    }
    if (isExact(leafInput, "HS"))
    {
        fillHemisphereResponse(r, h.onGetHemisphere());
        return r;
    }
    if (isExact(leafInput, "H"))
    {
        ExtraHms t = h.onGetHourAngle();
        fillCompactHmsResponse(r, t.hours, t.minutes, t.seconds);
        return r;
    }
    if (startsWith(leafInput, "H"))
    {
        fillBooleanResponse(r, false);
        return r;
    }
    if (isExact(leafInput, "L"))
    {
        ExtraHms t = h.onGetLocalSiderealTime();
        fillCompactHmsResponse(r, t.hours, t.minutes, t.seconds);
        return r;
    }
    if (isExact(leafInput, "N"))
    {
        fillFramedTextResponse(r, h.onGetNetworkStatus());
        return r;
    }
    return r;
}

void handleExtraSetLeaf(MeadeResponse &r, const char *leafInput, IMeadeExtraHandlers &h)
{
    (void) r;  // Set-leaves produce no response body
    if (leafInput == nullptr || leafInput[0] == '\0')
    {
        return;
    }

    if (isExact(leafInput, "DLl"))
    {
        h.onClearDecLimitLower();
        return;
    }
    if (isExact(leafInput, "DLu"))
    {
        h.onClearDecLimitUpper();
        return;
    }
    if (startsWith(leafInput, "DLL"))
    {
        const char *payload    = leafInput + 3;
        const bool havePayload = payload[0] != '\0';
        const float value      = havePayload ? static_cast<float>(strtod(payload, nullptr)) : 0.0f;
        h.onSetDecLimitLower(havePayload, value);
        return;
    }
    if (startsWith(leafInput, "DLU"))
    {
        const char *payload    = leafInput + 3;
        const bool havePayload = payload[0] != '\0';
        const float value      = havePayload ? static_cast<float>(strtod(payload, nullptr)) : 0.0f;
        h.onSetDecLimitUpper(havePayload, value);
        return;
    }
    if (startsWith(leafInput, "DP"))
    {
        return;
    }
    if (startsWith(leafInput, "HR"))
    {
        h.onSetRaHomingOffset(strtol(leafInput + 2, nullptr, 10));
        return;
    }
    if (startsWith(leafInput, "HD"))
    {
        h.onSetDecHomingOffset(strtol(leafInput + 2, nullptr, 10));
        return;
    }
    if (startsWith(leafInput, "D") && leafInput[1] != '\0')
    {
        const float v = static_cast<float>(strtod(leafInput + 1, nullptr));
        if (v > 0.0f)
        {
            h.onSetDecStepsPerDegree(v);
        }
        return;
    }
    if (startsWith(leafInput, "R"))
    {
        h.onSetRaStepsPerDegree(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return;
    }
    if (startsWith(leafInput, "A"))
    {
        h.onSetAzStepsPerDegree(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return;
    }
    if (startsWith(leafInput, "L"))
    {
        h.onSetAltStepsPerDegree(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return;
    }
    if (startsWith(leafInput, "S"))
    {
        h.onSetTrackingSpeedCalibration(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return;
    }
    if (startsWith(leafInput, "T"))
    {
        h.onSetTrackingStepperPosition(strtol(leafInput + 1, nullptr, 10));
        return;
    }
    if (startsWith(leafInput, "M"))
    {
        h.onSetManualSlewMode(leafInput[1] == '1');
        return;
    }
    if (startsWith(leafInput, "X"))
    {
        h.onSetRaManualSpeed(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return;
    }
    if (startsWith(leafInput, "Y"))
    {
        h.onSetDecManualSpeed(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return;
    }
    if (startsWith(leafInput, "B"))
    {
        h.onSetBacklashCorrection(static_cast<int>(strtol(leafInput + 1, nullptr, 10)));
        return;
    }
}

void handleExtraLevelLeaf(MeadeResponse &r, const char *leafInput, IMeadeExtraHandlers &h)
{
    if (!h.onLevelIsAvailable())
    {
        fillBooleanResponse(r, false);
        return;
    }

    if (leafInput == nullptr || leafInput[0] == '\0')
    {
        return;
    }

    if (startsWith(leafInput, "GR"))
    {
        ExtraPitchRoll pr = h.onLevelGetReferenceAngles();
        fillAnglePair4Response(r, pr.pitch, pr.roll);
        return;
    }
    if (startsWith(leafInput, "GC"))
    {
        ExtraPitchRoll pr = h.onLevelGetCurrentAngles();
        fillAnglePair4Response(r, pr.pitch, pr.roll);
        return;
    }
    if (startsWith(leafInput, "GT"))
    {
        fillNumericFloatResponse(r, h.onLevelGetTemperature(), 1);
        return;
    }
    if (startsWith(leafInput, "G"))
    {
        return;
    }
    if (startsWith(leafInput, "SP"))
    {
        h.onLevelSetReferencePitch(static_cast<float>(strtod(leafInput + 2, nullptr)));
        fillBooleanResponse(r, true);
        return;
    }
    if (startsWith(leafInput, "SR"))
    {
        h.onLevelSetReferenceRoll(static_cast<float>(strtod(leafInput + 2, nullptr)));
        fillBooleanResponse(r, true);
        return;
    }
    if (startsWith(leafInput, "S"))
    {
        return;
    }
    if (startsWith(leafInput, "1"))
    {
        h.onLevelStartup();
        fillBooleanResponse(r, true);
        return;
    }
    if (startsWith(leafInput, "0"))
    {
        h.onLevelShutdown();
        fillBooleanResponse(r, true);
        return;
    }

    // Echo "L" + the original leaf input, matching legacy behavior.
    char echoed[MeadeResponse::Capacity];
    echoed[0] = 'L';
    size_t i  = 0;
    for (; leafInput[i] != '\0' && (i + 2) < sizeof(echoed); ++i)
    {
        echoed[i + 1] = leafInput[i];
    }
    echoed[i + 1] = '\0';
    fillLevelUnknownResponse(r, echoed);
}

}  // namespace

void handleMeadeExtra(MeadeResponse &r, const char *suffix, IMeadeExtraHandlers &h)
{
    if (suffix == nullptr || suffix[0] == '\0')
    {
        return;
    }

    if (startsWith(suffix, "FR"))
    {
        h.onFactoryReset();
        fillBooleanResponse(r, true);
        return;
    }

    if (startsWith(suffix, "D"))
    {
        const int duration = static_cast<int>(strtol(suffix + 1, nullptr, 10)) - 3;
        h.onDriftAlignment(duration);
        return;
    }

    if (startsWith(suffix, "G"))
    {
        handleExtraGetLeaf(r, suffix + 1, h);
        return;
    }

    if (startsWith(suffix, "S"))
    {
        handleExtraSetLeaf(r, suffix + 1, h);
        return;
    }

    if (startsWith(suffix, "L"))
    {
        handleExtraLevelLeaf(r, suffix + 1, h);
        return;
    }
}

}  // namespace meade
}  // namespace core
}  // namespace oat
