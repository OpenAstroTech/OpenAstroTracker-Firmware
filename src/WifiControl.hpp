#pragma once

#if (WIFI_ENABLED == 1)
    #include "WiFiServer.h"
    #include "WiFiUdp.h"
    #include "WiFiClient.h"

    #ifdef ESP32
        #include <WiFi.h>
        #include <WiFiSTA.h>
    #endif

#define WIFI_UDP_DISCOVERY_PORT 4031

// Forward declarations
class Mount;
class LcdMenu;
class MeadeCommandProcessor;

class WifiControl
{
  public:
    WifiControl(Mount *mount, LcdMenu *lcdMenu);
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
    Mount *_mount;
    LcdMenu *_lcdMenu;
    MeadeCommandProcessor *_cmdProcessor;

    WiFiServer *_tcpServer = nullptr;
    WiFiUDP *_udp          = nullptr;
    WiFiClient client;

    unsigned long _infraStart = 0;
    unsigned long _infraWait  = 30000;  // 30 second timeout for
};

extern WifiControl wifiControl;

#endif  // WIFI_ENABLED
