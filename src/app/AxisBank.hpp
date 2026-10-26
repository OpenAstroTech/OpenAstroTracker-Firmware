#pragma once

#include "../../Configuration.hpp"
#include "core/PortVocabulary.hpp"
#include "../core/AxisLimits.hpp"
#include "../ports/IMotorDriver.hpp"
#include "../ports/IStepperAxis.hpp"
#include "BoardPresence.hpp"

class EndSwitchMonitor;

namespace app
{

class HallSensorHoming;
class EndSwitchMonitor;

/// The hardware this board has: seven axes, five drivers, four sensors, and the limits the axes
/// run at.
///
/// This is the ownership seam the executor had lost. Every cluster inside `MountExecutor` — the
/// tick, the slew sequencing, the guide pulses, the homing state machine, the readouts — reaches
/// for `_stepperRA` / `_stepperDEC`, and none of them owns them. Collecting the pointers here
/// gives exactly one place that knows which parts exist, which is what lets the pieces above be
/// separated at all.
///
/// Held **by value** by the executor, so the seventeen pointers cost the same 34 B they did when
/// they were loose members, the accessors inline, and there is no allocation.
///
/// The parts arrive from `app/MotorWiring.cpp`, `DriverWiring.cpp` and `SensorWiring.cpp`, already
/// built and programmed: which engine, which pin and which polarity are board facts and belong to
/// the composition root. A board without a part stores null — callers test
/// `app::kHasX && ptr != nullptr`, which folds to nothing where the part cannot exist.
class AxisBank
{
  public:
    /// Every axis this board has. A null entry means the build has no such axis; `az`, `alt` and
    /// `focus` are the optional ones.
    struct Steppers {
        ports::IStepperAxis *ra       = nullptr;
        ports::IStepperAxis *tracking = nullptr;
        ports::IStepperAxis *dec      = nullptr;
        ports::IStepperAxis *guide    = nullptr;
        ports::IStepperAxis *az       = nullptr;
        ports::IStepperAxis *alt      = nullptr;
        ports::IStepperAxis *focus    = nullptr;
    };

    /// The drivers the composition root fitted, null where this board has none. A null entry is
    /// replaced by the null driver, so every call site below stays unguarded.
    struct Drivers {
        ports::IMotorDriver *ra    = nullptr;
        ports::IMotorDriver *dec   = nullptr;
        ports::IMotorDriver *az    = nullptr;
        ports::IMotorDriver *alt   = nullptr;
        ports::IMotorDriver *focus = nullptr;
    };

    /// The end switches and hall sensors, null where this board has none. Created once: the homing
    /// state machine restarts itself on each run, so a single instance per axis is enough.
    struct Sensors {
        EndSwitchMonitor *raEndSwitch      = nullptr;
        EndSwitchMonitor *decEndSwitch     = nullptr;
        HallSensorHoming *raHoming  = nullptr;
        HallSensorHoming *decHoming = nullptr;
    };

    /// Empty: no axes, the null driver everywhere, default limits.
    AxisBank();

    void attachSteppers(const Steppers &steppers, const core::AxisLimits &limits);
    void attachDrivers(const Drivers &drivers);
    void attachSensors(const Sensors &sensors);

    /// Back to the empty state. What re-reading the configuration starts from — the drivers are
    /// re-attached separately, so this drops them too, exactly as the old field-by-field reset did.
    void reset();

    ports::IStepperAxis *ra() const
    {
        return _ra;
    }

    ports::IStepperAxis *tracking() const
    {
        return _tracking;
    }

    ports::IStepperAxis *dec() const
    {
        return _dec;
    }

    ports::IStepperAxis *guide() const
    {
        return _guide;
    }

    ports::IStepperAxis *az() const
    {
        return _az;
    }

    ports::IStepperAxis *alt() const
    {
        return _alt;
    }

    ports::IStepperAxis *focus() const
    {
        return _focus;
    }

    ports::IMotorDriver *raDriver() const
    {
        return _raDriver;
    }

    ports::IMotorDriver *decDriver() const
    {
        return _decDriver;
    }

    ports::IMotorDriver *azDriver() const
    {
        return _azDriver;
    }

    ports::IMotorDriver *altDriver() const
    {
        return _altDriver;
    }

    ports::IMotorDriver *focusDriver() const
    {
        return _focusDriver;
    }

    EndSwitchMonitor *raEndSwitch() const
    {
        return _raEndSwitch;
    }

    EndSwitchMonitor *decEndSwitch() const
    {
        return _decEndSwitch;
    }

    HallSensorHoming *raHoming() const
    {
        return _raHoming;
    }

    HallSensorHoming *decHoming() const
    {
        return _decHoming;
    }

    const core::AxisLimits &limits() const
    {
        return _limits;
    }

    /// Microsteps per degree of travel for the AZ/ALT axes: build-time geometry that the EEPROM
    /// can override on a calibrated mount. They live here because they describe the axis, and the
    /// only readers are the AZ/ALT move and speed code.
    float azStepsPerDegree() const
    {
        return _stepsPerAzDegree;
    }

    float altStepsPerDegree() const
    {
        return _stepsPerAltDegree;
    }

    void setAzStepsPerDegree(float stepsPerDegree)
    {
        _stepsPerAzDegree = stepsPerDegree;
    }

    void setAltStepsPerDegree(float stepsPerDegree)
    {
        _stepsPerAltDegree = stepsPerDegree;
    }

  private:
    ports::IStepperAxis *_ra       = nullptr;
    ports::IStepperAxis *_tracking = nullptr;
    ports::IStepperAxis *_dec      = nullptr;
    ports::IStepperAxis *_guide    = nullptr;
    ports::IStepperAxis *_az       = nullptr;
    ports::IStepperAxis *_alt      = nullptr;
    ports::IStepperAxis *_focus    = nullptr;

    ports::IMotorDriver *_raDriver    = nullptr;
    ports::IMotorDriver *_decDriver   = nullptr;
    ports::IMotorDriver *_azDriver    = nullptr;
    ports::IMotorDriver *_altDriver   = nullptr;
    ports::IMotorDriver *_focusDriver = nullptr;

    EndSwitchMonitor *_raEndSwitch      = nullptr;
    EndSwitchMonitor *_decEndSwitch     = nullptr;
    HallSensorHoming *_raHoming  = nullptr;
    HallSensorHoming *_decHoming = nullptr;

    core::AxisLimits _limits {};
    // Set in the constructor: `AZIMUTH_STEPS_PER_REV` expands to an expression containing `PI`,
    // which only exists once Arduino.h is in scope, and a header cannot rely on that.
    float _stepsPerAzDegree  = 0.0f;
    float _stepsPerAltDegree = 0.0f;
};

}  // namespace app
