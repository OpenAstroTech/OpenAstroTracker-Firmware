#include <gtest/gtest.h>

#include <string>

#include "app/ui/SlewStatusView.hpp"
#include "../test_common/ports_fakes/FakeDisplay.hpp"
#include "../test_common/ports_fakes/FakeStatusSources.hpp"

using app::SlewStatusView;
using fakes::FakeDisplay;
using fakes::FakeMountReadModel;

namespace
{

/// A read model whose two planned moves and two percentages the test sets directly.
class SlewSources final : public FakeMountReadModel
{
  public:
    float raSteps  = 0.0F;
    float decSteps = 0.0F;
    int raPercent  = 100;
    int decPercent = 100;

    float plannedRaSteps() const override
    {
        return raSteps;
    }

    float plannedDecSteps() const override
    {
        return decSteps;
    }

    bool getStepperProgress(int &ra, int &dec) override
    {
        ra  = raPercent;
        dec = decPercent;
        return (raPercent < 100) || (decPercent < 100);
    }
};

SlewSources sources()
{
    SlewSources read;
    read.raCurrentText  = "12:34:56";
    read.decCurrentText = "+45@12:34";
    read.raTargetText   = "12:00:00";
    read.decTargetText  = "+45@00:00";
    return read;
}

}  // namespace

TEST(SlewStatusViewTest, BothAxesMovingPaintsBothRows)
{
    SlewSources read = sources();
    FakeDisplay display;
    SlewStatusView view;
    read.raSteps    = 100.0F;
    read.decSteps   = 50.0F;
    read.raPercent  = 40;
    read.decPercent = 60;

    view.render(read, false, display);

    EXPECT_EQ("R 12:34:56 40%  ", std::string(display.row(0)));
    EXPECT_EQ("D+45@12:34 60%  ", std::string(display.row(1)));
}

TEST(SlewStatusViewTest, OnlyDecMovingPaintsTheSecondRow)
{
    SlewSources read = sources();
    FakeDisplay display;
    SlewStatusView view;
    read.decSteps   = 50.0F;
    read.decPercent = 25;

    view.render(read, false, display);

    EXPECT_EQ(1, display.lineWrites);  // exactly one row was written
    EXPECT_EQ("D+45@12:34 25%  ", std::string(display.row(1)));
}

TEST(SlewStatusViewTest, OnlyRaMovingDrawsOverTheMenuUnlessSerialOwnsIt)
{
    SlewSources read = sources();
    FakeDisplay serialDisplay;
    FakeDisplay menuDisplay;
    SlewStatusView view;
    read.raSteps   = 100.0F;
    read.raPercent = 10;

    view.render(read, false, menuDisplay);
    EXPECT_EQ("R 12:34:56 10%  ", std::string(menuDisplay.row(1)));

    view.render(read, true, serialDisplay);
    EXPECT_EQ("R 12:34:56 10%  ", std::string(serialDisplay.row(0)));
    EXPECT_EQ("", std::string(serialDisplay.row(1)));  // the untouched row keeps what it had
}

TEST(SlewStatusViewTest, NothingMovingAndNoSerialHostShowsTheCompactCoordinates)
{
    SlewSources read = sources();
    FakeDisplay display;
    SlewStatusView view;
    read.raCurrentText  = "12:34";
    read.decCurrentText = "+45@1";
    read.raTargetText   = "12:34";
    read.decTargetText  = "+45@1";

    view.render(read, false, display);

    // The fake returns the configured text for both flag words, so the row shows what the compact
    // readout would contain: "R<ra> D<dec>".
    EXPECT_EQ("R12:34 D+45@1   ", std::string(display.row(1)));
}

TEST(SlewStatusViewTest, NothingMovingUnderAControlHostShowsBothCoordinates)
{
    SlewSources read = sources();
    FakeDisplay display;
    SlewStatusView view;

    view.render(read, true, display);

    EXPECT_EQ(" RA: 12:34:56   ", std::string(display.row(0)));
    EXPECT_EQ("DEC: +45@12:34  ", std::string(display.row(1)));
}

TEST(SlewStatusViewTest, TheThrottlePaintsOncePerInterval)
{
    SlewSources read = sources();
    FakeDisplay display;
    SlewStatusView view;
    read.raSteps = 100.0F;

    view.renderThrottled(read, false, 10000, display);
    const uint8_t afterFirst = display.lineWrites;
    EXPECT_EQ(1, afterFirst);

    // The legacy comparison is `>`, so the boundary itself does not paint.
    view.renderThrottled(read, false, 10200, display);
    EXPECT_EQ(afterFirst, display.lineWrites);

    view.renderThrottled(read, false, 10201, display);
    EXPECT_EQ(afterFirst + 1, display.lineWrites);
}
