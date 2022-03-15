#pragma once

#define PROFILE_STEPPER 0
#if PROFILE_STEPPER && defined(ARDUINO)
#include "Pin.h"
#define PROFILE_MOVE_BEGIN() Pin<45>::high()
#define PROFILE_MOVE_END() Pin<45>::low()
#else
#define PROFILE_MOVE_BEGIN()
#define PROFILE_MOVE_END()
#endif

#ifndef noInterrupts
#define noInterrupts()
#endif

#ifndef interrupts
#define interrupts()
#endif

#include "AccelerationRamp.h"
#include "etl/delegate.h"
#include <stdint.h>
#include "Angle.h"

#define RAMP_STAIRS 64

using StepperCallback = etl::delegate<void()>;

/**
 * @brief
 *
 * @tparam INTERRUPT
 * @tparam DRIVER
 * @tparam MAX_SPEED_mRAD maximal possible speed in mrad/s
 * @tparam ACCELERATION_mRAD maximal possible speed in mrad/s/s
 */
template <typename INTERRUPT, typename DRIVER, typename RAMP>
class Stepper
{
public:
    static constexpr Angle ANGLE_PER_STEP = Angle::deg(360.0f) * DRIVER::SPR;

private:
    static volatile int32_t pos;

    static volatile int8_t dir;
    static volatile uint8_t ramp_stair;
    static volatile uint8_t ramp_stair_step;

    static volatile uint32_t run_interval;

    static volatile uint8_t pre_decel_stairs_left;
    static volatile uint16_t accel_steps_left;
    static volatile uint32_t run_steps_left;

    static StepperCallback cb_complete;

    /**
     * @brief Static class. We don't need constructor.
     */
    Stepper() = delete;

    static inline __attribute__((always_inline)) void step()
    {
        DRIVER::step();
        pos += dir;
    }

    /**
     * @brief Stop movement immediately and invoke onComplete interval if set.
     */
    static void terminate()
    {
        INTERRUPT::stop();
        INTERRUPT::setCallback(nullptr);

        dir = 0;
        ramp_stair = 0;
        ramp_stair_step = 0;
        run_interval = 0;

        if (cb_complete.is_valid())
        {
            cb_complete();
        }
    }

    static void start_movement(
        const int8_t direction,
        const uint8_t pre_decel_stairs,
        const uint16_t accel_steps,
        const uint32_t run_steps,
        const uint32_t new_run_interval)
    {
        pre_decel_stairs_left = pre_decel_stairs;
        accel_steps_left = accel_steps;
        run_steps_left = run_steps;
        run_interval = new_run_interval;

        // we need to pre-decelerate first (for direction change or lower speed)
        if (pre_decel_stairs > 0)
        {
            ramp_stair_step = RAMP::LAST_STEP;
            INTERRUPT::setCallback(pre_decelerate_handler);
            INTERRUPT::setInterval(RAMP::interval(ramp_stair));
        }
        // accelerate (for faster speed)
        else if (accel_steps > 0)
        {
            if (ramp_stair == 0)
            {
                ramp_stair = 1;
            }
            dir = direction;

            DRIVER::dir(dir > 0);
            INTERRUPT::setCallback(accelerate_handler);
            INTERRUPT::setInterval(RAMP::interval(ramp_stair));
        }
        // run directly, requested speed is similar to current one
        else if (run_steps > 0)
        {
            dir = direction;
            DRIVER::dir(dir > 0);
            INTERRUPT::setCallback(run_handler);
            INTERRUPT::setInterval(new_run_interval);
        }
        // decelerate until stopped
        else
        {
            INTERRUPT::setCallback(decelerate_handler);
            INTERRUPT::setInterval(RAMP::interval(ramp_stair));
        }
    }

    static void pre_decelerate_handler()
    {
        // always step first to ensure best accuracy.
        // other calculations should be done as quick as possible below.
        step();

        // did not reach end of this ramp stair yet
        if (ramp_stair_step > 0)
        {
            ramp_stair_step--;
        }
        // did not reach end of pre-deceleration, switch to next stair
        else if (pre_decel_stairs_left > 0)
        {
            pre_decel_stairs_left--;
            ramp_stair--;
            ramp_stair_step = RAMP::LAST_STEP;
            INTERRUPT::setInterval(RAMP::interval(ramp_stair));
        }
        // pre-deceleration finished, it was a direction switch, accelerate
        else if (accel_steps_left > 0)
        {
            dir = dir * (-1);
            DRIVER::dir(dir > 0);
            ramp_stair = 0;
            ramp_stair_step = 0;
            INTERRUPT::setCallback(accelerate_handler);
            INTERRUPT::setInterval(RAMP::interval(0));
        }
        // pre-deceleration finished, no need to accelerate
        else
        {
            DRIVER::dir(dir > 0);
            INTERRUPT::setCallback(run_handler);
            INTERRUPT::setInterval(run_interval);
        }
    }

    static void accelerate_handler()
    {
        // always step first to ensure best accuracy.
        // other calculations should be done as quick as possible below.
        step();

        accel_steps_left--;

        // reached max amount of acceleration steps
        if (accel_steps_left == 0)
        {
            if (run_steps_left == 0)
            {
                INTERRUPT::setCallback(decelerate_handler);
            }
            else
            {
                INTERRUPT::setCallback(run_handler);
                INTERRUPT::setInterval(run_interval);
            }
        }
        // did not finish current stair yet
        else if (ramp_stair_step < RAMP::LAST_STEP)
        {
            ramp_stair_step++;
        }
        // acceleration not finished, switch to next speed
        else
        {
            INTERRUPT::setInterval(RAMP::interval(++ramp_stair));
            ramp_stair_step = 0;
        }
    }

    static void run_handler()
    {
        // always step first to ensure best accuracy.
        // other calculations should be done as quick as possible below.
        step();

        if (--run_steps_left == 0)
        {
            if (ramp_stair == 0)
            {
                terminate();
            }
            else
            {
                INTERRUPT::setCallback(decelerate_handler);
                INTERRUPT::setInterval(RAMP::interval(ramp_stair));
            }
        }
    }

    static void decelerate_handler()
    {
        // always step first to ensure best accuracy.
        // other calculations should be done as quick as possible below.
        step();

        if (ramp_stair_step > 0)
        {
            ramp_stair_step--;
        }
        else if (ramp_stair > 1)
        {
            ramp_stair--;
            ramp_stair_step = RAMP::LAST_STEP;
            INTERRUPT::setInterval(RAMP::interval(ramp_stair));
        }
        else
        {
            terminate();
        }
    }

public:
    static Angle position()
    {
        noInterrupts();
        Angle ret = Angle::deg(360.0f / DRIVER::SPR) * pos;
        interrupts();
        return ret;
    }

    static void position(Angle value)
    {
        noInterrupts();
        pos = static_cast<int32_t>((DRIVER::SPR / (2.0f * 3.14159265358979323846f)) * value.rad() + 0.5f);
        interrupts();
    }

    static int8_t movementDir()
    {
        return dir;
    }

    static bool isRunning()
    {
        return run_interval > 0;
    }

    struct MovementSpec
    {
        const int8_t dir;
        const uint32_t steps;
        const uint32_t run_interval;
        const uint8_t full_accel_stairs;

        constexpr MovementSpec(
            const int8_t _dir,
            const uint32_t _steps,
            const uint32_t _run_interval,
            const uint8_t _full_accel_stairs)
            : dir(_dir),
              steps(_steps),
              run_interval(_run_interval),
              full_accel_stairs(_full_accel_stairs) {}

        constexpr MovementSpec(const Angle speed, const Angle distance)
            : MovementSpec(
                  (distance.mrad() > 0.0f) ? speed : -speed,
                  abs(distance.rad()) / (2.0f * 3.14159265358979323846f / DRIVER::SPR) + 0.5f)
        {
            // nothing to do here, all values have been initialized
        }

        constexpr MovementSpec(const Angle speed, const uint32_t _steps)
            : dir((speed.rad() > 0.0f) ? 1 : -1),
              steps(_steps),
              run_interval(RAMP::getIntervalForSpeed(speed.rad())),
              full_accel_stairs(RAMP::maxAccelStairs(speed.rad()))
        {
            // nothing to do here, all values have been initialized
        }

        constexpr static MovementSpec time(const Angle speed, const uint32_t time_ms)
        {
            int8_t dir = speed.rad() >= 0.0f;
            uint32_t steps = static_cast<uint32_t>(abs(speed.rad()) / (STEP_ANGLE.rad() * 1000.0f) * time_ms);
            uint32_t run_interval = RAMP::getIntervalForSpeed(speed.rad());
            uint8_t full_accel_stairs = RAMP::maxAccelStairs(speed.rad());
            return MovementSpec(dir, steps, run_interval, full_accel_stairs);
        }

    private:
        constexpr static Angle STEP_ANGLE = Angle::deg(360.0f) / DRIVER::SPR;
    };

    static void stop()
    {
        if (ramp_stair > 0)
        {
            start_movement(dir, 0, 0, 0, UINT32_MAX);
        }
        else
        {
            terminate();
        }
    }

    static void stop(StepperCallback onComplete)
    {
        cb_complete = onComplete;
        stop();
    }

    static void moveTime(const Angle speed, const uint32_t time_ms, StepperCallback onComplete = StepperCallback())
    {
        move(MovementSpec::time(speed, time_ms), onComplete);
    }

    static void moveTo(const Angle speed, const Angle target, StepperCallback onComplete = StepperCallback())
    {
        move(MovementSpec(speed, target - (Angle::deg(360.0f / DRIVER::SPR) * pos)), onComplete);
    }

    static void moveTo(const Angle speed, const int32_t target, StepperCallback onComplete = StepperCallback())
    {
        move(MovementSpec(speed, target - pos), onComplete);
    }

    static void moveBy(const Angle speed, const Angle distance, StepperCallback onComplete = StepperCallback())
    {
        move(MovementSpec(speed, distance), onComplete);
    }

    static void moveBy(const Angle speed, const uint32_t steps, StepperCallback onComplete = StepperCallback())
    {
        move(MovementSpec(speed, steps), onComplete);
    }

    static void move(MovementSpec spec, StepperCallback onComplete = StepperCallback())
    {
        PROFILE_MOVE_BEGIN();

        noInterrupts();

        INTERRUPT::stop();

        cb_complete = onComplete;

        uint8_t mv_pre_decel_stairs = 0;
        uint16_t mv_accel_steps = 0;
        uint32_t mv_run_steps = 0;
        uint32_t mv_run_interval = 0;

        if (spec.dir != dir && ramp_stair > 0)
        {
            // decelerate until full stop to move in other direction
            mv_pre_decel_stairs = ramp_stair;

            // we need to add steps made in pre deceleration to achieve accurate end position
            const uint32_t mv_steps_after_pre_decel = spec.steps + (static_cast<uint16_t>(mv_pre_decel_stairs) * RAMP::STEPS_PER_STAIR);

            // amount of steps needed (ideally) for full acceleration after stop
            const uint16_t full_accel_steps = static_cast<uint32_t>(spec.full_accel_stairs) * RAMP::STEPS_PER_STAIR;

            // amount of steps needed (ideally) for full acceleration + deceleration
            const uint32_t accel_decel_steps = static_cast<uint32_t>(full_accel_steps) * 2;

            // if final movement not long enough for a full acceleration + deceleration
            if (mv_steps_after_pre_decel < accel_decel_steps)
            {
                // acceleration has to before reaching its maximum
                mv_accel_steps = mv_steps_after_pre_decel / 2;
            }
            // enough steps for a full ramp (accelleration + run + decelleration)
            else
            {
                // acceleration will reach maximum
                mv_accel_steps = full_accel_steps;

                // there will be some steps with max speed
                mv_run_steps = spec.steps - (mv_accel_steps * 2);

                // run at requested speed
                mv_run_interval = spec.run_interval;
            }
        }
        // small speed change (if at all), no need for pre-deceleration or acceleration
        else if (spec.full_accel_stairs == ramp_stair)
        {
            // run all requested steps except deceleration steps (if needed)
            mv_run_steps = spec.steps - (static_cast<uint16_t>(ramp_stair) * RAMP::STEPS_PER_STAIR);

            // run at requested speed
            mv_run_interval = spec.run_interval;
        }
        // target speed is slower than current speed, we need to decelerate first
        else if (spec.full_accel_stairs < ramp_stair)
        {
            // steps needed for pre-deceleration and full deceleration
            const uint32_t total_decel_steps = static_cast<uint16_t>(ramp_stair) * RAMP::STEPS_PER_STAIR;

            // not enough steps to reach target position at full stop, we need a correction in opposite direction
            if (spec.steps < total_decel_steps)
            {
                // pre decelerate to full stop (overshooting actual target position)
                mv_pre_decel_stairs = ramp_stair;

                // specified steps were not enough for a full deceleration. compensation will be shorter than steps needed for full deceleration.
                // compensate with a partial ramp (only accel + decel)
                mv_accel_steps = (spec.steps - total_decel_steps) / 2;
            }
            else
            {
                // we need to decelerate by the diff of current and target speed first
                mv_pre_decel_stairs = ramp_stair - spec.full_accel_stairs;

                // steps at target speed are rest of all steps minus steps needed for both decelerations
                mv_run_steps = spec.steps - total_decel_steps;
            }
        }
        // requested speed higher than current one, need to accelerate
        else
        {
            // steps needed for instant deceleration
            const uint16_t immediate_decel_steps = static_cast<uint16_t>(ramp_stair) * RAMP::STEPS_PER_STAIR;

            // steps needed for full deceleration
            const uint16_t total_decel_steps = static_cast<uint16_t>(spec.full_accel_stairs) * RAMP::STEPS_PER_STAIR;

            // steps needed for acceleration between current and requested speed
            const uint16_t accel_steps = total_decel_steps - immediate_decel_steps;

            // not enough steps to even immediately decelerate, need to compensate backwards
            if (spec.steps < immediate_decel_steps)
            {
                // first decelerate to complete stop
                mv_pre_decel_stairs = ramp_stair;

                // amount of steps to compensate into opposite direction
                uint16_t compensate_steps = spec.steps + immediate_decel_steps;

                // specified steps were not enough for an immediate deceleration. compensation will be shorter.
                // compensate with a partial ramp (only accel + decel)
                mv_accel_steps = compensate_steps / 2;

                // if amount of steps to compensate is odd, we have to add 1 step to the run phase
                if (compensate_steps & 0x1)
                {
                    mv_run_steps = 1;
                    mv_run_interval = RAMP::interval(ramp_stair + (mv_accel_steps / RAMP::STEPS_PER_STAIR));
                }
            }
            // not enough steps for full acceleration to requested speed and later deceleration to full stop.
            else if (spec.steps < accel_steps + total_decel_steps)
            {
                // only accelerate as much to decelerate to requested position
                mv_accel_steps = spec.steps / 2;

                // make just one run step in case spec.steps is odd
                if (spec.steps & 0x1)
                {
                    mv_run_steps = 1;
                    mv_run_interval = RAMP::interval(ramp_stair + (mv_accel_steps / RAMP::STEPS_PER_STAIR));
                }
            }
            // enough steps for "full" ramp
            else
            {
                // perform acceleration to requested speed
                mv_accel_steps = accel_steps;

                // amount of steps to run is requested steps minus acceleration and deceleration
                mv_run_steps = spec.steps - accel_steps - total_decel_steps;

                // we can run at requested interval
                mv_run_interval = spec.run_interval;
            }
        }

        start_movement(spec.dir, mv_pre_decel_stairs, mv_accel_steps, mv_run_steps, mv_run_interval);

        interrupts();

        PROFILE_MOVE_END();
    }
};

template <typename INTERRUPT, typename DRIVER, typename RAMP>
int32_t volatile Stepper<INTERRUPT, DRIVER, RAMP>::pos = 0;

template <typename INTERRUPT, typename DRIVER, typename RAMP>
int8_t volatile Stepper<INTERRUPT, DRIVER, RAMP>::dir = 1;

template <typename INTERRUPT, typename DRIVER, typename RAMP>
uint8_t volatile Stepper<INTERRUPT, DRIVER, RAMP>::ramp_stair = 0;

template <typename INTERRUPT, typename DRIVER, typename RAMP>
uint8_t volatile Stepper<INTERRUPT, DRIVER, RAMP>::ramp_stair_step = 0;

template <typename INTERRUPT, typename DRIVER, typename RAMP>
uint32_t volatile Stepper<INTERRUPT, DRIVER, RAMP>::run_interval = 0;

template <typename INTERRUPT, typename DRIVER, typename RAMP>
StepperCallback Stepper<INTERRUPT, DRIVER, RAMP>::cb_complete = StepperCallback();

template <typename INTERRUPT, typename DRIVER, typename RAMP>
uint8_t volatile Stepper<INTERRUPT, DRIVER, RAMP>::pre_decel_stairs_left = 0;

template <typename INTERRUPT, typename DRIVER, typename RAMP>
uint16_t volatile Stepper<INTERRUPT, DRIVER, RAMP>::accel_steps_left = 0;

template <typename INTERRUPT, typename DRIVER, typename RAMP>
uint32_t volatile Stepper<INTERRUPT, DRIVER, RAMP>::run_steps_left = 0;