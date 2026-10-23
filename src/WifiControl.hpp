#pragma once

#if (WIFI_ENABLED == 1)
    #include "WiFiServer.h"
    #include "WiFiUdp.h"
    #include "WiFiClient.h"

    #include "adapters/SerialTransport.hpp"
    #include "hal/arduino/ArduinoSerialPort.hpp"

    #ifdef ESP32
        #include <WiFi.h>
        #include <WiFiSTA.h>
    #endif

    #define WIFI_UDP_DISCOVERY_PORT 4031

// Forward declarations
class LcdMenu;
class MeadeCommandAdapter;

class WifiControl
{
  public:
    WifiControl(void (*mountTick)(), LcdMenu *lcdMenu);
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
    wl_status_t _status = WL_DISCONNECTED;
    void (*_mountTick)();
    LcdMenu *_lcdMenu;
    MeadeCommandAdapter *_cmdProcessor;

    WiFiServer *_tcpServer = nullptr;
    WiFiUDP *_udp          = nullptr;
    WiFiClient client;

    // The TCP peer as a transport: the same framing, ACK probe and reply write the serial path
    // uses. The Wi-Fi path accepted 127-character commands, which is what the capacity keeps.
    hal::ArduinoSerialPort *_clientPort              = nullptr;
    adapters::SerialTransport<128> *_clientTransport = nullptr;

    unsigned long _infraStart = 0;
    unsigned long _infraWait  = 30000;  // 30 second timeout for
};

extern WifiControl wifiControl;

#endif  // WIFI_ENABLED
