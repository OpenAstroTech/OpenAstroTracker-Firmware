#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
#include <TinyGPSPlus.h>
POP_NO_WARNINGS

#include "adapters/TinyGpsAdapter.hpp"

namespace adapters
{

TinyGpsAdapter::TinyGpsAdapter(Stream &serial) : _serial(serial), _gps(new TinyGPSPlus())
{
}

TinyGpsAdapter::~TinyGpsAdapter()
{
    delete _gps;
    _gps = nullptr;
}

bool TinyGpsAdapter::begin()
{
    // Nothing to initialise: the stream's owner already opened the port, and
    // TinyGPSPlus starts decoding on the first byte.
    return true;
}

void TinyGpsAdapter::poll()
{
    if (_gps == nullptr)
    {
        return;
    }

    while (_serial.available() > 0)
    {
        _gps->encode(static_cast<char>(_serial.read()));
    }
}

bool TinyGpsAdapter::hasLocation()
{
    return _gps != nullptr && _gps->location.isValid();
}

ports::GpsFix TinyGpsAdapter::fix()
{
    ports::GpsFix result = {};

    if (_gps == nullptr)
    {
        return result;
    }

    result.locationValid = _gps->location.isValid();
    result.latitude      = _gps->location.lat();
    result.longitude     = _gps->location.lng();
    result.locationAgeMs = _gps->location.age();
    result.dateTimeValid = _gps->date.isValid() && _gps->time.isValid();
    result.year          = static_cast<uint16_t>(_gps->date.year());
    result.month         = static_cast<uint8_t>(_gps->date.month());
    result.day           = static_cast<uint8_t>(_gps->date.day());
    result.hour          = static_cast<uint8_t>(_gps->time.hour());
    result.minute        = static_cast<uint8_t>(_gps->time.minute());
    result.second        = static_cast<uint8_t>(_gps->time.second());
    result.satellites    = static_cast<uint8_t>(_gps->satellites.value());

    return result;
}

}  // namespace adapters
