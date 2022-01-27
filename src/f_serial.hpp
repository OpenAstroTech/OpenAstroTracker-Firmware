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
                LOGV1(DEBUG_SERIAL, F("[SERIAL]: Received: ACK request, replying 1"));
    #if DEBUG_LEVEL == DEBUG_NONE
                Serial.print('1');
    #endif
            }
            else
            {
                String inCmd = String(buffer[0]) + Serial.readStringUntil('#');
                LOGV3(DEBUG_SERIAL, F("[SERIAL]: ReceivedCommand(%d chars): [%s]"), inCmd.length(), inCmd.c_str());

                String retVal = MeadeCommandProcessor::instance()->processCommand(inCmd);
                if (retVal != "")
                {
                    LOGV2(DEBUG_SERIAL, F("[SERIAL]: RepliedWith:  [%s]"), retVal.c_str());
    #if DEBUG_LEVEL == DEBUG_NONE
                    Serial.print(retVal);
    #endif
                }
                else
                {
                    LOGV1(DEBUG_SERIAL, F("[SERIAL]: NoReply"));
                }
            }
        }

        mount.loop();
    }
}

#endif
