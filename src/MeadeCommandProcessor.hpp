#pragma once

// Forward declarations
class Mount;
class LcdMenu;

class MeadeCommandProcessor
{
  public:
    static MeadeCommandProcessor *createProcessor(Mount *mount, LcdMenu *lcdMenu);
    static MeadeCommandProcessor *instance();
    String processCommand(String incmd);

  private:
    MeadeCommandProcessor(Mount *mount, LcdMenu *lcdMenu);
    String handleMeadeSetInfo(String inCmd);
    String handleMeadeMovement(String inCmd);
    String handleMeadeGetInfo(String inCmd);
    String handleMeadeGPSCommands(String inCmd);
    String handleMeadeSyncControl(String inCmd);
    String handleMeadeHome(String inCmd);
    String handleMeadeInit(String inCmd);
    String handleMeadeQuit(String inCmd);
    String handleMeadeDistance(String inCmd);
    String handleMeadeSetSlewRate(String inCmd);
    String handleMeadeExtraCommands(String inCmd);
    String handleMeadeFocusCommands(String inCmd);
    Mount *_mount;
    LcdMenu *_lcdMenu;
    static MeadeCommandProcessor *_instance;
};
