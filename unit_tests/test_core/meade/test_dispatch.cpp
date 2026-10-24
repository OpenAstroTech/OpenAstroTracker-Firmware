// Routing tests for `dispatchMeadeCommand`.
//
// The family handlers have their own wire-byte tests, but each of those calls
// `handleMeadeXxx()` directly. Nothing checked that the *dispatcher* sends a command to the
// right family — a wrong `case` label in one switch would leave every family test green and
// still break the protocol. These tests drive `:Xxx#` strings through the real entry point and
// assert which callback fired.

#include <gtest/gtest.h>

#include <string>

#include "core/meade/MeadeParser.hpp"
#include "../test_common/meade_fakes/StubMeadeHandlers.hpp"

namespace meade = oat::core::meade;

namespace
{
/// Dispatches one command and returns the name of the callback it reached, or "" for none.
std::string routeOf(const char *command, fakes::StubMeadeHandlers &handlers)
{
    meade::MeadeResponse response;
    meade::dispatchMeadeCommand(response, command, handlers);
    return handlers.lastCall != nullptr ? handlers.lastCall : "";
}
}  // namespace

TEST(MeadeDispatchTest, SetFamilyReachesASetCallback)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("onSetTargetRa", routeOf(":Sr05:30:00#", handlers));
}

TEST(MeadeDispatchTest, MovementFamilyReachesAMovementCallback)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("onStartSlewToTarget", routeOf(":MS#", handlers));
}

TEST(MeadeDispatchTest, GetFamilyReachesAGetCallback)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("onCurrentRa", routeOf(":GR#", handlers));
}

TEST(MeadeDispatchTest, GpsFamilyReachesTheGpsCallback)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("onStartGpsAcquisition", routeOf(":gT#", handlers));
}

TEST(MeadeDispatchTest, SyncFamilyReachesTheSyncCallback)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("onSyncToTarget", routeOf(":CM#", handlers));
}

TEST(MeadeDispatchTest, HomeFamilyReachesAParkCallback)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("onPark", routeOf(":hP#", handlers));
}

TEST(MeadeDispatchTest, InitFamilyReachesTheSerialControlCallback)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("onEnterSerialControl", routeOf(":I#", handlers));
}

TEST(MeadeDispatchTest, QuitFamilyDistinguishesStopAllFromLeavingControlMode)
{
    // `:Q#` stops every axis; leaving the control mode is a different suffix. Both belong to the
    // Quit family, so the router has to keep them apart.
    fakes::StubMeadeHandlers stopAll;
    EXPECT_EQ("onStopAll", routeOf(":Q#", stopAll));

    fakes::StubMeadeHandlers quit;
    EXPECT_EQ("onQuitControlMode", routeOf(":Qq#", quit));
}

TEST(MeadeDispatchTest, SlewRateFamilyReachesTheRateCallback)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("onSetSlewRate", routeOf(":RS#", handlers));
}

TEST(MeadeDispatchTest, DistanceFamilyReachesTheStatusCallback)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("onIsSlewingRaOrDec", routeOf(":D#", handlers));
}

TEST(MeadeDispatchTest, ExtraFamilyReachesAnExtraCallback)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("onDriftAlignment", routeOf(":XD5#", handlers));
}

TEST(MeadeDispatchTest, FocusFamilyReachesAFocusCallback)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("onFocusContinuousIn", routeOf(":F+#", handlers));
}

TEST(MeadeDispatchTest, AnUnknownFamilyReachesNothing)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("", routeOf(":Z#", handlers));
}

TEST(MeadeDispatchTest, AMalformedCommandReachesNothing)
{
    fakes::StubMeadeHandlers handlers;
    EXPECT_EQ("", routeOf("GR", handlers));
    EXPECT_EQ("", routeOf("", handlers));
    EXPECT_EQ("", routeOf(":#", handlers));
}

TEST(MeadeDispatchTest, EveryFamilyReachesAHandler)
{
    // Nothing may fall through the switch silently. (A command need not answer — `:hP#` parks
    // and reports only through the pending-reply table in the real adapter — so reaching a
    // handler is the contract here.)
    const char *commands[] = {":Sr05:30:00#", ":MS#", ":GR#", ":gT#", ":CM#", ":hP#", ":I#", ":Q#", ":RS#", ":D#", ":XD5#", ":F+#"};

    for (const char *command : commands)
    {
        fakes::StubMeadeHandlers handlers;
        meade::MeadeResponse response;
        meade::dispatchMeadeCommand(response, command, handlers);

        EXPECT_TRUE(handlers.lastCall != nullptr) << command << " reached no handler";
    }
}

TEST(MeadeResponseTest, AssigningNullClearsTheBuffer)
{
    meade::MeadeResponse response;
    response.assign("12:34:56#");
    ASSERT_FALSE(response.empty());

    response.assign(nullptr);

    EXPECT_TRUE(response.empty());
    EXPECT_EQ(0u, response.length());
    EXPECT_STREQ("", response.c_str());
}

TEST(MeadeResponseTest, AssigningANullStringTerminatesTheBuffer)
{
    meade::MeadeResponse response;
    response.assign("abc");
    response.assign("de");

    EXPECT_STREQ("de", response.c_str());
    EXPECT_EQ(2u, response.length());
}
