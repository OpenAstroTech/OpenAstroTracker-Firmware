#pragma once

#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <WString.h>  // `String`: the Wi-Fi status lines are handed back as one
POP_NO_WARNINGS

#if (WIFI_ENABLED == 1)
    #include "SerialTransport.hpp"
    #include "TcpConnectionPort.hpp"
    #include "../hal/IWifiStack.hpp"

    #define WIFI_UDP_DISCOVERY_PORT 4031

// Forward declarations
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
    WifiControl(hal::IWifiStack &stack, void (*mountTick)());
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
