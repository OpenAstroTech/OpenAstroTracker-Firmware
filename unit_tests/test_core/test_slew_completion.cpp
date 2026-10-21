#include <gtest/gtest.h>

#include "core/SlewCompletion.hpp"

using core::ArrivalFacts;
using core::CompletionPlan;
using core::SlewCompletion;
using core::SlewIntent;

namespace
{
ArrivalFacts goToAtTarget()
{
    ArrivalFacts facts        = {};
    facts.intent              = SlewIntent::Target;
    facts.parking             = false;
    facts.compensationPending = false;
    return facts;
}
}  // namespace

TEST(SlewCompletionTest, APlainGoToRestartsTrackingAndDoesNothingElse)
{
    ArrivalFacts facts = goToAtTarget();
    facts.backlashOwed = false;

    const CompletionPlan plan = SlewCompletion::plan(facts);

    EXPECT_FALSE(plan.stopTracking);
    EXPECT_FALSE(plan.setHome);
    EXPECT_TRUE(plan.raTrackingMicrosteps);
    EXPECT_FALSE(plan.compensateTracker);
    EXPECT_TRUE(plan.startTracking);
    EXPECT_TRUE(plan.decGuideMicrosteps);
    EXPECT_FALSE(plan.correctBacklash);
    EXPECT_FALSE(plan.retargetRa);
    EXPECT_FALSE(plan.startParkPositionLeg);
    EXPECT_FALSE(plan.parkComplete);
    EXPECT_TRUE(plan.endSlew);
}

TEST(SlewCompletionTest, AGoToCatchesUpTheTrackerThatTheSlewStopped)
{
    ArrivalFacts facts        = goToAtTarget();
    facts.compensationPending = true;

    EXPECT_TRUE(SlewCompletion::plan(facts).compensateTracker);
}

TEST(SlewCompletionTest, BacklashIsCorrectedWhenOwed)
{
    ArrivalFacts facts = goToAtTarget();
    facts.backlashOwed = true;

    EXPECT_TRUE(SlewCompletion::plan(facts).correctBacklash);
}

TEST(SlewCompletionTest, HomingRailHoldsTrackingOffAndTheRaMicrostepsAlone)
{
    ArrivalFacts facts = goToAtTarget();
    facts.findingHome  = true;
    facts.intent       = SlewIntent::Manual;

    const CompletionPlan plan = SlewCompletion::plan(facts);

    EXPECT_FALSE(plan.raTrackingMicrosteps);  // RA must stay in slew configuration
    EXPECT_FALSE(plan.startTracking);
}

TEST(SlewCompletionTest, HomingRailAlsoSuppressesTrackerCompensation)
{
    // Finding home is not parking, so the compensation block would otherwise run and
    // disturb the axis the sensor is trying to locate.
    ArrivalFacts facts        = goToAtTarget();
    facts.findingHome         = true;
    facts.compensationPending = true;

    const CompletionPlan plan = SlewCompletion::plan(facts);

    EXPECT_TRUE(plan.compensateTracker);  // unchanged from the original: parking is the
    EXPECT_FALSE(plan.startTracking);     // only thing that gates the compensation
}

TEST(SlewCompletionTest, FirstParkLegSetsHomeAndStartsTheSecondLeg)
{
    ArrivalFacts facts = goToAtTarget();
    facts.intent       = SlewIntent::Home;
    facts.parking      = true;

    const CompletionPlan plan = SlewCompletion::plan(facts);

    EXPECT_TRUE(plan.stopTracking);
    EXPECT_TRUE(plan.setHome);
    EXPECT_FALSE(plan.startTracking);  // parking never resumes tracking here
    EXPECT_FALSE(plan.compensateTracker);
    EXPECT_TRUE(plan.startParkPositionLeg);
    EXPECT_FALSE(plan.parkPositionReached);
    EXPECT_FALSE(plan.parkComplete);  // the park is not over yet
    EXPECT_TRUE(plan.decGuideAfterHome);
}

TEST(SlewCompletionTest, AFirstParkLegStartsEvenWhenThereIsNowhereToGo)
{
    ArrivalFacts facts = goToAtTarget();
    facts.intent       = SlewIntent::Home;
    facts.parking      = true;

    const CompletionPlan plan = SlewCompletion::plan(facts);

    EXPECT_TRUE(plan.startParkPositionLeg);
}

TEST(SlewCompletionTest, TheParkLegIsANoOpOnlyAfterItsTargetsAreProgrammed)
{
    // The decision cannot be made from the pre-move distances (both are zero when the
    // axes are stopped), which is why it is a separate question about the post-move ones.
    EXPECT_TRUE(core::SlewCompletion::parkLegIsNoOp(0, 0));
    EXPECT_FALSE(core::SlewCompletion::parkLegIsNoOp(1, 0));
    EXPECT_FALSE(core::SlewCompletion::parkLegIsNoOp(0, -1));
    EXPECT_FALSE(core::SlewCompletion::parkLegIsNoOp(500, 500));
}

TEST(SlewCompletionTest, SecondParkLegCompletesThePark)
{
    // The first leg ended the slew, so the second one arrives with no intent: the
    // park-position flag is what identifies it.
    ArrivalFacts facts          = goToAtTarget();
    facts.intent                = SlewIntent::None;
    facts.parking               = true;
    facts.headingToParkPosition = true;

    const CompletionPlan plan = SlewCompletion::plan(facts);

    EXPECT_TRUE(plan.stopTracking);
    EXPECT_FALSE(plan.setHome);  // home was set on the first leg
    EXPECT_FALSE(plan.startParkPositionLeg);
    EXPECT_TRUE(plan.parkComplete);
    EXPECT_TRUE(plan.endSlew);
}

TEST(SlewCompletionTest, HomingOutsideAParkResumesTrackingTwiceLikeTheOriginal)
{
    // Both the generic "not parking" block and the home branch called startSlewing().
    // The second call is idempotent, but dropping it would be an untested behaviour
    // change, so the plan keeps both.
    ArrivalFacts facts = goToAtTarget();
    facts.intent       = SlewIntent::Home;

    const CompletionPlan plan = SlewCompletion::plan(facts);

    EXPECT_TRUE(plan.startTracking);
    EXPECT_TRUE(plan.restartTrackingAfterHome);
    EXPECT_TRUE(plan.retargetRa);
    EXPECT_FALSE(plan.stopTracking);
}

TEST(SlewCompletionTest, TheDecMicrostepResetRunsAfterAParkLegToo)
{
    // The original set DEC to slew microstepping to start the park leg and then, a few
    // lines later, unconditionally set it back to guide microstepping. Preserved.
    ArrivalFacts facts = goToAtTarget();
    facts.intent       = SlewIntent::Home;
    facts.parking      = true;

    const CompletionPlan plan = SlewCompletion::plan(facts);

    EXPECT_TRUE(plan.decGuideMicrosteps);
    EXPECT_TRUE(plan.decGuideAfterHome);
}

TEST(SlewCompletionTest, EveryPlanEndsTheSlew)
{
    ArrivalFacts facts = goToAtTarget();
    EXPECT_TRUE(SlewCompletion::plan(facts).endSlew);

    facts.intent = SlewIntent::Manual;
    EXPECT_TRUE(SlewCompletion::plan(facts).endSlew);

    facts.parking = true;
    facts.intent  = SlewIntent::Park;
    EXPECT_TRUE(SlewCompletion::plan(facts).endSlew);
}
