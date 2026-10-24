#include "RelativeMove.hpp"

namespace core
{

RelativeMovePlan planRelativeMove(StepperAxis axis, long steps, bool tracking)
{
    RelativeMovePlan plan;

    switch (axis)
    {
        case RA_AND_DEC_STEPS:
            // Rejected: one axis per request.
            break;

        case RA_STEPS:
            plan.accepted       = true;
            plan.stopTracking   = steps != 0 && tracking;
            plan.entersSlewMode = true;
            break;

        case DEC_STEPS:
            plan.accepted       = true;
            plan.entersSlewMode = true;
            break;

        case AZIMUTH_STEPS:
        case ALTITUDE_STEPS:
        case FOCUS_STEPS:
            plan.accepted = true;
            break;
    }

    return plan;
}

}  // namespace core
