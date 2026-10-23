#pragma once

#include <stdint.h>

namespace core
{

/// The firmware version as the number the EEPROM migration compares against.
///
/// `VERSION` is a release-please string such as `"v1.13.22"`; the stored "last flashed" value is
/// the same version in the form `major * 10000 + minor * 100 + patch`, i.e. 11322. A leading `v`
/// is optional, parsing stops at the first non-digit of each component, and a missing component
/// counts as zero — so `"v1.13"` is 11300 and an unparseable string is 0.
int16_t firmwareVersionCode(const char *version);

}  // namespace core
