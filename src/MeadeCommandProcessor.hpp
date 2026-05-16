#pragma once

#include "core/MeadeResponse.hpp"

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

    // Persist a freshly-built response across the handler return.
    // The returned pointer is valid until the next call to `store`.
    const char *store(oat::core::meade::MeadeResponse response);
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

    Mount *_mount;
    LcdMenu *_lcdMenu;
    static MeadeCommandProcessor *_instance;
    oat::core::meade::MeadeResponse _response;
};
