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

MeadeResponse handleExtraGetLeaf(const char *leafInput, IMeadeExtraHandlers &h)
{
    MeadeResponse r;

    if (leafInput == nullptr || leafInput[0] == '\0')
    {
        return r;
    }

    if (isExact(leafInput, "R"))
    {
        return makeNumericFloatResponse(h.onGetRaStepsPerDegree(), 1);
    }
    if (isExact(leafInput, "D"))
    {
        return makeNumericFloatResponse(h.onGetDecStepsPerDegree(), 1);
    }
    if (isExact(leafInput, "DL"))
    {
        ExtraDecLimits lim = h.onGetDecLimits();
        return makeDecLimitsPairResponse(lim.lo, lim.hi);
    }
    if (isExact(leafInput, "DLL"))
    {
        return makeNumericFloatResponse(h.onGetDecLimits().lo, 1);
    }
    if (isExact(leafInput, "DLU"))
    {
        return makeNumericFloatResponse(h.onGetDecLimits().hi, 1);
    }
    if (startsWith(leafInput, "DL"))
    {
        return makeBooleanResponse(false);
    }
    if (isExact(leafInput, "DP"))
    {
        return makeBooleanResponse(false);
    }
    if (isExact(leafInput, "S"))
    {
        return makeNumericFloatResponse(h.onGetTrackingSpeedCalibration(), 5);
    }
    if (isExact(leafInput, "ST"))
    {
        return makeNumericFloatResponse(h.onGetRemainingSafeTime(), 7);
    }
    if (isExact(leafInput, "T"))
    {
        return makeNumericFloatResponse(h.onGetTrackingSpeed(), 7);
    }
    if (isExact(leafInput, "B"))
    {
        return makeIntResponse(h.onGetBacklashSteps());
    }
    if (isExact(leafInput, "A"))
    {
        return makeNumericFloatResponse(h.onGetAltStepsPerDegree(), 1);
    }
    if (isExact(leafInput, "AH"))
    {
        return makeFramedTextResponse(h.onGetAutoHomingStates());
    }
    if (isExact(leafInput, "AA"))
    {
        ExtraAzAltPositions p = h.onGetAzAltPositions();
        return makeLongPairPipeResponse(p.az, p.alt);
    }
    if (isExact(leafInput, "Z"))
    {
        return makeNumericFloatResponse(h.onGetAzStepsPerDegree(), 1);
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
        return makeLongPairPipeResponse(pos.raPos, pos.decPos);
    }
    if (isExact(leafInput, "MS"))
    {
        return makeFramedTextResponse(h.onGetStepperInfo());
    }
    if (startsWith(leafInput, "M"))
    {
        return makeFramedTextResponse(h.onGetMountHardwareInfo());
    }
    if (isExact(leafInput, "O"))
    {
        return makeLiteralResponse(h.onGetLogBuffer());
    }
    if (isExact(leafInput, "HR"))
    {
        return makeLongResponse(h.onGetRaHomingOffset());
    }
    if (isExact(leafInput, "HD"))
    {
        return makeLongResponse(h.onGetDecHomingOffset());
    }
    if (isExact(leafInput, "HS"))
    {
        return makeHemisphereResponse(h.onGetHemisphere());
    }
    if (isExact(leafInput, "H"))
    {
        ExtraHms t = h.onGetHourAngle();
        return makeCompactHmsResponse(t.hours, t.minutes, t.seconds);
    }
    if (startsWith(leafInput, "H"))
    {
        return makeBooleanResponse(false);
    }
    if (isExact(leafInput, "L"))
    {
        ExtraHms t = h.onGetLocalSiderealTime();
        return makeCompactHmsResponse(t.hours, t.minutes, t.seconds);
    }
    if (isExact(leafInput, "N"))
    {
        return makeFramedTextResponse(h.onGetNetworkStatus());
    }
    return r;
}

MeadeResponse handleExtraSetLeaf(const char *leafInput, IMeadeExtraHandlers &h)
{
    MeadeResponse r;
    if (leafInput == nullptr || leafInput[0] == '\0')
    {
        return r;
    }

    if (isExact(leafInput, "DLl"))
    {
        h.onClearDecLimitLower();
        return r;
    }
    if (isExact(leafInput, "DLu"))
    {
        h.onClearDecLimitUpper();
        return r;
    }
    if (startsWith(leafInput, "DLL"))
    {
        const char *payload    = leafInput + 3;
        const bool havePayload = payload[0] != '\0';
        const float value      = havePayload ? static_cast<float>(strtod(payload, nullptr)) : 0.0f;
        h.onSetDecLimitLower(havePayload, value);
        return r;
    }
    if (startsWith(leafInput, "DLU"))
    {
        const char *payload    = leafInput + 3;
        const bool havePayload = payload[0] != '\0';
        const float value      = havePayload ? static_cast<float>(strtod(payload, nullptr)) : 0.0f;
        h.onSetDecLimitUpper(havePayload, value);
        return r;
    }
    if (startsWith(leafInput, "DP"))
    {
        return r;
    }
    if (startsWith(leafInput, "HR"))
    {
        h.onSetRaHomingOffset(strtol(leafInput + 2, nullptr, 10));
        return r;
    }
    if (startsWith(leafInput, "HD"))
    {
        h.onSetDecHomingOffset(strtol(leafInput + 2, nullptr, 10));
        return r;
    }
    if (startsWith(leafInput, "D") && leafInput[1] != '\0')
    {
        const float v = static_cast<float>(strtod(leafInput + 1, nullptr));
        if (v > 0.0f)
        {
            h.onSetDecStepsPerDegree(v);
        }
        return r;
    }
    if (startsWith(leafInput, "R"))
    {
        h.onSetRaStepsPerDegree(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "A"))
    {
        h.onSetAzStepsPerDegree(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "L"))
    {
        h.onSetAltStepsPerDegree(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "S"))
    {
        h.onSetTrackingSpeedCalibration(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "T"))
    {
        h.onSetTrackingStepperPosition(strtol(leafInput + 1, nullptr, 10));
        return r;
    }
    if (startsWith(leafInput, "M"))
    {
        h.onSetManualSlewMode(leafInput[1] == '1');
        return r;
    }
    if (startsWith(leafInput, "X"))
    {
        h.onSetRaManualSpeed(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "Y"))
    {
        h.onSetDecManualSpeed(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "B"))
    {
        h.onSetBacklashCorrection(static_cast<int>(strtol(leafInput + 1, nullptr, 10)));
        return r;
    }
    return r;
}

MeadeResponse handleExtraLevelLeaf(const char *leafInput, IMeadeExtraHandlers &h)
{
    MeadeResponse r;

    if (!h.onLevelIsAvailable())
    {
        return makeBooleanResponse(false);
    }

    if (leafInput == nullptr || leafInput[0] == '\0')
    {
        return r;
    }

    if (startsWith(leafInput, "GR"))
    {
        ExtraPitchRoll pr = h.onLevelGetReferenceAngles();
        return makeAnglePair4Response(pr.pitch, pr.roll);
    }
    if (startsWith(leafInput, "GC"))
    {
        ExtraPitchRoll pr = h.onLevelGetCurrentAngles();
        return makeAnglePair4Response(pr.pitch, pr.roll);
    }
    if (startsWith(leafInput, "GT"))
    {
        return makeNumericFloatResponse(h.onLevelGetTemperature(), 1);
    }
    if (startsWith(leafInput, "G"))
    {
        return r;
    }
    if (startsWith(leafInput, "SP"))
    {
        h.onLevelSetReferencePitch(static_cast<float>(strtod(leafInput + 2, nullptr)));
        return makeBooleanResponse(true);
    }
    if (startsWith(leafInput, "SR"))
    {
        h.onLevelSetReferenceRoll(static_cast<float>(strtod(leafInput + 2, nullptr)));
        return makeBooleanResponse(true);
    }
    if (startsWith(leafInput, "S"))
    {
        return r;
    }
    if (startsWith(leafInput, "1"))
    {
        h.onLevelStartup();
        return makeBooleanResponse(true);
    }
    if (startsWith(leafInput, "0"))
    {
        h.onLevelShutdown();
        return makeBooleanResponse(true);
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
    return makeLevelUnknownResponse(echoed);
}

}  // namespace

MeadeResponse handleMeadeExtra(const char *suffix, IMeadeExtraHandlers &h)
{
    MeadeResponse r;

    if (suffix == nullptr || suffix[0] == '\0')
    {
        return r;
    }

    if (startsWith(suffix, "FR"))
    {
        h.onFactoryReset();
        return makeBooleanResponse(true);
    }

    if (startsWith(suffix, "D"))
    {
        const int duration = static_cast<int>(strtol(suffix + 1, nullptr, 10)) - 3;
        h.onDriftAlignment(duration);
        return r;
    }

    if (startsWith(suffix, "G"))
    {
        return handleExtraGetLeaf(suffix + 1, h);
    }

    if (startsWith(suffix, "S"))
    {
        return handleExtraSetLeaf(suffix + 1, h);
    }

    if (startsWith(suffix, "L"))
    {
        return handleExtraLevelLeaf(suffix + 1, h);
    }

    return r;
}

}  // namespace meade
}  // namespace core
}  // namespace oat
