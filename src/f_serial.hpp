#pragma once

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
    char buffer[2];
    while (Serial.available() > 0)
    {
        if (Serial.readBytes(buffer, 1) == 1)
        {
            if (buffer[0] == 0x06)
            {
                LOG(DEBUG_SERIAL, "[SERIAL]: Received: ACK request, replying 1");
                // When not debugging, print the result to the serial port .
                // When debugging, only print the result to Serial if we're on seperate ports.
    #if (DEBUG_LEVEL == DEBUG_NONE) || (DEBUG_SEPARATE_SERIAL == 1)
                Serial.print('1');
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
