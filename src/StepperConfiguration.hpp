#pragma once
#include "../Configuration.hpp"

#include "Pin.h"
#include "IntervalInterrupt.h"
#include "Driver.h"

PUSH_NO_WARNINGS
#include "Stepper.h"
#include "InterruptAccelStepper.h"
POP_NO_WARNINGS

#define UINT32(x) static_cast<uint32_t>(x)

namespace config
{
    struct Ra
    {
        constexpr static uint32_t DRIVER_SPR_SLEW = static_cast<uint32_t>(RA_STEPPER_SPR) * static_cast<uint32_t>(RA_SLEW_MICROSTEPPING);
        constexpr static uint32_t DRIVER_SPR_TRK = static_cast<uint32_t>(RA_STEPPER_SPR) * static_cast<uint32_t>(RA_TRACKING_MICROSTEPPING);

        constexpr static float SPR_SLEW = DRIVER_SPR_SLEW * RA_TRANSMISSION;
        constexpr static float SPR_TRK = DRIVER_SPR_TRK * RA_TRANSMISSION;

        constexpr static float SPEED_SIDEREAL_SLEW = SPR_SLEW / SIDEREAL_SECONDS_PER_DAY;
        constexpr static float SPEED_SIDEREAL_TRK = SPR_TRK / SIDEREAL_SECONDS_PER_DAY;

        constexpr static float SPEED_TRK = SPEED_SIDEREAL_TRK;
        constexpr static float SPEED_SLEW = SPR_SLEW / 360.0f * RA_SLEWING_SPEED_DEG;
        constexpr static float ACCEL_SLEW = SPR_SLEW / 360.0f * RA_SLEWING_ACCELERATION_DEG;

        using pin_step = Pin<RA_STEP_PIN>;
        using pin_dir = Pin<RA_DIR_PIN>;

        using interrupt = IntervalInterrupt<Timer::TIMER_3>;
        using interrupt_trk = IntervalInterrupt<Timer::TIMER_5>;
        using driver = Driver<pin_step, pin_dir, RA_INVERT_DIR>;

        using ramp_slew = AccelerationRamp<256, interrupt::FREQ, UINT32(SPEED_SLEW), UINT32(ACCEL_SLEW)>;
        using ramp_trk = AccelerationRamp<2, interrupt_trk::FREQ, UINT32(SPEED_TRK), UINT32(SPEED_TRK)>;

        using stepper_slew = Stepper<interrupt, driver, ramp_slew>;
        using stepper_trk = Stepper<interrupt_trk, driver, ramp_trk>;

        constexpr static float SPEED_COMPENSATION = static_cast<float>(interrupt::FREQ) / static_cast<float>(ramp_slew::interval(1)) - (SPEED_TRK * 2.0f);
    };

    struct Dec
    {
        constexpr static uint32_t DRIVER_SPR_SLEW = static_cast<uint32_t>(DEC_STEPPER_SPR) * static_cast<uint32_t>(DEC_SLEW_MICROSTEPPING);
        constexpr static uint32_t DRIVER_SPR_TRK = static_cast<uint32_t>(DEC_STEPPER_SPR) * static_cast<uint32_t>(DEC_GUIDE_MICROSTEPPING);

        constexpr static float SPR_SLEW = DRIVER_SPR_SLEW * DEC_TRANSMISSION;
        constexpr static float SPR_TRK = DRIVER_SPR_TRK * DEC_TRANSMISSION;

        constexpr static float SPEED_SIDEREAL_SLEW = SPR_SLEW / SIDEREAL_SECONDS_PER_DAY;
        constexpr static float SPEED_SIDEREAL_TRK = SPR_TRK / SIDEREAL_SECONDS_PER_DAY;

        constexpr static float SPEED_TRK = SPEED_SIDEREAL_TRK;
        constexpr static float SPEED_SLEW = SPR_SLEW / 360.0f * DEC_SLEWING_SPEED_DEG;
        constexpr static float ACCEL_SLEW = SPR_SLEW / 360.0f * DEC_SLEWING_ACCELERATION_DEG;

        using pin_step = Pin<DEC_STEP_PIN>;
        using pin_dir = Pin<DEC_DIR_PIN>;

        using interrupt = IntervalInterrupt<Timer::TIMER_4>;
        using interrupt_trk = IntervalInterrupt<Timer::TIMER_1>;
        using driver = Driver<pin_step, pin_dir, DEC_INVERT_DIR>;

        using ramp_slew = AccelerationRamp<256, interrupt::FREQ, UINT32(SPEED_SLEW), UINT32(ACCEL_SLEW)>;
        using ramp_trk = AccelerationRamp<2, interrupt_trk::FREQ, UINT32(SPEED_TRK), UINT32(SPEED_TRK)>;

        using stepper_slew = Stepper<interrupt, driver, ramp_slew>;
        using stepper_trk = Stepper<interrupt_trk, driver, ramp_trk>;
    };

    // struct AZ
    // {
    //     constexpr static auto TRANSMISSION = AZ_TRANSMISSION;

    //     constexpr static auto SPR = AZ_STEPPER_SPR;

    //     constexpr static Angle SPEED_SIDEREAL = Angle::deg(360.0f) / SIDEREAL_SECONDS_PER_DAY;

    //     constexpr static Angle SPEED_TRACKING = Angle::deg(0.0f);
    //     constexpr static Angle SPEED_SLEWING = Angle::deg(AZ_SLEWING_SPEED);
    //     constexpr static Angle ACCELERATION = Angle::deg(AZ_SLEWING_ACCELERATION);

    //     constexpr static Angle SPEED_GUIDE_POS = Angle::deg(0.0f);
    //     constexpr static Angle SPEED_GUIDE_NEG = Angle::deg(0.0f);

    //     using pin_step = Pin<AZ_STEP_PIN>;
    //     using pin_dir = Pin<AZ_DIR_PIN>;

    //     using interrupt = IntervalInterrupt<Timer::TIMER_4>;
    //     using driver = Driver<SPR, Pin<AZ_STEP_PIN>, Pin<AZ_DIR_PIN>, AZ_DRIVER_INVERT_DIR>;
    //     using ramp = AccelerationRamp<64, interrupt::FREQ, driver::SPR, (SPEED_SLEWING * TRANSMISSION).mrad_u32(), (ACCELERATION * TRANSMISSION).mrad_u32()>;
    //     using stepper = Stepper<interrupt, driver, ramp>;
    // };
    
    // struct ALT
    // {
    //     constexpr static auto TRANSMISSION = ALT_TRANSMISSION;

    //     constexpr static auto SPR = ALT_STEPPER_SPR;

    //     constexpr static Angle SPEED_SIDEREAL = Angle::deg(360.0f) / SIDEREAL_SECONDS_PER_DAY;

    //     constexpr static Angle SPEED_TRACKING = Angle::deg(0.0f);
    //     constexpr static Angle SPEED_SLEWING = Angle::deg(ALT_SLEWING_SPEED);
    //     constexpr static Angle ACCELERATION = Angle::deg(ALT_SLEWING_ACCELERATION);

    //     constexpr static Angle SPEED_GUIDE_POS = Angle::deg(0.0f);
    //     constexpr static Angle SPEED_GUIDE_NEG = Angle::deg(0.0f);

    //     using pin_step = Pin<ALT_STEP_PIN>;
    //     using pin_dir = Pin<ALT_DIR_PIN>;

    //     using interrupt = IntervalInterrupt<Timer::TIMER_5>;
    //     using driver = Driver<SPR, Pin<ALT_STEP_PIN>, Pin<ALT_DIR_PIN>, ALT_DRIVER_INVERT_DIR>;
    //     using ramp = AccelerationRamp<64, interrupt::FREQ, driver::SPR, (SPEED_SLEWING * TRANSMISSION).mrad_u32(), (ACCELERATION * TRANSMISSION).mrad_u32()>;
    //     using stepper = Stepper<interrupt, driver, ramp>;
    // };

}