#pragma once

#include "Angle.h"
#include "Stepper.h"

template <typename Config> class Axis
{
  private:
    Axis() = delete;

    static void returnTracking()
    {
        track(true);
    }

    constexpr static inline Angle transmit(const Angle from)
    {
        return Config::TRANSMISSION * from;
    }

  public:
    constexpr static Angle STEP_ANGLE = Angle::deg(360.0f) / Config::driver::SPR / Config::TRANSMISSION;

    constexpr static Angle TRACKING_SPEED = Config::SPEED_TRACKING;

    constexpr static Angle STEPPER_SPEED_TRACKING = transmit(Config::SPEED_TRACKING);
    constexpr static Angle STEPPER_SPEED_SLEWING  = transmit(Config::SPEED_SLEWING);

    constexpr static Angle STEPPER_SPEED_GUIDING_POS = transmit(Config::SPEED_GUIDE_POS);
    constexpr static Angle STEPPER_SPEED_GUIDING_NEG = transmit(Config::SPEED_GUIDE_NEG);

    static void setup()
    {
        Config::driver::init();
        Config::interrupt::init();
    }

    static void track(bool enable)
    {
        if (enable)
        {
            Config::stepper::moveTo(STEPPER_SPEED_TRACKING, transmit(limit_max));
        }
        else
        {
            Config::stepper::stop();
        }

        if (is_tracking != enable)
        {
            is_tracking = enable;
        }
    }

    static void guide(bool direction, unsigned long time_ms)
    {
        if (is_tracking)
        {
            auto speed = (direction) ? STEPPER_SPEED_GUIDING_POS : STEPPER_SPEED_GUIDING_NEG;
            Config::stepper::moveTime(speed, time_ms, StepperCallback::create<returnTracking>());
        }
    }

    static void slewTo(Angle target)
    {
        slewing_from = Config::stepper::position();
        slewing_to = constrain(target, limit_min, limit_max);

        if (is_tracking)
        {
            auto speed = STEPPER_SPEED_TRACKING + (STEPPER_SPEED_SLEWING * slew_rate_factor);
            Config::stepper::moveTo(speed, transmit(slewing_to), StepperCallback::create<returnTracking>());
        }
        else
        {
            auto speed = STEPPER_SPEED_SLEWING * slew_rate_factor;
            Config::stepper::moveTo(speed, transmit(slewing_to));
        }
        
        is_slewing = true;
    }

    static void slew(bool direction)
    {
        auto target = (direction) ? limit_max : limit_min;
        slewTo(target);
    }

    static float slewingProgress()
    {
        if (is_slewing)
        {
            return (Config::stepper::position() - slewing_from) / (slewing_to - slewing_from);
        }
        else
        {
            return 1.0f;
        }
    }

    static void stopSlewing()
    {
        Config::stepper::stop(StepperCallback::create<returnTracking>());
    }

    static void limitMax(Angle value)
    {
        limit_max = value;
    }

    static void limitMin(Angle value)
    {
        limit_min = value;
    }

    static Angle position()
    {
        return Config::stepper::position() / Config::TRANSMISSION;
    }

    static void setPosition(Angle value)
    {
        Config::stepper::position(transmit(value));
    }

    static Angle trackingPosition()
    {
        return Angle::deg(0.0f);
    }

    static bool isRunning()
    {
        return Config::stepper::isRunning();
    }

    static bool isTracking()
    {
        return is_tracking;
    }

    static void setSlewRate(float factor)
    {
        slew_rate_factor = factor;
    }

    static float slewRate()
    {
        return slew_rate_factor;
    }

  private:
    static bool is_tracking;
    static float slew_rate_factor;

    static boolean is_slewing;
    static Angle slewing_from;
    static Angle slewing_to;

    static Angle limit_max;
    static Angle limit_min;
};

template <typename Config> bool Axis<Config>::is_tracking = false;

template <typename Config> bool Axis<Config>::is_slewing = false;

template <typename Config> float Axis<Config>::slew_rate_factor = 1.0;

template <typename Config> Angle Axis<Config>::slewing_from = Angle::deg(0.0f);

template <typename Config> Angle Axis<Config>::slewing_to = Angle::deg(0.0f);

template <typename Config> Angle Axis<Config>::limit_max = Angle::deg(101.0f);

template <typename Config> Angle Axis<Config>::limit_min = Angle::deg(-101.0f);
