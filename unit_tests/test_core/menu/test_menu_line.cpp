#include <gtest/gtest.h>

#include <string.h>
#include <string>

#include "core/menu/MenuLine.hpp"
#include "core/menu/MenuModel.hpp"

using core::composeMenuRow;
using core::MenuEntry;
using core::MenuModel;

namespace
{

constexpr uint8_t kColumns = 16;

MenuEntry entries[11];
char row[kColumns + 1];

std::string compose(MenuModel &model, uint8_t columns = kColumns)
{
    composeMenuRow(row, sizeof(row), model, columns);
    return std::string(row);
}

MenuModel makeMenu(const char *const *names, uint8_t count)
{
    MenuModel model(entries, 11);
    for (uint8_t i = 0; i < count; i++)
    {
        model.add(names[i], static_cast<uint8_t>(i + 1));
    }
    return model;
}

/// The shipped default menu, in the order `app/main.cpp` adds it.
MenuModel makeShippedMenu()
{
    static const char *const names[] = {"RA", "DEC", "GO", "HA", "CTRL", "CAL", "FOC", "INFO"};
    return makeMenu(names, 8);
}

}  // namespace

// The expected rows below are the output of the pre-refactor `LcdMenu::updateDisplay`, captured as
// the oracle for this move: same selection markers, same centring, same padding.
TEST(MenuLineTest, ActiveFirstItemIsCentredWithTheLongestText)
{
    static const char *const names[] = {"RA", "DEC", "GO"};
    MenuModel model                  = makeMenu(names, 3);

    EXPECT_EQ("      >RA< DEC  ", compose(model));
}

TEST(MenuLineTest, ActiveMiddleItemKeepsTheArrowsInTheSameColumns)
{
    static const char *const names[] = {"RA", "DEC", "GO"};
    MenuModel model                  = makeMenu(names, 3);
    model.setActive(2);

    EXPECT_EQ("   RA >DEC< GO  ", compose(model));
}

TEST(MenuLineTest, ActiveLastItemScrollsTheListAndPadsTheEnd)
{
    static const char *const names[] = {"RA", "DEC", "GO"};
    MenuModel model                  = makeMenu(names, 3);
    model.setActive(3);

    EXPECT_EQ("  DEC >GO<      ", compose(model));
}

TEST(MenuLineTest, ShippedMenuFirstItem)
{
    MenuModel model = makeShippedMenu();

    EXPECT_EQ("     >RA< DEC   ", compose(model));
}

TEST(MenuLineTest, ShippedMenuMiddleItems)
{
    MenuModel model = makeShippedMenu();

    model.setActive(4);
    EXPECT_EQ("  GO >HA< CTRL  ", compose(model));

    model.setActive(5);
    EXPECT_EQ("  HA >CTRL< CAL ", compose(model));
}

TEST(MenuLineTest, ShippedMenuLastItem)
{
    MenuModel model = makeShippedMenu();
    model.setActive(8);

    EXPECT_EQ(" FOC >INFO<     ", compose(model));
}

TEST(MenuLineTest, SingleItemIsCentred)
{
    static const char *const names[] = {"RA"};
    MenuModel model                  = makeMenu(names, 1);

    EXPECT_EQ("      >RA<      ", compose(model));
}

TEST(MenuLineTest, ItemLongerThanTheRowStillFitsTheArrows)
{
    static const char *const names[] = {"CALIBRATION", "RA"};
    MenuModel model                  = makeMenu(names, 2);

    EXPECT_EQ("  >CALIBRATION< ", compose(model));
}

TEST(MenuLineTest, EmptyMenuIsAllSpaces)
{
    MenuModel model(entries, 11);

    EXPECT_EQ("                ", compose(model));
}

TEST(MenuLineTest, NarrowRowClipsTheLeadingItem)
{
    static const char *const names[] = {"RA", "DEC"};
    MenuModel model                  = makeMenu(names, 2);
    model.setActive(2);

    EXPECT_EQ("A >DEC< ", compose(model, 8));
}

TEST(MenuLineTest, RowIsExactlyColumnsWideAndLeavesTheLastColumnForTheIndicator)
{
    MenuModel model = makeShippedMenu();

    for (uint8_t i = 0; i < model.count(); i++)
    {
        model.setActive(model.at(i)->id);
        const std::string text = compose(model);
        EXPECT_EQ(kColumns, text.size());
        EXPECT_EQ(' ', text[kColumns - 1]);
    }
}

TEST(MenuLineTest, ShortBufferTruncatesInsteadOfOverrunning)
{
    static const char *const names[] = {"RA", "DEC"};
    MenuModel model                  = makeMenu(names, 2);

    char small[5] = {'x', 'x', 'x', 'x', 'x'};
    composeMenuRow(small, sizeof(small), model, kColumns);
    EXPECT_EQ(4u, strlen(small));
    EXPECT_EQ('\0', small[4]);
}
