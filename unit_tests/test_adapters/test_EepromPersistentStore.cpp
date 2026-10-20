#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeEeprom.hpp"
#include "adapters/EepromPersistentStore.hpp"

namespace
{
const size_t STORE_SIZE = 512;
}

TEST(EepromPersistentStoreTest, ReservesTheConfiguredSize)
{
    fakes::FakeEeprom eeprom(4096);
    adapters::EepromPersistentStore store(eeprom, STORE_SIZE);

    store.begin();

    EXPECT_EQ(1u, eeprom.beginCount());
    EXPECT_EQ(STORE_SIZE, eeprom.length());
}

TEST(EepromPersistentStoreTest, ReadsAndWritesThrough)
{
    fakes::FakeEeprom eeprom(STORE_SIZE);
    adapters::EepromPersistentStore store(eeprom, STORE_SIZE);

    store.writeByte(7, 0xA5);

    EXPECT_EQ(0xA5, store.readByte(7));
    EXPECT_EQ(0xA5, eeprom.read(7));
}

TEST(EepromPersistentStoreTest, ErasedCellsReadAsFF)
{
    fakes::FakeEeprom eeprom(STORE_SIZE);
    adapters::EepromPersistentStore store(eeprom, STORE_SIZE);

    EXPECT_EQ(0xFF, store.readByte(0));
    EXPECT_EQ(0xFF, store.readByte(STORE_SIZE - 1));
}

TEST(EepromPersistentStoreTest, CommitReachesTheBackend)
{
    fakes::FakeEeprom eeprom(STORE_SIZE);
    adapters::EepromPersistentStore store(eeprom, STORE_SIZE);

    EXPECT_TRUE(store.commit());
    eeprom.setFailCommit(true);
    EXPECT_FALSE(store.commit());
    EXPECT_EQ(2u, eeprom.commitCount());
}
