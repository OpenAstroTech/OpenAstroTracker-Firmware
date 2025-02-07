#pragma once

#include "b_setup.hpp"

#if SUPPORT_SERIAL_CONTROL == 1
    #include "MeadeCommandProcessor.hpp"

void processSerialData();

////////////////////////////////////////////////
// The main loop when under serial control
void serialLoop()
{
    //Serial.print('.');
    mount.loop();
    mount.displayStepperPositionThrottled();

    #ifdef ESP32
    processSerialData();
    #endif

    #if (WIFI_ENABLED == 1)
    wifiControl.loop();
    #endif
    //Serial.println('x');
}

    //////////////////////////////////////////////////
    // Event that is triggered when the serial port receives data.
    #ifndef ESP32
void serialEvent()
{
    processSerialData();
}
    #endif

// ESP needs to call this in a loop :_(
void processSerialData()
{
    static char buffer[20];
    static unsigned int index = 0;
    while (Serial.available() > 0)
    {
        if (Serial.readBytes((buffer + index), 1) == 1)
        {
            if (buffer[index] == 0x06)
            {
                LOG(DEBUG_SERIAL, "[SERIAL]: Received: ACK request, replying P");
                // When not debugging, print the result to the serial port .
                // When debugging, only print the result to Serial if we're on seperate ports.
    #if (DEBUG_LEVEL == DEBUG_NONE) || (DEBUG_SEPARATE_SERIAL == 1)
                Serial.print('P');
    #endif
                index = 0;
            }
            else
            {
                if (buffer[index] == '#')
                {
                    // Ignoring trailing hash
                    buffer[index] = 0;
                    String inCmd      = String(buffer);
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
                    // Wait for next command
                    index = 0;
                }
                else if (buffer[index] > 31)
                {
                    index++;
                    if (index >= sizeof(buffer))
                    {
                        LOG(DEBUG_SERIAL, "[SERIAL]: Command buffer overflow! Ignoring received data.");
                        index = 0;
                    }
                }
            }
        }

        mount.loop();
    }
}

#endif
