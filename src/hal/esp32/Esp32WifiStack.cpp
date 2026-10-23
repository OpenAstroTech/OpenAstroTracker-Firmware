#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <WiFi.h>
#include <WiFiUdp.h>
POP_NO_WARNINGS

#include <string.h>

#include "hal/esp32/Esp32WifiStack.hpp"

namespace hal
{
namespace
{

IpAddress toHalAddress(const IPAddress &address)
{
    IpAddress converted;
    for (uint8_t i = 0; i < 4; i++)
    {
        converted.octets[i] = address[i];
    }
    return converted;
}

IPAddress toArduinoAddress(const IpAddress &address)
{
    return IPAddress(address.octets[0], address.octets[1], address.octets[2], address.octets[3]);
}

WifiStatus toHalStatus(wl_status_t status)
{
    switch (status)
    {
        case WL_IDLE_STATUS:
            return WifiStatus::Idle;
        case WL_NO_SSID_AVAIL:
            return WifiStatus::NoSsid;
        case WL_SCAN_COMPLETED:
            return WifiStatus::ScanCompleted;
        case WL_CONNECTED:
            return WifiStatus::Connected;
        case WL_CONNECT_FAILED:
            return WifiStatus::ConnectFailed;
        case WL_CONNECTION_LOST:
            return WifiStatus::ConnectionLost;
        case WL_DISCONNECTED:
        default:
            return WifiStatus::Disconnected;
    }
}

class Esp32TcpConnection final : public ITcpConnection
{
  public:
    bool connected() override
    {
        return _client.connected();
    }

    int available() override
    {
        return _client.available();
    }

    int read() override
    {
        return _client.read();
    }

    int peek() override
    {
        return _client.peek();
    }

    size_t write(const uint8_t *data, size_t length) override
    {
        return _client.write(data, length);
    }

    void stop() override
    {
        _client.stop();
    }

    WiFiClient _client;
};

class Esp32TcpListener final : public ITcpListener
{
  public:
    bool begin(uint16_t port) override
    {
        _server.begin(port);
        return true;
    }

    void setNoDelay(bool enabled) override
    {
        _server.setNoDelay(enabled);
    }

    ITcpConnection *accept() override
    {
        WiFiClient client = _server.accept();
        if (!client.connected())
        {
            return nullptr;
        }

        Esp32TcpConnection *connection = new Esp32TcpConnection();
        connection->_client            = client;
        return connection;
    }

    void stop() override
    {
        _server.stop();
    }

  private:
    WiFiServer _server {0};
};

class Esp32UdpSocket final : public IUdpSocket
{
  public:
    bool begin(uint16_t port) override
    {
        return _udp.begin(port) == 1;
    }

    int parsePacket() override
    {
        return _udp.parsePacket();
    }

    IpAddress remoteIp() override
    {
        return toHalAddress(_udp.remoteIP());
    }

    uint16_t remotePort() override
    {
        return _udp.remotePort();
    }

    int read(uint8_t *buffer, size_t capacity) override
    {
        return _udp.read(buffer, capacity);
    }

    bool beginPacket(IpAddress address, uint16_t port) override
    {
        return _udp.beginPacket(toArduinoAddress(address), port) == 1;
    }

    size_t write(const uint8_t *data, size_t length) override
    {
        return _udp.write(data, length);
    }

    bool endPacket() override
    {
        return _udp.endPacket() == 1;
    }

  private:
    WiFiUDP _udp;
};

}  // namespace

void Esp32WifiStack::setMode(WifiMode mode)
{
    switch (mode)
    {
        case WifiMode::Off:
            WiFi.mode(WIFI_OFF);
            break;
        case WifiMode::Station:
            WiFi.mode(WIFI_STA);
            break;
        case WifiMode::AccessPoint:
            WiFi.mode(WIFI_AP);
            break;
    }
}

void Esp32WifiStack::setHostname(const char *hostname)
{
    if (hostname == nullptr)
    {
        return;
    }

    strncpy(_hostname, hostname, sizeof(_hostname) - 1);
    _hostname[sizeof(_hostname) - 1] = '\0';
    WiFi.setHostname(_hostname);
}

const char *Esp32WifiStack::hostname()
{
    return _hostname;
}

bool Esp32WifiStack::beginStation(const char *ssid, const char *password)
{
    WiFi.begin(ssid, password);
    return true;  // association is asynchronous, poll status()
}

bool Esp32WifiStack::beginAccessPoint(const char *ssid, const char *password)
{
    return WiFi.softAP(ssid, password);
}

bool Esp32WifiStack::configureAccessPoint(IpAddress local, IpAddress gateway, IpAddress subnet)
{
    return WiFi.softAPConfig(toArduinoAddress(local), toArduinoAddress(gateway), toArduinoAddress(subnet));
}

WifiStatus Esp32WifiStack::status()
{
    return toHalStatus(WiFi.status());
}

IpAddress Esp32WifiStack::stationIp()
{
    return toHalAddress(WiFi.localIP());
}

IpAddress Esp32WifiStack::accessPointIp()
{
    return toHalAddress(WiFi.softAPIP());
}

void Esp32WifiStack::disconnect()
{
    WiFi.disconnect();
}

ITcpListener *Esp32WifiStack::createTcpListener()
{
    return new Esp32TcpListener();
}

IUdpSocket *Esp32WifiStack::createUdpSocket()
{
    return new Esp32UdpSocket();
}

}  // namespace hal
