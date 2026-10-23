#pragma once

#if (WIFI_ENABLED == 1)
    #include "SerialTransport.hpp"
    #include "TcpConnectionPort.hpp"
    #include "../hal/IWifiStack.hpp"

    #define WIFI_UDP_DISCOVERY_PORT 4031

// Forward declarations
class LcdMenu;
class MeadeCommandAdapter;

/// The Wi-Fi control path: bring the stack up, accept TCP peers that speak the same Meade
/// protocol as the serial port, and answer the SkyFi UDP discovery probe.
///
/// It names no Arduino Wi-Fi type: the stack, the listening socket, the accepted connection and
/// the UDP socket all arrive through `hal/IWifiStack.hpp`, so the ESP32 backend is the only place
/// that knows what the radio is.
class WifiControl
{
  public:
    WifiControl(hal::IWifiStack &stack, void (*mountTick)(), LcdMenu *lcdMenu);
    ~WifiControl();
    void setup();
    void loop();
    String getStatus();

  private:
    void startInfrastructureMode();
    void startAccessPointMode();
    void infraToAPFailover();
    void tcpLoop();
    void udpLoop();
    void establishServers();
    String getIP();

    hal::WifiStatus _status = hal::WifiStatus::Disconnected;
    hal::IWifiStack &_stack;
    void (*_mountTick)();
    LcdMenu *_lcdMenu;
    MeadeCommandAdapter *_cmdProcessor;

    hal::ITcpListener *_tcpServer = nullptr;
    hal::IUdpSocket *_udp         = nullptr;

    /// The accepted peer, if any. Owned: every accept yields a new connection.
    hal::ITcpConnection *_client                     = nullptr;
    adapters::TcpConnectionPort *_clientPort         = nullptr;
    adapters::SerialTransport<128> *_clientTransport = nullptr;

    unsigned long _infraStart = 0;
    unsigned long _infraWait  = 30000;  // 30 second timeout for
};

extern WifiControl wifiControl;

#endif  // WIFI_ENABLED
