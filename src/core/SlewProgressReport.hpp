#pragma once

#include <math.h>
#include <stdint.h>

namespace core
{
/// How far along a slew is, as a display shows it.
struct SlewProgress {
    int raPercent;    ///< 0..100, or 100 when RA is not moving
    int decPercent;   ///< 0..100, or 100 when DEC is not moving
    bool inProgress;  ///< false when neither axis had anything to do
};

/// Turns "distance still to go" into "percent done" for the progress display.
///
/// It used to be five branches inside `Mount::getStepperProgress()`, reading two axes and the
/// slew controller between them. The arithmetic is trivial and the interesting part is the
/// rules around it: an axis that was not asked to move reads as finished, a total move below a
/// thousandth of a step counts as "nothing to do" (that epsilon is what keeps a rounding error
/// from showing a progress bar that never fills), and the two axes are reported independently
/// so a DEC-only move does not sit at 0 %.
class SlewProgressReport
{
  public:
    /// `raDistanceToGo` / `decDistanceToGo` are the axes' remaining steps, i.e. the same
    /// numbers the step generators use, not a snapshot taken earlier.
    static SlewProgress evaluate(float totalRaSteps, float totalDecSteps, long raDistanceToGo, long decDistanceToGo)
    {
        SlewProgress progress;
        progress.raPercent  = 100;
        progress.decPercent = 100;
        progress.inProgress = (fabs(totalRaSteps) > kEpsilon) || (fabs(totalDecSteps) > kEpsilon);

        if (fabs(totalDecSteps) > kEpsilon)
        {
            progress.decPercent = percentOf(totalDecSteps, decDistanceToGo);
        }

        if (fabs(totalRaSteps) > kEpsilon)
        {
            progress.raPercent = percentOf(totalRaSteps, raDistanceToGo);
        }

        return progress;
    }

    /// Below this a total move is rounding noise rather than a move.
    static constexpr float kEpsilon = 0.001f;

  private:
    static int percentOf(float totalSteps, long distanceToGo)
    {
        return static_cast<int>(round(100.0f - (100.0f * static_cast<float>(distanceToGo) / totalSteps)));
    }
};

}  // namespace core
