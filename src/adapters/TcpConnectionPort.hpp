#pragma once

#include "hal/ISerialPort.hpp"
#include "hal/IWifiStack.hpp"

namespace adapters
{

/// `hal::ISerialPort` over one accepted TCP connection.
///
/// The Wi-Fi command path frames exactly the same bytes as the serial one, so it reuses
/// `adapters::SerialTransport`; all it needs is a byte port. That is this class: no framing, no
/// buffering, no protocol.
class TcpConnectionPort final : public hal::ISerialPort
{
  public:
    explicit TcpConnectionPort(hal::ITcpConnection &connection);

    int available() override;

    int read() override;

    int peek() override;

    size_t write(uint8_t value) override;

    size_t write(const uint8_t *data, size_t length) override;

    /// The sockets this firmware uses have no transmit-drain call; nothing to wait for.
    void flush() override;

  private:
    hal::ITcpConnection &_connection;
};

}  // namespace adapters
