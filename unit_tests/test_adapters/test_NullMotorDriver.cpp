#include <gtest/gtest.h>

#include "adapters/NullMotorDriver.hpp"

// The null driver exists so the domain can call the port unconditionally on boards
// that have no UART-connected driver. Pin the contract the composition root relies on:
// nothing throws, nothing is reported back, and configuration calls are harmless.

TEST(NullMotorDriverTest, AcceptsConfigurationWithoutSideEffects)
{
    adapters::NullMotorDriver driver;

    driver.begin();
    driver.beginSerial(19200);
    driver.microsteps(16);
    driver.rms_current(900);
    driver.rms_current(900, 1.0f);
    driver.toff(4);
    driver.blank_time(24);
    driver.vsense(true);
    driver.semin(0);
    driver.iholddelay(15);
    driver.TPOWERDOWN(255);
    driver.TCOOLTHRS(0xFFFFF);
    driver.SGTHRS(100);
    driver.en_spreadCycle(false);
    driver.pdn_disable(true);
    driver.mstep_reg_select(true);
    driver.I_scale_analog(false);
    driver.fclktrim(4);

    EXPECT_EQ(0u, driver.rms_current());
    EXPECT_EQ(0u, driver.cs_actual());
    EXPECT_FALSE(driver.vsense());
}
