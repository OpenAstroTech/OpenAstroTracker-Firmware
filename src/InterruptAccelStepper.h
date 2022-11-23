//
// Created by andre on 12.11.22.
//

#ifndef AVR_INTERRUPT_STEPPER_INTERRUPTACCELSTEPPER_H
#define AVR_INTERRUPT_STEPPER_INTERRUPTACCELSTEPPER_H

#include <stdint.h>
#include <math.h>
#include "Angle.h"

#define ABS(x) ((x >= 0) ? x : -x)

#define SIGN(x) ((x >= 0) ? 1 : -1)

template <typename STEPPER> class InterruptAccelStepper
{
  private:
    float max_speed;
    long target;

  public:
    InterruptAccelStepper() : max_speed(0.0f), target(0)
    {
        STEPPER::init();
    }

    void moveTo(long absolute)
    {
        LOG(DEBUG_STEPPERS, "[IAS] moveTo(%l) at %f", absolute, max_speed);
        target = absolute;

        auto movement = STEPPER::MovementSpec::distance(max_speed, target - STEPPER::getPosition());

        LOG(DEBUG_STEPPERS, "[IAS] steps=%l, run_interval=%l, accel_stair=%d", movement.steps, movement.run_interval, movement.accel_stair);

        STEPPER::moveTo(max_speed, target);
    }

    void move(long relative)
    {
        target = STEPPER::getPosition() + relative;
        moveTo(target);
    }

    void setMaxSpeed(float speed)
    {
        max_speed = ABS(speed);
    }

    void setAcceleration(float value)
    {
        // STUB
    }

    float maxSpeed()
    {
        return max_speed;
    }

    void setSpeed(float speed)
    {
        setMaxSpeed(speed);
        moveTo((speed >= 0.0f) ? INT32_MAX : INT32_MIN);
    }

    float speed()
    {
        return maxSpeed();
    }

    uint32_t distanceToGo()
    {
        return STEPPER::distanceToGo();
    }

    long targetPosition()
    {
        return target;
    }

    long currentPosition()
    {
        return STEPPER::getPosition();
    }

    void setCurrentPosition(long position)
    {
        STEPPER::setPosition(position);
    }

    void run()
    {
        // STUB
    }

    void runSpeed()
    {
        // STUB
    }

    void runToPosition()
    {
        while (STEPPER::isRunning())
        {
            yield();
        }
    }

    void runToNewPosition(long position)
    {
        moveTo(position);
        runToPosition();
    }

    void stop()
    {
        STEPPER::stop();
    }

    void setPinsInverted(bool directionInvert = false, bool stepInvert = false, bool enableInvert = false)
    {
        // STUB
    }

    bool isRunning()
    {
        return STEPPER::isRunning();
    }
};

#endif  //AVR_INTERRUPT_STEPPER_INTERRUPTACCELSTEPPER_H
