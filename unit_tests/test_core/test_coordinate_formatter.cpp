#include <gtest/gtest.h>
#include <string.h>
#include "core/CoordinateFormatter.hpp"
#include "types/RightAscension.hpp"
#include "types/Declination.hpp"

using types::Declination;
using types::RightAscension;

TEST(CoordinateFormatterTest, FormatRABasic)
{
    RightAscension ra(12, 34, 56);
    char buf[32];
    core::formatRA(buf, sizeof(buf), ra, "%02d:%02d:%02d#");
    EXPECT_STREQ("12:34:56#", buf);
}

TEST(CoordinateFormatterTest, FormatRAPadded)
{
    RightAscension ra(5, 7, 9);
    char buf[32];
    core::formatRA(buf, sizeof(buf), ra, "%02d:%02d:%02d#");
    EXPECT_STREQ("05:07:09#", buf);
}

TEST(CoordinateFormatterTest, FormatRALcdStyle)
{
    RightAscension ra(23, 59, 59);
    char buf[32];
    core::formatRA(buf, sizeof(buf), ra, " %02dh %02dm %02ds");
    EXPECT_STREQ(" 23h 59m 59s", buf);
}

TEST(CoordinateFormatterTest, FormatDECMeade)
{
    Declination dec(45, 30, 15);
    char buf[32];
    core::formatDEC(buf, dec, "{d}*{m}'{s}#");
    EXPECT_STREQ("+45*30'15#", buf);
}

TEST(CoordinateFormatterTest, FormatDECLcd)
{
    Declination dec(-15, 0, 0);
    char buf[32];
    core::formatDEC(buf, dec, " {d}@ {m}' {s}\"");
    EXPECT_STREQ(" -15@ 00' 00\"", buf);
}

TEST(CoordinateFormatterTest, FormatDECCompact)
{
    Declination dec(5, 30, 0);
    char buf[32];
    core::formatDEC(buf, dec, "{d}{m}{s}");
    EXPECT_STREQ("+053000", buf);
}

TEST(CoordinateFormatterTest, FormatRATruncatesToBuffer)
{
    RightAscension ra(12, 34, 56);
    char buf[8];
    core::formatRA(buf, sizeof(buf), ra, "%02d:%02d:%02d#");
    EXPECT_STREQ("12:34:5", buf);
}

TEST(CoordinateFormatterTest, FormatRAFitsMountScratchBuffer)
{
    // Mount::scratchBuffer is 24 bytes; the longest RA format must fit.
    RightAscension ra(23, 59, 59);
    char buf[24];
    core::formatRA(buf, sizeof(buf), ra, " %02dh %02dm %02ds");
    EXPECT_STREQ(" 23h 59m 59s", buf);
}
