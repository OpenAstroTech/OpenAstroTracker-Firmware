#include <gtest/gtest.h>

#include "core/ButtonDebounce.hpp"

using core::Button;
using core::ButtonDebounce;

TEST(ButtonDebounceTest, StartsOnNoKey)
{
    ButtonDebounce debounce;

    EXPECT_EQ(Button::None, debounce.committed());
}

TEST(ButtonDebounceTest, AStableKeyCommitsAfterTheHoldWindow)
{
    ButtonDebounce debounce;

    debounce.update(Button::Up, 100);
    EXPECT_EQ(Button::None, debounce.committed());

    debounce.update(Button::Up, 105);
    EXPECT_EQ(Button::None, debounce.committed());

    debounce.update(Button::Up, 106);
    EXPECT_EQ(Button::Up, debounce.committed());
}

TEST(ButtonDebounceTest, BounceNeverCommits)
{
    ButtonDebounce debounce;

    // Contact bounce: every reading is shorter than the window, so nothing is committed.
    debounce.update(Button::Up, 0);
    debounce.update(Button::None, 2);
    debounce.update(Button::Up, 4);
    debounce.update(Button::None, 6);
    debounce.update(Button::Up, 8);

    EXPECT_EQ(Button::None, debounce.committed());
}

TEST(ButtonDebounceTest, HeldKeyIsReportedOnlyOnce)
{
    ButtonDebounce debounce;
    Button reported = Button::None;

    debounce.update(Button::Select, 0);
    debounce.update(Button::Select, 10);

    EXPECT_TRUE(debounce.changed(&reported));
    EXPECT_EQ(Button::Select, reported);

    EXPECT_FALSE(debounce.changed(&reported));
    EXPECT_FALSE(debounce.changed(&reported));
}

TEST(ButtonDebounceTest, ReleaseIsAnEdgeToo)
{
    ButtonDebounce debounce;
    Button reported = Button::None;

    debounce.update(Button::Left, 0);
    debounce.update(Button::Left, 10);
    ASSERT_TRUE(debounce.changed(&reported));

    debounce.update(Button::None, 20);  // the release itself is not stable yet
    EXPECT_FALSE(debounce.changed(&reported));

    debounce.update(Button::None, 26);
    ASSERT_TRUE(debounce.changed(&reported));
    EXPECT_EQ(Button::None, reported);
}

TEST(ButtonDebounceTest, TheWindowSurvivesTheMillisecondWrap)
{
    ButtonDebounce debounce;
    const uint32_t nearWrap = 0xFFFFFFF0u;

    debounce.update(Button::Down, nearWrap);
    EXPECT_EQ(Button::None, debounce.committed());

    // 0x00000002 - 0xFFFFFFF0 is 18 ms in unsigned arithmetic, i.e. past the window.
    debounce.update(Button::Down, nearWrap + 18);
    EXPECT_EQ(Button::Down, debounce.committed());
}

TEST(ButtonDebounceTest, CommittedIsTheLastStableReadingNotTheCurrentOne)
{
    ButtonDebounce debounce;

    debounce.update(Button::Right, 0);
    debounce.update(Button::Right, 10);
    ASSERT_EQ(Button::Right, debounce.committed());

    // A new unstable reading must not disturb the committed key.
    debounce.update(Button::Left, 12);
    EXPECT_EQ(Button::Right, debounce.committed());
}
