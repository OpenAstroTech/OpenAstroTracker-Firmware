//
// Created by andre on 12.11.22.
//

#ifndef AVR_INTERRUPT_STEPPER_INTERRUPTACCELSTEPPER_H
#define AVR_INTERRUPT_STEPPER_INTERRUPTACCELSTEPPER_H

#include <stdint.h>
#include <math.h>

#ifdef NEW_STEPPER_LIB

    #define SIGN(x) ((x >= 0) ? 1 : -1)

    // minimal stepping frequency (steps/s) based on cpu frequency, timer counter overflow and max amount of overflows
    #define MIN_STEPS_PER_SEC (static_cast<float>(F_CPU) / (static_cast<long>(UINT16_MAX) * static_cast<long>(UINT8_MAX)))

// This engine is `hal/`, the bottom of the dependency graph, so it owns no logger: `ports/` is what
// the layers above ask for, and an adapter composes this backend rather than the other way round.
// Its call traces used to go through the domain `LOG()` macro; the caller that drives the engine
// (`app::MountExecutor`) already logs the same requests at `DEBUG_STEPPERS`, so the engine stays
// silent and HAL stays free of `ports/`.
template <typename STEPPER> class InterruptAccelStepper
{
  private:
    float _max_speed;
    float _speed;
    long _target;

  public:
    InterruptAccelStepper(...) : _max_speed(0.0f), _target(0)
    {
        STEPPER::init();
    }

    void moveTo(long absolute)
    {
        _target = absolute;

        STEPPER::moveTo(_max_speed, _target);
    }

    void move(long relative)
    {
        _target = STEPPER::getPosition() + relative;

        moveTo(_target);
    }

    void setMaxSpeed(float speed)
    {
        _max_speed = fabsf(speed);
    }

    void setAcceleration(float value)
    {
        // STUB
    }

    float maxSpeed()
    {
        return _max_speed;
    }

    void setSpeed(float speed)
    {
        _speed = speed;

        if (fabsf(speed) < MIN_STEPS_PER_SEC)
        {
            STEPPER::stop();
        }
        else
        {
            STEPPER::moveTo(_speed, INT32_MAX);
        }
    }

    float speed()
    {
        return _speed;
    }

    uint32_t distanceToGo()
    {
        return STEPPER::distanceToGo();
    }

    long targetPosition()
    {
        return _target;
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
    }

    void runToPosition()
    {
        while (isRunning())
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
        STEPPER::setInverted(directionInvert);
    }

    bool isRunning()
    {
        return STEPPER::isRunning();
    }
};
#endif

#endif  //AVR_INTERRUPT_STEPPER_INTERRUPTACCELSTEPPER_H
