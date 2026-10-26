#include "../Configuration.hpp"
#include "../Utility.hpp"
#include "WifiControl.hpp"
#include "MeadeCommandAdapter.hpp"

#if (WIFI_ENABLED == 1)

namespace
{
/// The stack's status in the words the status menu and `:GX#` have always shown.
String wifiStatus(hal::WifiStatus status)
{
    switch (status)
    {
        case hal::WifiStatus::Idle:
            return "Idle.";
        case hal::WifiStatus::NoSsid:
            return "No SSID available.";
        case hal::WifiStatus::ScanCompleted:
            return "Scan completed.";
        case hal::WifiStatus::Connected:
            return "Connected!";
        case hal::WifiStatus::ConnectFailed:
            return "Connect failed.";
        case hal::WifiStatus::ConnectionLost:
            return "Connection Lost.";
        case hal::WifiStatus::Disconnected:
            return "Disconnected.";
    }
    return "Idle.";
}

String formatIp(hal::IpAddress address)
{
    return String(address.octets[0]) + "." + String(address.octets[1]) + "." + String(address.octets[2]) + "." + String(address.octets[3]);
}

hal::IpAddress toIp(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    hal::IpAddress address;
    address.octets[0] = a;
    address.octets[1] = b;
    address.octets[2] = c;
    address.octets[3] = d;
    return address;
}
}  // namespace

WifiControl::WifiControl(hal::IWifiStack &stack, void (*mountTick)()) : _stack(stack)
{
    _mountTick = mountTick;
}

WifiControl::~WifiControl()
{
    delete _clientTransport;
    delete _clientPort;
    delete _client;
    delete _tcpServer;
    delete _udp;
}

void WifiControl::setup()
{
    LOG(DEBUG_WIFI, "[WIFI]: Starting up Wifi As Mode %d\n", WIFI_MODE);

    _cmdProcessor = MeadeCommandAdapter::instance();

    switch (WIFI_MODE)
    {
        case WIFI_MODE_INFRASTRUCTURE:  // startup Infrastructure Mode
            startInfrastructureMode();
            break;
        case WIFI_MODE_AP_ONLY:  // startup AP mode
            startAccessPointMode();
            break;
        case WIFI_MODE_ATTEMPT_INFRASTRUCTURE_FAIL_TO_AP:  // Attempt Infra, fail over to AP
            startInfrastructureMode();
            _infraStart = millis();
            break;
        case WIFI_MODE_DISABLED:  // Disabled
            _stack.setMode(hal::WifiMode::Off);
            break;
    }
}

void WifiControl::startInfrastructureMode()
{
    LOG(DEBUG_WIFI, "[WIFI]: Starting Infrastructure Mode Wifi");
    LOG(DEBUG_WIFI, "[WIFI]:    with host name: %s", String(WIFI_HOSTNAME).c_str());
    LOG(DEBUG_WIFI, "[WIFI]:          for SSID: %s", String(WIFI_INFRASTRUCTURE_MODE_SSID).c_str());
    LOG(DEBUG_WIFI, "[WIFI]:       and WPA key: %s", String(WIFI_INFRASTRUCTURE_MODE_WPAKEY).c_str());

    _stack.setHostname(WIFI_HOSTNAME);
    _stack.beginStation(WIFI_INFRASTRUCTURE_MODE_SSID, WIFI_INFRASTRUCTURE_MODE_WPAKEY);
}

void WifiControl::startAccessPointMode()
{
    LOG(DEBUG_WIFI, "[WIFI]: Starting AP Mode Wifi");

    _stack.setHostname(WIFI_HOSTNAME);
    _stack.setMode(hal::WifiMode::AccessPoint);
    _stack.configureAccessPoint(toIp(192, 168, 1, 1), toIp(192, 168, 1, 1), toIp(255, 255, 255, 0));
    if (!_stack.beginAccessPoint(WIFI_HOSTNAME, WIFI_AP_MODE_WPAKEY))
    {
        LOG(DEBUG_WIFI, "[WIFI]: AP mode could not be started!");
        return;
    }

    establishServers();
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

    if (WIFI_MODE != WIFI_MODE_AP_ONLY)
    {
        result += wifiStatus(_stack.status()) + ",";
    }
    result += _stack.hostname();
    result += ",";

    result += getIP() + ":" + WIFI_PORT + ",";
    if (WIFI_MODE != WIFI_MODE_AP_ONLY)
    {
        result += String(WIFI_INFRASTRUCTURE_MODE_SSID) + ",";
    }
    result += String(WIFI_HOSTNAME);

    return result;
}

void WifiControl::establishServers()
{
    delete _tcpServer;
    _tcpServer = _stack.createTcpListener();
    if (_tcpServer != nullptr)
    {
        _tcpServer->begin(WIFI_PORT);
        _tcpServer->setNoDelay(true);
    }

    delete _udp;
    _udp = _stack.createUdpSocket();
    if (_udp != nullptr)
    {
        _udp->begin(WIFI_UDP_DISCOVERY_PORT);
    }
}

String WifiControl::getIP()
{
    return WIFI_MODE == WIFI_MODE_DISABLED  ? "NONE"
           : WIFI_MODE == WIFI_MODE_AP_ONLY ? formatIp(_stack.accessPointIp())
                                            : formatIp(_stack.stationIp());
}

void WifiControl::loop()
{
    if (WIFI_MODE == WIFI_MODE_DISABLED)
    {
        return;
    }

    if (WIFI_MODE != WIFI_MODE_AP_ONLY)
    {
        if (_status != _stack.status())
        {
            _status = _stack.status();
            LOG(DEBUG_WIFI, "[WIFI]: Connected status changed to %s", wifiStatus(_status).c_str());
            if (_status == hal::WifiStatus::Connected)
            {
                establishServers();
                LOG(DEBUG_WIFI,
                    "[WIFI]: Connecting to SSID %s at %s:%d",
                    WIFI_INFRASTRUCTURE_MODE_SSID,
                    formatIp(_stack.stationIp()).c_str(),
                    WIFI_PORT);
            }
        }
        if (_status != hal::WifiStatus::Connected)
        {
            infraToAPFailover();
            return;
        }
    }

    if (_mountTick != nullptr)
    {
        _mountTick();
    }

    tcpLoop();
    udpLoop();
}

void WifiControl::infraToAPFailover()
{
    if (_infraStart != 0 && _stack.status() != hal::WifiStatus::Connected && _infraStart + _infraWait < millis())
    {
        _stack.disconnect();
        startAccessPointMode();
        _infraStart = 0;

        LOG(DEBUG_WIFI, "[WIFI]: Could not connect to Infra, Starting AP.");
    }
}

void WifiControl::tcpLoop()
{
    if (_tcpServer == nullptr)
        return;

    if (_client == nullptr || !_client->connected())
    {
        if (_clientTransport != nullptr)
        {
            _clientTransport->reset();
            // Replies still owed to the peer that just left have nowhere to go.
            _cmdProcessor->transportGone(_clientTransport);
        }

        delete _clientPort;
        delete _client;
        _client = _tcpServer->accept();
        if (_client == nullptr || !_client->connected())
        {
            return;
        }

        // The port and the transport are per-connection, because the accepted socket is a new
        // object every time while the framing state is not.
        _clientPort      = new adapters::TcpConnectionPort(*_client);
        _clientTransport = new adapters::SerialTransport<128>(*_clientPort, true);
    }

    // Framing, the 0x06 handshake reply and the write-back all live in the transport, so this
    // loop is the same shape as the serial one.
    while (_clientTransport->hasCommand())
    {
        char buffer[adapters::SerialTransport<128>::kCommandCapacity] = {0};
        const size_t length                                           = _clientTransport->readCommand(buffer, sizeof(buffer));
        if (length == 0)
        {
            continue;
        }

        LOG(DEBUG_WIFI, "[WIFITCP]: Query <-- %s", buffer);
        const char *retVal = _cmdProcessor->processCommand(String(buffer), _clientTransport);

        if (retVal[0] != '\0')
        {
            _clientTransport->writeResponse(retVal);
            LOG(DEBUG_WIFI, "[WIFITCP]: Reply --> %s", retVal);
        }
        else
        {
            LOG(DEBUG_WIFI, "[WIFITCP]: No Reply");
        }

        if (_mountTick != nullptr)
        {
            _mountTick();
        }
    }

    if (_clientTransport->takeOverflow())
    {
        LOG(DEBUG_WIFI, "[WIFITCP]: Command string reached maximum capacity --> clear");
    }
}

void WifiControl::udpLoop()
{
    if (_udp == nullptr)
        return;

    int packetSize = _udp->parsePacket();
    if (packetSize)
    {
        String lookingFor = "skyfi:";
        String reply      = "skyfi:";
        reply += WIFI_HOSTNAME;
        reply += "@";
        reply += getIP();
        LOG(DEBUG_WIFI,
            "[WIFIUDP]: Received %d bytes from %s, port %d",
            packetSize,
            formatIp(_udp->remoteIp()).c_str(),
            _udp->remotePort());
        char incomingPacket[256];
        int len             = _udp->read(reinterpret_cast<uint8_t *>(incomingPacket), sizeof(incomingPacket) - 1);
        incomingPacket[len] = 0;
        LOG(DEBUG_WIFI, "[WIFIUDP]: Received: %s", incomingPacket);

        incomingPacket[lookingFor.length()] = 0;
        if (lookingFor.equalsIgnoreCase(incomingPacket))
        {
            _udp->beginPacket(_udp->remoteIp(), WIFI_UDP_DISCOVERY_PORT);
            _udp->write(reinterpret_cast<const uint8_t *>(reply.c_str()), reply.length());
            _udp->endPacket();
            LOG(DEBUG_WIFI, "[WIFIUDP]: Replied: %s", reply.c_str());
        }
    }
}
#endif
