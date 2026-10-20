#pragma once

#include "hal/IWifiStack.hpp"

namespace hal
{

/// `IWifiStack` over the Arduino-ESP32 `WiFi` singleton, with `WiFiServer` and
/// `WiFiUDP` wrapped as the socket types.
class Esp32WifiStack final : public IWifiStack
{
  public:
    void setMode(WifiMode mode) override;

    void setHostname(const char *hostname) override;

    const char *hostname() override;

    bool beginStation(const char *ssid, const char *password) override;

    bool beginAccessPoint(const char *ssid, const char *password) override;

    bool configureAccessPoint(IpAddress local, IpAddress gateway, IpAddress subnet) override;

    WifiStatus status() override;

    IpAddress stationIp() override;

    IpAddress accessPointIp() override;

    void disconnect() override;

    ITcpListener *createTcpListener() override;

    IUdpSocket *createUdpSocket() override;

  private:
    // Cached so `hostname()` can hand out a stable pointer that outlives the stack's
    // own bookkeeping.
    char _hostname[33] = "openastrotracker";
};

}  // namespace hal
