#pragma once

#include <stdint.h>

namespace ports
{

/// Configuration access to one stepper driver.
///
/// Names mirror the TMCStepper register names because that is the vocabulary the
/// mount's tuning values are written in; a driver that has no such registers
/// (A4988, or a TMC in standalone mode) is represented by a null implementation
/// instead of a `#ifdef`.
class IMotorDriver
{
  public:
    virtual ~IMotorDriver()
    {
    }

    virtual void begin() = 0;

    virtual void beginSerial(uint32_t baudRate) = 0;

    // --- current and chopper configuration ---------------------------------

    virtual void microsteps(uint16_t value) = 0;

    virtual void rms_current(uint16_t milliamps) = 0;

    virtual void rms_current(uint16_t milliamps, float holdMultiplier) = 0;

    virtual void toff(uint8_t value) = 0;

    virtual void blank_time(uint8_t value) = 0;

    virtual void vsense(bool value) = 0;

    virtual void semin(uint8_t value) = 0;

    virtual void iholddelay(uint8_t value) = 0;

    virtual void TPOWERDOWN(uint8_t value) = 0;

    // --- stall guard and stealth chop --------------------------------------

    virtual void TCOOLTHRS(uint32_t value) = 0;

    virtual void SGTHRS(uint8_t value) = 0;

    virtual void en_spreadCycle(bool value) = 0;

    // --- driver configuration ---------------------------------------------

    virtual void pdn_disable(bool value) = 0;

    virtual void mstep_reg_select(bool value) = 0;

    virtual void I_scale_analog(bool value) = 0;

    virtual void fclktrim(uint8_t value) = 0;

    // --- read-back ---------------------------------------------------------

    virtual uint16_t rms_current() = 0;

    virtual uint16_t cs_actual() = 0;

    virtual bool vsense() = 0;
};

}  // namespace ports
