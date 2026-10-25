#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeCharLcd.hpp"
#include "../test_common/ports_fakes/FakeMountSettings.hpp"
#include "adapters/LcdMenuDisplay.hpp"

using adapters::LcdMenuDisplay;
using fakes::FakeCharLcd;

namespace
{

constexpr uint8_t kColumns = 16;
constexpr uint8_t kRows    = 2;

struct Fixture {
    FakeCharLcd panel;
    fakes::FakeMountSettings settings;
    LcdMenuDisplay display {panel, &settings, kColumns, kRows, {0, 255}};
};

}  // namespace

TEST(LcdMenuDisplayTest, BeginStartsThePanelAndAppliesTheStoredLevelWithoutWritingItBack)
{
    Fixture f;
    f.settings.brightness = 42;

    EXPECT_TRUE(f.display.begin());
    EXPECT_EQ(1, f.panel.beginCalls);
    EXPECT_EQ(kColumns, f.panel.beginColumns);
    EXPECT_EQ(kRows, f.panel.beginRows);
    EXPECT_EQ(42, f.panel.backlightLevel);
    EXPECT_EQ(42, f.settings.brightness);
}

TEST(LcdMenuDisplayTest, BeginDefinesEverySymbolOnAPanelWithUserGlyphs)
{
    Fixture f;

    f.display.begin();

    for (bool defined : f.panel.definedSymbols)
    {
        EXPECT_TRUE(defined);
    }
}

TEST(LcdMenuDisplayTest, BeginSkipsSymbolDefinitionOnAPanelWithoutUserGlyphs)
{
    Fixture f;
    f.panel.definable = false;

    f.display.begin();

    for (bool defined : f.panel.definedSymbols)
    {
        EXPECT_FALSE(defined);
    }
}

TEST(LcdMenuDisplayTest, WriteLinePadsToThePanelWidth)
{
    Fixture f;
    f.display.begin();

    f.display.writeLine(0, "RA");

    EXPECT_EQ(std::string("RA              "), std::string(f.panel.row(0), kColumns));
}

TEST(LcdMenuDisplayTest, WriteLineTruncatesAnOverlongRow)
{
    Fixture f;
    f.display.begin();

    f.display.writeLine(0, "ABCDEFGHIJKLMNOPQRSTUV");

    EXPECT_EQ(std::string("ABCDEFGHIJKLMNOP"), std::string(f.panel.row(0), kColumns));
}

TEST(LcdMenuDisplayTest, WriteLineSubstitutesTheSymbols)
{
    Fixture f;
    f.display.begin();

    f.display.writeLine(0, ">RA< 12@34' &`");

    EXPECT_EQ(std::string(">RA< 12@34' &`  "), std::string(f.panel.row(0), kColumns));
    EXPECT_EQ(6, f.panel.symbolWrites);  // > < @ ' & ` are all panel glyphs
}

TEST(LcdMenuDisplayTest, AnUnchangedRowIsNotRedrawn)
{
    Fixture f;
    f.display.begin();

    f.display.writeLine(1, "DEC");
    const uint8_t writes = f.panel.charWrites;

    f.display.writeLine(1, "DEC");

    EXPECT_EQ(writes, f.panel.charWrites);
}

TEST(LcdMenuDisplayTest, AChangedRowIsRedrawn)
{
    Fixture f;
    f.display.begin();

    f.display.writeLine(1, "DEC");
    const uint8_t writes = f.panel.charWrites;

    f.display.writeLine(1, "DEC 5");

    EXPECT_GT(f.panel.charWrites, writes);
}

TEST(LcdMenuDisplayTest, WritingACellForcesTheNextFullRowOut)
{
    Fixture f;
    f.display.begin();

    f.display.writeLine(0, "RA");
    f.display.writeCell(0, 15, '&');
    ASSERT_EQ('&', f.panel.row(0)[15]);
    const uint8_t writes = f.panel.charWrites;

    // The row no longer matches its copy, so the unchanged text is painted again - and the marker
    // cell goes back to padding, which is what the next menu frame expects.
    f.display.writeLine(0, "RA");

    EXPECT_GT(f.panel.charWrites, writes);
    EXPECT_EQ(' ', f.panel.row(0)[15]);
}

TEST(LcdMenuDisplayTest, ClearForgetsTheRowCache)
{
    Fixture f;
    f.display.begin();

    f.display.writeLine(0, "RA");
    f.display.clear();
    const uint8_t writes = f.panel.charWrites;

    f.display.writeLine(0, "RA");

    EXPECT_GT(f.panel.charWrites, writes);
}

TEST(LcdMenuDisplayTest, SetBacklightClampsToThePanelRangeAndPersists)
{
    FakeCharLcd panel;
    fakes::FakeMountSettings settings;
    LcdMenuDisplay display(panel, &settings, kColumns, kRows, {2, 250});

    display.setBacklight(0, true);
    EXPECT_EQ(2, display.backlight());
    EXPECT_EQ(2, settings.brightness);

    display.setBacklight(255, true);
    EXPECT_EQ(250, display.backlight());
    EXPECT_EQ(250, settings.brightness);
}

TEST(LcdMenuDisplayTest, PreviewingTheBacklightDoesNotPersist)
{
    FakeCharLcd panel;
    fakes::FakeMountSettings settings;
    settings.brightness = 10;
    LcdMenuDisplay display(panel, &settings, kColumns, kRows, {0, 255});

    display.setBacklight(99, false);

    EXPECT_EQ(99, panel.backlightLevel);
    EXPECT_EQ(10, settings.brightness);
}

TEST(LcdMenuDisplayTest, BacklightRangeReportsThePanelCapability)
{
    FakeCharLcd panel;
    LcdMenuDisplay display(panel, nullptr, kColumns, kRows, {0, 1});

    uint8_t minimum = 0;
    uint8_t maximum = 0;
    display.backlightRange(&minimum, &maximum);

    EXPECT_EQ(0, minimum);
    EXPECT_EQ(1, maximum);
}

TEST(LcdMenuDisplayTest, AWrongRowIsIgnoredRatherThanOverrunningThePanel)
{
    Fixture f;
    f.display.begin();

    f.display.writeLine(5, "RA");

    EXPECT_EQ(0, f.panel.charWrites);
}
