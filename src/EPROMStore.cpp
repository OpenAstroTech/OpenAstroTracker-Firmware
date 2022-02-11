#include "inc/Globals.hpp"
PUSH_NO_WARNINGS
#include <EEPROM.h>
POP_NO_WARNINGS

#include "../Configuration.hpp"
#include "Utility.hpp"
#include "EPROMStore.hpp"

// The platform-independant EEPROM class

///////////////////////////////////////
// PLATFORM-SPECIFIC IMPLEMENTATIONS

#if USE_DUMMY_EEPROM == true

static uint8_t dummyEepromStorage[EEPROMStore::STORE_SIZE];

// Initialize the EEPROM object for ESP boards, setting aside storage
void EEPROMStore::initialize()
{
    INFO(DEBUG_EEPROM, "[EEPROM]: Dummy: Startup with %d bytes", EEPROMStore::STORE_SIZE);
    memset(dummyEepromStorage, 0, sizeof(dummyEepromStorage));

    displayContents();  // Will always be empty at restart
}

// Update the given location with the given value
void EEPROMStore::update(uint8_t location, uint8_t value)
{
    INFO(DEBUG_EEPROM, "[EEPROM]: Dummy: Writing %x to %d", value, location);
    dummyEepromStorage[location] = value;
}

// Complete the transaction
void EEPROMStore::commit()
{
    // Nothing to do
}

// Read the value at the given location
uint8_t EEPROMStore::read(uint8_t location)
{
    uint8_t value;
    value = dummyEepromStorage[location];
    INFO(DEBUG_EEPROM, "[EEPROM]: Dummy: Read %x from %d", value, location);
    return value;
}

#elif defined(ESP32)

// Initialize the EEPROM object for ESP boards, setting aside space for storage
void EEPROMStore::initialize()
{
    INFO(DEBUG_EEPROM, "[EEPROM]: ESP32: Startup with %d bytes", STORE_SIZE);
    EEPROM.begin(STORE_SIZE);

    displayContents();
}

// Update the given location with the given value
void EEPROMStore::update(uint8_t location, uint8_t value)
{
    INFO(DEBUG_EEPROM, "[EEPROM]: ESP32: Writing %x to %d", value, location);
    EEPROM.write(location, value);
}

// Complete the transaction
void EEPROMStore::commit()
{
    INFO(DEBUG_EEPROM, "[EEPROM]: ESP32: Committing");
    EEPROM.commit();
}

// Read the value at the given location
uint8_t EEPROMStore::read(uint8_t location)
{
    uint8_t value;
    value = EEPROM.read(location);
    INFO(DEBUG_EEPROM, "[EEPROM]: ESP32: Read %x from %d", value, location);
    return value;
}

#else

// Initialize the EEPROM storage in a platform-independent abstraction
void EEPROMStore::initialize()
{
    INFO(DEBUG_EEPROM, "[EEPROM]: ATMega: Startup");

    displayContents();
}

// Update the given location with the given value
void EEPROMStore::update(uint8_t location, uint8_t value)
{
    INFO(DEBUG_EEPROM, "[EEPROM]: ATMega: Writing8 %x to %d", value, location);
    EEPROM.write(location, value);
}

// Complete the transaction
void EEPROMStore::commit()
{
    // Nothing to do
}

// Read the value at the given location
uint8_t EEPROMStore::read(uint8_t location)
{
    uint8_t value = EEPROM.read(location);
    INFO(DEBUG_EEPROM, "[EEPROM]: ATMega: Read8 %x from %d", value, location);
    return value;
}

#endif

void EEPROMStore::displayContents()
{
#if (DEBUG_LEVEL & (DEBUG_INFO | DEBUG_EEPROM))
    // Read the magic marker byte and state
    uint16_t marker = readUint16(MAGIC_MARKER_AND_FLAGS_ADDR);
    INFO(DEBUG_EEPROM, "[EEPROM]: Magic Marker: %x", marker);

    INFO(DEBUG_INFO, "[EEPROM]: Has %svalues", ((marker & MAGIC_MARKER_MASK) == MAGIC_MARKER_VALUE) ? "" : "NO");
    INFO(DEBUG_INFO, "[EEPROM]: Has %sextended values", ((marker & EXTENDED_FLAG) == EXTENDED_FLAG) ? "" : "NO");
    if (EEPROMStore::isPresent(EXTENDED_FLAG))
    {
        INFO(DEBUG_INFO, "[EEPROM]: IsPresent(EXTENDED): Yes");
    }
    else
    {
        INFO(DEBUG_INFO, "[EEPROM]: IsPresent(EXTENDED): No");
    }
    INFO(DEBUG_INFO, "[EEPROM]: Stored HATime: %s", getHATime().ToString());
    INFO(DEBUG_INFO, "[EEPROM]: Stored UTC Offset: %d", getUtcOffset());
    INFO(DEBUG_INFO, "[EEPROM]: Stored Brightness: %d", getBrightness());
    INFO(DEBUG_INFO, "[EEPROM]: Stored RA Steps per Degree: %f", getRAStepsPerDegree());
    INFO(DEBUG_INFO, "[EEPROM]: Stored DEC Steps per Degree: %f", getDECStepsPerDegree());
    INFO(DEBUG_INFO, "[EEPROM]: Stored Speed Factor: %f", getSpeedFactor());
    INFO(DEBUG_INFO, "[EEPROM]: Stored Backlash Correction Steps: %d", getBacklashCorrectionSteps());
    INFO(DEBUG_INFO, "[EEPROM]: Stored Latitude: %s", getLatitude().ToString());
    INFO(DEBUG_INFO, "[EEPROM]: Stored Longitude: %s", getLongitude().ToString());
    INFO(DEBUG_INFO, "[EEPROM]: Stored Pitch Calibration Angle: %f", getPitchCalibrationAngle());
    INFO(DEBUG_INFO, "[EEPROM]: Stored Roll Calibration Angle: %f", getRollCalibrationAngle());
    INFO(DEBUG_INFO, "[EEPROM]: Stored RA Parking Position: %l", getRAParkingPos());
    INFO(DEBUG_INFO, "[EEPROM]: Stored DEC Parking Position: %l", getDECParkingPos());
    INFO(DEBUG_INFO, "[EEPROM]: Stored DEC Lower Limit: %l", getDECLowerLimit());
    INFO(DEBUG_INFO, "[EEPROM]: Stored DEC Upper Limit: %l", getDECUpperLimit());
#endif
}

///////////////////////////////////////
// HELPER FUNCTIONS

// Helper to update the given location with the given 8-bit value
void EEPROMStore::updateUint8(EEPROMStore::ItemAddress location, uint8_t value)
{
    INFO(DEBUG_EEPROM, "[EEPROM]: Writing8 %x (%d) to %d", value, value, location);
    update(location, value);
}

// Helper to read an 8-bit value from the given location
uint8_t EEPROMStore::readUint8(EEPROMStore::ItemAddress location)
{
    uint8_t value;
    value = read(location);
    INFO(DEBUG_EEPROM, "[EEPROM]: Read8 %x (%d) from %d", value, value, location);
    return value;
}

// Helper to update the given location with the given 8-bit value
void EEPROMStore::updateInt8(EEPROMStore::ItemAddress location, int8_t value)
{
    INFO(DEBUG_EEPROM, "[EEPROM]: Writing8 %x (%d) to %d", value, value, location);
    update(location, static_cast<uint8_t>(value));
}

// Helper to read an 8-bit value from the given location
int8_t EEPROMStore::readInt8(EEPROMStore::ItemAddress location)
{
    int8_t value;
    value = static_cast<int8_t>(read(location));
    INFO(DEBUG_EEPROM, "[EEPROM]: Read8 %x (%d) from %d", value, value, location);
    return value;
}

// Helper to update the given location with the given 16-bit value
void EEPROMStore::updateInt16(EEPROMStore::ItemAddress location, int16_t value)
{
    INFO(DEBUG_EEPROM, "[EEPROM]: Writing16 %x (%d) to %d", value, value, location);
    update(location, value & 0x00FF);
    update(location + 1, (value >> 8) & 0x00FF);
}

// Helper to read a 16-bit value from the given location
int16_t EEPROMStore::readInt16(EEPROMStore::ItemAddress location)
{
    uint8_t valLo   = read(location);
    uint8_t valHi   = read(location + 1);
    uint16_t uValue = (uint16_t) valLo + (uint16_t) valHi * 256;
    int16_t value   = static_cast<int16_t>(uValue);
    INFO(DEBUG_EEPROM, "[EEPROM]: Read16 %x (%d) from %d", value, value, location);
    return value;
}

// Helper to update the given location with the given 16-bit value
void EEPROMStore::updateUint16(EEPROMStore::ItemAddress location, uint16_t value)
{
    INFO(DEBUG_EEPROM, "[EEPROM]: Writing16 %x (%d) to %d", value, value, location);
    update(location, value & 0x00FF);
    update(location + 1, (value >> 8) & 0x00FF);
}

// Helper to read a 16-bit value from the given location
uint16_t EEPROMStore::readUint16(EEPROMStore::ItemAddress location)
{
    uint8_t valLo  = read(location);
    uint8_t valHi  = read(location + 1);
    uint16_t value = (uint16_t) valLo + (uint16_t) valHi * 256;
    INFO(DEBUG_EEPROM, "[EEPROM]: Read16 %x (%d) from %d", value, value, location);
    return value;
}

// Helper to update the given location with the given 32-bit value
void EEPROMStore::updateInt32(EEPROMStore::ItemAddress location, int32_t value)
{
    INFO(DEBUG_EEPROM, "[EEPROM]: Writing32 %x (%l) to %d", value, value, location);
    update(location, value & 0x00FF);
    update(location + 1, (value >> 8) & 0x00FF);
    update(location + 2, (value >> 16) & 0x00FF);
    update(location + 3, (value >> 24) & 0x00FF);
}

// Helper to read a 32-bit value from the given location
int32_t EEPROMStore::readInt32(EEPROMStore::ItemAddress location)
{
    uint8_t val1    = read(location);
    uint8_t val2    = read(location + 1);
    uint8_t val3    = read(location + 2);
    uint8_t val4    = read(location + 3);
    uint32_t uValue = (uint32_t) val1 + (uint32_t) val2 * 256 + (uint32_t) val3 * 256 * 256 + (uint32_t) val4 * 256 * 256 * 256;
    int32_t value   = static_cast<int32_t>(uValue);
    INFO(DEBUG_EEPROM, "[EEPROM]: Read32 %x (%l) from %d", value, value, location);
    return value;
}

// Check if the specified item is present in the core set.
// Returns: true - if marker is present and item flag is set
//          false - otherwise
bool EEPROMStore::isPresent(ItemFlag item)
{
    uint16_t marker = readUint16(MAGIC_MARKER_AND_FLAGS_ADDR);

    unsigned check  = (MAGIC_MARKER_MASK | item);
    unsigned result = (MAGIC_MARKER_VALUE | item);
    bool res        = ((marker & check) == result);
    INFO(DEBUG_EEPROM, "[EEPROM]: IsDataPresent (%x). Checking (%x & %x) == %x => %d", item, marker, check, result, res);

    // Data is only present if both magic marker and item flag are present
    //return ((marker & (MAGIC_MARKER_MASK | item)) == (MAGIC_MARKER_VALUE | item));
    return res;
}

// Check if the specified item is present in the extended set.
// Returns: true - if marker is present, estended values are present, and the extended item flag is set
//          false - otherwise
bool EEPROMStore::isPresentExtended(ExtendedItemFlag item)
{
    // Check if any extended data is present
    if (!isPresent(EXTENDED_FLAG))
        return false;  // No extended data present

    // Have extended data, now see if required item is available
    uint16_t marker = readUint16(EXTENDED_FLAGS_ADDR);

    bool result = (marker & item);
    INFO(DEBUG_EEPROM, "[EEPROM]: IsExtendedDataPresent (%x). Checking (%x & %x) => %d", item, marker, item, result);

    return result;
}

// Updates the core flags for the specified item to indicate it is present.
// Note it is only possible to add items, not to remove them.
void EEPROMStore::updateFlags(ItemFlag item)
{
    uint16_t newMarkerAndFlags(MAGIC_MARKER_VALUE | item);

    // Grab any existing flags, if they're there
    uint16_t existingMarkerAndFlags = readUint16(MAGIC_MARKER_AND_FLAGS_ADDR);
    if ((existingMarkerAndFlags & MAGIC_MARKER_MASK) == MAGIC_MARKER_VALUE)
        newMarkerAndFlags |= (existingMarkerAndFlags & FLAGS_MASK);  // Accumulate flags

    updateUint16(MAGIC_MARKER_AND_FLAGS_ADDR, newMarkerAndFlags);
    // We will not commit until the actual item value has been written

    INFO(DEBUG_EEPROM, "[EEPROM]: Marker & flags updated from %x to %x", existingMarkerAndFlags, newMarkerAndFlags);
}

// Updates the core & extended flags for the specified extended item to indicate it is present.
// Note it is only possible to add items, not to remove them.
void EEPROMStore::updateFlagsExtended(ExtendedItemFlag item)
{
    uint16_t extendedFlags(0);
    // Grab any existing flags, if they're there
    if (isPresent(EXTENDED_FLAG))
        extendedFlags = readUint16(EXTENDED_FLAGS_ADDR);

    updateFlags(EXTENDED_FLAG);
    updateUint16(EXTENDED_FLAGS_ADDR, extendedFlags | item);
    // We will not commit until the actual item value has been written

    INFO(DEBUG_EEPROM, "[EEPROM]: Extended flags updated from %x to %x", extendedFlags, extendedFlags | item);
}

///////////////////////////////////////
// APPLICATION INTERFACE

// Erase all data in the store.
void EEPROMStore::clearConfiguration()
{
    updateUint16(MAGIC_MARKER_AND_FLAGS_ADDR, 0);  // Clear the magic marker and flags
    updateUint16(EXTENDED_FLAGS_ADDR, 0);          // Clear the extended flags
    commit();                                      // Complete the transaction
}

// Return the saved Hour Angle (HA)
DayTime EEPROMStore::getHATime()
{
    // There is no item flag for HA - it is assumed to always be present

    return DayTime(readUint8(HA_HOUR_ADDR), readUint8(HA_MINUTE_ADDR), 0);
}

// Store the Hour Angle (HA)
void EEPROMStore::storeHATime(DayTime const &ha)
{
    // There is no item flag for HA - it is assumed to always be present

    updateUint8(HA_HOUR_ADDR, ha.getHours());
    updateUint8(HA_MINUTE_ADDR, ha.getMinutes());
    commit();  // Complete the transaction
}

int EEPROMStore::getUtcOffset()
{
    int utcOffset = 0;
    if (isPresentExtended(UTC_OFFSET_MARKER_FLAG))
    {
        utcOffset = readInt8(UTC_OFFSET_ADDR);
        INFO(DEBUG_EEPROM, "[EEPROM]: UTC OFfset Marker OK! UTC Offset is %d", utcOffset);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored value for UTC Offset");
    }

    return utcOffset;
}

void EEPROMStore::storeUtcOffset(int utcOffset)
{
    updateInt8(UTC_OFFSET_ADDR, utcOffset);
    updateFlagsExtended(UTC_OFFSET_MARKER_FLAG);
    commit();  // Complete the transaction
}

// Return the dimensionless brightness value for the display.
byte EEPROMStore::getBrightness()
{
    // There is no item flag for brightness - it is assumed to always be present

    byte brightness = readUint8(LCD_BRIGHTNESS_ADDR);
    if (brightness == 0)
        brightness = 10;  // Have a reasonable minimum in case nothing is stored
    return brightness;    // dimensionless scalar
}

// Store the dimensionless brightness value for the display.
void EEPROMStore::storeBrightness(byte brightness)
{
    // There is no item flag for brightness - it is assumed to always be present

    updateUint8(LCD_BRIGHTNESS_ADDR, brightness);
    commit();  // Complete the transaction
}

// Return the RA steps per degree (actually microsteps per degree).
// If it is not present then the default uncalibrated RA_STEPS_PER_DEGREE value is returned.
float EEPROMStore::getRAStepsPerDegree()
{
    float raStepsPerDegree(RA_STEPS_PER_DEGREE);  // Default value

    if (isPresent(RA_STEPS_FLAG))
    {
        raStepsPerDegree = 0.1 * readInt16(RA_STEPS_DEGREE_ADDR);
        INFO(DEBUG_EEPROM, "[EEPROM]: RA Marker OK! RA steps/deg is %f", raStepsPerDegree);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored value for RA steps");
    }

    return raStepsPerDegree;  // microsteps per degree
}

// Store the RA steps per degree (actually microsteps per degree).
void EEPROMStore::storeRAStepsPerDegree(float raStepsPerDegree)
{
    int32_t val = raStepsPerDegree * 10;  // Store as tenths of degree
    val         = clamp(val, (int32_t) INT16_MIN, (int32_t) INT16_MAX);
    INFO(DEBUG_EEPROM, "[EEPROM]: Storing RA steps to %d (%f)", val, raStepsPerDegree);

    updateInt16(RA_STEPS_DEGREE_ADDR, val);
    updateFlags(RA_STEPS_FLAG);
    commit();  // Complete the transaction
}

// Return the DEC steps per degree (actually microsteps per degree).
// If it is not present then the default uncalibrated DEC_STEPS_PER_DEGREE value is returned.
float EEPROMStore::getDECStepsPerDegree()
{
    float decStepsPerDegree(DEC_STEPS_PER_DEGREE);  // Default value

    if (isPresent(DEC_STEPS_FLAG))
    {
        decStepsPerDegree = 0.1 * readInt16(DEC_STEPS_DEGREE_ADDR);
        INFO(DEBUG_EEPROM, "[EEPROM]: DEC Marker OK! DEC steps/deg is %f", decStepsPerDegree);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored value for DEC steps");
    }

    return decStepsPerDegree;  // microsteps per degree
}

// Store the DEC steps per degree (actually microsteps per degree).
void EEPROMStore::storeDECStepsPerDegree(float decStepsPerDegree)
{
    int32_t val = decStepsPerDegree * 10;  // Store as tenths of degree
    val         = clamp(val, (int32_t) INT16_MIN, (int32_t) INT16_MAX);
    INFO(DEBUG_EEPROM, "[EEPROM]: Storing DEC steps to %d (%f)", val, decStepsPerDegree);

    updateInt16(DEC_STEPS_DEGREE_ADDR, val);
    updateFlags(DEC_STEPS_FLAG);
    commit();  // Complete the transaction
}

// Return the Speed Factor scalar (dimensionless).
// If it is not present then the default uncalibrated value of 1.0 is returned.
float EEPROMStore::getSpeedFactor()
{
    float speedFactor(1.0);  // Default uncalibrated value

    if (isPresent(SPEED_FACTOR_FLAG))
    {
        // Speed factor bytes are in split locations :-(
        int val     = readUint8(SPEED_FACTOR_LOW_ADDR) + (int) readUint8(SPEED_FACTOR_HIGH_ADDR) * 256;
        speedFactor = 1.0 + val / 10000.0;
        INFO(DEBUG_EEPROM, "[EEPROM]: Speed Marker OK! Speed adjust is %d, speedFactor is %f", val, speedFactor);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored value for speed factor");
    }

    return speedFactor;
}

// Store the Speed Factor (dimensionless).
void EEPROMStore::storeSpeedFactor(float speedFactor)
{
    // Store the fractional speed factor since it is a number very close to 1
    int32_t val = (speedFactor - 1.0f) * 10000.0f;
    val         = clamp(val, (int32_t) INT16_MIN, (int32_t) INT16_MAX);
    INFO(DEBUG_EEPROM, "[EEPROM]: Storing Speed Factor to %d (%f)", val, speedFactor);

    // Speed factor bytes are in split locations :-(
    updateUint8(SPEED_FACTOR_LOW_ADDR, val & 0xFF);
    updateUint8(SPEED_FACTOR_HIGH_ADDR, (val >> 8) & 0xFF);
    updateFlags(SPEED_FACTOR_FLAG);
    commit();  // Complete the transaction
}

// Return the Backlash Correction step count (microsteps).
// If it is not present then the default value from the configuration is returned.
int16_t EEPROMStore::getBacklashCorrectionSteps()
{
    // Use nominal default values
    int16_t backlashCorrectionSteps(BACKLASH_STEPS);

    if (isPresent(BACKLASH_STEPS_FLAG))
    {
        backlashCorrectionSteps = readInt16(BACKLASH_STEPS_ADDR);
        INFO(DEBUG_EEPROM, "[EEPROM]: Backlash Steps Marker OK! Backlash correction is %d", backlashCorrectionSteps);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored value for backlash correction");
    }

    return backlashCorrectionSteps;  // Microsteps (slew)
}

// Store the Backlash Correction step count (microsteps).
void EEPROMStore::storeBacklashCorrectionSteps(int16_t backlashCorrectionSteps)
{
    INFO(DEBUG_EEPROM, "EEPROM Write: Updating Backlash to %d", backlashCorrectionSteps);

    updateInt16(BACKLASH_STEPS_ADDR, backlashCorrectionSteps);
    updateFlags(BACKLASH_STEPS_FLAG);
    commit();  // Complete the transaction
}

// Return the stored location Latitude.
// If it is not present then the default value of 45 degrees North is returned.
Latitude EEPROMStore::getLatitude()
{
    Latitude latitude(45.0);  // Default value (degrees, +ve is North)

    if (isPresent(LATITUDE_FLAG))
    {
        latitude = Latitude(1.0f * readInt16(LATITUDE_ADDR) / 100.0f);
        INFO(DEBUG_EEPROM, "[EEPROM]: Latitude Marker OK! Latitude is %s", latitude.ToString());
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored value for latitude");
    }

    return latitude;  // Object
}

// Store the configured location Latitude.
void EEPROMStore::storeLatitude(Latitude const &latitude)
{
    int32_t val = static_cast<int32_t>(roundf(latitude.getTotalHours() * 100.0f));
    val         = clamp(val, (int32_t) INT16_MIN, (int32_t) INT16_MAX);
    INFO(DEBUG_EEPROM, "[EEPROM]: Storing Latitude as %d (%f)", val, latitude.getTotalHours());

    updateInt16(LATITUDE_ADDR, val);
    updateFlags(LATITUDE_FLAG);
    commit();  // Complete the transaction
}

// Return the stored location Longitude.
// If it is not present then the default value of 100 degrees East is returned.
Longitude EEPROMStore::getLongitude()
{
    Longitude longitude(100.0);  // Default value (degrees, +ve is East)

    if (isPresent(LONGITUDE_FLAG))
    {
        longitude = Longitude(1.0f * readInt16(LONGITUDE_ADDR) / 100.0f);
        INFO(DEBUG_EEPROM, "[EEPROM]: Longitude Marker OK! Longitude is %s", longitude.ToString());
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored value for longitude");
    }

    return longitude;  // Object
}

// Store the configured location Longitude.
void EEPROMStore::storeLongitude(Longitude const &longitude)
{
    int32_t val = static_cast<int32_t>(roundf(longitude.getTotalHours() * 100.0f));
    val         = clamp(val, (int32_t) INT16_MIN, (int32_t) INT16_MAX);
    INFO(DEBUG_EEPROM, "[EEPROM]: Storing Longitude as %d (%f)", val, longitude.getTotalHours());

    updateInt16(LONGITUDE_ADDR, val);
    updateFlags(LONGITUDE_FLAG);
    commit();  // Complete the transaction
}

// Return the stored Pitch Calibration Angle (degrees).
// If it is not present then the default value of 0 degrees.
float EEPROMStore::getPitchCalibrationAngle()
{
    float pitchCalibrationAngle(0);  // degrees

    if (isPresent(PITCH_OFFSET_FLAG))
    {
        int32_t val           = readUint16(PITCH_OFFSET_ADDR);
        pitchCalibrationAngle = (val - 16384) / 100.0;
        INFO(DEBUG_EEPROM, "[EEPROM]: Pitch Offset Marker OK! Pitch Offset is %d (%f)", val, pitchCalibrationAngle);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored value for Pitch Offset");
    }

    return pitchCalibrationAngle;  // degrees
}

// Store the configured Pitch Calibration Angle (degrees).
void EEPROMStore::storePitchCalibrationAngle(float pitchCalibrationAngle)
{
    int32_t val = (pitchCalibrationAngle * 100) + 16384;
    val         = clamp(val, (int32_t) INT16_MIN, (int32_t) INT16_MAX);
    INFO(DEBUG_EEPROM, "[EEPROM]: Storing Pitch calibration %d (%f)", val, pitchCalibrationAngle);

    updateInt16(PITCH_OFFSET_ADDR, val);
    updateFlags(PITCH_OFFSET_FLAG);
    commit();  // Complete the transaction
}

// Return the stored Roll Calibration Angle (degrees).
// If it is not present then the default value of 0 degrees.
float EEPROMStore::getRollCalibrationAngle()
{
    float rollCalibrationAngle(0);  // degrees

    if (isPresent(ROLL_OFFSET_FLAG))
    {
        int32_t val          = readUint16(ROLL_OFFSET_ADDR);
        rollCalibrationAngle = (val - 16384) / 100.0;
        INFO(DEBUG_EEPROM, "[EEPROM]: Roll Offset Marker OK! Roll Offset is %d (%f)", val, rollCalibrationAngle);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored value for Roll Offset");
    }

    return rollCalibrationAngle;  // degrees
}

// Store the configured Roll Calibration Angle (degrees).
void EEPROMStore::storeRollCalibrationAngle(float rollCalibrationAngle)
{
    int32_t val = (rollCalibrationAngle * 100) + 16384;
    val         = clamp(val, (int32_t) INT16_MIN, (int32_t) INT16_MAX);
    INFO(DEBUG_EEPROM, "[EEPROM]: Storing Roll calibration %d (%f)", val, rollCalibrationAngle);

    updateInt16(ROLL_OFFSET_ADDR, val);
    updateFlags(ROLL_OFFSET_FLAG);
    commit();  // Complete the transaction
}

// Return the stored RA Parking Pos (slew microsteps relative to home).
// If it is not present then the default value of 0 steps.
int32_t EEPROMStore::getRAParkingPos()
{
    int32_t raParkingPos(0);  // microsteps (slew)

    // Note that flags doesn't verify that _both_ RA & DEC parking have been written - these should always be stored as a pair
    if (isPresentExtended(PARKING_POS_MARKER_FLAG))
    {
        raParkingPos = readInt32(RA_PARKING_POS_ADDR);
        INFO(DEBUG_EEPROM, "[EEPROM]: RA Parking position read as %l", raParkingPos);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored value for Parking position");
    }

    return raParkingPos;  // microsteps (slew)
}

// Store the configured RA Parking Pos (slew microsteps relative to home).
void EEPROMStore::storeRAParkingPos(int32_t raParkingPos)
{
    INFO(DEBUG_EEPROM, "[EEPROM]: Updating RA Parking Pos to %l", raParkingPos);

    // Note that flags doesn't verify that _both_ RA & DEC parking have been written - these should always be stored as a pair
    updateInt32(RA_PARKING_POS_ADDR, raParkingPos);
    updateFlagsExtended(PARKING_POS_MARKER_FLAG);
    commit();  // Complete the transaction
}

// Return the stored DEC Parking Pos (slew microsteps relative to home).
// If it is not present then the default value of 0 steps.
int32_t EEPROMStore::getDECParkingPos()
{
    int32_t decParkingPos(0);  // microsteps (slew)

    // Note that flags doesn't verify that _both_ RA & DEC parking have been written - these should always be stored as a pair
    if (isPresentExtended(PARKING_POS_MARKER_FLAG))
    {
        decParkingPos = readInt32(DEC_PARKING_POS_ADDR);
        INFO(DEBUG_EEPROM, "[EEPROM]: DEC Parking position read as %l", decParkingPos);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored value for Parking position");
    }

    return decParkingPos;  // microsteps (slew)
}

// Store the configured DEC Parking Pos (slew microsteps relative to home).
void EEPROMStore::storeDECParkingPos(int32_t decParkingPos)
{
    INFO(DEBUG_EEPROM, "[EEPROM]: Updating DEC Parking Pos to %l", decParkingPos);

    // Note that flags doesn't verify that _both_ RA & DEC parking have been written - these should always be stored as a pair
    updateInt32(DEC_PARKING_POS_ADDR, decParkingPos);
    updateFlagsExtended(PARKING_POS_MARKER_FLAG);
    commit();  // Complete the transaction
}

// Return the stored DEC Lower Limit (slew microsteps relative to home).
// If it is not present then the default value of 0 steps (limits are disabled).
int32_t EEPROMStore::getDECLowerLimit()
{
    int32_t decLowerLimit(0);  // microsteps (slew)

    // Note that flags doesn't verify that _both_ DEC limits have been written - these should always be stored as a pair
    if (isPresentExtended(DEC_LIMIT_MARKER_FLAG))
    {
        decLowerLimit = readInt32(DEC_LOWER_LIMIT_ADDR);
        INFO(DEBUG_EEPROM, "[EEPROM]: DEC lower limit read as %l", decLowerLimit);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored values for DEC limits");
    }

    return decLowerLimit;  // microsteps (slew)
}

// Store the configured DEC Lower Limit Pos (slew microsteps relative to home).
void EEPROMStore::storeDECLowerLimit(int32_t decLowerLimit)
{
    INFO(DEBUG_EEPROM, "EEPROM Write: Updating DEC Lower Limit to %l", decLowerLimit);

    // Note that flags doesn't verify that _both_ DEC limits have been written - these should always be stored as a pair
    updateInt32(DEC_LOWER_LIMIT_ADDR, decLowerLimit);
    updateFlagsExtended(DEC_LIMIT_MARKER_FLAG);
    commit();  // Complete the transaction
}

// Return the stored DEC Upper Limit (slew microsteps relative to home).
// If it is not present then the default value of 0 steps (limits are disabled).
int32_t EEPROMStore::getDECUpperLimit()
{
    int32_t decUpperLimit(0);  // microsteps (slew)

    // Note that flags doesn't verify that _both_ DEC limits have been written - these should always be stored as a pair
    if (isPresentExtended(DEC_LIMIT_MARKER_FLAG))
    {
        decUpperLimit = readInt32(DEC_UPPER_LIMIT_ADDR);
        INFO(DEBUG_EEPROM, "[EEPROM]: DEC upper limit read as %l", decUpperLimit);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored values for DEC limits");
    }

    return decUpperLimit;  // microsteps (slew)
}

// Store the configured DEC Upper Limit Pos (slew microsteps relative to home).
void EEPROMStore::storeDECUpperLimit(int32_t decUpperLimit)
{
    INFO(DEBUG_EEPROM, "EEPROM Write: Updating DEC Upper Limit to %l", decUpperLimit);

    // Note that flags doesn't verify that _both_ DEC limits have been written - these should always be stored as a pair
    updateInt32(DEC_UPPER_LIMIT_ADDR, decUpperLimit);
    updateFlagsExtended(DEC_LIMIT_MARKER_FLAG);
    commit();  // Complete the transaction
}

// Get the configured RA Homing offset for Hall sensor homing (slew microsteps relative to home).
int32_t EEPROMStore::getRAHomingOffset()
{
    int32_t raHomingOffset(0);  // microsteps (slew)

    if (isPresentExtended(RA_HOMING_MARKER_FLAG))
    {
        raHomingOffset = readInt32(RA_HOMING_OFFSET_ADDR);
        INFO(DEBUG_EEPROM, "[EEPROM]: RA Homing offset read as %l", raHomingOffset);
    }
    else
    {
        INFO(DEBUG_EEPROM, "[EEPROM]: No stored values for RA Homing offset");
    }

    return raHomingOffset;  // microsteps (slew)
}

// Store the configured RA Homing offset for Hall sensor homing (slew microsteps relative to home).
void EEPROMStore::storeRAHomingOffset(int32_t raHomingOffset)
{
    INFO(DEBUG_EEPROM, "EEPROM Write: Updating RA Homing offset to %l", raHomingOffset);

    updateInt32(RA_HOMING_OFFSET_ADDR, raHomingOffset);
    updateFlagsExtended(RA_HOMING_MARKER_FLAG);
    commit();  // Complete the transaction
}
