#include <gtest/gtest.h>

#include "core/menu/MenuModel.hpp"

using core::MenuEntry;
using core::MenuModel;

namespace
{

MenuEntry entries[6];

MenuModel makeModel()
{
    MenuModel model(entries, 6);
    model.add("RA", 1);
    model.add("DEC", 2);
    model.add("HA", 3);
    return model;
}

}  // namespace

TEST(MenuModelTest, EmptyMenuHasNoActiveIdAndNoLongestText)
{
    MenuModel model(entries, 6);

    EXPECT_EQ(0, model.count());
    EXPECT_EQ(0, model.activeId());
    EXPECT_EQ(0, model.longestDisplay());
    EXPECT_EQ(nullptr, model.at(0));
    EXPECT_EQ(nullptr, model.findById(1));
}

TEST(MenuModelTest, AddRemembersOrderIdAndLongestText)
{
    MenuModel model = makeModel();

    ASSERT_EQ(3, model.count());
    EXPECT_STREQ("RA", model.at(0)->display);
    EXPECT_EQ(1, model.at(0)->id);
    EXPECT_STREQ("DEC", model.at(1)->display);
    EXPECT_EQ(3, model.longestDisplay());
}

TEST(MenuModelTest, AddFailsWhenTheCallersTableIsFull)
{
    MenuModel model(entries, 2);

    EXPECT_TRUE(model.add("RA", 1));
    EXPECT_TRUE(model.add("DEC", 2));
    EXPECT_FALSE(model.add("HA", 3));
    EXPECT_EQ(2, model.count());
}

TEST(MenuModelTest, FindByIdReturnsNullForAnUnknownId)
{
    MenuModel model = makeModel();

    const MenuEntry *found = model.findById(2);
    ASSERT_NE(nullptr, found);
    EXPECT_STREQ("DEC", found->display);
    EXPECT_EQ(nullptr, model.findById(99));
}

TEST(MenuModelTest, SetActiveSelectsTheItemAndReportsAnUnknownId)
{
    MenuModel model = makeModel();

    EXPECT_TRUE(model.setActive(3));
    EXPECT_EQ(3, model.activeId());
    EXPECT_EQ(2, model.activeIndex());

    EXPECT_FALSE(model.setActive(42));
    EXPECT_EQ(3, model.activeId());
}

TEST(MenuModelTest, NextWrapsPastTheLastItem)
{
    MenuModel model = makeModel();

    model.next();
    EXPECT_EQ(2, model.activeId());
    model.next();
    EXPECT_EQ(3, model.activeId());
    model.next();
    EXPECT_EQ(1, model.activeId());
}

TEST(MenuModelTest, NextDoesNothingForAnEmptyOrSingleItemMenu)
{
    MenuModel empty(entries, 6);
    empty.next();
    EXPECT_EQ(0, empty.activeId());

    MenuModel single(entries, 6);
    single.add("RA", 1);
    single.next();
    EXPECT_EQ(1, single.activeId());
    EXPECT_EQ(0, single.activeIndex());
}
