#pragma once

#include "../Configuration.hpp"
#include "Utility.hpp"

#include "Angle.h"
#include "Stepper.h"

template <typename Config> class Axis
{
  private:
    Axis() = delete;

    static void returnTracking()
    {
        LOGV2(DEBUG_STEPPERS, F("[STEPLIB] : Motors stopped slewing. Resume Tracking : %d"), is_tracking);
        is_slewing = false;
        track(is_tracking);
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
        unsigned long timestamp = millis();

        if (STEPPER_SPEED_TRACKING.deg() != 0.0f)
        {
            if (_recentTrackingStartTime != 0UL)
            {
                _totalTrackingTime += timestamp - _recentTrackingStartTime;
            }

            if (enable)
            {
                LOGV2(DEBUG_STEPPERS, F("[STEPLIB] : Start tracking at speed : %f deg/s"), STEPPER_SPEED_TRACKING.deg());
                Config::stepper::moveTo(STEPPER_SPEED_TRACKING, transmit(limit_max));
                _recentTrackingStartTime = timestamp;
            }
            else
            {
                LOGV1(DEBUG_STEPPERS, F("[STEPLIB] : Stop tracking"));
                if (_recentTrackingStartTime > 0UL)
                {
                    _recentTrackingStartTime = 0UL;
                    LOGV2(DEBUG_STEPPERS, F("[STEPLIB] : tracking time is %l"), _totalTrackingTime);
                }

                Config::stepper::stop();
            }

            is_tracking = enable;
        }
        else
        {
            LOGV1(DEBUG_STEPPERS, F("[STEPLIB] : Tracking not supported on this axis"));
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
        slewBy(target - position());
    }

    static void slewBy(const Angle distance)
    {
        Angle slew_speed = STEPPER_SPEED_SLEWING * slew_rate_factor;

        if (is_tracking)
        {
            Angle move_speed = slew_speed + ((distance.rad() > 0.0f) ? STEPPER_SPEED_TRACKING : -STEPPER_SPEED_TRACKING);

            Angle move_distance = distance * (move_speed / STEPPER_SPEED_SLEWING);

            Config::stepper::moveBy(move_speed, transmit(move_distance), StepperCallback::create<returnTracking>());
        }
        else
        {
            Config::stepper::moveBy(slew_speed, transmit(distance));
        }
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
            return (position() - slewing_from) / (slewing_to - slewing_from);
        }
        else
        {
            return 1.0f;
        }
    }

    static void stopSlewing()
    {
        if (is_tracking && TRACKING_SPEED.deg() > 0)
        {
            LOGV1(DEBUG_STEPPERS, F("[STEPLIB] : stopSlewing()."));
            Config::stepper::stop(StepperCallback::create<returnTracking>());
        }
        else
        {
            Config::stepper::stop(StepperCallback());
        }
    }

    static void limitMax(Angle value)
    {
        limit_max = value;
    }

    static void limitMin(Angle value)
    {
        limit_min = value;
    }

    // Overridden for RA in Mount.cpp
    static Angle position()
    {
        return Config::stepper::position() / Config::TRANSMISSION;
    }

    // Overridden for RA in Mount.cpp
    static void setPosition(Angle value)
    {
        Config::stepper::position(transmit(value));
    }

    // Overridden for RA in Mount.cpp
    static Angle trackingPosition()
    {
        return Angle::deg(0.0f);
    }

    static bool isRunning()
    {
        return is_slewing;
    }

    static bool isTracking()
    {
        return is_tracking;
    }

    static int8_t direction()
    {
        return Config::stepper::movementDir();
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
    static unsigned long _recentTrackingStartTime;
    static unsigned long _totalTrackingTime;
};

template <typename Config> bool Axis<Config>::is_tracking = false;

template <typename Config> bool Axis<Config>::is_slewing = false;

template <typename Config> float Axis<Config>::slew_rate_factor = 1.0;

template <typename Config> Angle Axis<Config>::slewing_from = Angle::deg(0.0f);

template <typename Config> Angle Axis<Config>::slewing_to = Angle::deg(0.0f);

template <typename Config> Angle Axis<Config>::limit_max = Angle::deg(101.0f);

template <typename Config> Angle Axis<Config>::limit_min = Angle::deg(-101.0f);

template <typename Config> unsigned long Axis<Config>::_recentTrackingStartTime = 0UL;

template <typename Config> unsigned long Axis<Config>::_totalTrackingTime = 0UL;
