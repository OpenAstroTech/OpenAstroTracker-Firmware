#pragma once

#include "../../Configuration.hpp"
#include "core/PortVocabulary.hpp"
#include "../core/MountCoordinator.hpp"
#include "../core/MountRuntimeSettings.hpp"
#include "../ports/ILogger.hpp"
#include "../ports/IMountSettings.hpp"
#include "AxisBank.hpp"

namespace app
{

/// The write side of the configuration: every setting that reaches the EEPROM, and the RAM mirror
/// it is kept in.
///
/// `core::PersistentConfig` owns the *load* side — the migration rules and the defaults applied on
/// a blank or upgraded store. This is its counterpart: it owns the relationship between a setting,
/// the in-RAM mirror (`core::MountRuntimeSettings`) and the store, so "how a setting is persisted"
/// is one thing rather than twenty-one methods scattered through the executor.
///
/// What is deliberately **not** here is everything whose write moves the mount:
///
/// * `setSpeed` and `setManualSlewMode` start or stop the axes, so they are the executor's.
/// * `clearConfiguration` wipes the store and re-initialises the whole object graph.
/// * `readPersistentData` seeds the axes and the hemisphere from the store.
///
/// Those keep the executor as their owner; this class only ever changes a number and tells the
/// tracker about it.
class MountSettings
{
  public:
    /// The tracking rate has to be recomputed when a setting changes while the mount is tracking.
    /// That is a motion consequence, so the executor answers it through this callback rather than
    /// this class reaching for the slew status — the same shape as
    /// `MountReadout::RaLimitQuery` and `SiteTimePort::LatitudeObserver`. A function pointer and a
    /// context, not a virtual, because a vtable costs SRAM on AVR.
    struct TrackingQuery {
        void *context;
        bool (*isTracking)(void *context);

        bool valid() const
        {
            return isTracking != nullptr;
        }
    };

    MountSettings(core::MountRuntimeSettings &settings,
                  ports::IMountSettings &store,
                  AxisBank &axes,
                  core::MountCoordinator &coordinator,
                  ports::ILogger &logger,
                  TrackingQuery tracking);

    /// Zeroes the values this class keeps outside the RAM mirror and the store.
    void reset();

    float getSpeedCalibration();
    void setSpeedCalibration(float value, bool saveToStorage);

#if USE_GYRO_LEVEL == 1
    float getPitchCalibrationAngle();
    void setPitchCalibrationAngle(float angle);

    float getRollCalibrationAngle();
    void setRollCalibrationAngle(float angle);

    /// Takes a value straight from the store: the RAM value is set, the store is not written.
    /// Used only by the load path, which is why it is a separate entry point from the setter.
    void applyLoadedPitchCalibration(float angle);
    void applyLoadedRollCalibration(float angle);
#endif

    /// The getter stays on `app::MountReadout` — it is a read, and the info panel needs it there.
    void setStepsPerDegree(StepperAxis which, float steps);

    int getBacklashCorrection();
    void setBacklashCorrection(int steps);

    void setSlewRate(int rate);
    int getSlewRate();

    void setHomingOffset(StepperAxis axis, long offset);
    long getHomingOffset(StepperAxis axis);

    void setDecLimitPosition(bool upper, float limitAngle);
    void clearDecLimitPosition(bool upper);
    void getDecLimitPositions(float &lowerLimit, float &upperLimit);

  private:
    core::MountRuntimeSettings &_settings;
    ports::IMountSettings &_store;
    AxisBank &_axes;
    core::MountCoordinator &_coordinator;
    ports::ILogger &_logger;
    TrackingQuery _tracking;

#if USE_GYRO_LEVEL == 1
    float _pitchCalibrationAngle = 0.0f;
    float _rollCalibrationAngle  = 0.0f;
#endif
};

}  // namespace app
