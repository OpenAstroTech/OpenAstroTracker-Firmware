#pragma once

#include "DayTime.hpp"

namespace core
{

/// Pure Declination coordinate. Range: -180° to +180° (arc-seconds),
/// clamped rather than wrapped.
/// 0 = pole, ±180 = opposite pole (hemisphere-dependent interpretation).
class Declination : public DayTime
{
  public:
    Declination();
    Declination(const Declination &other);
    Declination(int h, int m, int s);
    Declination(float inDegrees);

    virtual void set(int h, int m, int s) override;

    // Add degrees, clamp to -180...180
    void addDegrees(int deltaDegrees);

    // Get total degrees (-180..180)
    float getTotalDegrees() const;

    // Hemisphere-aware conversions between the mount-axis coordinate (0 at the
    // pole above the mount, +/-180 at the opposite pole) and celestial
    // declination arc-seconds (-90 at the south celestial pole, +90 at the
    // north celestial pole).
    static long axisToCelestialSeconds(long axisSeconds, bool northernHemisphere);
    static long celestialToAxisSeconds(long celestialSeconds, bool northernHemisphere);

    // Construct from total (axis) seconds directly, avoiding float rounding.
    static Declination fromTotalSeconds(long totalSeconds);

  protected:
    virtual void checkHours() override;

  protected:
    static long const arcSecondsPerHemisphere = 180L * 60L * 60L;  // Arc-seconds in 180 degrees
};

}  // namespace core
