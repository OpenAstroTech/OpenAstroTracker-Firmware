#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal
{

enum class WifiMode : uint8_t
{
    Off,
    Station,
    AccessPoint,
};

/// Connection state, in the vocabulary of the Arduino WiFi libraries so a backend
/// can map its own status codes without inventing a second state machine.
enum class WifiStatus : uint8_t
{
    Idle,
    NoSsid,
    ScanCompleted,
    Connected,
    ConnectFailed,
    ConnectionLost,
    Disconnected,
};

struct IpAddress {
    uint8_t octets[4];
};

// Forward declarations: sockets are created by the stack they belong to.
class ITcpConnection;
class ITcpListener;
class IUdpSocket;

/// One accepted TCP client connection.
class ITcpConnection
{
  public:
    virtual ~ITcpConnection()
    {
    }

    virtual bool connected() = 0;

    virtual int available() = 0;

    virtual int read() = 0;

    virtual int peek() = 0;

    virtual size_t write(const uint8_t *data, size_t length) = 0;

    virtual void stop() = 0;
};

/// A listening TCP server socket.
class ITcpListener
{
  public:
    virtual ~ITcpListener()
    {
    }

    virtual bool begin(uint16_t port) = 0;

    virtual void setNoDelay(bool enabled) = 0;

    /// Accepts a pending client. Returns nullptr when none is waiting; the caller
    /// owns the returned connection and must delete it.
    virtual ITcpConnection *accept() = 0;

    virtual void stop() = 0;
};

/// A UDP socket bound to one local port.
class IUdpSocket
{
  public:
    virtual ~IUdpSocket()
    {
    }

    virtual bool begin(uint16_t port) = 0;

    /// Size of the datagram waiting to be read, 0 when none is.
    virtual int parsePacket() = 0;

    virtual IpAddress remoteIp() = 0;

    virtual uint16_t remotePort() = 0;

    virtual int read(uint8_t *buffer, size_t capacity) = 0;

    virtual bool beginPacket(IpAddress address, uint16_t port) = 0;

    virtual size_t write(const uint8_t *data, size_t length) = 0;

    virtual bool endPacket() = 0;
};

/// Network stack: connection management plus factories for the sockets above.
///
/// Sockets returned by the factories are owned by the caller, which must delete
/// them through the returned interface pointer (`stop()` alone does not free).
class IWifiStack
{
  public:
    virtual ~IWifiStack()
    {
    }

    virtual void setMode(WifiMode mode) = 0;

    virtual void setHostname(const char *hostname) = 0;

    /// The hostname the stack currently reports; never nullptr.
    virtual const char *hostname() = 0;

    virtual bool beginStation(const char *ssid, const char *password) = 0;

    virtual bool beginAccessPoint(const char *ssid, const char *password) = 0;

    virtual bool configureAccessPoint(IpAddress local, IpAddress gateway, IpAddress subnet) = 0;

    virtual WifiStatus status() = 0;

    virtual IpAddress stationIp() = 0;

    virtual IpAddress accessPointIp() = 0;

    virtual void disconnect() = 0;

    virtual ITcpListener *createTcpListener() = 0;

    virtual IUdpSocket *createUdpSocket() = 0;
};

}  // namespace hal
