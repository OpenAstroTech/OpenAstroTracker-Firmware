// Smoke tests for the hand-written HAL fakes that every adapter test builds on.
// They pin the fake behaviour the adapter tests rely on, so a broken fake fails here
// instead of looking like a broken adapter.

#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeEeprom.hpp"
#include "../test_common/hal_fakes/FakeGpioPin.hpp"
#include "../test_common/hal_fakes/FakeSerialPort.hpp"
#include "../test_common/hal_fakes/FakeSystemClock.hpp"

TEST(FakeSystemClockTest, DelayAdvancesTime)
{
    fakes::FakeSystemClock clock;

    clock.delay(250);

    EXPECT_EQ(250u, clock.millis());
    EXPECT_EQ(1u, clock.delaysRequested());
}

TEST(FakeSystemClockTest, MicroDelayAdvancesMicrosOnly)
{
    fakes::FakeSystemClock clock;

    clock.delayMicroseconds(500);

    EXPECT_EQ(0u, clock.millis());
    EXPECT_EQ(500u, clock.micros());
    EXPECT_EQ(1u, clock.microDelaysRequested());
}

TEST(FakeClockTest, DelayRecordsWithoutBlocking)
{
    fakes::FakeClock clock;

    clock.delay(1000);

    EXPECT_EQ(1000u, clock.now());
    EXPECT_EQ(1u, clock.delaysRequested());
}

TEST(FakeGpioPinTest, RecordsConfigurationAndOutput)
{
    fakes::FakeGpioPin pin;

    pin.configure(hal::PinMode::Output);
    pin.write(hal::PinLevel::High);

    EXPECT_TRUE(pin.isConfigured());
    EXPECT_EQ(hal::PinMode::Output, pin.mode());
    EXPECT_EQ(hal::PinLevel::High, pin.level());
    EXPECT_EQ(1u, pin.writeCount());
}

TEST(FakeGpioPinTest, ReadsWhateverTheTestSets)
{
    fakes::FakeGpioPin pin;

    pin.setInputLevel(hal::PinLevel::High);
    EXPECT_EQ(hal::PinLevel::High, pin.read());

    pin.setAnalogValue(512);
    EXPECT_EQ(512u, pin.analogRead());
}

TEST(FakeSerialPortTest, PlaysBackFedBytes)
{
    fakes::FakeSerialPort port;
    port.feed("P");

    ASSERT_EQ(1, port.available());
    EXPECT_EQ('P', port.peek());
    EXPECT_EQ('P', port.read());
    EXPECT_EQ(-1, port.read());
}

TEST(FakeSerialPortTest, CollectsWritesAndFlushes)
{
    fakes::FakeSerialPort port;

    port.write('O');
    port.write(reinterpret_cast<const uint8_t *>("K"), 1);
    port.flush();

    EXPECT_EQ("OK", port.output());
    EXPECT_EQ(1u, port.flushCount());

    port.clearOutput();
    EXPECT_EQ("", port.output());
}

TEST(FakeEepromTest, ErasedByDefaultAndAddressable)
{
    fakes::FakeEeprom eeprom(64);

    EXPECT_EQ(64u, eeprom.length());
    EXPECT_EQ(0xFF, eeprom.read(0));

    eeprom.write(5, 0x42);
    EXPECT_EQ(0x42, eeprom.read(5));
    EXPECT_EQ(1u, eeprom.writeCount());
}

TEST(FakeEepromTest, UpdateSkipsIdenticalValues)
{
    fakes::FakeEeprom eeprom(16);

    eeprom.update(3, 0x7E);
    eeprom.update(3, 0x7E);

    EXPECT_EQ(0x7E, eeprom.read(3));
    EXPECT_EQ(1u, eeprom.writeCount());
}

TEST(FakeEepromTest, CommitCanBeMadeToFail)
{
    fakes::FakeEeprom eeprom(16);

    EXPECT_TRUE(eeprom.commit());
    eeprom.setFailCommit(true);
    EXPECT_FALSE(eeprom.commit());
    EXPECT_EQ(2u, eeprom.commitCount());
}

TEST(FakeEepromTest, BeginReservesAndErases)
{
    fakes::FakeEeprom eeprom(16);
    eeprom.write(0, 0x11);

    eeprom.begin(32);

    EXPECT_EQ(32u, eeprom.length());
    EXPECT_EQ(0xFF, eeprom.read(0));
    EXPECT_EQ(1u, eeprom.beginCount());
}
