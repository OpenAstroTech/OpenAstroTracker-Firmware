#include <gtest/gtest.h>

#include <vector>

#include "core/DriftAlignmentController.hpp"

using core::DriftAction;
using core::DriftAlignmentController;
using core::DriftPhase;

namespace
{
constexpr float kStepsPerDegree = 720.0F;  // a typical RA u-steps/degree

/// Drives one tick and returns the action it produced.
DriftAction tick(DriftAlignmentController &controller, bool axesStopped, uint32_t nowMs)
{
    return controller.update(axesStopped, nowMs);
}

/// Runs the sequence to completion, reporting every action in order.
///
/// The axis is reported moving for one tick after each move action (which is what the real mount
/// does) and stopped otherwise, so the "seen moving" latch is exercised rather than assumed.
std::vector<DriftAction> runToCompletion(DriftAlignmentController &controller, uint32_t &nowMs)
{
    std::vector<DriftAction> actions;
    int moveTick = 0;  // 0 = idle, 1 = report running next tick, 2 = report stopped after that

    for (int guard = 0; guard < 1000 && controller.isActive(); guard++)
    {
        bool axesStopped = true;
        if (moveTick == 1)
        {
            axesStopped = false;  // the axis is moving
            moveTick    = 2;
        }
        else if (moveTick == 2)
        {
            axesStopped = true;  // and now it has stopped
            moveTick    = 0;
        }

        const DriftAction action = controller.update(axesStopped, nowMs);
        if (action != DriftAction::None)
        {
            actions.push_back(action);
            if ((action == DriftAction::MoveEastPass) || (action == DriftAction::MoveWestPass) || (action == DriftAction::MoveEastGap)
                || (action == DriftAction::ResetGap))
            {
                moveTick = 1;
            }
        }
        nowMs += 100;
    }
    return actions;
}
}  // namespace

// The pass geometry is the legacy one: 5.3 arcminutes covered in the requested duration, so the
// speed is derived rather than configured.
TEST(DriftAlignmentTest, PassGeometryMatchesTheLegacyCompute)
{
    DriftAlignmentController controller;

    controller.begin(30, kStepsPerDegree);

    const long expectedSteps = static_cast<long>(kStepsPerDegree * (5.3F / 60.0F));
    EXPECT_EQ(expectedSteps, controller.passSteps());
    EXPECT_FLOAT_EQ(static_cast<float>(expectedSteps) / 30.0F, controller.passSpeed());
    EXPECT_TRUE(controller.isActive());
    EXPECT_EQ(DriftPhase::Stopping, controller.phase());
}

// A zero or negative duration must not divide by zero; it is treated as one second.
TEST(DriftAlignmentTest, NonPositiveDurationIsClampedToOneSecond)
{
    DriftAlignmentController controller;

    controller.begin(0, kStepsPerDegree);

    EXPECT_FLOAT_EQ(static_cast<float>(controller.passSteps()), controller.passSpeed());

    controller.begin(-5, kStepsPerDegree);
    EXPECT_FLOAT_EQ(static_cast<float>(controller.passSteps()), controller.passSpeed());
}

// The whole sequence, in order: stop, east pass, its gearing take-up, west pass, the reset move,
// and tracking resumes last. A drift alignment that ended anywhere else would leave the mount
// untracked.
TEST(DriftAlignmentTest, SequenceRunsStopEastGapWestResetThenTracking)
{
    DriftAlignmentController controller;
    controller.begin(10, kStepsPerDegree);
    uint32_t nowMs = 1000;

    const std::vector<DriftAction> actions = runToCompletion(controller, nowMs);

    const std::vector<DriftAction> expected {
        DriftAction::MoveEastPass, DriftAction::MoveEastGap, DriftAction::MoveWestPass, DriftAction::ResetGap, DriftAction::ResumeTracking};
    EXPECT_EQ(expected, actions);
    EXPECT_FALSE(controller.isActive());
    EXPECT_EQ(DriftPhase::Idle, controller.phase());
}

// Each pause is 1.5 s on the clock, not a busy wait: before the deadline the controller does
// nothing, and only then does it issue the pass.
TEST(DriftAlignmentTest, PausesAreDeadlinesOnTheClock)
{
    DriftAlignmentController controller;
    controller.begin(10, kStepsPerDegree);

    uint32_t nowMs = 0;
    EXPECT_EQ(DriftAction::None, tick(controller, true, nowMs));  // axes already stopped: enter pause
    EXPECT_EQ(DriftPhase::PauseEast, controller.phase());

    nowMs += DriftAlignmentController::kPauseMillis - 1;
    EXPECT_EQ(DriftAction::None, tick(controller, true, nowMs));

    nowMs += 1;
    EXPECT_EQ(DriftAction::MoveEastPass, tick(controller, true, nowMs));
    EXPECT_EQ(DriftPhase::PassEast, controller.phase());
}

// A move that has not been seen running yet must not be treated as finished: issuing a move and
// asking on the same tick would otherwise skip the whole pass.
TEST(DriftAlignmentTest, PassWaitsForTheAxisToBeSeenMoving)
{
    DriftAlignmentController controller;
    controller.begin(10, kStepsPerDegree);

    uint32_t nowMs = 0;
    tick(controller, true, nowMs);  // Stopping -> PauseEast
    nowMs += DriftAlignmentController::kPauseMillis;
    EXPECT_EQ(DriftAction::MoveEastPass, tick(controller, true, nowMs));

    // The axis has not started yet; a stop here must not end the pass.
    EXPECT_EQ(DriftAction::None, tick(controller, true, nowMs + 10));
    EXPECT_EQ(DriftPhase::PassEast, controller.phase());

    // Now it runs, then stops.
    EXPECT_EQ(DriftAction::None, tick(controller, false, nowMs + 20));
    EXPECT_EQ(DriftAction::MoveEastGap, tick(controller, true, nowMs + 30));
    EXPECT_EQ(DriftPhase::GapEast, controller.phase());
}

// The stop request is issued before the first pause, and not repeated.
TEST(DriftAlignmentTest, StopsTheAxesBeforeTheFirstPass)
{
    DriftAlignmentController controller;
    controller.begin(10, kStepsPerDegree);

    uint32_t nowMs = 0;
    // Axes still moving: nothing to do but wait for them.
    EXPECT_EQ(DriftAction::None, tick(controller, false, nowMs));
    EXPECT_EQ(DriftPhase::Stopping, controller.phase());

    nowMs += 500;
    EXPECT_EQ(DriftAction::None, tick(controller, true, nowMs));
    EXPECT_EQ(DriftPhase::PauseEast, controller.phase());
}

// Aborting leaves the sequence inactive and produces no further actions.
TEST(DriftAlignmentTest, AbortStopsTheSequence)
{
    DriftAlignmentController controller;
    controller.begin(10, kStepsPerDegree);
    controller.abort();

    EXPECT_FALSE(controller.isActive());
    EXPECT_EQ(DriftAction::None, tick(controller, true, 5000));
}

// Idle controllers do nothing, so the loop can tick unconditionally.
TEST(DriftAlignmentTest, IdleControllerIsANoOp)
{
    DriftAlignmentController controller;

    EXPECT_FALSE(controller.isActive());
    EXPECT_EQ(DriftAction::None, tick(controller, false, 0));
    EXPECT_EQ(DriftAction::None, tick(controller, true, 100000));
}
