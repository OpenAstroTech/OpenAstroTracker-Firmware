#pragma once
#include "../Configuration.hpp"
#undef DEC

#include "Angle.h"
#include "Pin.h"
#include "IntervalInterrupt.h"
#include "Driver.h"
#include "Stepper.h"

#undef DEC

// CONFIGURATION
#define RA_STEPPER_SPR (400UL * 256UL)
#define RA_TRANSMISSION 35.46611505122143f
#define RA_SLEWING_SPEED 2.0f        // deg/s
#define RA_SLEWING_ACCELERATION 4.0f // deg/s/s
#define RA_GUIDING_SPEED 0.5f        // fraction of sidereal speed to add/substract to/from tracking speed
#define RA_DRIVER_INVERT_STEP false

#define DEC_STEPPER_SPR (400UL * 256UL)
#define DEC_TRANSMISSION 17.70597411692611f
#define DEC_SLEWING_SPEED 2.0f        // deg/s
#define DEC_SLEWING_ACCELERATION 4.0f // deg/s/s
#define DEC_GUIDING_SPEED 0.5f        // fraction of sidereal speed to add/substract to/from tracking speed
#define DEC_DRIVER_INVERT_STEP false

#define AZ_STEPPER_SPR (400UL * 32UL)
#define AZ_TRANSMISSION 17.70597411692611f
#define AZ_SLEWING_SPEED 0.5f        // deg/s
#define AZ_SLEWING_ACCELERATION 2.0f // deg/s/s
#define AZ_DRIVER_INVERT_STEP false

#define ALT_STEPPER_SPR (400UL * 32UL)
#define ALT_TRANSMISSION 17.70597411692611f
#define ALT_SLEWING_SPEED 0.5f        // deg/s
#define ALT_SLEWING_ACCELERATION 2.0f // deg/s/s
#define ALT_DRIVER_INVERT_STEP false

// Seconds per astronomical day (23h 56m 4.0905s)
#define SIDEREAL_SECONDS_PER_DAY 86164.0905f

namespace config
{
    struct RA
    {
        constexpr static auto TRANSMISSION = RA_TRANSMISSION;

        constexpr static auto SPR = RA_STEPPER_SPR;

        constexpr static Angle SPEED_SIDEREAL = Angle::deg(360.0f) / SIDEREAL_SECONDS_PER_DAY;

        constexpr static Angle SPEED_TRACKING = SPEED_SIDEREAL;
        constexpr static Angle SPEED_SLEWING = Angle::deg(RA_SLEWING_SPEED);
        constexpr static Angle ACCELERATION = Angle::deg(RA_SLEWING_ACCELERATION);

        constexpr static Angle SPEED_GUIDE_POS = SPEED_TRACKING + (SPEED_SIDEREAL * RA_GUIDING_SPEED);
        constexpr static Angle SPEED_GUIDE_NEG = SPEED_TRACKING - (SPEED_SIDEREAL * RA_GUIDING_SPEED);

        using pin_step = Pin<RA_STEP_PIN>;
        using pin_dir = Pin<RA_DIR_PIN>;

        using interrupt = IntervalInterrupt<Timer::TIMER_13>;
        using driver = Driver<SPR, pin_step, pin_dir, RA_DRIVER_INVERT_STEP>;
        using stepper = Stepper<interrupt, driver, (SPEED_SLEWING * TRANSMISSION).mrad_u32(), (ACCELERATION * TRANSMISSION).mrad_u32()>;
    };

    struct DEC
    {
        constexpr static auto TRANSMISSION = DEC_TRANSMISSION;

        constexpr static auto SPR = DEC_STEPPER_SPR;

        constexpr static Angle SPEED_SIDEREAL = Angle::deg(360.0f) / SIDEREAL_SECONDS_PER_DAY;

        constexpr static Angle SPEED_TRACKING = Angle::deg(0.0f);
        constexpr static Angle SPEED_SLEWING = Angle::deg(DEC_SLEWING_SPEED);
        constexpr static Angle ACCELERATION = Angle::deg(DEC_SLEWING_ACCELERATION);

        constexpr static Angle SPEED_GUIDE_POS = SPEED_TRACKING + (SPEED_SIDEREAL * DEC_GUIDING_SPEED);
        constexpr static Angle SPEED_GUIDE_NEG = SPEED_TRACKING - (SPEED_SIDEREAL * DEC_GUIDING_SPEED);

        using pin_step = Pin<DEC_STEP_PIN>;
        using pin_dir = Pin<DEC_DIR_PIN>;

        using interrupt = IntervalInterrupt<Timer::TIMER_14>;
        using driver = Driver<SPR, Pin<DEC_STEP_PIN>, Pin<DEC_DIR_PIN>, DEC_DRIVER_INVERT_STEP>;
        using stepper = Stepper<interrupt, driver, (SPEED_SLEWING * TRANSMISSION).mrad_u32(), (ACCELERATION * TRANSMISSION).mrad_u32()>;
    };

    struct AZ
    {
        constexpr static auto TRANSMISSION = AZ_TRANSMISSION;

        constexpr static auto SPR = AZ_STEPPER_SPR;

        constexpr static Angle SPEED_SIDEREAL = Angle::deg(360.0f) / SIDEREAL_SECONDS_PER_DAY;

        constexpr static Angle SPEED_TRACKING = Angle::deg(0.0f);
        constexpr static Angle SPEED_SLEWING = Angle::deg(AZ_SLEWING_SPEED);
        constexpr static Angle ACCELERATION = Angle::deg(AZ_SLEWING_ACCELERATION);

        constexpr static Angle SPEED_GUIDE_POS = Angle::deg(0.0f);
        constexpr static Angle SPEED_GUIDE_NEG = Angle::deg(0.0f);

        using pin_step = Pin<AZ_STEP_PIN>;
        using pin_dir = Pin<AZ_DIR_PIN>;

        using interrupt = IntervalInterrupt<Timer::TIMER_13>;
        using driver = Driver<SPR, Pin<AZ_STEP_PIN>, Pin<AZ_DIR_PIN>, AZ_DRIVER_INVERT_STEP>;
        using stepper = Stepper<interrupt, driver, (SPEED_SLEWING * TRANSMISSION).mrad_u32(), (ACCELERATION * TRANSMISSION).mrad_u32()>;
    };
    
    struct ALT
    {
        constexpr static auto TRANSMISSION = ALT_TRANSMISSION;

        constexpr static auto SPR = ALT_STEPPER_SPR;

        constexpr static Angle SPEED_SIDEREAL = Angle::deg(360.0f) / SIDEREAL_SECONDS_PER_DAY;

        constexpr static Angle SPEED_TRACKING = Angle::deg(0.0f);
        constexpr static Angle SPEED_SLEWING = Angle::deg(ALT_SLEWING_SPEED);
        constexpr static Angle ACCELERATION = Angle::deg(ALT_SLEWING_ACCELERATION);

        constexpr static Angle SPEED_GUIDE_POS = Angle::deg(0.0f);
        constexpr static Angle SPEED_GUIDE_NEG = Angle::deg(0.0f);

        using pin_step = Pin<ALT_STEP_PIN>;
        using pin_dir = Pin<ALT_DIR_PIN>;

        using interrupt = IntervalInterrupt<Timer::TIMER_14>;
        using driver = Driver<SPR, Pin<ALT_STEP_PIN>, Pin<ALT_DIR_PIN>, ALT_DRIVER_INVERT_STEP>;
        using stepper = Stepper<interrupt, driver, (SPEED_SLEWING * TRANSMISSION).mrad_u32(), (ACCELERATION * TRANSMISSION).mrad_u32()>;
    };

}