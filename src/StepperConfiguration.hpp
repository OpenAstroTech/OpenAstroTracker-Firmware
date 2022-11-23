#pragma once
#include "../Configuration.hpp"
#undef DEC

#include "Pin.h"
#include "IntervalInterrupt.h"
#include "Driver.h"

PUSH_NO_WARNINGS
#include "Stepper.h"
#include "InterruptAccelStepper.h"
POP_NO_WARNINGS

#undef DEC

// CONFIGURATION
#define RA_TRANSMISSION 35.46611505122143f
#define RA_SLEWING_SPEED_DEG 2.0f        // deg/s
#define RA_SLEWING_ACCELERATION_DEG 2.0f // deg/s/s
#define RA_GUIDING_SPEED 0.5f        // fraction of sidereal speed to add/substract to/from tracking speed
#define RA_DRIVER_INVERT_DIR false
#define RA_STEPPER_SPR 400
#define RA_MICROSTEPPING 256
#define RA_STEP_PIN 54
#define RA_DIR_PIN 55

#define DEC_TRANSMISSION 17.70597411692611f
#define DEC_SLEWING_SPEED_DEG 2.0f        // deg/s
#define DEC_SLEWING_ACCELERATION_DEG 2.0f // deg/s/s
#define DEC_GUIDING_SPEED 0.5f        // fraction of sidereal speed to add/substract to/from tracking speed
#define DEC_DRIVER_INVERT_DIR false
#define DEC_STEPPER_SPR 400
#define DEC_MICROSTEPPING 256
#define DEC_STEP_PIN 60
#define DEC_DIR_PIN 61

// #define AZ_STEPPER_SPR (400UL * 32UL)
// #define AZ_TRANSMISSION 17.70597411692611f
// #define AZ_SLEWING_SPEED 0.5f        // deg/s
// #define AZ_SLEWING_ACCELERATION 2.0f // deg/s/s
// #define AZ_DRIVER_INVERT_DIR false

// #define ALT_STEPPER_SPR (400UL * 32UL)
// #define ALT_TRANSMISSION 17.70597411692611f
// #define ALT_SLEWING_SPEED 0.5f        // deg/s
// #define ALT_SLEWING_ACCELERATION 2.0f // deg/s/s
// #define ALT_DRIVER_INVERT_DIR false

// Seconds per astronomical day (23h 56m 4.0905s)
#define SIDEREAL_SECONDS_PER_DAY 86164.0905f

#define UINT32(x) static_cast<uint32_t>(x)

namespace config
{
    struct Ra
    {
        // constexpr static auto TRANSMISSION = RA_TRANSMISSION;

        constexpr static uint32_t DRIVER_SPR = static_cast<uint32_t>(RA_STEPPER_SPR) * static_cast<uint32_t>(RA_MICROSTEPPING);

        constexpr static float SPR = DRIVER_SPR * RA_TRANSMISSION;

        constexpr static float SPEED_SIDEREAL = SPR / SIDEREAL_SECONDS_PER_DAY;

        constexpr static float SPEED_TRACKING = SPEED_SIDEREAL;
        constexpr static float SPEED_SLEWING = SPR / 360.0f * RA_SLEWING_SPEED_DEG;
        constexpr static float ACCELERATION = SPR / 360.0f * RA_SLEWING_ACCELERATION_DEG;

        constexpr static auto SPEED_GUIDE_POS = SPEED_TRACKING + (SPEED_SIDEREAL * RA_GUIDING_SPEED);
        constexpr static auto SPEED_GUIDE_NEG = SPEED_TRACKING - (SPEED_SIDEREAL * RA_GUIDING_SPEED);

        using pin_step = Pin<RA_STEP_PIN>;
        using pin_dir = Pin<RA_DIR_PIN>;

        using interrupt = IntervalInterrupt<Timer::TIMER_3>;
        using interrupt_trk = IntervalInterrupt<Timer::TIMER_5>;
        using driver = Driver<DRIVER_SPR, pin_step, pin_dir, RA_DRIVER_INVERT_DIR>;

        using ramp_slew = AccelerationRamp<256, interrupt::FREQ, UINT32(SPEED_SLEWING), UINT32(ACCELERATION)>;
        using ramp_trk = AccelerationRamp<2, interrupt_trk::FREQ, UINT32(SPEED_TRACKING), UINT32(SPEED_TRACKING)>;

        using stepper_slew = Stepper<interrupt, driver, ramp_slew>;
        using stepper_trk = Stepper<interrupt_trk, driver, ramp_trk>;
    };

    struct Dec
    {
        constexpr static uint32_t DRIVER_SPR = static_cast<uint32_t>(DEC_STEPPER_SPR) * static_cast<uint32_t>(DEC_MICROSTEPPING);

        constexpr static float SPR = DRIVER_SPR * DEC_TRANSMISSION;

        constexpr static float SPEED_SIDEREAL = SPR / SIDEREAL_SECONDS_PER_DAY;

        constexpr static float SPEED_TRACKING = SPEED_SIDEREAL;
        constexpr static float SPEED_SLEWING = SPR / 360.0f * DEC_SLEWING_SPEED_DEG;
        constexpr static float ACCELERATION = SPR / 360.0f * DEC_SLEWING_ACCELERATION_DEG;

        constexpr static auto SPEED_GUIDE_POS = SPEED_TRACKING + (SPEED_SIDEREAL * DEC_GUIDING_SPEED);
        constexpr static auto SPEED_GUIDE_NEG = SPEED_TRACKING - (SPEED_SIDEREAL * DEC_GUIDING_SPEED);

        using pin_step = Pin<DEC_STEP_PIN>;
        using pin_dir = Pin<DEC_DIR_PIN>;

        using interrupt = IntervalInterrupt<Timer::TIMER_3>;
        using interrupt_trk = IntervalInterrupt<Timer::TIMER_1>;
        using driver = Driver<DRIVER_SPR, pin_step, pin_dir, DEC_DRIVER_INVERT_DIR>;

        using ramp_slew = AccelerationRamp<256, interrupt::FREQ, UINT32(SPEED_SLEWING), UINT32(ACCELERATION)>;
        using ramp_trk = AccelerationRamp<2, interrupt_trk::FREQ, UINT32(SPEED_TRACKING), UINT32(SPEED_TRACKING)>;

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