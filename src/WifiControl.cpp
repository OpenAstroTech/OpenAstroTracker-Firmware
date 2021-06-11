#include "../Configuration.hpp"
#include "Utility.hpp"
#include "WifiControl.hpp"
#include "MeadeCommandProcessor.hpp"
#include "Mount.hpp"

#if (WIFI_ENABLED == 1)

WifiControl::WifiControl(Mount *mount, LcdMenu *lcdMenu)
{
    _mount = mount;
    _lcdMenu = lcdMenu;
}

void WifiControl::setup()
{

    LOGV2(DEBUG_WIFI, F("Wifi: Starting up Wifi As Mode %d\n"), WIFI_MODE);

    _cmdProcessor = MeadeCommandProcessor::instance();

    switch (WIFI_MODE)
    {
    case WIFI_MODE_INFRASTRUCTURE: // startup Infrastructure Mode
        startInfrastructureMode();
        break;
    case WIFI_MODE_AP_ONLY: // startup AP mode
        startAccessPointMode();
        break;
    case WIFI_MODE_ATTEMPT_INFRASTRUCTURE_FAIL_TO_AP: // Attempt Infra, fail over to AP
        startInfrastructureMode();
        _infraStart = millis();
        break;
    case WIFI_MODE_DISABLED: // Disabled
        WiFi.mode(WIFI_OFF);
        btStop();
        break;
    }
}

void WifiControl::startInfrastructureMode()
{
    LOGV1(DEBUG_WIFI, F("Wifi: Starting Infrastructure Mode Wifi"));
    LOGV2(DEBUG_WIFI, F("Wifi:    with host name: %s"), String(WIFI_HOSTNAME).c_str());
    LOGV2(DEBUG_WIFI, F("Wifi:          for SSID: %s"), String(WIFI_INFRASTRUCTURE_MODE_SSID).c_str());
    LOGV2(DEBUG_WIFI, F("Wifi:       and WPA key: %s"), String(WIFI_INFRASTRUCTURE_MODE_WPAKEY).c_str());

#if defined(ESP32)
    WiFi.setHostname(WIFI_HOSTNAME);
#endif
    WiFi.begin(WIFI_INFRASTRUCTURE_MODE_SSID, WIFI_INFRASTRUCTURE_MODE_WPAKEY);
}

void WifiControl::startAccessPointMode()
{
    LOGV1(DEBUG_WIFI, F("Wifi: Starting AP Mode Wifi"));
    IPAddress local_ip(192, 168, 1, 1);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);

#if defined(ESP32)
    WiFi.setHostname(WIFI_HOSTNAME);
#endif

    WiFi.softAP(WIFI_HOSTNAME, WIFI_AP_MODE_WPAKEY);
    WiFi.softAPConfig(local_ip, gateway, subnet);
}

String wifiStatus(int status)
{
    if (status == WL_IDLE_STATUS)
        return "Idle.";
    if (status == WL_NO_SSID_AVAIL)
        return "No SSID available.";
    if (status == WL_SCAN_COMPLETED)
        return "Scan completed.";
    if (status == WL_CONNECTED)
        return "Connected!";
    if (status == WL_CONNECT_FAILED)
        return "Connect failed.";
    if (status == WL_CONNECTION_LOST)
        return "Connection Lost.";
    if (status == WL_DISCONNECTED)
        return "Disconnected.";
    return "Status " + String(status);
}

String WifiControl::getStatus()
{
    if (WIFI_MODE == WIFI_MODE_DISABLED)
    {
        return "0,";
    }

    String result = "1,";

    if (WIFI_MODE == WIFI_MODE_INFRASTRUCTURE)
    {
        result += "Infrastructure,";
    }
    else if (WIFI_MODE == WIFI_MODE_AP_ONLY)
    {
        result += "Access Point,";
    }
    else if (WIFI_MODE == WIFI_MODE_ATTEMPT_INFRASTRUCTURE_FAIL_TO_AP)
    {
        result += "Infra-Fail-To-AP,";
    }

    result += wifiStatus(WiFi.status()) + ",";
#if defined(ESP32)
    result += WiFi.getHostname();
#endif

    result += "," + WiFi.localIP().toString() + ":" + WIFI_PORT;
    result += "," + String(WIFI_INFRASTRUCTURE_MODE_SSID) + "," + String(WIFI_HOSTNAME);

    return result;
}

void WifiControl::loop()
{
    if (WIFI_MODE == WIFI_MODE_DISABLED)
    {
        return;
    }
    if (_status != WiFi.status())
    {
        _status = WiFi.status();
        LOGV2(DEBUG_WIFI, F("Wifi: Connected status changed to %s"), wifiStatus(_status).c_str());
        if (_status == WL_CONNECTED)
        {
            _tcpServer = new WiFiServer(WIFI_PORT);
            _tcpServer->begin();
            _tcpServer->setNoDelay(true);

            _udp = new WiFiUDP();
            _udp->begin(4031);

            LOGV4(DEBUG_WIFI, F("Wifi: Connecting to SSID %s at %s:%d"), WIFI_INFRASTRUCTURE_MODE_SSID, WiFi.localIP().toString().c_str(), WIFI_PORT);
        }
    }

    _mount->loop();

    if (_status != WL_CONNECTED)
    {
        infraToAPFailover();
        return;
    }

    tcpLoop();
    udpLoop();
}

void WifiControl::infraToAPFailover()
{
    if (_infraStart != 0 &&
        !WiFi.isConnected() &&
        _infraStart + _infraWait < millis())
    {

        WiFi.disconnect();
        startAccessPointMode();
        _infraStart = 0;

        LOGV1(DEBUG_WIFI, F("Wifi: Could not connect to Infra, Starting AP."));
    }
}

void WifiControl::tcpLoop()
{
    if (client && client.connected())
    {
        while (client.available())
        {
            LOGV2(DEBUG_WIFI, F("WifiTCP: Available bytes %d. Peeking."), client.available());

            // Peek first byte and check for ACK (0x06) handshake
            LOGV2(DEBUG_WIFI, F("WifiTCP: First byte is %x"), client.peek());
            if (client.peek() == 0x06)
            {
                client.read();
                LOGV1(DEBUG_WIFI, F("WifiTCP: Query <-- Handshake request"));
                client.write("1");
                LOGV1(DEBUG_WIFI, F("WifiTCP: Reply --> 1"));
            }
            else
            {
                String cmd = client.readStringUntil('#');
                LOGV2(DEBUG_WIFI, F("WifiTCP: Query <-- %s#"), cmd.c_str());
                String retVal = _cmdProcessor->processCommand(cmd);

                if (retVal != "")
                {
                    client.write(retVal.c_str());
                    LOGV2(DEBUG_WIFI, F("WifiTCP: Reply --> %s"), retVal.c_str());
                }
                else
                {
                    LOGV1(DEBUG_WIFI, F("WifiTCP: No Reply"));
                }
            }

            _mount->loop();
        }
    }
    else
    {
        client = _tcpServer->available();
    }
}

void WifiControl::udpLoop()
{
    int packetSize = _udp->parsePacket();
    if (packetSize)
    {
        String lookingFor = "skyfi:";
        String reply = "skyfi:";
        reply += WIFI_HOSTNAME;
        reply += "@";
        reply += WiFi.localIP().toString();
        LOGV4(DEBUG_WIFI, F("WifiUDP: Received %d bytes from %s, port %d"), packetSize, _udp->remoteIP().toString().c_str(), _udp->remotePort());
        char incomingPacket[255];
        int len = _udp->read(incomingPacket, 255);
        incomingPacket[len] = 0;
        LOGV2(DEBUG_WIFI, F("WifiUDP: Received: %s"), incomingPacket);

        incomingPacket[lookingFor.length()] = 0;
        if (lookingFor.equalsIgnoreCase(incomingPacket))
        {
            _udp->beginPacket(_udp->remoteIP(), 4031);
            /*unsigned char bytes[255];
            reply.getBytes(bytes, 255);
            _udp->write(bytes, reply.length());*/

#if defined(ESP32)
            _udp->print(reply.c_str());
#endif

            _udp->endPacket();
            LOGV2(DEBUG_WIFI, F("WifiUDP: Replied: %s"), reply.c_str());
        }
    }
}
#endif
