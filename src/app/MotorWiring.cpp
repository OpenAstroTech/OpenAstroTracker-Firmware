#include "MotorWiring.hpp"

#include "BoardWiring.hpp"

#include "../../Configuration.hpp"
#include "ConfigBuilder.hpp"
#include "MountConfigBuilder.hpp"
#include "ports/ILogger.hpp"

#include "../adapters/StepperAxis.hpp"
#include "../hal/StepperMotorFactory.hpp"

#ifdef NEW_STEPPER_LIB
    #include "../hal/avr/StepperConfiguration.hpp"
#endif

namespace
{
/// Wraps an engine in its axis adapter. Both objects live for the lifetime of the program, like
/// the concrete steppers they replace.
ports::IStepperAxis *makeAxis(hal::IStepperMotor &motor)
{
    return new adapters::StepperAxis(motor);
}

#ifdef NEW_STEPPER_LIB
constexpr unsigned long raSlewSpeed  = config::Ra::SPEED_SLEW;
constexpr float raSlewAcceleration   = config::Ra::ACCEL_SLEW;
constexpr unsigned long decSlewSpeed = config::Dec::SPEED_SLEW;
constexpr float decSlewAcceleration  = config::Dec::ACCEL_SLEW;
#endif
}  // namespace

namespace app
{
void attachSteppers(app::MountExecutor &mount)
{
    app::AxisBank::Steppers steppers;
    core::AxisLimits limits;

    const core::MountConfig motion            = buildMountConfig();
    const core::AxisMechanicsConfig mechanics = buildAxisMechanicsConfig();

    // Read only by the boot diagnostics below, which a build without logging compiles out.
    (void) motion;

#if (RA_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure RA stepper NEMA.");
    LOG(DEBUG_ANY, "[STEPPERS]: Slew Microsteps : %d", motion.raSlewMicrosteps);
    LOG(DEBUG_ANY, "[STEPPERS]: Trk Microsteps  : %d", motion.raTrackingMicrosteps());
    LOG(DEBUG_ANY, "[STEPPERS]: Stepper SPR     : %d", mechanics.raStepperSpr);
    LOG(DEBUG_ANY, "[STEPPERS]: Transmission    : %f", mechanics.raTransmission);
    #ifdef NEW_STEPPER_LIB
    LOG(DEBUG_ANY, "[STEPPERS]: Driver Slew SPR : %l", config::Ra::DRIVER_SPR_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: Driver Trk SPR  : %l", config::Ra::DRIVER_SPR_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: SPR Slew        : %f", config::Ra::SPR_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: SPR Trk         : %f", config::Ra::SPR_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: Speed Trk       : %f", config::Ra::SPEED_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: Speed TrkComp   : %f", config::Ra::SPEED_COMPENSATION);
    #endif

    steppers.ra       = makeAxis(*hal::makeAxisMotor(hal::AxisRole::RaSlew, RA_STEP_PIN, RA_DIR_PIN));
    steppers.tracking = makeAxis(*hal::makeAxisMotor(hal::AxisRole::RaTracking, RA_STEP_PIN, RA_DIR_PIN));

    #ifdef NEW_STEPPER_LIB
    limits.raSpeed        = raSlewSpeed;
    limits.raAcceleration = raSlewAcceleration;
    #else
    limits.raSpeed         = mechanics.ra.speed;
    limits.raAcceleration  = mechanics.ra.acceleration;
    #endif
    LOG(DEBUG_ANY, "[STEPPERS]: Speed Slew      : %l", limits.raSpeed);
    LOG(DEBUG_ANY, "[STEPPERS]: Accel Slew      : %f", (float) limits.raAcceleration);

    steppers.ra->setMaxSpeed(limits.raSpeed);
    steppers.ra->setAcceleration(limits.raAcceleration);
    // The tracking engine has always run at a fixed, faster rate than the slew engine.
    steppers.tracking->setMaxSpeed(5000);
    steppers.tracking->setAcceleration(15000);
#endif

#if (DEC_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure DEC stepper NEMA.");
    LOG(DEBUG_ANY, "[STEPPERS]: Slew Microsteps  : %d", motion.decSlewMicrosteps);
    LOG(DEBUG_ANY, "[STEPPERS]: Guide Microsteps : %d", motion.decGuidingMicrosteps());
    LOG(DEBUG_ANY, "[STEPPERS]: Stepper SPR      : %d", mechanics.decStepperSpr);
    LOG(DEBUG_ANY, "[STEPPERS]: Transmission     : %f", mechanics.decTransmission);
    #ifdef NEW_STEPPER_LIB
    LOG(DEBUG_ANY, "[STEPPERS]: Driver Slew SPR  : %l", config::Dec::DRIVER_SPR_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: Driver Trk SPR   : %l", config::Dec::DRIVER_SPR_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: SPR Slew         : %f", config::Dec::SPR_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: SPR Trk          : %f", config::Dec::SPR_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: Speed Trk        : %f", config::Dec::SPEED_TRK);
    #endif

    steppers.dec   = makeAxis(*hal::makeAxisMotor(hal::AxisRole::DecSlew, DEC_STEP_PIN, DEC_DIR_PIN));
    steppers.guide = makeAxis(*hal::makeAxisMotor(hal::AxisRole::DecGuide, DEC_STEP_PIN, DEC_DIR_PIN));

    #ifdef NEW_STEPPER_LIB
    limits.decSpeed        = decSlewSpeed;
    limits.decAcceleration = decSlewAcceleration;
    #else
    limits.decSpeed        = mechanics.dec.speed;
    limits.decAcceleration = mechanics.dec.acceleration;
    #endif
    LOG(DEBUG_ANY, "[STEPPERS]: Speed Slew       : %l", limits.decSpeed);
    LOG(DEBUG_ANY, "[STEPPERS]: Accel Slew       : %f", (float) limits.decAcceleration);

    steppers.dec->setMaxSpeed(limits.decSpeed);
    steppers.dec->setAcceleration(limits.decAcceleration);
    steppers.guide->setMaxSpeed(limits.decSpeed);
    steppers.guide->setAcceleration(limits.decAcceleration);
    if (mechanics.dec.invertDirection)
    {
        steppers.dec->setPinsInverted(true, false, false);
        steppers.guide->setPinsInverted(true, false, false);
    }
    steppers.guide->setCurrentPosition(0);
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure AZ stepper...");
    LOG(DEBUG_ANY, "[STEPPERS]: AZ Microsteps    : %d", mechanics.azMicrosteps);
    LOG(DEBUG_ANY, "[STEPPERS]: AZ Stepper SPR   : %d", mechanics.azStepperSpr);
    LOG(DEBUG_ANY, "[STEPPERS]: AZ Circumference : %f", mechanics.azCircumference);
    LOG(DEBUG_ANY, "[STEPPERS]: AZ steps/rev     : %f", mechanics.azimuthStepsPerRev);
    LOG(DEBUG_ANY, "[STEPPERS]: AZ steps/deg     : %f", mount.getStepsPerDegree(AZIMUTH_STEPS));
    LOG(DEBUG_ANY, "[STEPPERS]: AZ steps/minute  : %f", mechanics.azimuthStepsPerArcMinute);

    steppers.az           = makeAxis(*hal::makeAxisMotor(hal::AxisRole::Az, AZ_STEP_PIN, AZ_DIR_PIN));
    limits.azSpeed        = mechanics.az.speed;
    limits.azAcceleration = mechanics.az.acceleration;
    steppers.az->setMaxSpeed(limits.azSpeed);
    steppers.az->setAcceleration(limits.azAcceleration);
    if (mechanics.az.invertDirection)
    {
        steppers.az->setPinsInverted(true, false, false);
    }
#endif

#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure Alt stepper...");
    LOG(DEBUG_ANY, "[STEPPERS]: ALT Microsteps    : %d", mechanics.altMicrosteps);
    LOG(DEBUG_ANY, "[STEPPERS]: ALT Stepper SPR   : %d", mechanics.altStepperSpr);
    LOG(DEBUG_ANY, "[STEPPERS]: ALT Circumference : %f", mechanics.altCircumference);
    LOG(DEBUG_ANY, "[STEPPERS]: ALT steps/rev     : %f", mechanics.altitudeStepsPerRev);
    LOG(DEBUG_ANY, "[STEPPERS]: ALT steps/deg     : %f", mount.getStepsPerDegree(ALTITUDE_STEPS));
    LOG(DEBUG_ANY, "[STEPPERS]: ALT steps/minute  : %f", mechanics.altitudeStepsPerArcMinute);

    steppers.alt           = makeAxis(*hal::makeAxisMotor(hal::AxisRole::Alt, ALT_STEP_PIN, ALT_DIR_PIN));
    limits.altSpeed        = mechanics.alt.speed;
    limits.altAcceleration = mechanics.alt.acceleration;
    steppers.alt->setMaxSpeed(limits.altSpeed);
    steppers.alt->setAcceleration(limits.altAcceleration);
    if (mechanics.alt.invertDirection)
    {
        steppers.alt->setPinsInverted(true, false, false);
    }
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure Focus stepper...");

    steppers.focus           = makeAxis(*hal::makeAxisMotor(hal::AxisRole::Focus, FOCUS_STEP_PIN, FOCUS_DIR_PIN));
    limits.focusSpeed        = mechanics.focus.speed;
    limits.focusAcceleration = mechanics.focus.acceleration;
    steppers.focus->setMaxSpeed(limits.focusSpeed);
    steppers.focus->setAcceleration(limits.focusAcceleration);
    steppers.focus->setSpeed(0);
    steppers.focus->setCurrentPosition(50000);
#endif

    mount.attachSteppers(steppers, limits);
}

}  // namespace app
