#pragma once

#include "../Configuration.hpp"
#include "EPROMStore.hpp"

#if USE_GPS == 1

    #if DEBUG_LEVEL & DEBUG_GPS
char gpsBuf[256];
int gpsBufPos = 0;
    #endif

long lastGPSUpdate = 0;
bool gpsAqcuisitionComplete(int &indicator)
{
    while (GPS_SERIAL_PORT.available())
    {
        int gpsChar = GPS_SERIAL_PORT.read();

    #if DEBUG_LEVEL & DEBUG_GPS
        if ((gpsBufPos < 254) && (gpsChar > 31))
        {
            gpsBuf[gpsBufPos++] = gpsChar;
        }
    #endif
        if (gpsChar == 36)
        {
            // $ (ASCII 36) marks start of message, so we switch indicator every message
            if (millis() - lastGPSUpdate > 500)
            {
                indicator     = adjustWrap(indicator, 1, 0, 3);
                lastGPSUpdate = millis();
            }
        }
        if (gps.encode(gpsChar))
        {
    #if DEBUG_LEVEL & DEBUG_GPS
            gpsBuf[gpsBufPos++] = 0;
            LOGV2(DEBUG_GPS, F("GPS: Sentence: [%s]"), gpsBuf);
            gpsBufPos = 0;
    #endif

            LOGV4(DEBUG_GPS,
                  F("GPS: Encoded. %l sats, Location is%svalid, age is %lms"),
                  gps.satellites.value(),
                  (gps.location.isValid() ? " " : " NOT "),
                  gps.location.age());
            // Make sure we got a fix in the last 30 seconds
            if ((gps.location.lng() != 0) && (gps.location.age() < 30000UL))
            {
                LOGV2(DEBUG_INFO, F("GPS: Sync'd GPS location. Age is %d secs"), gps.location.age() / 1000);
                LOGV3(DEBUG_INFO, F("GPS: Location: %f  %f"), gps.location.lat(), gps.location.lng());
                LOGV4(DEBUG_INFO, F("GPS: UTC time is %dh%dm%ds"), gps.time.hour(), gps.time.minute(), gps.time.second());
                lcdMenu.printMenu("GPS sync'd....");

                DayTime utcNow = DayTime(gps.time.hour(), gps.time.minute(), gps.time.second());
                utcNow.addHours(mount.getLocalUtcOffset());
                mount.setLocalStartTime(utcNow);
                mount.setLocalStartDate(gps.date.year(), gps.date.month(), gps.date.day());
                mount.setLatitude(gps.location.lat());
                mount.setLongitude(gps.location.lng());

                mount.delay(500);

                return true;
            }
        }
    }
    return false;
}

    #if DISPLAY_TYPE > 0

// States that HA menu displays goes through
enum haMenuState_t
{
    SHOWING_HA_SYNC = 1,
    SHOWING_HA_SET,
    ENTER_HA_MANUALLY,
    STARTING_GPS,
};

int indicator         = 0;
haMenuState_t haState = STARTING_GPS;

bool processHAKeys()
{
    lcdButton_t key;
    bool waitForRelease = false;

    if (haState == STARTING_GPS)
    {
        if (gpsAqcuisitionComplete(indicator))
        {
            LOGV1(DEBUG_INFO, F("HA: GPS acquired"));
            GPS_SERIAL_PORT.end();
            haState = SHOWING_HA_SYNC;
        #if SUPPORT_GUIDED_STARTUP == 1
            if (startupState == StartupWaitForHACompletion)
            {
                LOGV1(DEBUG_INFO, F("HA: We were in startup, so confirm HA"));
                startupState = StartupHAConfirmed;
                inStartup    = true;
            }
        #endif
        }
    }

    if (lcdButtons.keyChanged(&key))
    {
        waitForRelease = true;
        LOGV3(DEBUG_INFO, F("HA: Key %d was pressed in state %d"), key, haState);
        if (haState == SHOWING_HA_SYNC)
        {
            if (key == btnSELECT)
            {
                haState = STARTING_GPS;
                GPS_SERIAL_PORT.begin(GPS_BAUD_RATE);
            }
            else if ((key == btnUP) || (key == btnDOWN))
            {
                haState = SHOWING_HA_SET;
            }
        }
        else if (haState == SHOWING_HA_SET)
        {
            if (key == btnSELECT)
            {
                haState = ENTER_HA_MANUALLY;
            }
            else if ((key == btnUP) || (key == btnDOWN))
            {
                haState = SHOWING_HA_SYNC;
            }
        }
        else if (haState == ENTER_HA_MANUALLY)
        {
            if (key == btnSELECT)
            {
                DayTime ha(mount.HA());
                EEPROMStore::storeHATime(mount.HA());
                lcdMenu.printMenu("Stored.");
                mount.delay(500);
                haState = SHOWING_HA_SET;
        #if SUPPORT_GUIDED_STARTUP == 1
                if (startupState == StartupWaitForHACompletion)
                {
                    startupState = StartupHAConfirmed;
                    inStartup    = true;
                }
        #endif
            }
            else if (key == btnUP)
            {
                DayTime ha(mount.HA());
                if (HAselect == 0)
                    ha.addHours(1);
                if (HAselect == 1)
                    ha.addMinutes(1);
                mount.setHA(ha);
            }
            else if (key == btnDOWN)
            {
                DayTime ha(mount.HA());
                if (HAselect == 0)
                    ha.addHours(-1);
                if (HAselect == 1)
                    ha.addMinutes(-1);
                mount.setHA(ha);
            }
            else if (key == btnLEFT)
            {
                HAselect = adjustWrap(HAselect, 1, 0, 1);
            }
        }

        if (key == btnRIGHT)
        {
            LOGV1(DEBUG_INFO, F("HA: Right Key was pressed"));
            if (haState == STARTING_GPS)
            {
                LOGV1(DEBUG_INFO, F("HA: In GPS Start mode, switching to manual"));
                GPS_SERIAL_PORT.end();
                haState = SHOWING_HA_SYNC;
            }
        #if SUPPORT_GUIDED_STARTUP == 1
            else if (startupState == StartupWaitForHACompletion)
            {
                LOGV1(DEBUG_INFO, F("HA: In Startup, not in GPS Start mode, leaving"));
                startupState = StartupHAConfirmed;
                inStartup    = true;
            }
        #endif
            else
            {
                LOGV1(DEBUG_INFO, F("HA: leaving HA"));
                lcdMenu.setNextActive();
            }
        }
    }

    return waitForRelease;
}

void printHASubmenu()
{
    const char *ind = "*+* ";
    char satBuffer[20];
    if (haState == SHOWING_HA_SYNC)
    {
        sprintf(satBuffer, "%02dh %02dm >Sync", mount.HA().getHours(), mount.HA().getMinutes());
    }
    else if (haState == SHOWING_HA_SET)
    {
        sprintf(satBuffer, "%02dh %02dm >Set", mount.HA().getHours(), mount.HA().getMinutes());
    }
    else if (haState == STARTING_GPS)
    {
        sprintf(satBuffer, "  Found %u sats", static_cast<unsigned>(gps.satellites.value()));
        satBuffer[0] = ind[indicator];
    }
    else if (haState == ENTER_HA_MANUALLY)
    {
        sprintf(satBuffer, " %02dh %02dm", mount.HA().getHours(), mount.HA().getMinutes());
        satBuffer[HAselect * 4] = '>';
    }
    lcdMenu.printMenu(satBuffer);
}

    #endif
#endif
