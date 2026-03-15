#pragma once

#include <stdarg.h>

// Forward declarations
class Mount;
class LcdMenu;

class MeadeCommandProcessor
{
  public:
    static MeadeCommandProcessor *createProcessor(Mount *mount, LcdMenu *lcdMenu);
    static MeadeCommandProcessor *instance();
    const char *processCommand(String inCmd);

  private:
    MeadeCommandProcessor(Mount *mount, LcdMenu *lcdMenu);
    const char *handleMeadeSetInfo(const String &inCmd);
    const char *handleMeadeMovement(const String &inCmd);
    const char *handleMeadeGetInfo(const String &inCmd);
    const char *handleMeadeGPSCommands(const String &inCmd);
    const char *handleMeadeSyncControl(const String &inCmd);
    const char *handleMeadeHome(const String &inCmd);
    const char *handleMeadeInit(const String &inCmd);
    const char *handleMeadeQuit(const String &inCmd);
    const char *handleMeadeDistance(const String &inCmd);
    const char *handleMeadeSetSlewRate(const String &inCmd);
    const char *handleMeadeExtraCommands(const String &inCmd);
    const char *handleMeadeFocusCommands(const String &inCmd);

    static const char *copyToResponse(const char *src);
    static const char *formatResponse(const char *fmt, ...);
    static const char *copyToResponse_P(const char *pgmSrc);

    Mount *_mount;
    LcdMenu *_lcdMenu;
    static MeadeCommandProcessor *_instance;
    static char _responseBuffer[200];
};
