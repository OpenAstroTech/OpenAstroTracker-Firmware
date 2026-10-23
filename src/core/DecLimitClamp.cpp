#include "DecLimitClamp.hpp"

namespace core
{

DecLimitResult clampDecStepsToLimits(float targetSteps, long lowerLimitSteps, long upperLimitSteps)
{
    DecLimitResult result;
    result.steps = targetSteps;

    if (upperLimitSteps != 0 && result.steps > static_cast<float>(upperLimitSteps))
    {
        result.steps          = static_cast<float>(upperLimitSteps);
        result.clampedToUpper = true;
    }

    if (lowerLimitSteps != 0 && result.steps < static_cast<float>(lowerLimitSteps))
    {
        result.steps          = static_cast<float>(lowerLimitSteps);
        result.clampedToLower = true;
    }

    return result;
}

}  // namespace core
