#include "SerialPump.hpp"

#if TEST_VERIFY_MODE == 1
    #include "app/test/TestMenu.hpp"
    #include "app/test/TestMenuDefinitions.hpp"
#endif

#include "types/MathUtil.hpp"
#include "app/BoardWiring.hpp"
#include "app/UiScreens.hpp"

#if SUPPORT_SERIAL_CONTROL == 1
    #include "adapters/MeadeCommandAdapter.hpp"
    #include "adapters/SerialTransport.hpp"
    #include "hal/arduino/ArduinoSerialPort.hpp"

////////////////////////////////////////////////
// The main loop when under serial control
void serialLoop()
{
    mount.loop();
    if (menuDisplayPort != nullptr)
    {
        app::slewStatusView.renderThrottled(*ui.read, true, ui.clock->now(), *menuDisplayPort);
    }
    serviceInfoDisplay();

    // Deferred Meade replies whose deadline ran out while nothing else was talking. The
    // event-driven half is delivered by `mount.loop()` above.
    if (MeadeCommandAdapter *processor = MeadeCommandAdapter::instance())
    {
        processor->tick();
    }

    #if defined(ESP32)
    processSerialData();
    #endif

    #if (WIFI_ENABLED == 1)
    wifiControl.loop();
    #endif

    #if TEST_VERIFY_MODE == 1
    mainTestMenu.tick();
    #endif
}

    //////////////////////////////////////////////////
    // Event that is triggered when the serial port receives data.
    #ifndef ESP32
void serialEvent()
{
    processSerialData();
}
    #endif

    #if TEST_VERIFY_MODE == 1

namespace
{
void processTestState()
{
    static char buffer[32];
    static unsigned int index = 0;
    switch (TestMenu::getMenuState())
    {
        case testMenuState_t::CLEAR:
            TestMenu::setMenuState(testMenuState_t::WAITING_ON_INPUT);
            break;

        case testMenuState_t::WAITING_ON_INPUT:
            while (Serial.available() > 0)
            {
                if (Serial.readBytes(buffer, 1) == 1)
                {
                    if ((buffer[0] >= '0') && (buffer[0] <= '9'))
                    {
                        Serial.println(buffer[0]);
                        int pressedKey = buffer[0] - '0';
                        TestMenu::getCurrentMenu()->onKeyPressed(pressedKey);
                    }
                }
            }
            break;

        case testMenuState_t::WAITING_ON_COMMAND:
            while (Serial.available() > 0)
            {
                char ch;
                if (Serial.readBytes(&ch, 1) == 1)
                {
                    if (isascii(ch))
                    {
                        buffer[index] = ch;
                        if (ch == '#')
                        {
                            buffer[index + 1] = '\0';
                            TestMenu::getCurrentMenu()->onCommandReceived(buffer);
                            TestMenu::setMenuState(testMenuState_t::WAITING_ON_INPUT);
                            TestMenu::getCurrentMenu()->display();
                            index = 0;
                        }
                        else
                        {
                            index++;
                            if (index > types::arraySize(buffer) - 1)
                            {
                                Serial.println(F("Buffer overflow, too many chars received"));
                                index = 0;
                            }
                        }
                    }
                }
            }
            break;
    }
}

}  // namespace

void processSerialData()
{
    processTestState();
}
    #else
/// The serial control path as a transport: framing, the ACK probe and the reply write live in
/// `adapters::SerialTransport`, so the loop below only moves commands to the adapter.
///
/// The ACK reply is suppressed when the debug log shares this port, which is the `#if` this
/// used to carry inline; with the log on the same wire a stray `P` would interleave with a
/// log line.
namespace
{
adapters::SerialTransport<> &serialTransport()
{
    static hal::ArduinoSerialPort port(Serial);
    static adapters::SerialTransport<> transport(port, (DEBUG_LEVEL == DEBUG_NONE) || (DEBUG_SEPARATE_SERIAL == 1));
    return transport;
}
}  // namespace

// ESP needs to call this in a loop :_(
void processSerialData()
{
    adapters::SerialTransport<> &transport = serialTransport();

    while (transport.hasCommand())
    {
        char buffer[adapters::SerialTransport<>::kCommandCapacity] = {0};
        const size_t length                                        = transport.readCommand(buffer, sizeof(buffer));
        if (length == 0)
        {
            // The command did not fit the buffer; the transport already dropped it.
            continue;
        }

        const String inCmd = String(buffer);
        LOG(DEBUG_SERIAL, "[SERIAL]: ReceivedCommand(%d chars): [%s]", inCmd.length(), inCmd.c_str());

        const char *retVal = MeadeCommandAdapter::instance()->processCommand(inCmd, &transport);
        if (retVal[0] != '\0')
        {
            LOG(DEBUG_SERIAL, "[SERIAL]: RepliedWith:  [%s]", retVal);
            transport.writeResponse(retVal);
        }
    }

    if (transport.takeOverflow())
    {
        LOG(DEBUG_SERIAL, "[SERIAL]: Command buffer overflow! Ignoring received data.");
    }
}

    #endif
#endif
