#pragma once

#include "Angle.h"
#include "Stepper.h"

template <typename Config>
class Axis
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
    constexpr static Angle STEPPER_SPEED_SLEWING = transmit(Config::SPEED_SLEWING);

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
            Config::stepper::moveTo(STEPPER_SPEED_TRACKING, stepper_limit_max);
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
        auto trans_target = transmit(target);
        if (trans_target > stepper_limit_max)
        {
            trans_target = stepper_limit_max;
        }
        else if (trans_target < stepper_limit_min)
        {
            trans_target = stepper_limit_min;
        }

        if (is_tracking)
        {
            Config::stepper::moveTo(STEPPER_SPEED_TRACKING + STEPPER_SPEED_SLEWING, trans_target, StepperCallback::create<returnTracking>());
        }
        else
        {
            Config::stepper::moveTo(STEPPER_SPEED_SLEWING, trans_target);
        }
    }

    static void slew(bool direcion)
    {
        auto speed = (direcion) ? STEPPER_SPEED_SLEWING : -STEPPER_SPEED_SLEWING;

        if (is_tracking)
        {
            speed = speed + STEPPER_SPEED_TRACKING;
        }

        Config::stepper::moveTo(speed, (direcion) ? stepper_limit_max : stepper_limit_min);
    }

    static void stopSlewing()
    {
        Config::stepper::stop(StepperCallback::create<returnTracking>());
    }

    static void limitMax(Angle value)
    {
        stepper_limit_max = transmit(value);
    }

    static void limitMin(Angle value)
    {
        stepper_limit_min = transmit(value);
    }

    static Angle position()
    {
        return STEP_ANGLE * Config::stepper::position();
    }

    static void position(Angle value)
    {
        Config::stepper::position(transmit(value));
    }


    static Angle trackingPosition()
    {
        return Angle(0.0f);
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

    static Angle stepper_limit_max;
    static Angle stepper_limit_min;
};

template <typename Config>
bool Axis<Config>::is_tracking = false;

template <typename Config>
float Axis<Config>::slew_rate_factor = 1.0;

template <typename Config>
Angle Axis<Config>::stepper_limit_max = transmit(Angle::deg(101.0f));

template <typename Config>
Angle Axis<Config>::stepper_limit_min = transmit(Angle::deg(-101.0f));
