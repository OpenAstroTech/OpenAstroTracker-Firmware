#pragma once

#include "hal/ISerialPort.hpp"
#include "ports/IGps.hpp"

class Stream;
class TinyGPSPlus;

namespace adapters
{

/// `ports::IGps` over a `TinyGPSPlus` decoder and a serial stream.
///
/// Its only consumer today is the HA/GPS menu, so it is deliberately small: pump the
/// stream, then read the flattened `GpsFix`.
class TinyGpsAdapter final : public ports::IGps
{
  public:
    /// The stream's owner configures the port (baud rate, pins).
    explicit TinyGpsAdapter(Stream &serial);

    ~TinyGpsAdapter() override;

    bool begin() override;

    void poll() override;

    bool hasLocation() override;

    ports::GpsFix fix() override;

  private:
    Stream &_serial;
    TinyGPSPlus *_gps;
};

}  // namespace adapters
