#pragma once

#include <stdint.h>

// Polaris is the one entry whose coordinate a user can configure, so on a device build the value
// comes from `Configuration.hpp`. The host build has no board configuration at all (that header
// #errors without a board), so it falls back to the same defaults, which is why the two lists have
// to stay in step — the table is data, not a build input.
#if defined(ARDUINO)
    #include "Configuration.hpp"
#endif

#if defined(POLARIS_RA_HOUR)
    #define OAT_POI_POLARIS_RA POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND
#else
    #define OAT_POI_POLARIS_RA 3, 0, 8
#endif

namespace app
{

/// One entry of the point-of-interest list: a name plus a celestial coordinate.
///
/// The name already carries the `>` the menu draws in front of the selected row, exactly as the
/// legacy table did, because the row text and the list order are the user-visible contract.
struct PointOfInterest {
    const char *display;
    uint8_t raHours;
    uint8_t raMinutes;
    uint8_t raSeconds;
    int decDegrees;
    uint8_t decMinutes;
    uint8_t decSeconds;
};

/// The list is sorted by declination, which is what makes the park/home/unpark entries usable as the
/// wrap-around end of the menu. New objects go above the three commands, never below them.
inline constexpr PointOfInterest kPointsOfInterest[] = {
    // 0123456789012345
    {">Polaris", OAT_POI_POLARIS_RA, 89, 21, 6},
    {">Altair", 19, 51, 45, 8, 55, 15},
    {">Arcturus", 14, 16, 37, 19, 5, 21},
    {">Big Dipper", 12, 16, 26, 56, 55, 7},
    {">Cederblad 214", 0, 5, 46, 67, 16, 45},
    {">Deneb (Cygnus)", 20, 42, 7, 45, 21, 12},
    {">Elephant Trunk", 21, 39, 44, 57, 35, 31},
    {">Heart Nebula", 2, 34, 57, 61, 31, 17},
    {">Lagoon Nebula", 18, 5, 2, -24, 22, 52},
    {">M31 Andromeda", 0, 43, 52, 41, 22, 53},
    {">M33 Triangulum", 1, 35, 2, 30, 46, 5},
    {">M42 Orion Nbula", 5, 36, 18, -5, 22, 44},
    {">M51 Whirlpool", 13, 30, 45, 47, 5, 21},
    {">M63 Sunflower", 13, 16, 45, 41, 55, 14},
    {">M81 Bodes Galxy", 9, 57, 13, 68, 58, 1},
    {">M101 Pinwheel", 14, 3, 56, 54, 15, 0},
    {">Navi", 0, 57, 57, 60, 49, 33},
    {">Pleiades 7Sistr", 3, 48, 15, 24, 10, 54},
    {">Soul Nebula", 2, 52, 47, 60, 30, 56},
    {">Vega", 18, 37, 37, 38, 48, 7},
    {">Veil Nebula", 20, 51, 28, 30, 59, 30},

    // Add new objects above here, not below.
    {">Home", 0, 0, 0, 90, 0, 0},
    {">Unpark", 0, 0, 0, 90, 0, 0},
    {">Park", 0, 0, 0, 90, 0, 0},
};

inline constexpr uint16_t kPointOfInterestCount = static_cast<uint16_t>(sizeof(kPointsOfInterest) / sizeof(kPointsOfInterest[0]));

/// The three commands that share the list with the objects, in the order the table stores them.
inline constexpr uint16_t kParkPointOfInterest   = static_cast<uint16_t>(kPointOfInterestCount - 1);
inline constexpr uint16_t kUnparkPointOfInterest = static_cast<uint16_t>(kPointOfInterestCount - 2);
inline constexpr uint16_t kHomePointOfInterest   = static_cast<uint16_t>(kPointOfInterestCount - 3);

}  // namespace app
