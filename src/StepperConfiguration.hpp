#pragma once

#include "../Configuration.hpp"

#ifdef NEW_STEPPER_LIB

    #ifdef ARDUINO_AVR_ATmega2560
        #include "Pin.h"
        #include "IntervalInterrupt.h"
        #include "Driver.h"

PUSH_NO_WARNINGS
        #include "Stepper.h"
        #include "InterruptAccelStepper.h"
POP_NO_WARNINGS
    #endif

    #define UINT32(x) static_cast<uint32_t>(x)

namespace config
{
struct Ra {
    constexpr static uint32_t DRIVER_SPR_SLEW = static_cast<uint32_t>(RA_STEPPER_SPR) * static_cast<uint32_t>(RA_SLEW_MICROSTEPPING);
    constexpr static uint32_t DRIVER_SPR_TRK  = static_cast<uint32_t>(RA_STEPPER_SPR) * static_cast<uint32_t>(RA_TRACKING_MICROSTEPPING);

    constexpr static float SPR_SLEW = DRIVER_SPR_SLEW * RA_TRANSMISSION;
    constexpr static float SPR_TRK  = DRIVER_SPR_TRK * RA_TRANSMISSION;

    constexpr static float SPEED_TRK  = SPR_TRK / SIDEREAL_SECONDS_PER_DAY;
    constexpr static float SPEED_SLEW = SPR_SLEW / 360.0f * RA_SLEWING_SPEED_DEG;
    constexpr static float ACCEL_SLEW = SPR_SLEW / 360.0f * RA_SLEWING_ACCELERATION_DEG;

    #ifdef ARDUINO_AVR_ATmega2560
    using pin_step = Pin<RA_STEP_PIN>;
    using pin_dir  = Pin<RA_DIR_PIN>;

    using interrupt = IntervalInterrupt<Timer::TIMER_3>;
    using driver    = Driver<pin_step, pin_dir>;

    using ramp_slew = AccelerationRamp<256, interrupt::FREQ, UINT32(SPEED_SLEW), UINT32(ACCEL_SLEW)>;
    using ramp_trk  = ConstantRamp<interrupt::FREQ>;

    using stepper_slew = Stepper<interrupt, driver, ramp_slew>;
    using stepper_trk  = Stepper<interrupt, driver, ramp_trk>;

    constexpr static float SPEED_COMPENSATION = SPEED_SLEW;
    #else
    constexpr static float SPEED_COMPENSATION = SPEED_SLEW;
    #endif
};

struct Dec {
    constexpr static uint32_t DRIVER_SPR_SLEW = static_cast<uint32_t>(DEC_STEPPER_SPR) * static_cast<uint32_t>(DEC_SLEW_MICROSTEPPING);
    constexpr static uint32_t DRIVER_SPR_TRK  = static_cast<uint32_t>(DEC_STEPPER_SPR) * static_cast<uint32_t>(DEC_GUIDE_MICROSTEPPING);

    constexpr static float SPR_SLEW = DRIVER_SPR_SLEW * DEC_TRANSMISSION;
    constexpr static float SPR_TRK  = DRIVER_SPR_TRK * DEC_TRANSMISSION;

    constexpr static float SPEED_TRK      = 0;
    constexpr static float SPEED_SIDEREAL = SPR_TRK / SIDEREAL_SECONDS_PER_DAY;
    constexpr static float SPEED_SLEW     = SPR_SLEW / 360.0f * DEC_SLEWING_SPEED_DEG;
    constexpr static float ACCEL_SLEW     = SPR_SLEW / 360.0f * DEC_SLEWING_ACCELERATION_DEG;

    #ifdef ARDUINO_AVR_ATmega2560
    using pin_step = Pin<DEC_STEP_PIN>;
    using pin_dir  = Pin<DEC_DIR_PIN>;

    using interrupt = IntervalInterrupt<Timer::TIMER_4>;
    using driver    = Driver<pin_step, pin_dir>;

    using ramp_slew = AccelerationRamp<256, interrupt::FREQ, UINT32(SPEED_SLEW), UINT32(ACCEL_SLEW)>;
    using ramp_trk  = ConstantRamp<interrupt::FREQ>;

    using stepper_slew = Stepper<interrupt, driver, ramp_slew>;
    using stepper_trk  = Stepper<interrupt, driver, ramp_trk>;
    #endif
};

    #if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
struct Az {
    constexpr static float SPEED_SLEW = static_cast<float>(AZ_STEPPER_SPEED);
    constexpr static float ACCEL_SLEW = static_cast<float>(AZ_STEPPER_ACCELERATION);

        #ifdef ARDUINO_AVR_ATmega2560
    using pin_step = Pin<AZ_STEP_PIN>;
    using pin_dir  = Pin<AZ_DIR_PIN>;

    using interrupt = IntervalInterrupt<Timer::TIMER_1>;
    using driver    = Driver<pin_step, pin_dir>;

    using ramp_slew = AccelerationRamp<64, interrupt::FREQ, UINT32(SPEED_SLEW), UINT32(ACCEL_SLEW)>;

    using stepper_slew = Stepper<interrupt, driver, ramp_slew>;
        #endif
};
    #endif

    #if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
struct Alt {
    constexpr static float SPEED_SLEW = static_cast<float>(ALT_STEPPER_SPEED);
    constexpr static float ACCEL_SLEW = static_cast<float>(ALT_STEPPER_ACCELERATION);

        #ifdef ARDUINO_AVR_ATmega2560
    using pin_step = Pin<ALT_STEP_PIN>;
    using pin_dir  = Pin<ALT_DIR_PIN>;

    using interrupt = IntervalInterrupt<Timer::TIMER_5>;
    using driver    = Driver<pin_step, pin_dir>;

    using ramp_slew = AccelerationRamp<64, interrupt::FREQ, UINT32(SPEED_SLEW), UINT32(ACCEL_SLEW)>;

    using stepper_slew = Stepper<interrupt, driver, ramp_slew>;
        #endif
};
    #endif

    #if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
struct Focus {
    constexpr static float SPEED_SLEW = static_cast<float>(FOCUS_STEPPER_SPEED);
    constexpr static float ACCEL_SLEW = static_cast<float>(FOCUS_STEPPER_ACCELERATION);

        #ifdef ARDUINO_AVR_ATmega2560
    using pin_step = Pin<FOCUS_STEP_PIN>;
    using pin_dir  = Pin<FOCUS_DIR_PIN>;

    using interrupt = IntervalInterrupt<Timer::TIMER_5>;
    using driver    = Driver<pin_step, pin_dir>;

    using ramp_slew = AccelerationRamp<64, interrupt::FREQ, UINT32(SPEED_SLEW), UINT32(ACCEL_SLEW)>;

    using stepper_slew = Stepper<interrupt, driver, ramp_slew>;
        #endif
};
    #endif

}  // namespace config

#endif
