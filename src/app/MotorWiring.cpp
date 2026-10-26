#include "MotorWiring.hpp"

#include "BoardWiring.hpp"

#include "../../Configuration.hpp"
#include "../Utility.hpp"

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
#else
constexpr unsigned long raSlewSpeed         = RA_STEPPER_SPEED;
constexpr unsigned long raSlewAcceleration  = RA_STEPPER_ACCELERATION;
constexpr unsigned long decSlewSpeed        = DEC_STEPPER_SPEED;
constexpr unsigned long decSlewAcceleration = DEC_STEPPER_ACCELERATION;
#endif
}  // namespace

namespace app
{

void attachSteppers(app::MountExecutor &mount)
{
    app::AxisBank::Steppers steppers;
    core::AxisLimits limits;

#if (RA_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure RA stepper NEMA.");
    LOG(DEBUG_ANY, "[STEPPERS]: Slew Microsteps : %d", RA_SLEW_MICROSTEPPING);
    LOG(DEBUG_ANY, "[STEPPERS]: Trk Microsteps  : %d", RA_TRACKING_MICROSTEPPING);
    LOG(DEBUG_ANY, "[STEPPERS]: Stepper SPR     : %d", RA_STEPPER_SPR);
    LOG(DEBUG_ANY, "[STEPPERS]: Transmission    : %f", RA_TRANSMISSION);
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

    limits.raSpeed        = raSlewSpeed;
    limits.raAcceleration = raSlewAcceleration;
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
    LOG(DEBUG_ANY, "[STEPPERS]: Slew Microsteps  : %d", DEC_SLEW_MICROSTEPPING);
    LOG(DEBUG_ANY, "[STEPPERS]: Guide Microsteps : %d", DEC_GUIDE_MICROSTEPPING);
    LOG(DEBUG_ANY, "[STEPPERS]: Stepper SPR      : %d", DEC_STEPPER_SPR);
    LOG(DEBUG_ANY, "[STEPPERS]: Transmission     : %f", DEC_TRANSMISSION);
    #ifdef NEW_STEPPER_LIB
    LOG(DEBUG_ANY, "[STEPPERS]: Driver Slew SPR  : %l", config::Dec::DRIVER_SPR_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: Driver Trk SPR   : %l", config::Dec::DRIVER_SPR_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: SPR Slew         : %f", config::Dec::SPR_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: SPR Trk          : %f", config::Dec::SPR_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: Speed Trk        : %f", config::Dec::SPEED_TRK);
    #endif

    steppers.dec   = makeAxis(*hal::makeAxisMotor(hal::AxisRole::DecSlew, DEC_STEP_PIN, DEC_DIR_PIN));
    steppers.guide = makeAxis(*hal::makeAxisMotor(hal::AxisRole::DecGuide, DEC_STEP_PIN, DEC_DIR_PIN));

    limits.decSpeed        = decSlewSpeed;
    limits.decAcceleration = decSlewAcceleration;
    LOG(DEBUG_ANY, "[STEPPERS]: Speed Slew       : %l", limits.decSpeed);
    LOG(DEBUG_ANY, "[STEPPERS]: Accel Slew       : %f", (float) limits.decAcceleration);

    steppers.dec->setMaxSpeed(limits.decSpeed);
    steppers.dec->setAcceleration(limits.decAcceleration);
    steppers.guide->setMaxSpeed(limits.decSpeed);
    steppers.guide->setAcceleration(limits.decAcceleration);
    #if DEC_INVERT_DIR == 1
    steppers.dec->setPinsInverted(true, false, false);
    steppers.guide->setPinsInverted(true, false, false);
    #endif
    steppers.guide->setCurrentPosition(0);
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure AZ stepper...");
    LOG(DEBUG_ANY, "[STEPPERS]: AZ Microsteps    : %d", AZ_MICROSTEPPING);
    LOG(DEBUG_ANY, "[STEPPERS]: AZ Stepper SPR   : %d", AZ_STEPPER_SPR);
    LOG(DEBUG_ANY, "[STEPPERS]: AZ Circumference : %f", AZ_CIRCUMFERENCE);
    LOG(DEBUG_ANY, "[STEPPERS]: AZ steps/rev     : %f", AZIMUTH_STEPS_PER_REV);
    LOG(DEBUG_ANY, "[STEPPERS]: AZ steps/deg     : %f", mount.getStepsPerDegree(AZIMUTH_STEPS));
    LOG(DEBUG_ANY, "[STEPPERS]: AZ steps/minute  : %f", AZIMUTH_STEPS_PER_ARC_MINUTE);

    steppers.az           = makeAxis(*hal::makeAxisMotor(hal::AxisRole::Az, AZ_STEP_PIN, AZ_DIR_PIN));
    limits.azSpeed        = AZ_STEPPER_SPEED;
    limits.azAcceleration = AZ_STEPPER_ACCELERATION;
    steppers.az->setMaxSpeed(limits.azSpeed);
    steppers.az->setAcceleration(limits.azAcceleration);
    #if AZ_INVERT_DIR == 1
    steppers.az->setPinsInverted(true, false, false);
    #endif
#endif

#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure Alt stepper...");
    LOG(DEBUG_ANY, "[STEPPERS]: ALT Microsteps    : %d", ALT_MICROSTEPPING);
    LOG(DEBUG_ANY, "[STEPPERS]: ALT Stepper SPR   : %d", ALT_STEPPER_SPR);
    LOG(DEBUG_ANY, "[STEPPERS]: ALT Circumference : %f", ALT_CIRCUMFERENCE);
    LOG(DEBUG_ANY, "[STEPPERS]: ALT steps/rev     : %f", ALTITUDE_STEPS_PER_REV);
    LOG(DEBUG_ANY, "[STEPPERS]: ALT steps/deg     : %f", mount.getStepsPerDegree(ALTITUDE_STEPS));
    LOG(DEBUG_ANY, "[STEPPERS]: ALT steps/minute  : %f", ALTITUDE_STEPS_PER_ARC_MINUTE);

    steppers.alt           = makeAxis(*hal::makeAxisMotor(hal::AxisRole::Alt, ALT_STEP_PIN, ALT_DIR_PIN));
    limits.altSpeed        = ALT_STEPPER_SPEED;
    limits.altAcceleration = ALT_STEPPER_ACCELERATION;
    steppers.alt->setMaxSpeed(limits.altSpeed);
    steppers.alt->setAcceleration(limits.altAcceleration);
    #if ALT_INVERT_DIR == 1
    steppers.alt->setPinsInverted(true, false, false);
    #endif
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure Focus stepper...");

    steppers.focus           = makeAxis(*hal::makeAxisMotor(hal::AxisRole::Focus, FOCUS_STEP_PIN, FOCUS_DIR_PIN));
    limits.focusSpeed        = FOCUS_STEPPER_SPEED;
    limits.focusAcceleration = FOCUS_STEPPER_ACCELERATION;
    steppers.focus->setMaxSpeed(limits.focusSpeed);
    steppers.focus->setAcceleration(limits.focusAcceleration);
    steppers.focus->setSpeed(0);
    steppers.focus->setCurrentPosition(50000);
#endif

    mount.attachSteppers(steppers, limits);
}

}  // namespace app
