#pragma once

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace core
{

/// Pure (no Arduino deps) 24-hour time value stored as total seconds.
/// Handles hours/minutes/seconds arithmetic with 24h wrap-around.
class DayTime
{
  protected:
    long totalSeconds;

  public:
    DayTime();

    DayTime(const DayTime &other);
    DayTime(int h, int m, int s);

    // From hours (fractional)
    DayTime(float timeInHours);

    int getHours() const;
    int getMinutes() const;
    int getSeconds() const;
    float getTotalHours() const;
    float getTotalMinutes() const;
    long getTotalSeconds() const;

    void getTime(int &h, int &m, int &s) const;

    // Split signed seconds into (signed) hours plus unsigned minutes/seconds.
    static void splitSeconds(long secs, int &h, int &m, int &s);

    virtual void set(int h, int m, int s);
    void set(const DayTime &other);

    // Add hours, wrapping days (which are not tracked). Negative or positive.
    virtual void addHours(float deltaHours);

    // Add minutes, wrapping hours if needed
    void addMinutes(int deltaMins);

    // Add seconds, wrapping minutes and hours if needed
    void addSeconds(long deltaSecs);

    // Add time components, wrapping seconds, minutes and hours if needed
    void addTime(int deltaHours, int deltaMinutes, int deltaSeconds);

    // Add another time, wrapping seconds, minutes and hours if needed
    void addTime(const DayTime &other);

    // Subtract another time, wrapping seconds, minutes and hours if needed
    void subtractTime(const DayTime &other);

    // Format to char buffer (pure, no Arduino deps). Format tokens:
    // {d}=degrees, {m}=minutes, {s}=seconds, {+}=sign.
    virtual const char *formatString(char *targetBuffer, const char *format, long *pSeconds = nullptr) const;

    //protected:
    virtual void checkHours();

  protected:
    const char *formatStringImpl(char *targetBuffer, const char *format, char sgn, long degs, long mins, long secs) const;
    void printTwoDigits(char *achDegs, int num) const;

  private:
    static long const secondsPerDay = 24L * 3600L;  /// Real seconds (not sidereal)
};

// Inline helpers — equivalent to Utility.hpp sign/fsign but without Arduino deps.
inline int sign(long num)
{
    return num < 0 ? -1 : 1;
}

inline int fsign(float num)
{
    return num < 0.0f ? -1 : 1;
}

}  // namespace core
