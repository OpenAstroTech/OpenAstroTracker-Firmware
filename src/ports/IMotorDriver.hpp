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
///
/// Every setter is write-only and takes effect on the driver immediately: there is no
/// read-modify-write and no cached copy, so the value the driver reports through the read-back
/// methods is the driver's own, not the last one this port was handed.
class IMotorDriver
{
  public:
    virtual ~IMotorDriver()
    {
    }

    /// Opens the driver's control channel and applies the library defaults. Safe to call on a
    /// driver that has no registers (the null object does nothing).
    virtual void begin() = 0;

    /// Opens the driver's software-serial link at `baudRate`, where the platform has one.
    virtual void beginSerial(uint32_t baudRate) = 0;

    /// Asks the driver to identify itself. False for a driver that cannot answer
    /// (no UART, or a null driver).
    virtual bool testConnection() = 0;

    // --- current and chopper configuration ---------------------------------

    /// Microstep resolution: 0 selects full step, otherwise 2, 4, 8, 16, 32, 64, 128 or 256.
    /// The mount passes `core::microstepRegisterValue()` so a 1x axis maps to 0.
    virtual void microsteps(uint16_t value) = 0;

    /// Run current, in milliamps RMS. The hold current defaults to the driver's own ratio.
    virtual void rms_current(uint16_t milliamps) = 0;

    /// Run current and the ratio the hold current is scaled by (1.0 holds at the run current).
    virtual void rms_current(uint16_t milliamps, float holdMultiplier) = 0;

    /// Chopper off-time, 1-15. 0 disables the driver's outputs, which is how the boot sequence
    /// switches a channel off before reconfiguring it.
    virtual void toff(uint8_t value) = 0;

    /// Comparator blank time, one of 16, 24, 36 or 54 microseconds: how long the chopper ignores
    /// the comparator after a step edge.
    virtual void blank_time(uint8_t value) = 0;

    /// Chopper voltage-sense range: false for the low range (higher resolution), true for high.
    virtual void vsense(bool value) = 0;

    /// CoolStep minimum StallGuard value. 0 disables CoolStep, which is what keeps the current
    /// constant instead of letting the driver reduce it.
    virtual void semin(uint8_t value) = 0;

    /// Standstill current-delay exponent: how quickly the current drops to the hold value.
    virtual void iholddelay(uint8_t value) = 0;

    /// Standstill delay, in units of the internal clock, before the hold current is applied.
    virtual void TPOWERDOWN(uint8_t value) = 0;

    // --- stall guard and stealth chop --------------------------------------

    /// Velocity threshold below which CoolStep and StallGuard are active, in the driver's
    /// internal velocity units.
    virtual void TCOOLTHRS(uint32_t value) = 0;

    /// StallGuard4 threshold: higher values report a stall sooner (less load).
    virtual void SGTHRS(uint8_t value) = 0;

    /// Chopper mode: true for spreadCycle, false for stealthChop.
    virtual void en_spreadCycle(bool value) = 0;

    // --- driver configuration ---------------------------------------------

    /// True takes the enable input from UART instead of the PDN pin.
    virtual void pdn_disable(bool value) = 0;

    /// True takes the microstep resolution from the register instead of the MS1/MS2 pins.
    virtual void mstep_reg_select(bool value) = 0;

    /// False uses the internal current reference instead of the VREF pin.
    virtual void I_scale_analog(bool value) = 0;

    /// Internal oscillator trim, 0-31.
    virtual void fclktrim(uint8_t value) = 0;

    // --- motion and shaft --------------------------------------------------

    /// Raw velocity mode: drives the shaft at a fixed rate with no step pulses from the MCU, so the
    /// motion never passes through `IStepperAxis`. 0 stops it. Used by the UART self-test.
    virtual void VACTUAL(int32_t value) = 0;

    /// Selects which coil pair the self-test velocity drives (false: one shaft, true: the other).
    virtual void shaft(bool value) = 0;

    // --- read-back ---------------------------------------------------------

    /// The RMS current the driver is configured for, in milliamps.
    virtual uint16_t rms_current() = 0;

    /// The current the chopper is actually delivering, as the driver's internal current-scale
    /// value. Diagnostics only.
    virtual uint16_t cs_actual() = 0;

    /// The chopper's voltage-sense range as the driver reports it.
    virtual bool vsense() = 0;
};

}  // namespace ports
