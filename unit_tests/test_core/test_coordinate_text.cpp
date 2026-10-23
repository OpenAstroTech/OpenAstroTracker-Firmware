#include <gtest/gtest.h>

#include <cstring>

#include "core/CoordinateText.hpp"

TEST(CoordinateTextTest, RaFormatsAreTheHistoricalStrings)
{
    EXPECT_STREQ(" %02dh %02dm %02ds", core::raFormat(core::CoordinateFormat::LcdMenu));
    EXPECT_STREQ("%02d:%02d:%02d#", core::raFormat(core::CoordinateFormat::Meade));
    EXPECT_STREQ("%02dh %02dm %02ds", core::raFormat(core::CoordinateFormat::Print));
    EXPECT_STREQ("%02dh%02dm%02ds", core::raFormat(core::CoordinateFormat::Lcd));
    EXPECT_STREQ("%02d%02d%02d", core::raFormat(core::CoordinateFormat::Compact));
}

TEST(CoordinateTextTest, DecFormatsAreTheHistoricalStrings)
{
    EXPECT_STREQ(" {d}@ {m}' {s}\"", core::decFormat(core::CoordinateFormat::LcdMenu));
    EXPECT_STREQ("{d}*{m}'{s}#", core::decFormat(core::CoordinateFormat::Meade));
    EXPECT_STREQ("{d} {m}'{s}\"", core::decFormat(core::CoordinateFormat::Print));
    EXPECT_STREQ("{d}@{m}'{s}\"", core::decFormat(core::CoordinateFormat::Lcd));
    EXPECT_STREQ("{d}{m}{s}", core::decFormat(core::CoordinateFormat::Compact));
}

TEST(CoordinateTextTest, OnlyTheMeadeFormatCarriesTheTerminator)
{
    EXPECT_EQ('#', core::raFormat(core::CoordinateFormat::Meade)[std::strlen(core::raFormat(core::CoordinateFormat::Meade)) - 1]);
    EXPECT_EQ('#', core::decFormat(core::CoordinateFormat::Meade)[std::strlen(core::decFormat(core::CoordinateFormat::Meade)) - 1]);
    EXPECT_EQ(nullptr, std::strchr(core::raFormat(core::CoordinateFormat::Print), '#'));
}

TEST(CoordinateTextTest, TypeByteDecodesToItsFormat)
{
    EXPECT_EQ(core::CoordinateFormat::Compact, core::coordinateFormat(core::kTargetFlag | 5));
    EXPECT_EQ(core::CoordinateFormat::LcdMenu, core::coordinateFormat(1));
    EXPECT_EQ(core::CoordinateFormat::Meade, core::coordinateFormat(core::kTargetFlag | 2));
}

TEST(CoordinateTextTest, OutOfRangeFormatsFallBackToTheEmptyString)
{
    EXPECT_STREQ("", core::raFormat(core::CoordinateFormat {0}));
    EXPECT_STREQ("", core::decFormat(core::CoordinateFormat {0}));
    EXPECT_STREQ("", core::raFormat(core::CoordinateFormat {9}));
    EXPECT_STREQ("", core::decFormat(core::CoordinateFormat {9}));
}

TEST(CoordinateTextTest, RaCursorLandsOnTheStartOfEachField)
{
    char line[] = " 12h 34m 56s";

    for (uint8_t active = 0; active < 3; ++active)
    {
        char copy[sizeof(line)];
        std::memcpy(copy, line, sizeof(line));
        core::insertLcdMenuCursor(copy, active, /*hasSignColumn=*/false);
        EXPECT_EQ('>', copy[active * 4]) << "active=" << int(active);
    }
}

TEST(CoordinateTextTest, DecCursorSkipsTheSignColumnAfterTheFirstField)
{
    // " +12@ 34' 56\"" — the sign sits where the cursor would go, so fields 1 and 2 shift by one.
    char line[] = " +12@ 34' 56\"";

    char copy[sizeof(line)];
    std::memcpy(copy, line, sizeof(line));
    core::insertLcdMenuCursor(copy, 0, /*hasSignColumn=*/true);
    EXPECT_EQ('>', copy[0]);

    // Field 1 begins at 4 with the '@' separator, so the cursor takes the space after it.
    std::memcpy(copy, line, sizeof(line));
    core::insertLcdMenuCursor(copy, 1, /*hasSignColumn=*/true);
    EXPECT_EQ('>', copy[5]);
    EXPECT_EQ('@', copy[4]);

    std::memcpy(copy, line, sizeof(line));
    core::insertLcdMenuCursor(copy, 2, /*hasSignColumn=*/true);
    EXPECT_EQ('>', copy[9]);
}

TEST(CoordinateTextTest, CursorTouchesOnlyOneCharacter)
{
    char line[] = " 12h 34m 56s";
    char copy[sizeof(line)];
    std::memcpy(copy, line, sizeof(line));

    core::insertLcdMenuCursor(copy, 1, false);

    int differences = 0;
    for (size_t i = 0; i < sizeof(line); ++i)
    {
        if (copy[i] != line[i])
        {
            ++differences;
        }
    }
    EXPECT_EQ(1, differences);
}

TEST(CoordinateTextTest, ANullBufferIsIgnored)
{
    core::insertLcdMenuCursor(nullptr, 1, true);  // must not crash
}
