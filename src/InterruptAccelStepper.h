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
    Angle max_speed;
    long target;

  public:
    InterruptAccelStepper() : max_speed(Angle::deg(0)), target(0)
    {
    }

    void moveTo(long absolute)
    {
        target = absolute;
        STEPPER::moveTo(max_speed, target);
    }

    void move(long relative)
    {
        target = STEPPER::getPosition() + relative;
        if (relative >= 0)
        {
            STEPPER::moveTo(max_speed, target);
        }
        else
        {
            STEPPER::moveTo(-max_speed, target);
        }
    }

    void setMaxSpeed(float speed)
    {
        max_speed = STEPPER::ANGLE_PER_STEP * ABS(speed);
    }

    void setAcceleration(float value)
    {
    }

    float maxSpeed()
    {
        return max_speed / STEPPER::ANGLE_PER_STEP;
    }

    void setSpeed(float speed)
    {
        setMaxSpeed(speed);
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
    }

    void runSpeed()
    {
    }

    void runToPosition()
    {
        while (STEPPER::isRunning())
        {
            yield();
        }
    }

    //    void runToPosition();

    //    bool runSpeedToPosition();

    void runToNewPosition(long position)
    {
        moveTo(position);
        runToPosition();
    }

    void stop()
    {
        STEPPER::stop();
    }

    //    virtual void disableOutputs();

    //    virtual void enableOutputs();

    //    void setMinPulseWidth(unsigned int minWidth);

    //    void setEnablePin(uint8_t enablePin = 0xff);

    void setPinsInverted(bool directionInvert = false, bool stepInvert = false, bool enableInvert = false)
    {
    }

    //    void setPinsInverted(bool pin1Invert, bool pin2Invert, bool pin3Invert, bool pin4Invert, bool enableInvert);

    bool isRunning()
    {
        return STEPPER::isRunning();
    }
};

#endif  //AVR_INTERRUPT_STEPPER_INTERRUPTACCELSTEPPER_H
