#pragma once

#if TEST_VERIFY_MODE == 1
    #include "testmenu.hpp"
    #include "testmenudef.hpp"
#endif

#include "b_setup.hpp"

#if SUPPORT_SERIAL_CONTROL == 1
    #include "MeadeCommandProcessor.hpp"

void processSerialData();

////////////////////////////////////////////////
// The main loop when under serial control
void serialLoop()
{
    mount.loop();
    mount.displayStepperPositionThrottled();

    #ifdef ESP32
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
                            if (index > ARRAY_SIZE(buffer) - 1)
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

void processSerialData()
{
    processTestState();
}
    #else
// ESP needs to call this in a loop :_(
void processSerialData()
{
    char buffer[2];
    while (Serial.available() > 0)
    {
        if (Serial.readBytes(buffer, 1) == 1)
        {
            if (buffer[0] == 0x06)
            {
                LOG(DEBUG_SERIAL, "[SERIAL]: Received: ACK request, replying P");
                    // When not debugging, print the result to the serial port .
                    // When debugging, only print the result to Serial if we're on seperate ports.
        #if (DEBUG_LEVEL == DEBUG_NONE) || (DEBUG_SEPARATE_SERIAL == 1)
                Serial.print('P');
        #endif
            }
            else
            {
                String inCmd = String(buffer[0]) + Serial.readStringUntil('#');
                LOG(DEBUG_SERIAL, "[SERIAL]: ReceivedCommand(%d chars): [%s]", inCmd.length(), inCmd.c_str());

                String retVal = MeadeCommandProcessor::instance()->processCommand(inCmd);
                if (retVal != "")
                {
                    LOG(DEBUG_SERIAL, "[SERIAL]: RepliedWith:  [%s]", retVal.c_str());
                        // When not debugging, print the result to the serial port .
                        // When debugging, only print the result to Serial if we're on seperate ports.
        #if (DEBUG_LEVEL == DEBUG_NONE) || (DEBUG_SEPARATE_SERIAL == 1)
                    Serial.print(retVal);
        #endif
                }
                else
                {
                    LOG(DEBUG_SERIAL, "[SERIAL]: NoReply");
                }
            }
        }

        mount.loop();
    }
}

    #endif
#endif
