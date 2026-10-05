// Tests for the grammar primitives shared by the Meade family dispatchers
// (`Cursor`) and for the coordinate writers that turn parsed values back into
// wire bytes.
//
// These sit below the family dispatchers: the wire-byte behaviour of `:Sd`,
// `:St` and `:Sg` is covered in test_MeadeSet.cpp / test_MeadeGet.cpp. What is
// pinned here is the primitive that keeps a coordinate's sign in a channel of
// its own, so that a zero magnitude can still be negative.

#include <gtest/gtest.h>

#include "core/meade/MeadeParserHelpers.hpp"

namespace meade = oat::core::meade;

namespace
{

const char *bytes(const meade::MeadeResponse &r)
{
    return r.c_str();
}

}  // namespace

// ---- Cursor::optionalSign ---------------------------------------------

TEST(MeadeParserHelpers, optional_sign_consumes_minus)
{
    meade::Cursor c("-42");
    int sign = 0;
    EXPECT_TRUE(c.optionalSign(sign));
    EXPECT_EQ(-1, sign);
    EXPECT_STREQ("42", c.remaining());
}

TEST(MeadeParserHelpers, optional_sign_consumes_plus)
{
    meade::Cursor c("+42");
    int sign = 0;
    EXPECT_TRUE(c.optionalSign(sign));
    EXPECT_EQ(1, sign);
    EXPECT_STREQ("42", c.remaining());
}

TEST(MeadeParserHelpers, optional_sign_absent_leaves_cursor_put)
{
    meade::Cursor c("42");
    int sign = 0;
    EXPECT_TRUE(c.optionalSign(sign));
    EXPECT_EQ(1, sign);
    EXPECT_STREQ("42", c.remaining());
}

TEST(MeadeParserHelpers, optional_sign_at_end_of_input)
{
    meade::Cursor c("");
    int sign = 0;
    EXPECT_TRUE(c.optionalSign(sign));
    EXPECT_EQ(1, sign);
    EXPECT_TRUE(c.atEnd());
}

TEST(MeadeParserHelpers, optional_sign_keeps_sign_separate_from_magnitude)
{
    // The whole point of the primitive: "-00" carries a sign that no signed
    // two-digit magnitude could hold.
    meade::Cursor c("-00");
    int sign     = 0;
    unsigned mag = 99;
    EXPECT_TRUE(c.optionalSign(sign));
    EXPECT_TRUE(c.digits(2, mag));
    EXPECT_EQ(-1, sign);
    EXPECT_EQ(0u, mag);
}

// ---- Cursor::advance ---------------------------------------------------

TEST(MeadeParserHelpers, advance_moves_one_character)
{
    meade::Cursor c("abc");
    c.advance();
    EXPECT_EQ('b', c.peek());
}

TEST(MeadeParserHelpers, advance_at_end_is_a_no_op)
{
    meade::Cursor c("");
    c.advance();
    EXPECT_TRUE(c.atEnd());
}

// ---- Coordinate writers ------------------------------------------------

TEST(MeadeParserHelpers, write_dec_emits_sign_for_zero_degrees)
{
    meade::MeadeResponse r;
    writeDec(r, meade::DecCoordinate {0, 30, 0, true});
    EXPECT_STREQ("-00*30'00#", bytes(r));

    meade::MeadeResponse r2;
    writeDec(r2, meade::DecCoordinate {0, 30, 0, false});
    EXPECT_STREQ("+00*30'00#", bytes(r2));
}

TEST(MeadeParserHelpers, write_latitude_emits_sign_for_zero_degrees)
{
    meade::MeadeResponse r;
    writeLatitude(r, meade::MeadeLatitude {0, 30, true});
    EXPECT_STREQ("-00*30#", bytes(r));
}

// The struct is east-positive and the wire is east-negative, so the sign
// inverts on the way out: `negative` (west of Greenwich) emits '+'.
TEST(MeadeParserHelpers, write_longitude_pads_to_three_digits_and_inverts_the_sign)
{
    meade::MeadeResponse r;
    writeLongitude(r, meade::MeadeLongitude {0, 5, true});
    EXPECT_STREQ("+000*05#", bytes(r));

    meade::MeadeResponse r2;
    writeLongitude(r2, meade::MeadeLongitude {122, 45, false});
    EXPECT_STREQ("-122*45#", bytes(r2));
}

// Greenwich is on neither side, and "-000*00" would read as a negative zero,
// so the zero meridian always goes out positive.
TEST(MeadeParserHelpers, write_longitude_emits_greenwich_as_positive)
{
    meade::MeadeResponse r;
    writeLongitude(r, meade::MeadeLongitude {0, 0, false});
    EXPECT_STREQ("+000*00#", bytes(r));

    meade::MeadeResponse r2;
    writeLongitude(r2, meade::MeadeLongitude {0, 0, true});
    EXPECT_STREQ("+000*00#", bytes(r2));
}
